NEEDATEXIT	= 0		| make it 1 to enable the atexit function/logic
FLUSHONCR 	= 1		| 1--Flush stdout if last char is '\n', 0--Disable
CHECKCTRLC 	= 1		| 1--Enable ctrl-c check on bflush, 0--Disable

			.macro SYS function
				jsr	(_LVO\function).W(a6)
			.endm

			.text
			.even
			.globl  mystart
			|.globl  _mysprintf
			|.globl  _myprintf
mystart:
			move.l	d0,d2
			move.l	a0,a2
			move.l	0x04.w,a6
			move.l	a6,_SysBase
			sub.l	a1,a1
			SYS		FindTask
			move.l	d0,a5		| a5 will hold this Task/Process
			move.l	d0,a4		| a4 copy to reach stderr
			lea.l	_dosname,a1
			moveq	#33,d0
			SYS		OpenLibrary
			move.l	d0,_DOSBase
			jeq		.nodos
			move.l	d0,a6
			move.l	pr_WindowPtr.W(a5),OldWindowPtr
			move.l	#-1,pr_WindowPtr.W(a5)
			SYS		Output
			move.l	d0,_stdout
			SYS		Input
			move.l	d0,_stdin
			move.l	pr_CLI.W(a4),a4
			adda.l	a4,a4				| normal pointer = 4 * BPTR
			adda.l	a4,a4
			move.l	cli_StandardOutput.W(a4),_stderr
			move.l	cli_CommandName.W(a4),a4
			adda.l	a4,a4				| normal pointer = 4 * BPTR
			adda.l	a4,a4
			move.l	a4,_arg0
			moveb	(a4),d0			| BSTR's first byte holds the length
			ext.w	d0
			subq	#1,d0
larg0:		move.b	1.W(a4),(a4)+	| shift bytes left...(covering leading len)
			dbra	d0,larg0
			clrb	(a4)			| ...and put zero at end

			move.l	d2,d0	|restore cmd line arguments, len at d0
			move.l	a2,a0	| actual argument (as one string) in a0
			jsr 	_dl		| call main program
			movel	d0,d2	| preserve return value
			move.l	OldWindowPtr,pr_WindowPtr.W(a5)
			jbsr	_bflush	| flush stdout
			.ifgt NEEDATEXIT
			jbsr	callexits
			.endif
			movel	d2,d0	| restore return value to d0, where OS expects it
			rts
.nodos:		moveq	#20,d0
			rts
			| although I don't CloseLibrary the dos.library, seems that there is no memory leak
			|


BUFFERSIZE	= 4096
MPIB		=  512	| Internal buffer for rawdofmt, lives on stack

_LVOAllocMem 	=  -0xc6
_LVOFreeMem  	=  -0xd2
_LVORawDoFmt 	=   -522
_LVOOutput 		=  -0x3c
_LVOWrite 		=  -0x30
_LVOSetSignal 	= -0x132

/*
_mysprintf:
            movem.l a2-a3/a6,-(sp)

            move.l  4*4(sp),a3
            move.l  5*4(sp),a0
            lea   	6*4(sp),a1
            lea.l   stuffChar(pc),a2
            move.l  _SysBase,a6
            jsr     _LVORawDoFmt(a6)

            movem.l (sp)+,a2/a3/a6
            rts
*/
			.ifgt NEEDATEXIT
			| Functions registered with atexit are called in reverse order
			.globl  _atexit
_atexit:	move.l	fpindex,d0
			lea.l	fpexit,a1
			move.l	a0,(a1,d0.l)
			addq	#4,d0
			move.l	d0,fpindex
			rts

callexits:
			move.l	fpindex,d0
			jeq		.noentries
.el:		subq	#4,d0
			lea.l	fpexit,a1
			move.l	(a1,d0.l),a0
			movem.l	d0/a1,-(sp)
			jsr		(a0)
			movem.l	(sp)+,d0/a1
			tst.l	d0
			jne		.el
.noentries:	rts
.lcomm 		fpexit,32*4
			.endif
			/*
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
			*/

stuffChar:
            moveb  d0,(a3)+
            rts

			.global _SysBase
			.global _DOSBase
			.global _stdout
			.global _stdin
			.global _stderr
			.global _arg0
			.global _gotBreak
	        .data

	        .even
_SysBase:	.long 0
_DOSBase:	.long 0
_stdout:	.long 0
_stdin:	.long 0
_stderr:	.long 0
_arg0:		.long 0
_gotBreak:	.long 0
OldWindowPtr: .long 0
_dosname:	.asciz "dos.library"
			.even
buffIndex:	.long 0
fpindex: 	.long 0
	        .text
	        .even

.lcomm 		_buffer,BUFFERSIZE+MPIB


			.globl	_write
_write:	move.l	8(sp),d1
			move.l	4(sp),a0
			move.l	buffIndex,d0
			add.w	d1,d0
			cmp.w	#BUFFERSIZE,d0
			jle		.nofl
			jbsr	_bflush
			move.l	8(sp),d1			| flush trashes a0/d1, so restore them
			move.l	4(sp),a0			|   from stack
.nofl:		move.l	buffIndex,d0
			lea.l	_buffer,a1
			subq	#1,d1
.lc:		moveb	(a0)+,(a1,d0.l)
			addq	#1,d0
			dbra	d1,.lc
			move.l	d0,buffIndex
			.ifgt FLUSHONCR
			subq	#1,d0
			cmp.b	#'\n',(a1,d0.l)
			jne		.noov
			jbsr	_bflush
			.endif
.noov:		rts

|returnedLength =  Write( file, buffer, length )
|    D0                       D1    D2      D3
|additonaly, check for ctrl-c and set global 'gotBreak' to non-zero upon ctrl-c
			.globl  _bflush
_bflush:	moveml	d2-d3/a6,-(sp)
			move.l	buffIndex,d3			| if len is zero nothing to flush
			jeq		.len0			| just check for ctrl-c
			lea		_buffer,a0
			move.l	a0,d2
			move.l	_DOSBase,a6
			move.l	_stdout,d1
			SYS		Write
			clrl 	buffIndex
.len0:
			.ifgt	CHECKCTRLC
scheck:	move.l	_SysBase,a6
			moveql	#0,d0
			move.l	#12,d1				|#define SIGBREAKB_CTRL_C   12
			SYS		SetSignal
			and.l	#1<<12,d0
			move.l	d0,_gotBreak		| the 'extern int gotBreak;' is updated
			.endif
			moveml	(sp)+,d2-d3/a6
			rts


			.global _bcopy
			| a1 src
			| a0 dest
			| d0 size
_bcopy:	moveal	4(sp),a1
			moveal	8(sp),a0
			movel	12(sp),d0
			jbeq	.bdone

			moveq   #12,d1
			cmp.l   d1,d0
			jbcs	.notl

			move.l	a1,d1
			and		#1,d1
			jbne	.notl				| nope, do byte copy

			move.l	a0,d1
			and		#1,d1
			jbne	.notl				| nope, do byte copy

			movel	d0,d1
			and		#3,d1				| save the bytes that will remain
			move.w	d1,-(sp)

			moveq	#96,d1
			cmp.l	d1,d0
			jbcs	.nobatch
			movem.l	d1-d7/a2-a6,-(sp)
.batchl:	movem.l	(a1)+,d1-d7/a2-a6
			movem.l	d1-d7/a2-a6,(a0)
			moveq	#48,d1
			add.l	d1,a0
			sub.l	d1,d0
			cmp.l	d1,d0
			jbcc	.batchl
			movem.l	(sp)+,d1-d7/a2-a6

.nobatch:	lsr.l	#2,d0
			jbeq	.bdone
			subq.l	#1,d0
.ll:		move.l	(a1)+,(a0)+
			dbra	d0,.ll
			clrw	d0
			subql	#1,d0
			jbcc	.ll

			move.w	(sp)+,d0
			jbeq	.bdone
			ext.l	d0

.notl:		subq.l	#1,d0
.bl:		move.b	(a1)+,(a0)+
			dbra	d0,.bl
			clrw	d0
			subql	#1,d0
			jbcc	.bl
.bdone:		rts



| void putchar(char c)
| - has hardcoded the flush on '\n'
			.text
        	.proc
        	.globl  _putchar
_putchar:
			move.l	buffIndex,d0
			lea		_buffer,a0
			moveb	7(sp),d1
			moveb	d1,(a0,d0:l)
			addql	#1,d0
			move.l	d0,buffIndex
			cmpl	#BUFFERSIZE,d0
			jeq		.flushit
			cmp.b	#'\n',d1
			jne		.putcok
.flushit:	jbsr	_bflush
.putcok:	rts


/* Note that this is unbuffered.
** If you need it buffered, use another buffer.  You shouldn't mix with the
** stdout's buffer.
** Also note the horrible crashing you'd achieve with a string > MPIB bytes (512)
*/
		.text
        .proc
        .globl  _myerror
_myerror:
		    moveml 	d2-d3/a2-a3/a6,-(sp)
            moveal  6*4(sp),a0
            lea   	7*4(sp),a1
	    	lea     -MPIB(sp),sp		| allocate stack space for produced string
	    	moveal  sp,a3
            lea.l   stuffChar(pc),a2
            move.l  _SysBase,a6
            SYS		RawDoFmt
            move.l	a3,d2			| buffer
            move.l	_stderr,d1		| fh - filehandle
            moveql	#0,d3
.e1:    	tst.b	(a3)+
            beq		.e2
            addq	#1,d3			| d3 will hold the length
            jbra	.e1
.e2:     	move.l	_DOSBase,a6
			SYS		Write
			lea	    MPIB(sp),sp		| deallocate buffer
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
	        SYS		AllocMem		|AllocMem(size + 4, 0L)
			tst.l	d0				|Got it?
		 	beq.s	1f
			movea.l	d0,a6			|Copy pointer
			move.l	d2,(a6)+		|Stash size in first 4 bytes
			move.l	a6,d0			|return final pointer in d0
1:			movem.l	(sp)+,d2/a6
			rts


ALLOWFREENULL = 0
		.text
        .proc
        .globl  _realfree
_realfree:
		.ifgt ALLOWFREENULL
		tst.l	a0				| check for null
		jeq		.nof			| and don't attempt to free it
		.endif
		move.l	a6,-(sp)
		lea		-4(a0),a1		|Put in sys reg
		move.l	(a1),d0			|d0 = size to free
		move.l  _SysBase,a6
        SYS     FreeMem
		movea.l	(sp)+,a6
.nof:	rts

		.macro PICCALL addr
        jbsr    \addr
        .endm


.ifdef needmath
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

.text
        .proc
        .globl  ___umodsi3
___umodsi3:
        movel   sp@(8), d1      /* d1 = divisor */
        movel   sp@(4), d0      /* d0 = dividend */
        movel   d1, sp@-
        movel   d0, sp@-
        PICCALL ___udivsi3
        addql   #8, sp
        movel   sp@(8), d1      /* d1 = divisor */
#define __mcoldfire__
#ifndef __mcoldfire__
        movel   d1, sp@-
        movel   d0, sp@-
        PICCALL ___mulsi3  /* d0 = (a/b)*b */
        addql   #8, sp
#else

        |mulsl   d1,d0
#endif
        movel   sp@(4), d1      /* d1 = dividend */
        subl    d0, d1          /* d1 = a - (a/b)*b */
        movel   d1, d0
        rts

.endif

_LVOFindTask 	= -0x126
_LVOOpenLibrary = -0x228
_LVOAllocMem	= -0xc6
_LVOFreeMem		= -210
_LVOAlert		= -108
_LVOWaitPort	= -384
_LVOGetMsg		= -372
_LVOReplyMsg	= -378
_LVOCloseLibrary= -414
_LVOForbid		= -132
_LVORawDoFmt 	= -522

_LVOWrite 		= -0x30
_LVOInput		= -0x36
_LVOOutput 		= -0x3c

pr_CLI	 		= 172
pr_ReturnAddr 	= 176
pr_MsgPort 		= 92
cli_CommandName = 16
pr_WindowPtr	= 184
cli_StandardOutput = 56
