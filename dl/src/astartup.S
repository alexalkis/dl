  .text
  .even
  .globl  mystart
  .globl  _mysprintf
  .globl  _myprintf
mystart:
  			jsr 	_hello
  			movel	d0,d2
  			jsr		_bflush
  			movel	d2,d0
  			rts

FLUSHONCR 	= 0		| 1--Flush stdout if last char is '\n', 0--Disable
CHECKCTRLC 	= 0		| 1--Enable ctrl-c check on bflush, 0--Disable

BUFFERSIZE	= 4096
MPIB		=  512	| Internal buffer for rawdofmt, lives on stack

_LVOAllocMem 	=  -0xc6
_LVOFreeMem  	=  -0xd2
_LVORawDoFmt 	=   -522
_LVOOutput 		=  -0x3c
_LVOWrite 		=  -0x30
_LVOSetSignal 	= -0x132

_mysprintf:
            moveml a2-a3/a6,-(sp)

            moveal  4*4(sp),a3
            moveal  5*4(sp),a0
            lea   	6*4(sp),a1
            lea.l   stuffChar(pc),a2
            move.l  _SysBase,a6
            jsr     _LVORawDoFmt(a6)

            movem.l (sp)+,a2/a3/a6
            rts


_myprintf:
            moveml a2-a3/a6,-(sp)
            moveal  4*4(sp),a0
            lea   	5*4(sp),a1
	    	lea     -MPIB(sp),sp		| allocate stack space for produced string
	    	moveal  sp,a3
            lea.l   stuffChar(pc),a2
            move.l  _SysBase,a6
            jsr     _LVORawDoFmt(a6)

	    	move.l  a3,-(sp)		| stack passing parameter, dont use -mregparm
	    	jsr	    _myPutStr		| just use bprintf it's better ;-)
            lea	    MPIB+4(sp),sp	| deallocate buffer and the parameter we pushed
            movem.l (sp)+,a2/a3/a6
            rts

stuffChar:
            moveb  d0,(a3)+
            rts

        .data
        .even
_i:
        .long   0
        .text
        .even
        .globl  _bprintf

_bprintf:
            moveml a2-a3/a6,-(sp)
            moveal  4*4(sp),a0
            lea   	5*4(sp),a1
	    	lea     -MPIB(sp),sp		| allocate stack space for produced string
	    	moveal  sp,a3
            lea.l   stuffChar(pc),a2
            move.l  _SysBase,a6
            jsr     _LVORawDoFmt(a6)
			move.l	_i,d0
	    	lea.l	_buffer,a2
str:    	moveb	(a3)+,d1
	    	moveb	d1,(a2,d0.l)
	    	addq.l	#1,d0
	    	tst.b	d1
	    	bne.s	str
			subq.l	#1,d0		| compensate for \0
			move.l	d0,_i
			cmpl	#BUFFERSIZE,d0	| can kill the system with a big write >256bytes
			jle		keep		| string, but I trust you.
			.ifgt	FLUSHONCR
			cmp.b	#'\n',d1	| if last char was \n flush the buffer
			bne		keep
			.endif
			bsr		_bflush
keep:		lea	    MPIB(sp),sp	| deallocate buffer
            movem.l (sp)+,a2-a3/a6
            rts
.lcomm _buffer,BUFFERSIZE+256

|returnedLength =  Write( file, buffer, length )
|    D0                       D1    D2      D3
|additonaly, check for ctrl-c and set global 'gotBreak' to non-zero upon ctrl-c
			.globl  _bflush
_bflush:	moveml	d2-d3/a6,-(sp)
			move.l	_i,d3			| if len is zero nothing to flush
			beq		.len0			| just check for ctrl-c
			lea		_buffer,a0
			move.l	a0,d2
			move.l	_DOSBase,a6
			jsr		_LVOOutput(a6)
			move.l	d0,d1
			jsr		_LVOWrite(a6)
			clrl 	_i
.len0:
			.ifgt	CHECKCTRLC
scheck:		move.l	_SysBase,a6
			moveql	#0,d0
			move.l	#12,d1				|#define SIGBREAKB_CTRL_C   12
			jsr		_LVOSetSignal(a6)
			and.l	#1<<12,d0
			move.l	d0,_gotBreak
			.endif
			moveml	(sp)+,d2-d3/a6
			rts

			.text
        	.proc
        	.globl  _bputchar
_bputchar:
			move.l	_i,d0
			lea		_buffer,a0
			moveb	7(sp),(a0,d0:l)
			addql	#1,d0
			move.l	d0,_i
			cmpl	#BUFFERSIZE,d0
			jle		.putcok
			bsr		_bflush
.putcok:	rts


/* Note that this is unbuffered.
** If you need it buffered, use another buffer.  You shouldn't mix with the
** stdout's buffer.
*/
		.text
        .proc
        .globl  _myerror
_myerror:
		    moveml 	d2-d3/a2-a3/a6,-(sp)
            moveal  6*4(sp),a0
            lea   	7*4(sp),a1
	    	lea     -256(sp),sp		| allocate stack space for produced string
	    	moveal  sp,a3
            lea.l   stuffChar(pc),a2
            move.l  _SysBase,a6
            jsr     _LVORawDoFmt(a6)
            move.l	a3,d2			| buffer
            move.l	_StdErr,d1		| fh - filehandle
            moveql	#0,d3
.e1:    	tst.b	(a3)+
            beq		.e2
            addq	#1,d3			| d3 will hold the length
            jbra	.e1
.e2:     	move.l	_DOSBase,a6
			jsr		_LVOWrite(a6)
			lea	    256(sp),sp		| deallocate buffer
			moveml 	(sp)+,d2-d3/a2-a3/a6
			rts

		.text
        .proc
        .globl  _realmalloc
_realmalloc:
		movem.l	d2/a6,-(sp)
		addq.l	#4,d0			|Include sizeof(LONG)
		move.l	d0,d2			|Copy to survive AllocMem
		moveq	#0,d1			|MEMF_ANYTHING
		move.l  _SysBase,a6
        jsr     _LVOAllocMem(a6)|AllocMem(size + 4, 0L)
		tst.l	d0				|Got it?
	 	beq.s	1f
		movea.l	d0,a6			|Copy pointer
		move.l	d2,(a6)+		|Stash size in first 4 bytes
		move.l	a6,d0			|return final pointer in d0
1:		movem.l	(sp)+,d2/a6
		rts


FREENULL = 0
		.text
        .proc
        .globl  _realfree
_realfree:
		.ifgt FREENULL
		tst.l	a0
		jeq		.nof
		.endif
		move.l	a6,-(sp)
		lea		-4(a0),a1		|Put in sys reg
		move.l	(a1),d0			|d0 = size to free
		move.l  _SysBase,a6
        jsr     _LVOFreeMem(a6)
		movea.l	(sp)+,a6
.nof:	rts

		.macro PICCALL addr
        jbsr    \addr
        .endm

        .text
        .proc
        .globl  ___divsi3
___divsi3:
        movel   d2, sp@-

        moveq   #1, d2     		/* sign of result stored in d2 (=1 or =-1) */
        movel   sp@(12), d1     /* d1 = divisor */
        jpl     L1
        negl    d1
#ifndef __mcoldfire__
        negb    d2              /* change sign because divisor <0  */
#else
        negl    d2              /* change sign because divisor <0  */
#endif
L1:     movel   sp@(8), d0      /* d0 = dividend */
        jpl     L2
        negl    d0
#ifndef __mcoldfire__
        negb    d2
#else
        negl    d2
#endif

L2:     movel   d1, sp@-
        movel   d0, sp@-
        PICCALL ___udivsi3 		/* divide abs(dividend) by abs(divisor) */
        addql   #8, sp

        tstb    d2
        jpl     L3
        negl    d0

L3:     movel   sp@+, d2
        rts
#endif /* L_divsi3 */


        .text
        .proc
        .globl  ___udivsi3
___udivsi3:
        movel   d2, sp@-
        movel   sp@(12), d1     /* d1 = divisor */
        movel   sp@(8), d0      /* d0 = dividend */

        cmpl    #0x10000, d1 	/* divisor >= 2 ^ 16 ?   */
        jcc     .L3             /* then try next algorithm */
        movel   d0, d2
        clrw    d2
        swap    d2
        divu    d1, d2          /* high quotient in lower word */
        movew   d2, d0          /* save high quotient */
        swap    d0
        movew   sp@(10), d2     /* get low dividend + high rest */
        divu    d1, d2          /* low quotient */
        movew   d2, d0
        jra     L6

.L3:     movel   d1, d2         /* use d2 as divisor backup */
L4:     lsrl    #1, d1     		/* shift divisor */
        lsrl    #1, d0     		/* shift dividend */
        cmpl    #0x10000, d1 	/* still divisor >= 2 ^ 16 ?  */
        jcc     L4
        divu    d1, d0          /* now we have 16-bit divisor */
    andl    #0xffff, d0 		/* mask out divisor, ignore remainder */

/* Multiply the 16-bit tentative quotient with the 32-bit divisor.  Because of
   the operand ranges, this might give a 33-bit product.  If this product is
   greater than the dividend, the tentative quotient was too large. */
        movel   d2, d1
        mulu    d0, d1          /* low part, 32 bits */
        swap    d2
        mulu    d0, d2          /* high part, at most 17 bits */
        swap    d2              /* align high part with low part */
        tstw    d2              /* high part 17 bits? */
        jne     L5              /* if 17 bits, quotient was too large */
        addl    d2, d1          /* add parts */
        jcs     L5              /* if sum is 33 bits, quotient was too large */
        cmpl    sp@(8), d1      /* compare the sum with the dividend */
        jls     L6              /* if sum > dividend, quotient was too large */
L5:     subql   #1, d0     		/* adjust quotient */

L6:     movel   sp@+, d2
        rts

        .text
        .proc
        .globl  ___modsi3
___modsi3:
        movel   sp@(8), d1      /* d1 = divisor */
        movel   sp@(4), d0      /* d0 = dividend */
        movel   d1, sp@-
        movel   d0, sp@-
        PICCALL ___divsi3
        addql   #8, sp
        movel   sp@(8), d1      /* d1 = divisor */

        movel   d1, sp@-
        movel   d0, sp@-
        PICCALL ___mulsi3  		/* d0 = (a/b)*b */
        addql   #8, sp

|        mulsl   d1,d0

        movel   sp@(4), d1      /* d1 = dividend */
        subl    d0, d1          /* d1 = a - (a/b)*b */

        movel   d1, d0
        rts

        .text
        .proc
        .globl  ___mulsi3
___mulsi3:
        movew   sp@(4), d0      /* x0 -> d0 */
        muluw   sp@(10), d0     /* x0*y1 */
        movew   sp@(6), d1      /* x1 -> d1 */
        muluw   sp@(8), d1      /* x1*y0 */

        addw    d1, d0

|        addl    d1, d0

        swap    d0
        clrw    d0
        movew   sp@(6), d1      /* x1 -> d1 */
        muluw   sp@(10), d1     /* x1*y1 */
        addl    d1, d0
        rts

