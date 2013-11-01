/*
 * sort.c
 *
 *  Created on: Sep 11, 2013
 *      Author: alex
 */
#include "stdio.h"
#include "sort.h"
#include "human.h"
#include "string.h"

struct column_info {
	bool valid_len;
	size_t line_len;
	size_t *col_arr;
};

/* Array with information about column filledness.  */
static struct column_info *column_info = 0L;
struct pending *pending_dirs = 0L;
struct plist *theplist = NULL;

/* Vector of pointers to files, in proper sorted order, and the number
 of entries allocated for it.  */
static void **sorted_file;
static size_t sorted_file_alloc;
/* The table of files in the current directory:

 'cwd_file' points to a vector of 'struct fileinfo', one per file.
 'cwd_n_alloc' is the number of elements space has been allocated for.
 'cwd_n_used' is the number actually in use.  */

/* Address of block containing the files that are described.  */
static struct fileinfo *cwd_file;

/* Length of block that 'cwd_file' points to, measured in files.  */
static size_t cwd_n_alloc;

/* Index of first unused slot in 'cwd_file'.  */
size_t cwd_n_used;

/* If true, the file listing format requires that stat be called on
 each file.  */
bool format_needs_stat;
/* Similar to 'format_needs_stat', but set if only the file type is
 needed.  */
bool format_needs_type;

enum sort_type sort_type;
enum format format;
bool print_block_size;
bool print_inode;
bool recursive;
bool print_with_color;
int inode_number_width;
int block_size_width;
int file_size_width;
int human_output_opts;
int output_block_size;
int file_human_output_opts;
int file_output_block_size = 1;
int line_length;

enum indicator_style indicator_style;
bool directories_first;
enum Dereference_symlink dereference;

/* these are basically related to fib_DirEntryType after being
 remaped by RemapDirEntryType (). */
struct highlight highlight_tabx[13] = {
/* -7 FILE_DEFAULT */{ "", "", 0 },
/* -6 LINKFILE_EXE */{ "\x9b" "1m", "\x9b" "22m", 0 },
/* -5 FILE_EXE     */{ "", "", 0 },
/* -4 LINKFILE     */{ "\x9b" "1m", "\x9b" "22m", 0 },
/* -3 FILE         */{ "", "", 0 },
/* -2 SOFTLINK_EXE */{ "\x9b" "3m", "\x9b" "23m", 0 },
/* -1 SOFTLINK     */{ "\x9b" "3m", "\x9b" "23m", 0 },
/*  0 COMMENT      */{ "\x9b" "2m/* ", " */\x9b" "22m", 6 },
/*  1 ROOT         */{ "\x9b" "2m", "\x9b" "22m", 0 },
/*  2 USERDIR      */{ "\x9b" "2m", "\x9b" "22m", 0 },
/*  3 LABEL        */{ "\x9b" "2m", "\x9b" "22m", 0 },
/*  4 LINKDIR      */{ "\x9b" "2;1m", "\x9b" "22m", 0 },
/*  5 DIR_DEFAULT  */{ "", "", 0 }, }, *highlight_tab = &highlight_tabx[7];

struct highlight highlight_tabx13[13] = {
/* -7 FILE_DEFAULT */{ "", "", 0 },
/* -6 LINKFILE_EXE */{ "\x9b" "1m", "\x9b" "22m", 0 },
/* -5 FILE_EXE     */{ "", "", 0 },
/* -4 LINKFILE     */{ "\x9b" "1m", "\x9b" "22m", 0 },
/* -3 FILE         */{ "", "", 0 },
/* -2 SOFTLINK_EXE */{ "\x9b" "3m", "\x9b" "23m", 0 },
/* -1 SOFTLINK     */{ "\x9b" "3m", "\x9b" "23m", 0 },
/*  0 COMMENT      */{ "\x9b" "2m/* ", " */\x9b" "22m", 6 },
/*  1 ROOT         */{ "\x9b" "33m", "\x9b" "0m", 0 },
/*  2 USERDIR      */{ "\x9b" "33m", "\x9b" "0m", 0 },
/*  3 LABEL        */{ "\x9b" "33m", "\x9b" "0m", 0 },
/*  4 LINKDIR      */{ "\x9b" "33m", "\x9b" "0m", 0 },
/*  5 DIR_DEFAULT  */{ "", "", 0 }, };
extern int gReverse;
extern int gSort;
extern int gotBreak;

void TestBreak(void);
void shellSort(struct fileinfo **a, int n);
void quicksort_iterative(struct fileinfo **array, unsigned len);
void print_many_per_line(void);
static void print_horizontal(void);
size_t print_file_name_and_frills(const struct fileinfo *f, size_t start_col);
size_t length_of_file_name_and_frills(const struct fileinfo *f);
static void print_long_format(const struct fileinfo *f);
void print_with_commas(void);
void free_plist(struct plist *list);

int init_structures(void)
{
	cwd_n_alloc = 100;
	cwd_file = (struct fileinfo *) mymalloc(cwd_n_alloc * (sizeof *cwd_file));
	cwd_n_used = 0;
	sorted_file_alloc = 0;
	if (cwd_file) {
		return 1;
	}
	return 0;
}

void clear_files(void)
{
	size_t i;

	for (i = 0; i < cwd_n_used; i++) {
		struct fileinfo *f = sorted_file[i];
		free_ent(f);
	}
	cwd_n_used = 0;
	inode_number_width = 0;
	block_size_width = 0;
	file_size_width = 0;
}

void free_structures(void)
{
	if (cwd_file)
		myfree((char * ) cwd_file);
	if (sorted_file)
		myfree((char * ) sorted_file);
	if (column_info) {
		free_plist(theplist);
		myfree((char * )column_info);
	}
	free_pending_ent(pending_dirs);
	cwd_file = sorted_file = column_info = theplist = pending_dirs = NULL;
}

void initialize_ordering_vector(void)
{
	size_t i;
	for (i = 0; i < cwd_n_used; i++)
		sorted_file[i] = &cwd_file[i];
}

#define USESHELLSORT
#ifdef USESHELLSORT
#define sort shellSort
#else
#define sort quicksort_iterative
#endif

void sort_files(void)
{
	if (sorted_file_alloc < cwd_n_used + cwd_n_used / 2) {
		if (sorted_file)
			myfree((char * ) sorted_file);
		sorted_file = (void **) mymalloc(cwd_n_used * 3 * sizeof *sorted_file);
		sorted_file_alloc = 3 * cwd_n_used;
	}
	initialize_ordering_vector();
	if (gSort == SORT_NONE)
		return;
	sort((struct fileinfo **) sorted_file, cwd_n_used);
}

char *file_name_concat(char *dirname, char *name, char *dunno)
{
	int len1 = 0;
	int len2 = strlen(name);
	int spare = 0;
	/* cover the dirname="" case first */
	if (!strlen(dirname))
		return strdup(name);

	if (dirname) {
		len1 = strlen(dirname);
		if (dirname[len1 - 1] != '/' && dirname[len1 - 1] != ':')
			spare = 1;
	}
	char *new = mymalloc(len1 + len2 + 1 + spare);
	if (new) {
		if (len1) {
			memcpy(new, dirname, len1);
			if (spare)
				new[len1] = '/';
		}
		strcpy(new + len1 + spare, name);
	}
	return new;
}
void extract_dirs_from_files(char const *dirname, bool command_line_arg)
{
	size_t i;
	size_t j;

	if (dirname) // && LOOP_DETECT)
	{
		/* Insert a marker entry first.  When we dequeue this marker entry,
		 we'll know that DIRNAME has been processed and may be removed
		 from the set of active directories.  */
		//myerror("queuing \"%s\" ...\n", dirname);
		queue_directory(NULL, dirname, false);
	}

//	for (i = cwd_n_used; i-- != 0;) {
//			struct fileinfo *f = sorted_file[i];
//			myerror("Index: %ld/%ld Filename = \"%s\"\n",i,cwd_n_used,f->fib.fib_FileName);
//	}
	/* Queue the directories last one first, because queueing reverses the
	 order.  */
	for (i = cwd_n_used; i-- != 0;) {
		struct fileinfo *f = sorted_file[i];

		if (f->fib.fib_EntryType > 0) {
			//myerror("Index: %ld/%ld Filename = \"%s\"\n",i,cwd_n_used,f->fib.fib_FileName);
			if (!dirname) {
				///* Sort the directory contents.  */
				//myerror("2. queuing \"%s\" ...command_line_arg=%s\n", dirname,command_line_arg ? "TRUE" : "FALSE");
				queue_directory(f->fib.fib_FileName, 0, command_line_arg);
			} else {
				//myerror("--> %s + %s\n", dirname, f->fib.fib_FileName);
				char *name = file_name_concat(dirname, f->fib.fib_FileName,
				NULL);
				//myerror("----------------------> %s\n", name);
				queue_directory(name, 0, command_line_arg);
				myfree(name);
			}
			if (f->fib.fib_DirEntryType == arg_directory)
				free_ent(f);
		}
	}

	/* Now delete the directories from the table, compacting all the remaining
	 entries.  */

	for (i = 0, j = 0; i < cwd_n_used; i++) {
		struct fileinfo *f = sorted_file[i];
		sorted_file[j] = f;
		j += (f->fib.fib_DirEntryType != arg_directory);
	}
	cwd_n_used = j;
	//printf("j=%ld\n",j);
}

void free_ent(struct fileinfo *f)
{

}
void queue_directory(char const *name, char const *realname,
		bool command_line_arg)
{
	struct pending *new = (struct pending *) mymalloc(sizeof *new);
	if (new) {
		new->realname = realname ? strdup(realname) : NULL;
		new->name = name ? strdup(name) : NULL;
		new->command_line_arg = command_line_arg;
		new->next = pending_dirs;
		pending_dirs = new;
	}
}

void free_pending_ent(struct pending *pend)
{
	if (!pend)
		return;
	if (pend->name)
		myfree(pend->name);
	if (pend->realname)
		myfree(pend->realname);
	myfree((char * )pend);
}

void free_plist(struct plist *list)
{
	while (list) {
		struct plist *next = list->next;
		myfree(list->mem);
		myfree(list);
		list = next;
	}
}

void print_current_files(void)
{
	int i;

	switch (format) {
	case one_per_line:
		for (i = 0; i < cwd_n_used && !gotBreak; i++) {
			print_file_name_and_frills(sorted_file[i], 0);
			putchar('\n');
		}
		break;
	case many_per_line:
		print_many_per_line();
		break;
	case horizontal:
		print_horizontal();
		break;
	case with_commas:
		print_with_commas();
		break;

	case long_format:
		for (i = 0; !gotBreak && i < cwd_n_used; i++) {
			print_long_format((struct fileinfo *) sorted_file[i]);
			putchar('\n');
		}
		break;
	}
	TestBreak();
}

void addEntry(struct FileInfoBlock *fib)
{
	if (cwd_n_used == cwd_n_alloc) {
		//myprintf("Gonna allocate %ld bytes for %ld items...\n",	cwd_n_alloc * 2 * sizeof *cwd_file,cwd_n_alloc * 2);
		cwd_file = (struct fileinfo *) realloc(cwd_file,
				cwd_n_alloc * 2 * sizeof *cwd_file);
		cwd_n_alloc *= 2;
	}

	//myerror("adding %s\n",fib->fib_FileName);
	//Remapping the fib for the highlight table to work
	switch (fib->fib_DirEntryType) {
	case ST_LINKFILE:
	case ST_FILE:
		if ((fib->fib_Protection & FIBF_SCRIPT)
				|| (~fib->fib_Protection & FIBF_EXECUTE))
			fib->fib_DirEntryType -= 2;
		break;
	case ST_ROOT:
	case ST_USERDIR:
	case ST_LINKDIR:
		break;
	case ST_SOFTLINK:
		if ((fib->fib_Protection & FIBF_SCRIPT)
				|| (~fib->fib_Protection & FIBF_EXECUTE))
			fib->fib_DirEntryType = HI_SOFTLINK_EXE;
		else
			fib->fib_DirEntryType = HI_SOFTLINK;
		break;
	default:
		if (fib->fib_DirEntryType >= 0)
			fib->fib_DirEntryType = HI_DIR_DEFAULT;
		else
			fib->fib_DirEntryType = HI_FILE_DEFAULT;
		break;
	}

	cwd_file[cwd_n_used].fib = *fib;
	cwd_n_used++;

	if (format == long_format || print_block_size) {
		char buf[LONGEST_HUMAN_READABLE + 1];
		int len = strlen(
				human_readable(fib->fib_NumBlocks, buf, human_output_opts,
				ST_NBLOCKSIZE, output_block_size));
		if (block_size_width < len) {
			block_size_width = len;
			//myerror("\nnew len: %ld (%ld) \"%s\"\n", len, fib->fib_NumBlocks, buf);
			//if (format==long_format) myerror("long_format\n");
			//if (print_block_size) myerror("print_block_size\n");
		}
		if (format == long_format) {
			len = strlen(
					human_readable(fib->fib_Size, buf, file_human_output_opts,
							1, file_output_block_size));
			if (file_size_width < len)
				file_size_width = len;
		}

	}

	if (print_inode) {
		char buf[16];
		int len = strlen(umaxtostr(fib->fib_DiskKey, buf));
		//myerror("len=%ld, width=%ld\n",len,inode_number_width);
		if (inode_number_width < len)
			inode_number_width = len;
		//myerror("len=%ld, width=%ld\n",len,inode_number_width);
	}
}

/* Returns pointer just past the '.' of the string when there is a . in string
 * Returns pointer to empty string (points to the \0 at the end) when the is no .
 */
char *extindex(char *str)
{
	int i;
	int len = strlen(str);
	for (i = len - 1; i; --i)
		if (str[i] == '.')
			return &str[i + 1];
	return &str[i + len];		//not found, point at \0 byte at the end
}

int cmp(struct FileInfoBlock *fib1, struct FileInfoBlock *fib2)
{
	char *s1, *s2;
	int ret;
	if (directories_first) {
		ret = (fib2->fib_DirEntryType > 0 ? 1 : 0)
				- (fib1->fib_DirEntryType > 0 ? 1 : 0);
		if (ret)
			return ret;
	}
	switch (gSort) {
	case SORT_FILENAME:
		ret = stricmp(fib1->fib_FileName, fib2->fib_FileName);
		break;
	case SORT_FILENAME_EXTENSION:
		s1 = extindex(fib1->fib_FileName);
		s2 = extindex(fib2->fib_FileName);
		ret = stricmp(s1, s2);
		if (ret == 0)
			ret = stricmp(fib1->fib_FileName, fib2->fib_FileName);
		break;
	case SORT_SIZE:
		ret = fib2->fib_Size - fib1->fib_Size;
		break;
	case SORT_DATE:
		ret = fib2->fib_Date.ds_Days - fib1->fib_Date.ds_Days;
		if (ret == 0) {
			ret = fib2->fib_Date.ds_Minute - fib1->fib_Date.ds_Minute;
			if (ret == 0)
				ret = fib2->fib_Date.ds_Tick - fib1->fib_Date.ds_Tick;
		}
		break;
	default:
		ret = 1;
		break;
	}
	if (gReverse)
		ret = -ret;
	return ret;
}

#ifndef USESHELLSORT
///source: http://en.wikibooks.org/wiki/Algorithm_Implementation/Sorting/Quicksort#C
#define MAX 64            /* stack size for max 2^(64/2) array elements  */
void quicksort_iterative(struct fileinfo **array, unsigned len) {
	unsigned left = 0, stack[MAX], pos = 0, seed = 1971;		//rand();
	unsigned right;

	for (;; ) { /* outer loop */
		for (; left+1 < len; len++) { /* sort left to len-1 */
			if (pos == MAX) len = stack[pos = 0]; /* stack overflow, reset */
			struct fileinfo * pivot = array[left+seed%(len-left)]; /* pick random pivot */
			seed = seed*69069+1; /* next pseudorandom number */
			stack[pos++] = len; /* sort right part later */
			for (right = left-1;; ) { /* inner loop: partitioning */
				while (cmp(array[++right],pivot)<0); /* look for greater element */
				while (cmp(pivot,array[--len])<0); /* look for smaller element */
				if (right >= len) break; /* partition point found? */
				struct fileinfo * temp = array[right];
				array[right] = array[len]; /* the only swap */
				array[len] = temp;
			} /* partitioned, continue left part */
		}
		if (pos == 0) break; /* stack empty? */
		left = len; /* left to right is sorted */
		len = stack[--pos]; /* get next range to sort */
	}
}
#else
/* Well shell sort cause:
 * A) It's small and pretty
 * B) You'll never encounter directories with so many entries that quicksort will be faster
 *    (hopefully :P)
 */
void shellSort(struct fileinfo **a, int n)
{

	int i, j;
	struct fileinfo *temp;
	int gap;
	for (gap = n / 2; gap > 0; gap /= 2)
		for (i = gap; i < n; i++)
			for (j = i - gap; j >= 0 && cmp(&a[j]->fib, &a[j + gap]->fib) > 0;
					j -= gap) {
				temp = a[j];
				a[j] = a[j + gap];
				a[j + gap] = temp;
			}
}
#endif
///Nothing remotely related to sorting below this line

extern int windowWidth;

static void init_column_info(void);
static size_t calculate_columns(bool by_columns);
/* Information about filling a column.  */

/* Maximum number of columns ever possible for this display.  */
size_t max_idx;

/* The minimum width of a column is 3: 1 character for the name and 2
 for the separating white space.  */
#define MIN_COLUMN_WIDTH	3

extern size_t tabsize;

static void indent(size_t from, size_t to)
{
	while (from < to) {
		if (tabsize != 0 && to / tabsize > (from + 1) / tabsize) {
			printf("\t");
			from += tabsize - from % tabsize;
		} else {
			printf(" ");
			from++;
		}
	}
}

void print_with_commas(void)
{
	size_t filesno;
	size_t pos = 0;

	for (filesno = 0; filesno < cwd_n_used; filesno++) {
		struct fileinfo const *f = sorted_file[filesno];
		size_t len = length_of_file_name_and_frills(f);
		if (filesno != 0) {
			char separator;
			if (pos + len + 2 < line_length) {
				pos += 2;
				separator = ' ';
			} else {
				pos = 0;
				separator = '\n';
			}
			putchar(',');
			putchar(separator);
		}
		print_file_name_and_frills(f, pos);
		pos += len;
	}
	putchar('\n');
}

void print_many_per_line(void)
{

	size_t row; /* Current row.  */
	size_t cols = calculate_columns(true);

	struct column_info const *line_fmt = &column_info[cols - 1];

	/* Calculate the number of rows that will be in each column except possibly
	 for a short column on the right.  */
	size_t rows = cwd_n_used / cols + (cwd_n_used % cols != 0);

	for (row = 0; (row < rows) && !gotBreak; row++) {
		size_t col = 0;
		size_t filesno = row;
		size_t pos = 0;
		/* Print the next row.  */
		while (1) {
			struct fileinfo const *f = sorted_file[filesno];

			size_t name_length = length_of_file_name_and_frills(f);
			size_t max_name_length = line_fmt->col_arr[col++];
			print_file_name_and_frills(f, pos);
			filesno += rows;
			if (filesno >= cwd_n_used)
				break;
			indent(pos + name_length, pos + max_name_length);
			pos += max_name_length;
		}
		putchar('\n');
	}
}

static char * format_inode(char *buf, size_t buflen, const struct fileinfo *f)
{

	return (f->fib.fib_DiskKey != NOT_AN_INODE_NUMBER ?
			umaxtostr(f->fib.fib_DiskKey, buf) : (char *) "?");
}
size_t print_file_name_and_frills(const struct fileinfo *f, size_t start_col)
{

	char buf[LONGEST_HUMAN_READABLE + 1];

	//myerror("----> %ld %ld\n",inode_number_width,f->fib.fib_DiskKey);
	if (print_inode)
		printf("%*s ", format == with_commas ? 0 : inode_number_width,
				format_inode(buf, sizeof buf, f));
	if (print_block_size) {
		printf("%*s ", format == with_commas ? 0 : block_size_width,
				human_readable(f->fib.fib_NumBlocks, buf, human_output_opts,
				ST_NBLOCKSIZE, output_block_size));
		//printf("%s ", /*format == with_commas ? 0 : block_size_width,*/buf);
	}
	printf("%s%s%s", highlight_tab[f->fib.fib_DirEntryType].on,
			f->fib.fib_FileName, highlight_tab[f->fib.fib_DirEntryType].off);

}

/* Calculate the number of columns needed to represent the current set
 of files in the current display width.  */

size_t calculate_columns(bool by_columns)
{
	size_t filesno; /* Index into cwd_file.  */
	size_t cols; /* Number of files across.  */

	/* Normally the maximum number of columns is determined by the
	 screen width.  But if few files are available this might limit it
	 as well.  */
	size_t max_cols = MIN(max_idx, cwd_n_used);

	init_column_info();

	/* Compute the maximum number of possible columns.  */
	for (filesno = 0; filesno < cwd_n_used; ++filesno) {
		struct fileinfo const *f = sorted_file[filesno];
		size_t name_length = length_of_file_name_and_frills(f);
		size_t i;

		for (i = 0; i < max_cols; ++i) {
			if (column_info[i].valid_len) {
				size_t idx = (
						by_columns ?
								filesno / ((cwd_n_used + i) / (i + 1)) :
								filesno % (i + 1));
				size_t real_length = name_length + (idx == i ? 0 : 2);

				if (column_info[i].col_arr[idx] < real_length) {
					column_info[i].line_len += (real_length
							- column_info[i].col_arr[idx]);
					column_info[i].col_arr[idx] = real_length;
					column_info[i].valid_len = (column_info[i].line_len
							< windowWidth);
				}
			}
		}
	}

	/* Find maximum allowed columns.  */
	for (cols = max_cols; 1 < cols; --cols) {
		if (column_info[cols - 1].valid_len)
			break;
	}

	return cols;
}

char *umaxtostr(LONG num, char *buf)
{
	sprintf(buf, "%d", num);
	return buf;
}

char get_type_indicator(const struct fileinfo *f)
{
	char c;

	switch (f->fib.fib_DirEntryType) {
	case ST_ROOT:
	case ST_USERDIR:
		c = '/';
		break;
	case ST_LINKDIR:
		c = '@';
		break;
	default:
		c = 0;

	}
	return c;
}

size_t length_of_file_name_and_frills(const struct fileinfo *f)
{
	size_t len = 0;
	size_t name_width;
	char buf[LONGEST_HUMAN_READABLE + 1];

	if (print_inode)
		len += 1
				+ (format == with_commas ?
						strlen(umaxtostr(f->fib.fib_DiskKey, buf)) :
						inode_number_width);

	if (print_block_size)
		len += 1
				+ (format == with_commas ?
						strlen(
								human_readable(ST_NBLOCKS(f->fib.fib_NumBlocks),
										buf, human_output_opts, ST_NBLOCKSIZE,
										output_block_size)) :
						block_size_width);

//  if (print_scontext)
//    len += 1 + (format == with_commas ? strlen (f->scontext) : scontext_width);

	//quote_name (NULL, f->fib.fib_FileName, filename_quoting_options, &name_width);
	name_width = strlen(f->fib.fib_FileName);
	len += name_width;

	if (indicator_style != none) {
		char c = get_type_indicator(f);
		len += (c != 0);
	}

	return len;
}

void *allocp(int n)
{
	struct plist *new = (struct plist *) mymalloc(sizeof *new);
	if (new) {
		new->next = theplist;
		new->mem = mymalloc(n);
		theplist = new;
		return new->mem;
	}
	return NULL;
}
static void init_column_info(void)
{
	size_t i;
	size_t max_cols = MIN(max_idx, cwd_n_used);

	/* Currently allocated columns in column_info.  */
	static size_t column_info_alloc;

	if (column_info_alloc < max_cols) {
		size_t new_column_info_alloc;
		size_t *p;
		if (max_cols < max_idx / 2) {
			/* The number of columns is far less than the display width
			 allows.  Grow the allocation, but only so that it's
			 double the current requirements.  If the display is
			 extremely wide, this avoids allocating a lot of memory
			 that is never needed.  */
			column_info = realloc(column_info,
					max_cols * 2 * sizeof *column_info);
			new_column_info_alloc = 2 * max_cols;
		} else {
			column_info = realloc(column_info, max_idx * sizeof *column_info);
			new_column_info_alloc = max_idx;
		}

		/* Allocate the new size_t objects by computing the triangle
		 formula n * (n + 1) / 2, except that we don't need to
		 allocate the part of the triangle that we've already
		 allocated.  Check for address arithmetic overflow.  */
		{
			size_t column_info_growth = new_column_info_alloc
					- column_info_alloc;
			size_t s = column_info_alloc + 1 + new_column_info_alloc;
			size_t t = s * column_info_growth;
			if (s < new_column_info_alloc || t / column_info_growth != s)
				myerror("Dealloc and close everything\n"__FILE__);

			p = (int *) allocp((t / 2) * sizeof *p);

			//myerror("malloc'ing %ld\n",t / 2 * sizeof *p);
		}

		/* Grow the triangle by parceling out the cells just allocated.  */
		for (i = column_info_alloc; i < new_column_info_alloc; i++) {
			column_info[i].col_arr = p;
			p += i + 1;
		}

		column_info_alloc = new_column_info_alloc;
	}

	//myerror("Before the for at the bottom...max_cols:%ld (%ld)\n",max_cols,column_info_alloc);
	for (i = 0; i < max_cols; ++i) {
		size_t j;
		column_info[i].valid_len = true;
		//myerror("i=%ld\n",i);
		column_info[i].line_len = (i + 1) * MIN_COLUMN_WIDTH;
		for (j = 0; j <= i; ++j) {
			//myerror("j=%ld\n",j);
			column_info[i].col_arr[j] = MIN_COLUMN_WIDTH;
		}
	}
	//myerror("After the for at the bottom...\n");
}

static void print_horizontal(void)
{
	size_t filesno;
	size_t pos = 0;
	size_t cols = calculate_columns(false);
	struct column_info const *line_fmt = &column_info[cols - 1];
	struct fileinfo const *f = sorted_file[0];
	size_t name_length = length_of_file_name_and_frills(f);
	size_t max_name_length = line_fmt->col_arr[0];

	/* Print first entry.  */
	print_file_name_and_frills(f, 0);

	/* Now the rest.  */
	for (filesno = 1; (filesno < cwd_n_used) && !gotBreak; ++filesno) {
		size_t col = filesno % cols;

		if (col == 0) {
			putchar('\n');
			pos = 0;
		} else {
			indent(pos + name_length, pos + max_name_length);
			pos += max_name_length;
		}

		f = sorted_file[filesno];
		print_file_name_and_frills(f, pos);

		name_length = length_of_file_name_and_frills(f);
		max_name_length = line_fmt->col_arr[col];
	}
	putchar('\n');
}

void attach(char *dest, const char *dirname, const char *name)
{
	const char *dirnamep = dirname;

	/* Copy dirname if it is not ""   */
	if (dirname[0] != '\0') {
		while (*dirnamep)
			*dest++ = *dirnamep++;
		/* Add '/' if 'dirname' doesn't already end with it.  */
		if (dirnamep > dirname && (dirnamep[-1] != '/' && dirnamep[-1] != ':'))
			*dest++ = '/';
	}
	while (*name)
		*dest++ = *name++;
	*dest = 0;
}

int stat(char *file, struct fileinfo *f)
{
	BPTR lock = 0L;
	int ret;

	if ((lock = Lock(file, SHARED_LOCK))) {
		if ((Examine(lock, &f->fib))) {
			ret = 0;
		} else {
			//myerror("Oopps, examine failed on %s ", file);
			ret = -1;
		}
		UnLock(lock);
	} else
		ret = -1;
	errno = IoErr();
	return ret;
}

int gobble_file(char const *name, enum filetype type, long inode,
		bool command_line_arg, char const *dirname)
{
	int blocks = 0;
	struct fileinfo *f;

	if (cwd_n_used == cwd_n_alloc) {
		cwd_file = realloc(cwd_file, cwd_n_alloc * 2 * sizeof *cwd_file);
		cwd_n_alloc *= 2;
	}
	//printf("gobble_file: %s\n",name);
	f = &cwd_file[cwd_n_used];
	memset(f, '\0', sizeof *f);
	f->fib.fib_DiskKey = inode;
	f->fib.fib_DirEntryType = type;
	if (command_line_arg || format_needs_stat) {
		/* Absolute name of this file.  */
		char *absolute_name, *ff = NULL;
		bool do_deref;
		int err;

		if (name[0] == '/' || dirname[0] == 0) {
			absolute_name = (char *) name;
		} else {
			ff = absolute_name = mymalloc(strlen(name) + strlen(dirname) + 2);
			attach(absolute_name, dirname, name);
		}
		switch (dereference) {
		case DEREF_ALWAYS:
			err = stat(absolute_name, f);
			do_deref = true;
			break;
		case DEREF_COMMAND_LINE_ARGUMENTS:
		case DEREF_COMMAND_LINE_SYMLINK_TO_DIR:
			if (command_line_arg) {
				bool need_lstat;
				err = stat(absolute_name, f);
				do_deref = true;

				if (dereference == DEREF_COMMAND_LINE_ARGUMENTS)
					break;

				need_lstat = 0; //(err < 0 ? errno == ENOENT : !S_ISDIR(f->stat.st_mode));
				myerror("Fix me\n sort.c");
				if (!need_lstat)
					break;

				/* stat failed because of ENOENT, maybe indicating a dangling
				 symlink.  Or stat succeeded, ABSOLUTE_NAME does not refer to a
				 directory, and --dereference-command-line-symlink-to-dir is
				 in effect.  Fall through so that we call lstat instead.  */
			}

		default: /* DEREF_NEVER */

			err = stat(absolute_name, f);
			if (*absolute_name)
				strcpy(f->fib.fib_FileName, absolute_name);

			do_deref = false;
			break;
		}

		if (err != 0) {
			/* Failure to stat a command line argument leads to
			 an exit status of 2.  For other files, stat failure
			 provokes an exit status of 1.  */
			//file_failure (command_line_arg,_("cannot access %s"), absolute_name);
			myerror("%s: cannot access %s [%ld]\n", arg0, absolute_name, errno);
			if (command_line_arg) {
				if (ff)
					myfree(ff);
				return 0;
			}

			myerror("weird %s\n", name);
			// FIXME: f->name = xstrdup (name);
			cwd_n_used++;
			myfree(absolute_name);
			return 0;
		}

		//displayFib(f);
		if (ff)
			myfree(ff);
	}

	if (f->fib.fib_DirEntryType > 0 && command_line_arg)
		f->fib.fib_DirEntryType = arg_directory;
	blocks = f->fib.fib_NumBlocks;
	if (format == long_format || print_block_size) {
		char buf[LONGEST_HUMAN_READABLE + 1];
		int len = strlen(human_readable(blocks, buf, human_output_opts,
		ST_NBLOCKSIZE, output_block_size));
		//printf("clen=%ld Blocks:%ld  %s\n",len,blocks,buf);
		if (block_size_width < len) {
			block_size_width = len;
			//printf("len=%ld\n",len);
		}
		if (format == long_format) {
			len = strlen(
					human_readable(f->fib.fib_Size, buf, file_human_output_opts,
							1, file_output_block_size));
			if (file_size_width < len)
				file_size_width = len;
		}
	}
	if (print_inode) {
		char buf[16];
		int len = strlen(umaxtostr(f->fib.fib_DiskKey, buf));
		//myerror("len=%ld, width=%ld\n",len,inode_number_width);
		if (inode_number_width < len)
			inode_number_width = len;
		//myerror("len=%ld, width=%ld\n",len,inode_number_width);
	}
	cwd_n_used++;
	return f->fib.fib_NumBlocks;
}

static void print_long_format(const struct fileinfo *f)
{
	//char modebuf[12];
	char buf[80];
	size_t s;
	char *p;
	//struct timespec when_timespec;
	//struct tm *when_local;

	p = buf;

	p[0] = f->fib.fib_EntryType > 0 ? 'D' : '-';
	p[1] = (f->fib.fib_Protection & FIBF_SCRIPT) ? 's' : '-';
	p[2] = (f->fib.fib_Protection & FIBF_PURE) ? 'p' : '-';
	p[3] = (f->fib.fib_Protection & FIBF_ARCHIVE) ? 'a' : '-';
	p[4] = (f->fib.fib_Protection & FIBF_READ) ? '-' : 'r';
	p[5] = (f->fib.fib_Protection & FIBF_WRITE) ? '-' : 'w';
	p[6] = (f->fib.fib_Protection & FIBF_EXECUTE) ? '-' : 'x';
	p[7] = (f->fib.fib_Protection & FIBF_DELETE) ? '-' : 'd';
	p[8] = ' ';
	p += 9;
	if (print_inode) {
		char hbuf[LONGEST_HUMAN_READABLE + 1];
		sprintf(p, "%*s ", inode_number_width,
				format_inode(hbuf, sizeof hbuf, f));
		/* Increment by strlen (p) here, rather than by inode_number_width + 1.
		 The latter is wrong when inode_number_width is zero.  */
		p += strlen(p);
	}

	if (print_block_size) {
		char hbuf[LONGEST_HUMAN_READABLE + 1];
		char const *blocks = human_readable(f->fib.fib_NumBlocks, hbuf,
				human_output_opts,
				ST_NBLOCKSIZE, output_block_size);
		int pad;
		for (pad = block_size_width - strlen(blocks); 0 < pad; pad--)
			*p++ = ' ';
		while ((*p++ = *blocks++))
			continue;
		p[-1] = ' ';
	}

	/* Increment by strlen (p) here, rather than by, e.g.,
	 sizeof modebuf - 2 + any_has_acl + 1 + nlink_width + 1.
	 The latter is wrong when nlink_width is zero.  */
	//p += strlen(p); ALKIS
	{

		char hbuf[LONGEST_HUMAN_READABLE + 1];
		char const *size = human_readable(f->fib.fib_Size, hbuf,
				file_human_output_opts, 1, file_output_block_size);
		int pad;
		for (pad = file_size_width - strlen(size); 0 < pad; pad--)
			*p++ = ' ';
		//myerror("file_size_width: %ld\n",file_size_width);
		while ((*p++ = *size++))
			continue;
		p[-1] = ' ';
	}

	//when_local = localtime (&when_timespec.tv_sec);
	s = 0;
	*p = '\1';

	s = dates(p, &f->fib.fib_Date);
	p += 16; //10 for date + 1space + 5 for time

	if (s || !*p) {
		p += s;
		*p++ = ' ';

		/* NUL-terminate the string -- fputs (via DIRED_FPUTS) requires it.  */
		*p = '\0';
	}

	//	size_t w = print_name_with_quoting (f, false, &dired_obstack, p - buf);

	printf("%s %s", buf, f->fib.fib_FileName);

//  if (f->filetype == symbolic_link)
//    {
//      if (f->linkname)
//        {
//          DIRED_FPUTS_LITERAL (" -> ", stdout);
//          print_name_with_quoting (f, true, NULL, (p - buf) + w + 4);
//          if (indicator_style != none)
//            print_type_indicator (true, f->linkmode, unknown);
//        }
//    }
//  else if (indicator_style != none)
//    print_type_indicator (f->stat_ok, f->stat.st_mode, f->filetype);
}

