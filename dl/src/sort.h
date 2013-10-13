/*
 * sort.h
 *
 *  Created on: Sep 11, 2013
 *      Author: alex
 */

#ifndef SORT_H_
#define SORT_H_
#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/dos.h>
#include <proto/exec.h>

typedef short int bool;
typedef int size_t;


#define NOT_AN_INODE_NUMBER (-1)
#define MIN(a,b)	((a>b)?b:a)
#define MAX(a,b)	((a>b)?a:b)
#define LOOP_DETECT (!!active_dir_set)
#define DIRED_PUTCHAR(c) do {bprintf("%lc",((c))); ++dired_pos;} while (0)

enum DISPMODE {
	DISPLAY_NORMAL = 0, DISPLAY_HORIZONTAL,DISPLAY_LONG,
};
extern int gDisplayMode;

enum SORTING {
	SORT_NONE = 0, SORT_FILENAME,SORT_FILENAME_EXTENSION, SORT_SIZE, SORT_DATE
};

enum filetype
  {
    unknown,
    fifo,
    chardev,
    directory,
    blockdev,
    normal,
    symbolic_link,
    sock,
    whiteout,
    arg_directory
  };

enum Dereference_symlink
  {
    DEREF_UNDEFINED = 1,
    DEREF_NEVER,
    DEREF_COMMAND_LINE_ARGUMENTS,	/* -H */
    DEREF_COMMAND_LINE_SYMLINK_TO_DIR,	/* the default, in certain cases */
    DEREF_ALWAYS			/* -L */
  };

enum sort_type
  {
    sort_none = -1,		/* -U */
    sort_name,			/* default */
    sort_extension,		/* -X */
    sort_size,			/* -S */
    sort_version,		/* -v */
    sort_time,			/* -t */
    sort_numtypes		/* the number of elements of this enum */
  };

enum format
  {
    long_format,		/* -l and other options that imply -l */
    one_per_line,		/* -1 */
    many_per_line,		/* -C */
    horizontal,			/* -x */
    with_commas			/* -m */
  };
enum indicator_style
  {
    none,	/*     --indicator-style=none */
    slash,	/* -p, --indicator-style=slash */
    file_type,	/*     --indicator-style=file-type */
    classify	/* -F, --indicator-style=classify */
  };

extern enum format format;
extern enum sort_type sort_type;
extern LONG errno;

struct fileinfo {
	struct FileInfoBlock fib;
};

#define true	(1)
#define false	(0)

/* Structure used to hold type and protection highlight info in an array */
struct highlight
{
  unsigned char *on;
  unsigned char *off;
  int printable_len;
};

struct pending {
	char *name;
	/* If the directory is actually the file pointed to by a symbolic link we
	 were told to list, 'realname' will contain the name of the symbolic
	 link, otherwise zero.  */
	char *realname;
	bool command_line_arg;
	struct pending *next;
};


#define HIGHLIGHT_MAX 5
#define HIGHLIGHT_MIN -7

#define HI_FILE_DEFAULT -7
#define HI_LINKFILE_EXE -6
#define HI_FILE_EXE -5
#define HI_LINKFILE -4
#define HI_FILE -3
#define HI_SOFTLINK_EXE -2
#define HI_SOFTLINK -1
#define HI_COMMENT 0
#define HI_ROOT 1
#define HI_USERDIR 2
#define HI_LABEL 3
#define HI_LINKDIR 4
#define HI_DIR_DEFAULT 5
#define arg_directory 6


int init_structures(void);
void clear_files(void);
void free_structures(void);
void sort_files (void);
int gobble_file(char const *name, enum filetype type, long inode,bool command_line_arg, char const *dirname);
void extract_dirs_from_files (char const *dirname, bool command_line_arg);
void queue_directory (char const *name, char const *realname, bool command_line_arg);
void free_ent (struct fileinfo *f);
void print_current_files(void);
void addEntry(struct FileInfoBlock *fib);
void free_pending_ent(struct pending *pend);

void addFib(struct FileInfoBlock *fib);
void displaySorted(int show);
char *umaxtostr(LONG num, char *buf);
#endif /* SORT_H_ */
