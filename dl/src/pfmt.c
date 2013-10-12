/*
 *  PFMT.C
 *
 *    (c)Copyright 1992-1997 Obvious Implementations Corp.  Redistribution and
 *    use is allowed under the terms of the DICE-LICENSE FILE,
 *    DICE-LICENSE.TXT.
 *
 *  func format same as fwrite: func(buf, 1, n, desc)
 *
 *  NOTE: PFMT is part of ROM.LIB, which means that it is standalone.  It
 *  may NOT access any non-const global or static data items because of
 *  this.  The same PFMT is in C.LIB as is in ROM.LIB
 */

#include <stdarg.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include "string.h"

typedef unsigned long size_t;
BPTR	stderr;
BPTR	stdout;

#define F_SPAC	    1
#define F_PLUS	    2
#define F_MINUS     4
#define F_HASH	    8
#define F_ZERO	    16

#define F_SHORT     32
#define F_LONG	    64
#define F_DLONG     128

#define MATH_BUFSIZE	32

int fwrite(const void *vbuf,size_t elmsize,size_t elms,BPTR fi);
int _pfmtone(char c, va_list *pva,
		unsigned int (*func)(char *, size_t, size_t, void *), void *desc,
		short flags, short i1, short i2, int baseBytes);

int _pfmt(	char *ctl,va_list va,unsigned int (*func)(char *, size_t, size_t, void *),void *desc)
{
	char *base = ctl;
	char c;
	int error = 0;
	int bytes = 0;
	short flags;
	short i1, i2;

	for (;;) {
		while (*ctl && *ctl != '%')
			++ctl;
		if (ctl != base) {
			error = (*func)(base, 1, ctl - base, desc);
			if (error < 0)
				break;
			bytes += error;
		}
		if (*ctl++ == 0)
			break;
		c = *ctl;
		if (c == '%') {
			base = ctl++;
			continue;
		}

		/*
		 *  %[flags][val[.val]]c
		 */

		flags = 0;
		for (;; c = *++ctl) {
			if (c == '-') {
				flags |= F_MINUS;
				continue;
			}
			if (c == '+') {
				flags |= F_PLUS;
				continue;
			}
			if (c == ' ') {
				flags |= F_SPAC;
				continue;
			}
			if (c == '#') {
				flags |= F_HASH;
				continue;
			}
			if (c == '0') {
				flags |= F_ZERO;
				continue;
			}
			break;
		}
		if (c == '*') {
			c = *++ctl;
			i1 = va_arg(va, int);
		} else if (c >= '0' && c <= '9') {
			i1 = 0;
			while (c >= '0' && c <= '9') {
				i1 = i1 * 10 + (c - '0');
				c = *++ctl;
			}
		} else {
			i1 = -1;
		}
		i2 = -1;
		if (c == '.') {
			c = *++ctl;
			if (c == '*') {
				c = *++ctl;
				i2 = va_arg(va, int);
			} else {
				i2 = 0;
				while (c >= '0' && c <= '9') {
					i2 = i2 * 10 + (c - '0');
					c = *++ctl;
				}
			}
		}
		if (i1 > 4096 || i2 > 4096) /*  bad for business! */
			continue;
		for (;;) {
			if (c == 'h') {
				c = *++ctl;
				flags |= F_SHORT; /*	only if sizeof(int) != 4 */
				continue;
			}
			if (c == 'l') {
				c = *++ctl;
				flags |= F_LONG;
				continue;
			}
			if (c == 'L') {
				c = *++ctl;
				flags |= F_DLONG;
				continue;
			}
			break;
		}
		error = _pfmtone(c, &va, func, desc, flags, i1, i2, bytes);
		if (error < 0)
			break;
		// pfmtone returns the new total, not just the output bytes for
		// this particular control code.
		//
		bytes = error;
		base = ++ctl;
	}
	if (error < 0)
		return (error);
	return (bytes); /*  # bytes written */
}

/*
 *  format one item (c) using output function (func), (flags),
 *  field width (i1) and precision (i2).  The number of bytes written
 *  so far (baseBytes) is passed and the new total should be returned
 *
 *  i1 and i2 are -1 for unspecified widths.
 *
 *  note: first 30 bytes of the buffer cannot hold a value
 */

int _pfmtone(char c, va_list *pva,
		unsigned int (*func)(char *, size_t, size_t, void *), void *desc,
		short flags, short i1, short i2, int baseBytes)
{
	va_list va = *pva;
	char *prefix1 = ""; /*  output prefix       */
	char *value = ""; /*  output value        */
	char *postfix1 = ""; /*  output postfix      */
	char buf[64]; /*  temporary buffer	*/
	int len = 0; /*  length of <value>	*/
	short error = 0;
	short trail_zero = 0; /*  trailing zeros from math	*/
	short i; /*  temporary		*/
	long *pnum;


	switch (c) {
	case 'c':
		buf[sizeof(buf) - 1] = va_arg(va, int);
		len = 1;
		value = buf + sizeof(buf) - 1;
		break;
	case 'i':
	case 'd':
	case 'u':
	case 'o': {
		unsigned long v = va_arg(va, unsigned long);
		short base = (c == 'o') ? 8 : 10;

		if (c != 'u' && (long) v < 0) {
			v = -v;
			prefix1 = "-";
		}
		if (i2 < 0) /*	default precision   */
			i2 = 1;
		for (len = 0; len < sizeof(buf) && (len < i2 || v); ++len) {
			buf[sizeof(buf) - 1 - len] = v % base + '0';
			//bprintf("'%lc' %ld %ld %ld\n",buf[sizeof(buf) - 1 - len],v,base, v % base);bflush();
			v = v / base;
		}
		value = buf + sizeof(buf) - len;
	}
		break;
#ifdef MATH_PFMT
#include <math/pfmt.c>	    /*	cases for math	*/
#else
	case 'e': /*  [-]d.dddddde[+/-]dd     */
	case 'E': /*  [-]d.ddddddE[+/-]dd     */
	case 'f': /*  [-]d.dddddd             */
	case 'g': /*  if exp < -4 or exp > prec use 'e', else use 'f' */
	case 'G': /*  if exp < -4 or exp > prec use 'E', else use 'f' */
		value = "<float>";
		len = 7;
		if (flags & F_DLONG)
			va_arg(va, long double);
		else
			va_arg(va, double);
		break;
#endif
	case 'n':
		pnum = va_arg(va, long *);
		*pnum = baseBytes;
		break;
	case 's':
		value = va_arg(va, char *);
		/*
		 *  can't simply call strlen because buffer may overflow a short,
		 *  or even be unterminated.
		 */
		{
			while ((i2 < 0 || len < i2) && value[len])
				++len;

		}
		break;
	case 'p':
	case 'x':
	case 'X': {
		unsigned long v = va_arg(va, unsigned long);
		static const char IToHCL[] = { "0123456789abcdef0x" };
		static const char IToHCU[] = { "0123456789ABCDEF0X" };
		char *itohc = (c == 'X') ? IToHCU : IToHCL;

		if (flags & F_HASH)
			prefix1 = itohc + 16;
		if (i2 < 0) /*	default precision   */
			i2 = 1;
		for (len = 0; len < sizeof(buf) && (len < i2 || v); ++len) {
			buf[sizeof(buf) - 1 - len] = itohc[v & 15];
			v = v >> 4;
		}
		value = buf + sizeof(buf) - len;
	}
		break;
	default:
		return (-2); /*	unknown conversion specifier	*/
	}

	/*
	 *	Now handle actually outputing the object using
	 *	F_SPAC, F_PLUS, F_MINUS, F_HASH, F_ZERO
	 *
	 *	i1 is the field width or -1 for unlimited
	 *	i2 is the precision or -1 if undefined (already handled above)
	 */

	/*
	 *	Handle prefix, note that F_SPAC & F_PLUS are not relevant to %x/X
	 */

	if (prefix1[0] == 0) {
		if (flags & F_PLUS)
			prefix1 = "+";
		else if (flags & F_SPAC)
			prefix1 = " ";
	}

	/*
	 *	prefix goes before zero padding, but after space padding
	 */

	if (flags & F_ZERO) {
		strcpy(buf, prefix1);
		i = strlen(prefix1);
	} else {
		i = 0;
	}

	if (i1 > 0 && !(flags & F_MINUS)) {
		short j = i1 - strlen(prefix1) - len - strlen(postfix1) - trail_zero;
		short cc = (flags & F_ZERO) ? '0' : ' ';

		while (j > 20) {
			setmem(buf + i, 20, cc);
			error = (*func)(buf, 1, 20 + i, desc);
			if (error > 0)
				baseBytes += error;
			i1 -= 20 + i;
			j -= 20;
			i = 0;
		}
		if (j > 0) {
			setmem(buf + i, j, cc);
			i += j;
		}
	}

	if ((flags & F_ZERO) == 0) {
		strcpy(buf + i, prefix1);
		i += strlen(prefix1);
	}

	if (i) {
		error = (*func)(buf, 1, i, desc);
		if (error > 0)
			baseBytes += error;
		i1 -= i; /*	subtract from fieldwidth, ok if i1 goes negative */
	}

	/*
	 *	value
	 */

	if (len > 0) {
		error = (*func)(value, 1, len, desc);
		if (error > 0) {
			baseBytes += error;
			i1 -= error; /*	subtract from field width, ok if goes neg */
		}
	}

	/*
	 *	trailing zero's
	 */

	if (trail_zero) {
		short j;

		while ((j = trail_zero) > 0) {
			if (j > 20)
				j = 20;
			setmem(buf, j, '0');
			error = (*func)(buf, 1, j, desc);
			if (error > 0)
				baseBytes += error;
			i1 -= j;
			trail_zero -= j;
		}
	}

	/*
	 *	postfix
	 */

	if (postfix1[0]) {
		error = (*func)(postfix1, 1, strlen(postfix1), desc);
		if (error > 0) {
			baseBytes += error;
			i1 -= error; /*	subtract from field width, ok if goes neg */
		}
	}

	/*
	 *	post padding
	 */

	if (i1 > 0 && (flags & F_MINUS)) {
		short j = i1;

		while (j > sizeof(buf)) {
			setmem(buf, sizeof(buf), ' ');
			error = (*func)(buf, 1, sizeof(buf), desc);
			if (error > 0)
				baseBytes += error;
			j -= sizeof(buf);
		}
		if (j > 0) {
			setmem(buf, j, ' ');
			error = (*func)(buf, 1, j, desc);
			if (error > 0)
				baseBytes += error;
		}
	}
	*pva = va;
	if (error < 0)
		return (error);
	return (baseBytes);
}

#ifdef MATH_PFMT
#include <math/pfmt.c>	    /*	aux routines for math	*/
#endif

/*
 * vfwrite is a special write just for printf (buffered)
 */
static int vfwrite(const void *vbuf,size_t elmsize,size_t elms,BPTR fi)
{
	write(vbuf,elmsize*elms);
}

int printf(const char *ctl, ...)
{
    int error;
    va_list va;

    va_start(va, ctl);
    error = _pfmt(ctl, va, vfwrite, stdout);
    va_end(va);
    return(error);      /*  count/error */
}


int fwrite(const void *vbuf,size_t elmsize,size_t elms,BPTR fi)
{
	return Write(fi,vbuf,elmsize*elms);
}

void puts(char *str)
{
	int len = strlen(str);
	write(str, len);
}

int fprintf(BPTR fi,const char *ctl,...)
{
    int error;
    va_list va;

    va_start(va, ctl);
    error = _pfmt(ctl, va, fwrite, fi);
    va_end(va);
    return(error);      /*  count/error */
}

static unsigned int _swrite(char *buf,size_t n1,size_t n2,const char **sst)
{
    size_t n;

    if (n1 == 1)
	n = n2;
    else if (n2 == 1)
	n = n1;
    else
	n = n1 * n2;

    bcopy(buf, *sst, n);
    *sst += n;
    return(n2);
}

int sprintf(char *buf,const char *ctl,...)
{
    char *ptr = buf;
    int error;
    va_list va;

    va_start(va, ctl);
    error = _pfmt(ctl, va, _swrite, &ptr);
    va_end(va);
    *ptr = 0;
    return(error);      /*  count/error */
}
