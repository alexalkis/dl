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
		//strcpy(ret, str);
		bcopy(str,ret,len);
	}
	return ret;
}

// bcopy is in assembly now (astartup.S)
//#define FULLBCOPY
#ifndef FULLBCOPY
//void bcopy(const void *s1, void *s2, size_t n)
//{
//		while (n--)
//			*((char *) s2)++ = *((char *) s1)++;
//
//}
#else
void bcopy(const void *s1,void *s2,size_t n)
{	size_t m;
	if(!n)
	return;
	if(s2<s1)
	{	if((long)s1&1)
		{	*((char *)s2)++=*((char *)s1)++;
			n--;}
		if(!((long)s2&1))
		{	m=n/sizeof(long);
			n&=sizeof(long)-1;
			for(;m;m--)
			*((long *)s2)++=*((long *)s1)++;
		}
		for(;n;n--)
		*((char *)s2)++=*((char *)s1)++;
	} else
	{	(char *)s1+=n;
		(char *)s2+=n;
		if((long)s1&1)
		{	*--((char *)s2)=*--((char *)s1);
			n--;}
		if(!((long)s2&1))
		{	m=n/sizeof(long);
			n&=sizeof(long)-1;
			for(;m;m--)
			*--((long *)s2)=*--((long *)s1);
		}
		for(;n;n--)
		*--((char *)s2)=*--((char *)s1);
	}
}

#endif

void bzero(char *dest, int n)
{
	while (n--)
		*dest++ = 0;
}

void setmem(char *buf, int n, char c)
{
	//int i;
	//for (i = 0; i < n; ++i)
	while(n--)
		*buf++ = c;
}


#ifdef TRACKSTRLENCALLS
int realstrlen(const char *string,char *f, char *s,int line)
{
	const char *save = string;
	static int c=0;
	myerror("\"%s\" -- %ld -- %s,%s,%ld\n",string,++c,f,s,line);
	do
		; while (*save++);
	return ~(string - save);
}

#else
int __regargs strlen(const char *string)
{
	const char *s = string;
	//static int c=0;myerror("\"%s\" -- %ld\n",string,++c);
	do
		; while (*s++);
	return ~(string - s);
}

#endif

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

int myindex(const char *str, const char c)
{
	int i;
	for (i = 0; str[i]; ++i)
		if (str[i] == c)
			return i;
	return -1;
}
int strcmp(const char *str1, const char *str2)
{
	while (*str1 == *str2 && *str2) {
		++str1;
		++str2;
	}
	return *str1 - *str2;
}

int strncmp(const char *s1, const char *s2, int n)
{
	unsigned char *p1 = (unsigned char *) s1, *p2 = (unsigned char *) s2;
	unsigned long r, c;

	if ((r = n))
		do
			; while (r = *p1++, c = *p2++, !(r -= c) && (char) c && --n);
	return r;
}

//seems to gain minor speedup with the table lookup
#define TABLESTRICMP
#ifdef TABLESTRICMP

unsigned char __ctype[]=
{ 0x00,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28,0x28,0x28,0x28,0x28,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x88,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
  0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x10,0x10,0x10,0x10,0x10,0x10,
  0x10,0x41,0x41,0x41,0x41,0x41,0x41,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x10,0x10,0x10,0x10,0x10,
  0x10,0x42,0x42,0x42,0x42,0x42,0x42,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x10,0x10,0x10,0x10,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x08,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
  0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x10,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x02,
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x10,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
  0x00,0x00,0x00
};
const unsigned char *_ctype_=__ctype;

#undef ToLower
#define ToLower(c) _ctype_[1+c]&1?c+'a'-'A':c

int stricmp(const char *str1, const char *str2)
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
	return (int) c1 - (int) c2;
}
#else
int stricmp(const char *str1, const char *str2)
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
	return (int) c1 - (int) c2;
}
#endif

// Don't have any overlaps and bcopy is good enough
// switched all occurrences of nencpy to bcopy.
//       Note the pointers order is reversed
//
///* Safe for overlaps */
//void *memcpy(void *dst, void *src, size_t size)
//{
//	register char *d;
//	register const char *s;
//	register int n;
//
//	if (size <= 0)
//		return (dst);
//
//	s = src;
//	d = dst;
//	if (s <= d && s + (size - 1) >= d) {
//		/* Overlap, must copy right-to-left. */
//		s += size - 1;
//		d += size - 1;
//		for (n = size; n > 0; n--)
//			*d-- = *s--;
//	} else
//		for (n = size; n > 0; n--)
//			*d++ = *s++;
//
//	return (dst);
//}

//void fastmemcpy(long *dst, long *src, int len)
//{
//	//WARNING: Not safe on overlaps
//	if (len & 3) {
//		memcpy(dst, src, len);
//		return;
//	}
//	len >>= 2;
//	while (len--)
//		*dst++ = *src++;
//}

char *realloc(char *orig, int newsize)
{

	char *new = mymalloc(newsize);
	if (new) {
		if (orig) {
			int *getsize = (int *) orig;
			--getsize;	//decrease the pointer to go to saved size
			//memcpy(new, orig, (*getsize) - 4);
			bcopy(orig,new,(*getsize) - 4);
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
