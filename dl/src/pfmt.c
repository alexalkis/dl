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

/*
 * Basically, we need this instead of Exec's RawDoFmt cause of the '*' format variable width specifier
 * Adds 2620 bytes (.o size) :-(
 * Taken from the dice src distribution (if the original top comment wasn't clear enough :P)
 * No float support.
 */
#include <stdarg.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include "stdio.h"
#include "string.h"

//typedef unsigned long size_t;
#define F_SPAC	    1
#define F_PLUS	    2
#define F_MINUS     4
#define F_HASH	    8
#define F_ZERO	    16

#define F_SHORT     32
#define F_LONG	    64
#define F_DLONG     128

#define MATH_BUFSIZE	32

//used to skip error checking
#define NOERROR


//#define MATH_PFMT   1

#ifdef MATH_PFMT
#include <math.h>
#endif

int fwrite(const void *vbuf, size_t elmsize, size_t elms, BPTR fi);
int _pfmtone(char c, va_list *pva,
		unsigned int (*func)(char *, size_t, size_t, void *), void *desc,
		short flags, short i1, short i2, int baseBytes);

int _pfmt(char *ctl, va_list va,
		unsigned int (*func)(char *, size_t, size_t, void *), void *desc)
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
			v = v / base;
		}
		value = buf + sizeof(buf) - len;
	}
		break;
#ifdef MATH_PFMT
//#include 'mathpfmt.c"	    /*	cases for math	*/
    case 'e':       /*  [-]d.dddddde[+/-]dd     */
    case 'E':       /*  [-]d.ddddddE[+/-]dd     */
    case 'f':       /*  [-]d.dddddd             */
    case 'g':       /*  if exp < -4 or exp > prec use 'e', else use 'f' */
    case 'G':       /*  if exp < -4 or exp > prec use 'E', else use 'f' */
	{
	    static char prefix[32];
	    short xp;
	    short _pfmt_round(char *, short, short);
	    short gtrunc;

	    if ((c == 'g' || c == 'G') && !(flags & F_HASH))
		gtrunc = 1;
	    else
		gtrunc = 0;

	    if (i2 < 0) 	/*  default precision	    */
		i2 = 6;

	    if (flags & F_DLONG) {
		va_arg(va, long double);
		i = xp = 0;
	    } else {
		double d;

		d = va_arg(va, double);
		if (d < 0.0) {
		    d = -d;
		    prefix1 = "-";
		}

		/*
		 *  construct prefix buffer by extracting one digit at
		 *  a time
		 */

		if (d >= 1.0)
		    xp = (int)log10(d);
		else
		    xp = (int)log10(d) - 1;

		d = d * pow(10.0, (double)-xp) + (DBL_EPSILON * 5);

		for (i = 0; i < DBL_DIG; ++i) {
		    prefix[i] = (int)d;
		    d = (d - (double)prefix[i]) * 10.0;
		}
	    }

	    /*
	     *	prefix	- prefix array (might have leading zeros)
	     *	i	- number of digits in array
	     *	xp	- exponent of first digit
	     *
	     *	handle %g and %G
	     */

	    if (c == 'g' || c == 'G') {
		if (xp < -4 || xp >= i2)
		    c = (c == 'g') ? 'e' : 'E';
		else
		    c = 'f';
	    }

	    /*
	     *	handle %e and %E
	     */

	    value = buf + sizeof(buf) - MATH_BUFSIZE;

	    if (c == 'e' || c == 'E') {
		if ((i = _pfmt_round(prefix, i2 + 1, i)) < 0) {
		    i = -i;
		    ++xp;
		}
		if (i == 0)
		    xp = 0;
		value[0] = prefix[0] + '0';
		if (i2 > 0 && (gtrunc == 0 || i > 1)) {
		    value[1] = '.';
		    for (len = 0; len < i2 && len < (MATH_BUFSIZE - 8); ++len) {
			if (len + 1 >= i) {
			    if (gtrunc == 0)
				trail_zero = i2 - len;
			    break;
			}
			value[len+2] = prefix[len+1] + '0';
		    }

		    /*
		     *	put in at least one '0' after the decimal!
		     *	(don't have to check gtrunc because prefix
		     *	array has at least 2 entries)
		     */

		    if (trail_zero == 0 && len == 0) {
			value[2] = '0';
			++len;
		    }
		    ++len;
		}
		++len;
		{
		    short yp = (xp < 0) ? -xp : xp;

		    postfix1 = value + len;
		    postfix1[0] = c;
		    postfix1[1] = (xp < 0) ? '-' : '+';
		    postfix1[2] = yp / 100 + '0';
		    postfix1[3] = yp / 10 % 10 + '0';
		    postfix1[4] = yp % 10 + '0';
		    postfix1[5] = 0;
		}
	    } else {
		short pi;

		if ((i = _pfmt_round(prefix, i2 + 1 + xp, i)) < 0) {
		    i = -i;
		    ++xp;
		}
		for (len = pi = 0; len < MATH_BUFSIZE && xp >= 0; ++len) {
		    if (pi < i) {
			value[len] = prefix[pi] + '0';
			++pi;
		    } else {
			value[len] = '0';
		    }
		    --xp;
		}
		if (len == 0)
		    value[len++] = '0';

		/* XXX */

		/*
		 *  pre-zeros
		 */

		if (len < MATH_BUFSIZE && i2 > 0 && (gtrunc == 0 || pi < i)) {
		    short prec = 0;

		    value[len++] = '.';

		    while (len < MATH_BUFSIZE && -(prec + 1) > xp && prec < i2) {
			value[len++] = '0';
			++prec;
		    }
		    while (len < MATH_BUFSIZE && prec < i2) {
			if (pi < i)
			    value[len++] = prefix[pi++] + '0';
			else if (gtrunc == 0)
			    value[len++] = '0';
			++prec;
		    }
		}
	    }
	}
	break;

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

	if ((flags & F_ZERO) && *prefix1) {
		strcpy(buf, prefix1);
		i = strlen(prefix1);
	} else {
		i = 0;
	}

	if (i1 > 0 && !(flags & F_MINUS)) {
		short j = i1 - (*prefix1 ? strlen(prefix1) : 0) - len
				- (*postfix1 ? strlen(postfix1) : 0) - trail_zero;
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

	if ((flags & F_ZERO) == 0 && *prefix1) {
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
//#include "mathpfmt.c"	    /*	aux routines for math	*/
short int  _pfmt_round(char *prefix, short int ri, short int max)
{
    short carry;
    char *ptr;

    if (ri < max) {	/*  handle rounding + the log boundry case	    */
	max = ri;
	if (max < 0)
	    max = 0;
	carry = 5;
    } else {		/*  handle log boundry case (entries that are 10)   */
	if (max)
	    ri = --max;
	else
	    ri = -1;
	carry = 0;
    }
    for (ptr = prefix + ri; ri >= 0; --ri, --ptr) {
	*ptr += carry;
	if (*ptr >= 10) {
	    *ptr -= 10;
	    carry = 1;
	} else {
	    carry = 0;
	}
	if (*ptr == 0 && ri == max - 1)
	    --max;
    }
    if (carry) {
	movmem(prefix, prefix + 1, max);
	++max;
	prefix[0] = 1;
	return(-max);
    }
    return(max);
}

#endif

/*
 * vfwrite is a special write just for printf (buffered)
 * (Only stdout is buffered in astartup.S)
 */
static int vfwrite(const void *vbuf, size_t elmsize, size_t elms, BPTR fi)
{
	//static int count=0;
	//myerror("%ld elmsize: %ld elms: %ld '%s'\n",++count,elmsize,elms,vbuf);

	//if (elmsize == 1) {
		write(vbuf, elms);
	//} else {
	//	write(vbuf, (short) ((short) elmsize * (short) elms));
	//}
}

int printf(const char *ctl, ...)
{
	int error;
	va_list va;

	va_start(va, ctl);
	error = _pfmt(ctl, va, vfwrite, stdout);
	va_end(va);
	return (error); /*  count/error */
}

int fwrite(const void *vbuf, size_t elmsize, size_t elms, BPTR fi)
{
	return Write(fi, vbuf, elmsize * elms);
}

void puts(const char *str)
{
	int len;

	if (*str) {
		len = strlen(str);
		write(str, len);
	}
	write("\n", 1);
}

int fprintf(BPTR fi, const char *ctl, ...)
{
	int error;
	va_list va;

	va_start(va, ctl);
	error = _pfmt(ctl, va, fwrite, fi);
	va_end(va);
	return (error); /*  count/error */
}

static unsigned int _swrite(char *buf, size_t n1, size_t n2, const char **sst)
{
	size_t n;

	if (n1 == 1) {
		n = n2;
	} else if (n2 == 1) {
		n = n1;
	} else {
		n = n1 * n2;
	}
	bcopy(buf, *sst, n);
	*sst += n;
	return (n2);
}

int sprintf(char *buf, const char *ctl, ...)
{
	char *ptr = buf;
	int error;
	va_list va;

	va_start(va, ctl);
	error = _pfmt(ctl, va, _swrite, &ptr);
	va_end(va);
	*ptr = 0;
	return (error); /*  count/error */
}
