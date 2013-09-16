	.macro SYS function
		jsr	_LVO\function(a6)
	.endm

height=	-4
width=	-8
conid=	-12
packet=	-16
rpport=	-20
rpstr=	-40
_LVOInput = -0x36
_LVOIsInteractive = -0xd8
_LVOOutput = -0x3c
_LVOFindTask = -0x126
_LVOWrite = -0x30
_LVOWaitForChar = -0xcc
_LVORead = -0x2a
_LVOPutMsg = -0x16e
_LVOGetMsg = - 0x174
_LVOWaitPort = - 0x180
_LVOAllocSignal = -0x14a
_LVOFreeSignal = -0x150
_LVOAddPort = -0x162
_LVORemPort = -0x168

pr_ConsoleTask	=	0xA4
MEMF_CLEAR		=	0x10000
sp_Msg			=	0x0
sp_Pkt			=	0x14
sp_SIZEOF		=	0x44
dp_Link			=	0
dp_Port			=	4
dp_Arg1			=	0x14
dp_Type			=	8
ACTION_SCREEN_MODE	=	0x3E2
LN_NAME			=	0xA
LN_PRI			=	9
LN_TYPE			=	8
MP_FLAGS		=	0xE
MP_MSGLIST		=	0x14
MP_SIGBIT		=	0xF
MP_SIGTASK		=	0x10
MP_SIZE			=	0x22
NT_MSGPORT		=	4
PA_SIGNAL		=	0
		.text
        .proc
        .globl  _GetWinBounds

/* --------------------------------------------------------------------- *
* void GetWinBounds(width, height)
*   long *width, *height;
*          a0       a1
* Find current console window, determine width and height
* in terms of current font, update width and height VPARMS passed.
* --------------------------------------------------------------------- */
_GetWinBounds:
	link	a5,#-44
	movem.l	d2-d4/a2/a6,-(sp)
	movel	a0,width(a5)				|Save width/height pointers on stack
	move.l	a1,height(a5)

	moveal	_DOSBase,a6
	jsr		_LVOInput(a6)				|Grab Input filehandle
	move.l	d0,d1
	jsr		_LVOIsInteractive(a6)		|IsInteractive(Input())?
	tst.l	d0
	beq.s	gwbnowbrep					|Nope, can't get a bounds report
	SYS		Output
	move.l	d0,d1
	SYS		IsInteractive				|IsInteractive(Output())?
	tst.l	d0
	beq.s	gwbnowbrep					|Nope, don't clutter output stream

	suba.l	a1,a1
	moveal	_SysBase,a6
	SYS		FindTask					|d0 = FindTask(0L), our process
	movea.l	d0,a0						|Transfer to address reg
	move.l	pr_ConsoleTask(a0),conid(a5) |Save proc->pr_ConsoleTask
	tst.l	conid(a5)					|Is there really a console there?
	bne.s	gwbgotcon					|Yep

gwbnowbrep:
| Else we cannot get a window bounds report from this source
	|move.l	pr_ConsoleTask(a0),d1
	moveq	#23,d1			|return H=23
	moveq	#77,d2			|       W=77
	bra	gwbupdate

gwbgotcon:
	moveq	#0,d4			|Clear our success status register

	moveq	#0,d0
	movea.l	d0,a0
	jsr		CreatePort
	move.l	d0,rpport(a5)		|rpport = CreatePort(0L, 0L)
	beq		gwbdone					|Oops, no signals or ram available!
	moveq	#sp_SIZEOF,d0
	jsr		_realmalloc
	move.l	d0,packet(a5)		|packet = AllocBlock(sizeof(*packet))
	beq		gwbfreeport		|Oops, no ram, free up port

| Okay, we got our process id, reply port, and packet
| Now toggle the console into raw mode
	movea.l	rpport(a5),a2
	movea.l	d0,a1
	movea.l	conid(a5),a0
	moveq	#1,d0
	jsr		SetConsoleType		|SetConsoleType(1L, conid, packet, rpport)

| Request a window bounds report
	moveal	_DOSBase,a6
	SYS		Output
	move.l	d0,d1
	moveq	#4,d3
	lea		_gwbrstr,a0
	move.l	a0,d2
	SYS		Write			|Write(Output(), "\2330 q", 4L)|
	cmpi.l	#4,d0			|Did the console choke on it?
	bne		gwbsetcook		|hmmm, see if we can back out gracefully

| Read the report string into stack buffer, if there is one
	move.l	#10000,d2
	SYS		Input
	move.l	d0,d1

	SYS		WaitForChar		|WaitForChar(Input(), 10000L) (.01 secs)
	tst.l	d0				|Did we get the report?
	beq		gwbsetcook		|Nope, must not be a report coming

	SYS		Input
	move.l	d0,d1
	moveq	#16,d3			|Don't let it get longer than 16 characters
	lea		rpstr(a5),a0		|Point to input string area
	move.l	a0,d2
	SYS		Read			|Read(Input(), rpstr, 16L)
	move.l	d0,d4			|Save read length while we close shop

| Turn the console back to cooked mode pronto to avoid cursor blink
gwbsetcook:
	movea.l	rpport(a5),a2
	movea.l	packet(a5),a1
	movea.l	conid(a5),a0
	moveq	#0,d0
	jsr		SetConsoleType		|SetConsoleType(0L, conid, packet, rpport)

| Release resources we borrowed
gwbfreepack:
	move.l	packet(a5),d0		|Did we allocate a packet?
	beq.s	gwbfreeport		|nay, check for port to free
	movea.l	d0,a0
	jsr		_realfree			|Else FreeBlock(packet)

gwbfreeport:
	move.l	rpport(a5),d0		|if (rpport)...
	beq		gwbdone			|nope
	jsr		DeletePort		|Else DeletePort(rpport)


	moveml	d0-d7/a0-a6,-(sp)
	moveal	_DOSBase,a6
	SYS		Output
	move.l	d0,d1			| filehandle, i.e. output()
	movel	d4,d3			| strlen
	lea	rpstr(a5),a2


	movel	a2,d2			| the string
	SYS		Write			| Write(Output(), string, strlen)|
	moveml	(sp)+,d0-d7/a0-a6




| Finally, sanity check window bounds report string
| d4 = length of report string according to Read()
	cmpi.l	#9,d4			|Less than 8 characters returned?
	 ble	gwbdone			|hmmm, phonky bounds report from DOS?
	lea	rpstr(a5),a2		|a2 = rpstr
	cmpi.b	#';',4(a2)		|Matches a typical report template?
	 bne	gwbdone			|nope, got some weird junk back?
	cmpi.b	#'r',-1(a2,d4.w)	|Last byte is 'r' for report?
	 bne	gwbdone			|Nope, message fubar!

/*
* Parse the height and width variables from the field now
* Our report format looks like this in hex:
* 	9b 31 3b 31 3b y2 y1 3b x2 x1 20 72
* Or in ascii:
*	<0x9b>1|1|20|77 r
* Which would indicate a width of 77 cols and a height of 20 rows for
* the current console device
*
* REGS:	a2 points to beginning of 'r' terminated string */

	addq.w	#5,a2			|Point to first char of Y size
	moveq	#0,d1			|Clear out work reg

| Convert ascii rows value to LONG, update host data
	move.b	(a2)+,d1		|Grab a Y
	subi.w	#'0',d1			|Less ascii offset
	cmpi.b	#';',(a2)		|Any more Y digits?
	beq.s	ml1			|Nope
	mulu	#10,d1			|Else shift by 10
	add.b	(a2)+,d1		|Add least significant Y digit
	subi.b	#'0',d1			|Less ascii offset
	cmpi.b	#';',(a2)		|Any more Y digits?
	 beq.s	ml1			|Nope
	mulu	#0x000a,d1		|Else shift by 10
	add.b	(a2)+,d1		|Add least significant Y digit
	subi.b	#'0',d1			|Less ascii offset
					|We'll assume screen height < 999 rows
ml1:
| Convert ascii columns value to LONG, update host data
	addq.w	#1,a2			|Move past the ';' separator
	moveq	#0,d2			|Zap work reg
	move.b	(a2)+,d2		|Grab msd of X
	cmpi.b	#' ',d2			|Premature end?
	 beq	gwbdone			|Huh, must be garbage - don't update VPARMS
	cmpi.b	#';',d2			|Also a possible error
	 beq	gwbdone
	cmpi.b	#'r',d2			|And what about this?
	 beq	gwbdone

	subi.b	#'0',d2			|Okay, adjust ascii offset
	cmpi.b	#' ',(a2)		|Hit end of report?
	 beq.s	ml2			|Yep
	mulu	#0x000a,d2		|Else shift by 10
	add.b	(a2)+,d2		|Add next digit
	subi.b	#'0',d2			|Ascii adjust
	cmpi.b	#' ',(a2)		|Hit end of report?
	 beq.s	ml2				|Yep
	mulu	#0x000a,d2		|Else shift by 10
	add.b	(a2),d2			|Add next digit
	subi.b	#'0',d2			|Ascii adjust

ml2:
gwbupdate:
| Finally, update parameters by reference
	movea.l	height(a5),a0		|Grab height VPARM
	move.l	d1,(a0)			|*height = d1
	movea.l	width(a5),a0		|Grab width VPARM
	move.l	d2,(a0)			|*width = d2

gwbdone:
	movem.l	(sp)+,d2-d4/a2/a6
	unlk	a5
	rts
alkis:
	bra		gwbupdate

SetConsoleType:
	movem.l	a2/a3/a5/a6,-(sp)

	movea.l	a0,a3			|Copy process pointer
	movea.l	a1,a5			|Copy packet pointer
	lea		sp_Pkt(a5),a0		|a0 = &packet->sp_Pkt
	move.l	a0,sp_Msg+LN_NAME(a5)	|p->sp_Msg.mn_Node.ln_Name = &p->sp_Pkt
	lea		sp_Msg(a5),a0		|a0 = &packet->sp_Msg
	move.l	a0,sp_Pkt+dp_Link(a5)	|p->sp_Pkt.dp_Link = &p->sp_Msg
	move.l	a2,sp_Pkt+dp_Port(a5)	|p->sp_Pkt.dp_Port = replyport
	move.l	#ACTION_SCREEN_MODE,sp_Pkt+dp_Type(a5)	|Set function

	tst.w	d0			|On or Off?
	beq		sl1
	move.l	#-1,sp_Pkt+dp_Arg1(a5)	|RAW ON
	bra.s	sl2
sl1:
	clr.l	sp_Pkt+dp_Arg1(a5)	|RAW OFF
sl2:
	movea.l	a3,a0
	movea.l	a5,a1
	movea.l	_SysBase,a6
	SYS		PutMsg		|PutMsg(proc, packet)

	movea.l	a2,a0
	SYS		WaitPort		|WaitPort(port)
	movea.l	a2,a0
	SYS		GetMsg			|(void)GetMsg(port)

	movem.l	(sp)+,a2/a3/a5/a6
	rts


CreatePort:
	movem.l	d5/d7/a2/a5/a6,-(sp)

	move.l	a0,a5			|Save Name
	move.l	d0,d5			|Save Pri

| Allocate a free signal, crap out if we can't
	moveq	#-1,d0
	movea.l	_SysBase,a6
	SYS		AllocSignal
	cmp.l	#-1,d0			|Did we get a signal?
	bne.s	cpgotsig		|Yep
	moveq	#0,d0			|Otherwise return NULL
	bra		cpdone

cpgotsig:
	move.l	d0,d7			|Save our signal

| Allocate memory for MsgPort
	moveq	#MP_SIZE,d0		|Size of MsgPort
	jsr		_realmalloc		|Allocate it
	tst.l	d0			|Did we get it?
	bne.s	cpgotport		|Yep

	move.l	d7,d0			|Otherwise crap out, free signal
	SYS		FreeSignal
	moveq	#0,d0			|Return NULL
	bra		cpdone

cpgotport:
	move.l	d0,a2			|This is our new port!
	move.l	a5,LN_NAME(a2)		|port->mp_Node.ln_Name = name
	move.b	d5,LN_PRI(a2)		|port->mp_Node.ln_Pri = priority
	move.b	#NT_MSGPORT,LN_TYPE(a2) |port->mp_Node.ln_Type = NT_MSGPORT
	move.b	#PA_SIGNAL,MP_FLAGS(a2) |port->mp_Flags = PA_SIGNAL
	move.b	d7,MP_SIGBIT(a2)	|port->mp_SIGBIT = sigBit
	suba.l	a1,a1
	SYS		FindTask
	move.l	d0,MP_SIGTASK(a2)	|port->mp_SIGTASK = FindTask(0L)

	cmpa.l	#0,a5			|Is this a new name?
	beq.s	cpnoname		|Nope, add it to the msg list

	movea.l	a2,a1
	SYS		AddPort			|Otherwise add this port
	move.l	a2,d0			|Return port pointer
	bra.s	cpdone

cpnoname:
| Initialized New List head
	lea		MP_MSGLIST(a2),a0 	|a0 = &port->mp_MsgList
	move.l	a0,(a0)			|list->lh_Head = list
	addq.l	#4,(a0)			|list->lh_Head += 4L
	clr.l	4(a0)			|list->lh_Tail = 0L
	move.l	a0,8(a0)		|list->lh_TailPred = list
	move.l	a2,d0			|Return port pointer

cpdone:
	movem.l	(sp)+,d5/d7/a2/a5/a6
	rts

/* ------------------------------------------------------------------------- *
* DeletePort(port)(d0)
* ------------------------------------------------------------------------- */

DeletePort:
	movem.l	a5/a6,-(sp)

	move.l	d0,a5
	movea.l	_SysBase,a6
	tst.l	LN_NAME(a5)		|Is there a name?
	beq.s	dpnoname

	move.l	d0,a1
	SYS		RemPort			|RemPort(port)

dpnoname:
	move.b	#0xff,LN_TYPE(a5) 	|port->mp_Node.ln_Type = 0xff
	move.l	#-1,MP_MSGLIST(a5) 	|port->mp_MsgList.lh_Head = -1L

	moveq	#0,d0
	move.b	MP_SIGBIT(a5),d0	|d0 = port->mp_SigBit
	SYS		FreeSignal		|FreeSignal(d0)

	movea.l	a5,a0
	jsr		_realfree			|FreeBlock(port)

	movem.l	(sp)+,a5/a6
	rts

