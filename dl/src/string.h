/*
 * string.h
 *
 *  Created on: Sep 12, 2013
 *      Author: alex
 */

#ifndef STRING_H_
#define STRING_H_

#include <exec/types.h>
typedef unsigned long size_t;

/* Implemented in asm */
char *realmalloc(register int n __asm("d0"));
void realfree(register char *mem __asm("a0"));
void atexit(register void (*function)(void) __asm("a0"));
void bflush(void);
/* End of asm prototyping */

#define uch		unsigned char
#define IsUpper(c) (c>=0xC0 ? c<=0xDE && c!=0xD7 : c>=0x41 && c<=0x5A)
#define ToLower(c) (IsUpper((uch) c) ? (unsigned) c | 0x20 : (unsigned) c)


void __stdargs bcopy(const void *s1,void *s2,size_t n);
void bzero(char *dest, int n);
void setmem(char *buf, int n, char c);
char *strtok(char *s, const char *delim);
int myindex(const char *str, const char c);
int strcmp(const char *str1, const char *str2);
int strncmp(const char *s1, const char *s2, int n);
int stricmp(const char *str1, const char *str2);
char *strcpy(char *dst, const char *src);
char *strdup(const char *str);

//void *memcpy(void *dst, void *src, size_t size);
char *realloc(char *orig, int newsize);

//#define TRACKSTRLENCALLS
#ifdef TRACKSTRLENCALLS
int realstrlen(const char *str,char *f, char *s,int line);
#define strlen(x) realstrlen((x),__FUNCTION__,__FILE__,__LINE__)
#else
int __regargs strlen(const char *str);
#endif

//#define USEMALLOC

//#define DEBUGMEMORY
#ifdef DEBUGMEMORY
char *dmalloc(int n,char *f, char *s,int line);
void dfree(char *mem,char *f,char *s,int line);
#define mymalloc(x) dmalloc((x),__FUNCTION__,__FILE__,__LINE__)
#define myfree(x) 	dfree((x),__FUNCTION__,__FILE__,__LINE__)
#else
#ifdef USEMALLOC

void __initmalloc(void);
void __exitmalloc(void);
void free(void *ptr);
void *malloc(size_t size);

#define mymalloc(x) malloc((x))
#define myfree(x)	 free((x))
#else
#define mymalloc(x) realmalloc((x))
#define myfree(x)	 realfree((x))
#endif
#endif
#endif /* STRING_H_ */
