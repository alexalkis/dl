/*
 * stdio.h
 *
 *  Created on: Oct 6, 2013
 *      Author: alex
 */

#ifndef STDIO_H_
#define STDIO_H_
#include <stdarg.h>
#include <dos/dos.h>
#include <proto/dos.h>

extern BPTR		stderr;
extern BPTR		stdout;
int printf(const char *ctl, ...);
int fprintf(BPTR fi,const char *ctl,...);
int sprintf(char *buf,const char *ctl,...);

#endif /* STDIO_H_ */
