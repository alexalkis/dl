/*
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ============================================================================
 Name        : dl.c
 Author      : Alkis Argiropoulos
 Version     :
 Copyright   : Free
 Description : An ls experiment, trying to keep it working in KS1.3
 ============================================================================
 test 10000  files
 dir  490328   (49 bytes per entry) 27.465249s
 ls  2768336  (277 bytes per entry) 47.194482s
 dl  3463296  (346 bytes per entry)	38.662612
 list    							73.26


 test2 1000 files
 dl 297896 (298 bpe)  	3.66s
 dir 74232 ( 74 bpe)  	2.65s
 ls 323096 (323 bpe)  	1.88s
 list					7.38s
 */
#define __USE_SYSBASE

#include <stdarg.h>

#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include "string.h"
#include "stdio.h"
#include "sort.h"
#include "pattern.h"
#include "human.h"

void GetWinBounds(register int *w __asm("a0"), register int *h __asm("a1"));

char *dates(char *s, struct DateStamp *dss);
char *times(char *s, struct DateStamp *dss);


void dir(struct pending *pend);

int containsWildchar(char *text);
char *getDirectory(char *text);
void testBreak(void);
int parseSwitches(char *filedir);

#define VERSION_STRING "1.0"
BYTE version[] = "\0$VER: dl " VERSION_STRING " (" __DATE__ ")";

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static struct DateStamp Now;

enum SIZEMODE {
	SIZE_NORMAL = 0, SIZE_BLOCKS
};
enum TIMEDATEMODE {
	TIMEDATE_NORMAL = 0, TIMEDATE_HUMAN, TIMEDATE_FULL
};
int gSort = SORT_FILENAME;

int gReverse = 0;

int gTimeDateFormat = TIMEDATE_NORMAL;

extern bool recursive;
static bool immediate_dirs = 0;
unsigned long bsize = 0;
bool print_dir_name;
LONG errno;

/* If true, the file listing format requires that stat be called on each file.  */
extern bool format_needs_stat;
/* Similar to 'format_needs_stat', but set if only the file type is needed.  */
extern bool format_needs_type;
extern bool print_block_size;
extern bool print_with_color;
extern bool print_dir_name;
extern bool print_inode;
extern size_t cwd_n_used;
extern size_t max_idx;
extern enum indicator_style indicator_style;
extern bool directories_first;
extern enum Dereference_symlink dereference;
extern int human_output_opts;
extern int line_length;
extern int output_block_size;
extern int file_human_output_opts;
extern int file_output_block_size;

extern struct pending *pending_dirs;
size_t tabsize = 8;
extern struct highlight highlight_tabx13[13], *highlight_tab;
int OSVersion;

//extern APTR OldWindowPtr;

struct highlight highlight_cursor = { "\x9b" " p", "\x9b" "0 p", 0 };
int windowWidth = 77;
int windowHeight = 30;
int nDirs, nFiles, nTotalSize, ntotal_blocks;
int gDirs, gFiles, gTotalSize, gtotal_blocks;
int summarise = 0;

extern char *arg0;

int dl(register char *cliline __asm("a0"), register int linelen __asm("d0"))
{
	DateStamp(&Now);
	char *arg;
	GetWinBounds(&windowWidth, &windowHeight);
	max_idx = MAX(1, windowWidth / 3); //MIN_COLUMN_WIDTH);
	OSVersion = (SysBase->LibNode.lib_Version > 34) ? 20 : 13;
//	struct Process *procp = (struct Process *) FindTask(0L);
//	OldWindowPtr = procp->pr_WindowPtr;
//	procp->pr_WindowPtr = (APTR) -1L; /* Shutdown requesters */
//	stderr =
//			((struct CommandLineInterface *) BADDR(procp->pr_CLI))->cli_StandardOutput;

//	struct CommandLineInterface *cli=((struct CommandLineInterface *)BADDR(procp->pr_CLI));
//		bprint(cli->cli_CommandName);
//		bprint(cli->cli_SetName);
//		bprint(cli->cli_Prompt);
//		bprint(cli->cli_CommandFile);
	if (OSVersion == 13) {
		highlight_tab = &highlight_tabx13[7];
	}
	gDirs = gFiles = gTotalSize = gtotal_blocks = 0;
	printf("%s", highlight_cursor.off);
	cliline[linelen - 1] = '\0';
	//printf("going in..,\n");bflush();
	print_block_size = false;
	print_inode = false;
	print_dir_name = true;
	format = many_per_line;
	line_length = windowWidth;
	int goon = parseSwitches(cliline);
	//printf("got out..,\n");bflush();

	format_needs_stat = sort_type == sort_time || sort_type == sort_size
			|| format == long_format || print_block_size;
	format_needs_type = (!format_needs_stat
			&& (recursive || print_with_color || indicator_style != none
					|| directories_first));
	if (dereference == DEREF_UNDEFINED)
		dereference = (
				(immediate_dirs || indicator_style == classify
						|| format == long_format) ?
						DEREF_NEVER : DEREF_COMMAND_LINE_SYMLINK_TO_DIR);

	//Done: Recursion  (Seems to work, not fully tested...)
	//TODO: Handle the non-existant shell substitution of amiga's CLI
	//this will be new
	int n_files = 0;
	if (goon != -1) {
		cliline = &cliline[goon];
		arg = strtok(cliline, " ");
		if (init_structures()) {
			//int seenMany = 0;
			do {
				if (!strlen(arg) || !strcmp(arg, "\"\"")) {
					if (immediate_dirs)
						gobble_file("", directory, NOT_AN_INODE_NUMBER, true,
								"");
					else
						queue_directory("", NULL, true);
				} else {
					if (containsWildchar(arg))
						queue_directory(arg, NULL, true);
					else
						gobble_file(arg, unknown, NOT_AN_INODE_NUMBER, true,
								"");
				}
				testBreak();
				++n_files;
			} while ((arg = strtok(NULL, " ")) && !gotBreak);
			if (!gotBreak) {
				if (cwd_n_used) {
					sort_files();
					if (!immediate_dirs)
						extract_dirs_from_files(NULL, true);
				}
				/* 'cwd_n_used' might be zero now.  */
				if (cwd_n_used) {
					print_current_files();
					if (pending_dirs)
						putchar('\n');
					//if (pending_dirs) printf("huh?%s\n",pending_dirs->realname);
				} else if (n_files <= 1 && pending_dirs
						&& pending_dirs->next == 0)
					print_dir_name = false;
				struct pending *thispend;
				while (pending_dirs && !gotBreak) {

//					int c = 0;
//					for (thispend = pending_dirs; thispend;
//							thispend = thispend->next) {
//						printf("%d \"%s\"\n", ++c, thispend->name);
//					}

					thispend = pending_dirs;
					pending_dirs = pending_dirs->next;

					//if (LOOP_DETECT)
					//{
					if (thispend->name == NULL) {
						/* thispend->name == NULL means this is a marker entry
						 indicating we've finished processing the directory.
						 Use its dev/ino numbers to remove the corresponding
						 entry from the active_dir_set hash table.  */
						//struct dev_ino di = dev_ino_pop ();
						//struct dev_ino *found = hash_delete (active_dir_set, &di);
						/* ASSERT_MATCHING_DEV_INO (thispend->realname, di); */
						//assert (found);
						//dev_ino_free (found);
						//printf("free..--->%s\n",thispend->realname);
						free_pending_ent(thispend);
						continue;
					}
					//}
					//myerror("Calling dir with \"%s\" (%s) (%s)\n",thispend->name,thispend->realname,thispend->command_line_arg);
					clear_files();
					//myerror("Dir: %s\n",thispend->name);
					dir(thispend);
					//myerror("Got back from Dir...\n");
					if (print_dir_name)
						printf("%s%s%s\n", highlight_tab[HI_USERDIR].on,
								*thispend->name ?
										thispend->name : thispend->realname,
								highlight_tab[HI_USERDIR].off);
					if (format == long_format || print_block_size) {
						const char *p;
						char buf[LONGEST_HUMAN_READABLE + 1];
						p = human_readable(ntotal_blocks, buf,
								human_output_opts,
								ST_NBLOCKSIZE, output_block_size);
//						if (pending_dirs && !seenMany)
//							seenMany = 1;
//						if (seenMany)
//							printf("%s\n", thispend->name);
						printf("total %s\n", p);

					}

					if (cwd_n_used)
						print_current_files();
					if (summarise) {
						printf("Dirs: %ld Files: %ld Blocks: %ld Bytes: %ld\n",
								nDirs, nFiles, ntotal_blocks, nTotalSize);
						++summarise;
					}
					gDirs += nDirs;
					gFiles += nFiles;
					gtotal_blocks += ntotal_blocks;
					gTotalSize += nTotalSize;

					//print_dir (thispend->name, thispend->realname,thispend->command_line_arg);

					free_pending_ent(thispend);
					print_dir_name = true;
				}
				clear_files();
			}
			free_structures();
		}
	}
	if (summarise > 2) {
		printf("\2331mDirs: %ld Files: %ld Blocks: %ld Bytes: %ld\2330m\n",
				gDirs, gFiles, gtotal_blocks, gTotalSize);
	}
	printf("%s", highlight_cursor.on);
	return 0;
}

/* Parses the command line, acts on switches
 * Returns -1 for stop, else the position in cmdline that we should parse from
 */
int parseSwitches(char *filedir)
{
	char *save = filedir;
	int notSeenEnd;
	bool kibibytes_specified = false;
	//printf("Before switch parsing '%s'\n", filedir);
	while (*filedir) {
		while (*filedir == ' ') //skip space
			++filedir;
		if (filedir[0] == '-') {
			notSeenEnd = 1;
			char *f = &filedir[1];
			while (*f && notSeenEnd) {
				switch (*f) {
				case 'd':
					immediate_dirs = 1;
					break;
				case 'r':
					gReverse = 1;
					break;
				case 'S':
					gSort = SORT_SIZE;
					break;
				case 't':
					gSort = SORT_DATE;
					break;
				case 'i':
					print_inode = true;
					break;
				case 'k':
					kibibytes_specified = true;
					break;
				case 'f':
					gSort = SORT_NONE;
					break;
				case 'h':
					file_human_output_opts = human_output_opts = human_autoscale
							| human_SI | human_base_1024;
					file_output_block_size = output_block_size = 1;
					gTimeDateFormat = TIMEDATE_HUMAN;
					break;
				case 'l':
					format = long_format;
					break;
				case 'm':
					format = with_commas;
					break;
				case 's':
					print_block_size = true;
					//gSize = SIZE_BLOCKS;
					break;
				case 'x':
					format = horizontal;

					break;
				case '1':
					/* -1 has no effect after -l.  */
					if (format != long_format)
						format = one_per_line;
					break;
				case 'R':
					recursive = true;
					break;
				case 'X':
					gSort = SORT_FILENAME_EXTENSION;
					break;
				case '-':
					if (!strcmp(f + 1, "version")) {
						printf(
								"dl Version "VERSION_STRING" (" __DATE__ ")\nWritten by Alex Argiropoulos\n\n"
								"Uses fpattern 1.08, Copyright (C) 1997-1998 David R. Tribble\n"
								"Uses parts from LS by Justin V. McCormick\n"
								"Uses parts from ls (GNU coreutils) 8.13\n"
								"Copyright (C) 2011 Free Software Foundation, Inc.\n"
								"License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
								"This is free software: you are free to change and redistribute it.\n"
								"There is NO WARRANTY, to the extent permitted by law.\n"
								"Written by Richard M. Stallman and David MacKenzie.\n\n");
						return -1;
					} else if (!strncmp(f + 1, "time=full", 9)) {
						gTimeDateFormat = TIMEDATE_FULL;
						f += 9;
					} else if (!strncmp(f + 1, "sum", 3)) {
						summarise = 1;
						f += 3;
					} else if (!strncmp(f + 1, "group-directories-first", 23)) {
						directories_first = true;
						f += 23;
						break;
					} else if (!strncmp(f + 1, "help", 9)) {
						printf(
								"dl Version "VERSION_STRING" (" __DATE__ ")\n"
								"Written by Alex Argiropoulos\n\n"
								"-1 list one file per line\n"
								"-d list directory entries instead of contents\n"
								"-f do not sort\n"
								"-h print sizes in human readable format\n"
								"-i print the index number of each file\n"
								"-k use 1024-byte blocks\n"
								"-l use a long listing format\n"
								"-m fill width with a comma separated list of entries\n"
								"-R list subdirectories recursively\n"
								"-r reverse order while sorting\n"
								"-s print the allocated size of each file, in blocks\n"
								"-S sort by file size\n"
								"-t sort by modification time, newest first\n"
								"-x list entries by lines instead of by columns\n"
								"-X sort alphabetically by entry extension\n"
								"--group-directories-first group directories before files\n"
								"--help display this help and exit\n"
								"--sum show summary of dirs, files, blocks and bytes\n"
								"--time=full display time verbose\n"
								"--version output version information and exit\n"

								);
						return -1;
					} else {
						myerror("%s: invalid option '-%s'\n", arg0, f);
						return -1;
					}
					//printf("%s <------\n",f+1);
					break;
				case ' ':
				case '\0':
					filedir = f;
					notSeenEnd = 0;
					break;
				default:
					myerror("%s: invalid option '%lc'\n", arg0, *f);
					gotBreak = 1;
					return -1;
				}
				++f;
			}
			filedir = f;
		} else {
			break;
		}
	}

	if (!output_block_size) {
		output_block_size = 512;
		if (kibibytes_specified) {
			human_output_opts = 0;
			output_block_size = 1024;
		}
	}
	//myerror("output_block_size: %ld\n",output_block_size);
	return filedir - save;
}

/* numBlocks -- work out how many blocks a file takes */
unsigned long numBlocks(const LONG size)
{
	unsigned long blocks;

	/* number of data blocks + fixed file overhead */
	blocks = ((size + bsize - 1) / bsize) + 1;
	/* add in header blocks */
	blocks += (blocks / 72); //72=512/sizeof(long)-56header
	return blocks;
}

void dir(struct pending *pend)
{
	__aligned struct FileInfoBlock fib;
	__aligned struct InfoData infodata;
	char *dir;
	char *file = "";
	BPTR lock = 0L;
	int noPattern;
	static bool first = true;

	if (recursive || print_dir_name) {
		if (!first)
			putchar('\n');
		first = false;
	}
	nDirs = nFiles = nTotalSize = ntotal_blocks = 0;

	if (containsWildchar(pend->name)) {
		dir = getDirectory(pend->name);
		file = pend->name + strlen(dir);
		noPattern = 0;
	} else {
		dir = pend->name;
		noPattern = 1;
	}

	//myerror("dir: %s file: %s %ld\n",dir,file,noPattern);
	if (!(lock = Lock(dir, SHARED_LOCK))) {
		myerror("%s: cannot access %s - No such file or directory\n", arg0,
				pend->name);
		return;
	}
	if (!bsize) {
		if (Info(lock, &infodata)) {
			bsize = infodata.id_BytesPerBlock;
		} else {
			bsize = 512;
			myerror("%s: error on Info(), assuming 512bytes blocks. [%ld]\n",
					arg0, IoErr());
		}
	}

	if (Examine(lock, &fib)) {
		/* We check for pend->name=="", in that case get name from fib */
		if (!*pend->name) {
			pend->realname = strdup(fib.fib_FileName);
		}
		while (ExNext(lock, &fib) && !gotBreak) {
			testBreak();
			if ((fpattern_match(file, fib.fib_FileName) || noPattern)
					&& !gotBreak) {
				ntotal_blocks += numBlocks(fib.fib_Size);
				nTotalSize += fib.fib_Size;
				if (fib.fib_DirEntryType <= 0) {
					++nFiles;
				} else {
					++nDirs;
					fib.fib_Size = 0; ///kludge to fix the 'sorting by size' issue
				}
				addEntry(&fib);
			}
		}
	} else {
		myerror("Hmm, couldn't examine\nUse the Source Luke and fix it!\n");
	}
	UnLock(lock);
	/* Sort the directory contents.  */
	sort_files();
	if (recursive && !gotBreak)
		extract_dirs_from_files(dir, false);
}

void testBreak(void)
{
	ULONG oldsig = SetSignal(0L, (LONG)SIGBREAKF_CTRL_C);
	if ((oldsig & SIGBREAKF_CTRL_C) != 0) {
		bflush();
		myerror("\2330m\233 p***BREAK\n");
		//zapBuffer();
		gotBreak = 1;
	}
}

char *dates(char *s, struct DateStamp *dss)
{
	bool old = false;
	int year, month, day;
	static char dpm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	static const char *weekdayname[] =
			{ "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
	static char *nm[12] =
			{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	year = 1978;
	day = dss->ds_Days;

	while (day >= 366) {
		if ((year & 3) == 0 && ((year % 25) != 0 || (year & 15) == 0))
			day -= 366;
		else
			day -= 365;
		++year;
	}
	if ((year & 3) == 0 && ((year % 25) != 0 || (year & 15) == 0))
		dpm[1] = 29;
	else
		dpm[1] = 28;
	for (month = 0; day >= dpm[month]; ++month)
		day -= dpm[month];
	if (gTimeDateFormat == TIMEDATE_FULL) {
		sprintf(s, "%4d-%02d-%02d", year, month + 1, day + 1);
	} else {
		int recent = Now.ds_Days - dss->ds_Days;
		if (recent < 7) {
			if (recent == 0)
				sprintf(s, "%10.10s", "Today");
			else if (recent == 1)
				sprintf(s, "%10.10s", "Yesterday");
			else {
				sprintf(s, "%10.10s", weekdayname[dss->ds_Days % 7]);
				//sprintf(s, "%10.10s", wd(year, month + 1, day + 1));
			}
		} else {

			sprintf(s, "    %02d %3s", day + 1, nm[month]);
			if (recent > 365) {
				/*
				 if (gTimeDateFormat == TIMEDATE_HUMAN)
				 sprintf(s, "%4d years", recent / 365);
				 else
				 sprintf(s, "%3d %s %2d", day + 1, nm[month], year % 100);
				 */
				old = true;
			}
		}
	}
	char *t = s + 10;
	*t++ = ' ';
	if (old) {
		sprintf(t, "%5d", year);
	} else {
		sprintf(t, "%s", times(t, dss));
	}
	return (s);
}

char *times(char *s, struct DateStamp *dss)
{
	int hours, minutes, seconds;
	seconds = dss->ds_Tick / TICKS_PER_SECOND;
	minutes = dss->ds_Minute;
	hours = minutes / 60;
	minutes %= 60;
	if (gTimeDateFormat == TIMEDATE_FULL)
		sprintf(s, "%02d:%02d:%02d.%02d", hours, minutes, seconds,
				2 * (dss->ds_Tick % TICKS_PER_SECOND));
	else
		sprintf(s, "%02d:%02d", hours, minutes);
	return (s);
}

/*
 const char *wd(int year, int month, int day)
 {
 static const char *weekdayname[] =
 { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
 int JND = day + ((153 * (month + 12 * ((14 - month) / 12) - 3) + 2) / 5)
 + (365 * (year + 4800 - ((14 - month) / 12)))
 + ((year + 4800 - ((14 - month) / 12)) / 4)
 - ((year + 4800 - ((14 - month) / 12)) / 100)
 + ((year + 4800 - ((14 - month) / 12)) / 400) - 32045;
 return weekdayname[JND % 7];
 }
 */

int containsWildchar(char *text)
{
	/* these are defined in pattern.h */
	static char patternchars[] = {
	FPAT_NOT,
	FPAT_ANY,
	FPAT_CLOS,
	FPAT_SET_L,
	FPAT_SET_R,
	FPAT_SET_NOT,
	FPAT_SET_THRU };
	int i;
	for (i = 0; patternchars[i]; ++i)
		if (myindex(text, patternchars[i]) != -1)
			return 1;
	return 0;
}

/*
 * returns the non-filename prefix
 * ie: 	df0:foo -> returns df0:
 * 		foo/boo -> returns foo/
 */
char *getDirectory(char *text)
{
	static char buffer[80];
	int lastcolon = -1;
	int lastslash = -1;
	int i;
	//int len = strlen(text);
	for (i = 0; text[i]; ++i) {
		buffer[i] = text[i];
		if (buffer[i] == ':')
			lastcolon = i;
		if (buffer[i] == '/')
			lastslash = i;
	}
	if (lastcolon == -1 && lastslash == -1)
		return "";
	if (lastslash != -1) {
		buffer[lastslash + 1] = '\0';
	} else {
		buffer[lastcolon + 1] = '\0';
	}
	return buffer;
}

