/*
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


 test2 1000
 dl 297896 (298 bpe) 17.368152s  with -Os,
 19.158888s with -O3, wtf?
 16.872603s with -02
 dir 74232 ( 74 bpe)  2.650148s
 ls 323096 (323 bpe)  1.887124s

 */

//m68k-amigaos-gcc -Os -nostdlib -noixemul -fomit-frame-pointer -o hello  mystart.s hello.c
//m68k-amigaos-gcc -Os -nostdlib -noixemul -fomit-frame-pointer -mregparm -o hello mystart.s hello.c
#define __USE_SYSBASE

#include <stdarg.h>

#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include "string.h"
#include "sort.h"
#include "pattern.h"
#include "human.h"

void GetWinBounds(register int *w __asm("a0"), register int *h __asm("a1"));

void myPutStr(char *str);
char *dates(char *s, struct DateStamp *dss);
char *times(char *s, struct DateStamp *dss);
const char *wd(int year, int month, int day);

void showTotals(void);
void Dir(char *filedir);
void displayFib(struct FileInfoBlock *fib);

int ContainsWildchar(char *text);
char *getDirectory(char *text);
void TestBreak(void);
int ParseSwitches(char *filedir);

#define VERSION_STRING "1.0"
BYTE version[] = "\0$VER: dl " VERSION_STRING " (" __DATE__ ")";

struct DosLibrary *DOSBase;
struct ExecBase *SysBase;

static struct DateStamp Now;
int gotBreak = 0;

enum SIZEMODE {
	SIZE_NORMAL = 0, SIZE_BLOCKS
};
enum TIMEDATEMODE {
	TIMEDATE_NORMAL = 0, TIMEDATE_HUMAN, TIMEDATE_FULL
};
int gSort = SORT_FILENAME;

int gReverse = 0;
int gDisplayMode = DISPLAY_NORMAL;
int gTimeDateFormat = TIMEDATE_NORMAL;
int gSize = SIZE_NORMAL;
extern bool recursive;
static bool immediate_dirs = 0;
bool print_dir_name;
LONG errno;

/* If true, the file listing format requires that stat be called on each file.  */
extern bool format_needs_stat;
/* Similar to 'format_needs_stat', but set if only the file type is needed.  */
extern bool format_needs_type;
extern bool print_block_size;
extern bool print_with_color;
extern size_t cwd_n_used;
extern size_t max_idx;
extern enum indicator_style indicator_style;
extern bool directories_first;
extern enum Dereference_symlink dereference;
extern int human_output_opts;

extern int output_block_size;
extern int file_human_output_opts;
extern int file_output_block_size;

extern struct pending *pending_dirs;
size_t tabsize = 8;
extern struct highlight highlight_tabx13[13], *highlight_tab;
int OSVersion;
BPTR StdErr;
struct highlight highlight_cursor = { "\x9b" " p", "\x9b" "0 p", 0 };
int windowWidth = 77;
int windowHeight = 30;
int nDirs, nFiles, nTotalSize, total_blocks;
#define DOSLIB	"dos.library"
#define DOSVER	33L			/* We require AT LEAST V33 of OS */
int hello(register char *cliline __asm("a0"), register int linelen __asm("d0"))
{
	SysBase = (*((struct ExecBase **) 4));
	if ((DOSBase = (struct DosLibrary *) OpenLibrary(DOSLIB, DOSVER))) {
		DateStamp(&Now);
		char *arg;
		GetWinBounds(&windowWidth, &windowHeight);
		max_idx = MAX(1, windowWidth / 3); //MIN_COLUMN_WIDTH);
		OSVersion = (SysBase->LibNode.lib_Version > 34) ? 20 : 13;
		struct Process *procp = (struct Process *) FindTask(0L);
		StdErr =
				((struct CommandLineInterface *) BADDR(procp->pr_CLI))->cli_StandardOutput;
		//struct CommandLineInterface *cli=((struct CommandLineInterface *)BADDR(procp->pr_CLI));
//		myprintf("Commandname: %b\n",cli->cli_CommandName);
//		myprintf("SetName: %b\n",cli->cli_SetName);
//		myprintf("Prompt: %b\n",cli->cli_Prompt);
//		myprintf("CommandFile: %b\n",cli->cli_CommandFile);
		if (OSVersion == 13) {
			highlight_tab = &highlight_tabx13[7];
		}
		bprintf("%s", highlight_cursor.off);
		cliline[linelen - 1] = '\0';
		//bprintf("going in..,\n");bflush();
		print_block_size = false;
		format = many_per_line;
		int goon = ParseSwitches(cliline);
		//bprintf("got out..,\n");bflush();

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

		//this will be new
		if (goon != -1) {
			cliline = &cliline[goon];
			arg = strtok(cliline, " ");
			if (init_structures()) {
				do {
					//Dir(arg);
					gobble_file(arg, unknown, NOT_AN_INODE_NUMBER, true, "");
					//queue_directory ("/", NULL, true);
				} while ((arg = strtok(NULL, " ")) && !gotBreak);
				if (cwd_n_used) {
					sort_files();
					if (!immediate_dirs)
						extract_dirs_from_files(NULL, true);
					/* 'cwd_n_used' might be zero now.  */
					if (cwd_n_used) {
						print_current_files();
						//if (pending_dirs) bprintf("huh?%s\n",pending_dirs->realname);
					}
					struct pending *thispend;
					while (pending_dirs) {

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
							//bprintf("free..\n");
							free_pending_ent(thispend);
							continue;
						}
						//}
						//myprintf("Calling dir with \"%s\" (%s) (%s)\n",thispend->name,thispend->realname,thispend->command_line_arg);
						clear_files();

						Dir(thispend->name);
						/* Sort the directory contents.  */
						sort_files();
						if (format == long_format || print_block_size) {
							const char *p;
							char buf[LONGEST_HUMAN_READABLE + 1];
							p = human_readable(total_blocks, buf,
									human_output_opts,
									ST_NBLOCKSIZE, output_block_size);
							bprintf("total %s\n", p);

						}
						//bprintf("cwd_n_used = %ld\n",cwd_n_used);
						if (cwd_n_used)
							print_current_files();
						//print_dir (thispend->name, thispend->realname,thispend->command_line_arg);

						free_pending_ent(thispend);
						print_dir_name = true;
					}

					clear_files();
				}
				//} while ((arg = strtok(NULL, " ")) && !gotBreak);
				free_structures();
			}
		}
		bprintf("%s", highlight_cursor.on);
		bflush();
		CloseLibrary((struct Library * )DOSBase);
		return 0;
	}
	return 20;
}

void showTotals(void)
{
	if (cwd_n_used)
		bprintf("Files: %ld Size: %ld Blocks: %ld  Directories: %ld\n", nFiles,
				nTotalSize, total_blocks, nDirs);
}

/* Parses the command line, acts on switches
 * Returns -1 for stop, else the position in cmdline that we should parse from
 */
int ParseSwitches(char *filedir)
{
	char *save = filedir;
	int notSeenEnd;

	//bprintf("Before switch parsing '%s'\n", filedir);bflush();
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
					gDisplayMode = DISPLAY_LONG;
					break;
				case 's':
					print_block_size = true;
					//gSize = SIZE_BLOCKS;
					break;
				case 'x':
					gDisplayMode = DISPLAY_HORIZONTAL;
					break;
				case 'X':
					gSort = SORT_FILENAME_EXTENSION;
					break;
				case '-':
					if (!strcmp(f + 1, "version")) {
						myprintf(
								"dl Version 1.0 (" __DATE__ ")\nWritten by Alex Argiropoulos\n\nUses fpattern 1.08, Copyright ©1997-1998 David R. Tribble\n"
								"\nUses parts from:\n"
								"ls (GNU coreutils) 8.13\n"
								"Copyright (C) 2011 Free Software Foundation, Inc.\n"
								"License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
								"This is free software: you are free to change and redistribute it.\n"
								"There is NO WARRANTY, to the extent permitted by law.\n"
								"Written by Richard M. Stallman and David MacKenzie.\n");
						return -1;
					} else if (!strcmp(f + 1, "time=full")) {
						gTimeDateFormat = TIMEDATE_FULL;
						f += 9;
					}
					//myprintf("%s <------\n",f+1);
					break;
				case ' ':
				case '\0':
					filedir = f;
					notSeenEnd = 0;
					break;
				default:
					myerror("dl: invalid option -- '%lc'\n", *f);
					gotBreak = 1;
					return -1;
				}
				++f;
			}
			filedir = f;
		} else {
			//bprintf("final in parse: %s (%ld)\n", filedir, filedir - save);
			return filedir - save;
		}
	}
	return filedir - save;
}

void Dir(char *filedir)
{
	__aligned struct FileInfoBlock fib;
	char *dir;
	char *file = "";
	BPTR lock = 0L;
	int noPattern;

	nDirs = nFiles = nTotalSize = total_blocks = 0;

	if (ContainsWildchar(filedir)) {
		dir = getDirectory(filedir);
		file = filedir + strlen(dir);
		noPattern = 0;
	} else {
		dir = filedir;
		noPattern = 1;
	}

	if (!(lock = Lock(dir, SHARED_LOCK))) {
		myerror("dl: cannot access %s - No such file or directory\n", filedir);
		return;
	}

	//bprintf("Dir: %s File: %s\n",dir,file);
	if (Examine(lock, &fib)) {

		if (fib.fib_DirEntryType <= 0) {
			strcpy(fib.fib_FileName, dir);
			addEntry(&fib);
		} else {
			//if (strlen(filedir))bprintf("%s\n", filedir);
			//myprintf("Directory of %s (%s):\n", fib.fib_FileName, dir);
			while (ExNext(lock, &fib) && !gotBreak) {
				TestBreak();
				//if ((match(file, fib.fib_FileName) || noPattern) && !gotBreak) {
				if ((fpattern_match(file, fib.fib_FileName) || noPattern)
						&& !gotBreak) {
					if (fib.fib_DirEntryType <= 0) {
						nTotalSize += fib.fib_Size;
						++nFiles;
					} else {
						++nDirs;
						fib.fib_Size = 0; ///kludge to fix the 'sorting by size' issue
					}
					addEntry(&fib);
					total_blocks += fib.fib_NumBlocks;
				}
			}

		}
	} else {
		myerror("Hmm, couldn't examine\nUse the Source Luke and fix it!\n");
	}
	UnLock(lock);
}

void displayFib(struct FileInfoBlock *fib)
{
	char s[12], t[12], str[5];
	str[4] = '\0';
	if (gDisplayMode == DISPLAY_LONG) {
		int ds = fib->fib_Size;
		if (fib->fib_DirEntryType <= 0) {
			if (gTimeDateFormat == TIMEDATE_HUMAN && gSize == SIZE_NORMAL) {
				str[1] = str[2] = '\0';
				if (ds < 1024) {
					str[0] = 'b';
					str[1] = ' ';
				} else if (ds < 1024000) {
					ds = (ds + 512) / 1024;
					str[0] = 'k';
					str[1] = 'b';
				} else {
					ds = (ds + 512000) / (1024000);
					str[0] = 'm';
					str[1] = 'b';
				}
				bprintf("%5ld%s ", ds, str);
			} else
				bprintf("%7ld ",
						(gSize == SIZE_NORMAL) ?
								fib->fib_Size : fib->fib_NumBlocks);
		} else {
			bprintf("%7s ", "[dir]");
		}
		str[0] = (fib->fib_Protection & FIBF_READ) ? '-' : 'r';
		str[1] = (fib->fib_Protection & FIBF_WRITE) ? '-' : 'w';
		str[2] = (fib->fib_Protection & FIBF_EXECUTE) ? '-' : 'x';
		str[3] = (fib->fib_Protection & FIBF_DELETE) ? '-' : 'd';
		bprintf("%s %s ", dates(s, &fib->fib_Date), times(t, &fib->fib_Date));

		if (gTimeDateFormat != TIMEDATE_HUMAN)
			bprintf("%s ", str);

//		if (fib->fib_DirEntryType > 0)
//			myPutStr("\x9b" "2m");
//		myPutStr(fib->fib_FileName);
//		if (fib->fib_DirEntryType > 0)
//			myPutStr("\x9b" "22m");
//		myPutStr("\n");
	} else {
		bprintf("Code the short listing.\n");
	}
}

void TestBreak(void)
{
	ULONG oldsig;
	oldsig = SetSignal(0L, (LONG)SIGBREAKF_CTRL_C);
	if ((oldsig & SIGBREAKF_CTRL_C) != 0) {
		myerror("\2330m\233 p***BREAK\n");
		gotBreak = 1;
	}
}

void myPutStr(char *str)
{
	int len = strlen(str);
	Write(Output(), str, len);
}

char *dates(char *s, struct DateStamp *dss)
{

	int year, month, day;
	static char dpm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
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
	int recent = Now.ds_Days - dss->ds_Days;
	if (recent < 7) {
		if (recent == 0)
			mysprintf(s, "%10.10s", "Today");
		else if (recent == 1)
			mysprintf(s, "%10.10s", "Yesterday");
		else
			mysprintf(s, "%10.10s", wd(year, month + 1, day + 1));
	} else {
		if (recent < 365) {
			mysprintf(s, "    %02ld %3s", day + 1, nm[month]);
		} else if (recent < 2 * 365) {
			mysprintf(s, "%5s %4ld", nm[month], year);
		} else {
			if (gTimeDateFormat == TIMEDATE_HUMAN)
				mysprintf(s, "%4ld years", recent / 365);
			else
				mysprintf(s, "%02ld/%02ld/%04ld", day + 1, month + 1, year);
		}
		//mysprintf(s, "%02ld/%02ld/%04ld", day + 1, month + 1, year);
	}
	return (s);
}

char *times(char *s, struct DateStamp *dss)
{
	struct tzones {
		int from;
		int to;
		char *desc;
	};
	// @formatter:off
	struct tzones zones[] =
			{ { 7, 11, "morning" }, { 11, 13, "noon" }, { 13, 20, "afternoon" }, { 20, 23, "night" }, { 23, 23, "midnight" }, { 0, 0, "midnight" }, { 1, 3, "late night" }, { 3, 7, "early morn" } };
	// @formatter:on
	int hours, minutes, seconds;
	int i;

	seconds = dss->ds_Tick / TICKS_PER_SECOND;
	minutes = dss->ds_Minute;
	hours = minutes / 60;
	minutes %= 60;
	//mysprintf(s, "%02ld:%02ld:%02ld", hours, minutes, seconds);
	if (gTimeDateFormat == TIMEDATE_HUMAN) {
		for (i = 0; i < sizeof(zones); ++i)
			if (hours >= zones[i].from && hours <= zones[i].to)
				break;
		mysprintf(s, "%-10.10s", zones[i].desc);
	} else if (gTimeDateFormat == TIMEDATE_FULL)
		mysprintf(s, "%02ld:%02ld:%02ld.%02ld", hours, minutes, seconds,
				2 * (dss->ds_Tick % TICKS_PER_SECOND));
	else
		mysprintf(s, "%02ld:%02ld:%02ld", hours, minutes, seconds);
	return (s);
}

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

int ContainsWildchar(char *text)
{
	static char patternchars[] = "!?*[]-";
	int i;
	int len = strlen(patternchars);
	for (i = 0; i < len; ++i)
		if (myindex(text, patternchars[i]) != -1)
			return 1;
	return 0;
}

char *getDirectory(char *text)
{
	static char buffer[80];
	int lastcolon = -1;
	int lastslash = -1;
	int i;
	int len = strlen(text);
	for (i = 0; i < len; ++i) {
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

