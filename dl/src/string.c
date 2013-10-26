/*
 * string.c
 *
 *  Created on: Sep 12, 2013
 *      Author: alex
 */
#include "string.h"

static char *scanpoint = NULL;

char *strcpy(char *dst, const char *src)
{
	register char *dscan;
	register const char *sscan;

	dscan = dst;
	sscan = src;
	while ((*dscan++ = *sscan++) != '\0')
		;
	return (dst);
}

char *strdup(const char *str)
{
	int len = strlen(str) + 1;
	char *ret = mymalloc(len);
	if (ret) {
		strcpy(ret, str);
	}
	return ret;
}

void bcopy(char *src, char *dest, int n)
{
	while (n--)
		*dest++ = *src++;
}

void bzero(char *dest, int n)
{
	while (n--)
		*dest++ = 0;
}

void setmem(char *buf, int n, char c)
{
	int i;

	for (i = 0; i < n; ++i)
		*buf++ = c;
}


int strlen(const char *string)
{
	const char *s = string;

	do
		; while (*s++);
	return ~(string - s);
}

char *strtok(char *s, const char *delim)
{
	char *scan;
	char *tok;
	const char *dscan;

	if (s == NULL && scanpoint == NULL)
		return (NULL);
	if (s != NULL)
		scan = s;
	else
		scan = scanpoint;

	/*
	 * Scan leading delimiters.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0'; dscan++)
			if (*scan == *dscan)
				break;
		if (*dscan == '\0')
			break;
	}
	if (*scan == '\0') {
		scanpoint = NULL;
		return (NULL);
	}

	tok = scan;

	/*
	 * Scan token.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0';) /* ++ moved down. */
			if (*scan == *dscan++) {
				scanpoint = scan + 1;
				*scan = '\0';
				return (tok);
			}
	}

	/*
	 * Reached end of string.
	 */
	scanpoint = NULL;
	return (tok);
}

int myindex(char *str, char c)
{
	int i;
	int len = strlen(str);

	for (i = 0; i < len; ++i)
		if (str[i] == c)
			return i;
	return -1;
}
int strcmp(char *str1, char *str2)
{
	while (*str1 == *str2 && *str2) {
		++str1;
		++str2;
	}
	if (*str2 == *str1)
		return 0;
	return *str1 - *str2;
}

int strncmp(const char *s1,const char *s2,int n)
{ unsigned char *p1=(unsigned char *)s1,*p2=(unsigned char *)s2;
  unsigned long r,c;

  if ((r=n))
    do;while(r=*p1++,c=*p2++,!(r-=c) && (char)c && --n);
  return r;
}

int stricmp(char *str1, char *str2)
{
	UBYTE c1, c2;

	/* Loop as long as the strings are identical and valid. */
	do {
		/* Get characters, convert them to lower case. */
		c1 = *str1++;
		c1 = ToLower(c1);
		c2 = *str2++;
		c2 = ToLower(c2);
	} while (c1 == c2 && c1);

	/* Get result. */
	return (LONG) c1 - (LONG) c2;
}

/* Safe for overlaps */
char *memcpy(char *dst, char *src, int size)
{
	register char *d;
	register const char *s;
	register int n;

	if (size <= 0)
		return (dst);

	s = src;
	d = dst;
	if (s <= d && s + (size - 1) >= d) {
		/* Overlap, must copy right-to-left. */
		s += size - 1;
		d += size - 1;
		for (n = size; n > 0; n--)
			*d-- = *s--;
	} else
		for (n = size; n > 0; n--)
			*d++ = *s++;

	return (dst);
}

void fastmemcpy(long *dst, long *src, int len)
{
	//WARNING: Not safe on overlaps
	if (len & 3) {
		memcpy(dst, src, len);
		return;
	}
	len >>= 2;
	while (len--)
		*dst++ = *src++;
}

char *realloc(char *orig, int newsize)
{

	char *new = mymalloc(newsize);
	if (new) {
		if (orig) {
			int *getsize = (int *) orig;
			--getsize;	//decrease the pointer to go to saved size
			fastmemcpy(new, orig, (*getsize) - 4);
			myfree(orig);
		}
	}
#ifdef DEBUGMEMORY
	else {
		myerror("Realloc failed on %ld bytes.\n", newsize);
	}
#endif
	return new;
}

#ifdef DEBUGMEMORY
//char *mymalloc(register int n __asm("d0"));
char *dmalloc(int size,char *function, char *file, int line)
{
	static int c = 0;
	static int sum = 0;
	++c;
	sum += size;
	myerror("A: #%ld %ld/%ld %s %s, line %ld\n", c, size,sum,
			function,file, line);
	return realmalloc(size);
}

void dfree(char *mem,char *function, char *file, int line)
{
	static int c = 0;
	static int sum = 0;
	++c;
	int *getsize = (int *) mem;
	--getsize;
	sum += ((*getsize) - 4);
	myerror("F: #%ld %ld/%ld %s %s, line %ld\n", c,
			(*getsize) - 4,sum,function, file, line);
	return realfree(mem);
}
#endif
