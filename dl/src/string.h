/*
 * string.h
 *
 *  Created on: Sep 12, 2013
 *      Author: alex
 */

#ifndef STRING_H_
#define STRING_H_

#include <exec/types.h>

/* Implemented in asm */
char *realmalloc(register int n __asm("d0"));
void  realfree(register char *mem __asm("a0"));
void atexit(register void (*function)(void) __asm("a0"));
void __stdargs mysprintf(char *buffer, char *fmt, ...);
void __stdargs myprintf(char *fmt, ...);
void __stdargs bprintf(char *fmt, ...);
void __stdargs myerror(char *fmt, ...);
void __stdargs bputchar(char c);
void bflush(void);
/* End of asm prototyping */


#define uch		unsigned char
#define IsUpper(c) (c>=0xC0 ? c<=0xDE && c!=0xD7 : c>=0x41 && c<=0x5A)
#define ToLower(c) (IsUpper((uch) c) ? (unsigned) c | 0x20 : (unsigned) c)

int strlen(const char *str);
void bcopy(char *src, char *dest, int n);
void bzero(char *dest, int n);
void setmem(char *buf, int n, char c);
char *strtok(char *s, const char *delim);
int myindex(char *str, char c);
int strcmp(char *str1, char *str2);
int strncmp(const char *s1,const char *s2,int n);
int stricmp(char *str1, char *str2);
char *strcpy(char *dst,const char *src);
char *strdup(const char *str);
char *memcpy(char *dst,char *src,int size);
char *realloc(char *orig,int newsize);

//#define DEBUGMEMORY
#ifdef DEBUGMEMORY
char *dmalloc(int n,char *f, char *s,int line);
void dfree(char *mem,char *f,char *s,int line);
#define mymalloc(x) dmalloc((x),__FUNCTION__,__FILE__,__LINE__)
#define myfree(x) 	dfree((x),__FUNCTION__,__FILE__,__LINE__)
#else
#define mymalloc(x) realmalloc((x))
#define myfree(x)	 realfree((x))
#endif
#endif /* STRING_H_ */
