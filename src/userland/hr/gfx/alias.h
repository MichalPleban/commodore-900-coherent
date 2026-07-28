
/* some real pretty macros stolen from rico */
#define BadHandle	(-1)
#define	MAX(a, b)	((a) > (b) ? (a) : (b))
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#define gkTexture(p)	((p) == MAXPAT ? gkPattern : texture[(p)])


#define	sm_frame sm_chrval1
#define	sm_log   sm_chrval2

/*
 *		Element of the global structure 'gk'
 */
#define gkLorigin	gk.wn_Lorigin
#define gkCrect		gk.wn_Crect
#define gkUpdRgn	gk.wn_UpdRgn
#define	gkBitMap	(* (BITMAP *) gk.wn_Layer)
#define	gkDp		gk.wn_Dp
#define gkPen		gk.wn_G.wn_Pen
#define gkFpat		gk.wn_G.wn_Fpat
#define gkBpat		gk.wn_G.wn_Bpat
#define gkMask		gk.wn_G.wn_Mask
#define gkFcolor	gk.wn_G.wn_Fcolor
#define gkBcolor	gk.wn_G.wn_Bcolor
#define gkLogop		gk.wn_G.wn_Logop
#define gkFont		gk.wn_G.wn_Font
#define gkFfid          gk.wn_G.wn_Font.fi_Id
#define	gkPattern	gk.wn_G.wn_Pattern
#define gkEvmask	gk.wn_Evmask
#define gkWid		gk.wn_Wid
#define gkWmgr		gk.wn_Wmgr
#define gkType		gk.wn_Type
#define gkLayer		((LAYER *) gk.wn_Layer)
#define gkPsize		gk.wn_Psize
#define gkCursor	gk.wn_Cursor
#define	gkCascnt	gk.wn_Cascnt
#define	gkFlags		gk.wn_Flags

#define gkFullyVis	(gkFlags & 0x01)

#define legalwindow(i) 	( i >= 0 && i <= MAX_WINDOWS )


/*
 *		Defines for getting to fields of messages 
 */
#define msgSender	msg.msg_Sender
#define msgReceiver	msg.msg_Sender
#define msgCmd		msg.msg_Cmd
#define msgData0	(msg.msg_Data[0])
#define msgData1	(msg.msg_Data[1])
#define msgData2	(msg.msg_Data[2])
#define msgBytH0	(* ((char *)  &msgData0) )
#define msgBytL0	(* (((char *) &msgData0) + 1) )
#define msgBytH1	(* ((char *)  &msgData1) )
#define msgBytL1	(* (((char *) &msgData1) + 1) )
#define msgBytH2	(* ((char *)  &msgData2) )
#define msgBytL2	(* (((char *) &msgData2) + 1) )
#define msgWid		(msgBytH0)
#define msgPt		(* (POINT *) &msgData1)
#define msgPtr		(* (char **) &msgData1)

/* 
 * 		Message related defines 
 */
#define iswmgr(i)	( (i) > WMGR && (i) < WINDOW )
#define DESKTOP		( WMGR )
#define isdesktop(i)	( (i) == DESKTOP )
#define iskbdmouse(i)	( (i) == DRIVER )
#define isvsender(i)( iswmgr((i)) || isdesktop((i)) || iskbdmouse((i)) )
#define isvcmd(i)  	( (i) >= SM_BASE_CMD && (i) <= SM_TOP_CMD )

/* 
 *		and a few defines for the graphics state stuff 
 */

#define	gk_Width	gk_A
#define	gk_Height	gk_dA

#define	msgPat	( ((MSHAPE *) gkmsg)->sm_p  )
#define	msgVerb	( ((MSHAPE *) gkmsg)->sm_verb )
#define	msgRect	( ((MSHAPE *) gkmsg)->sm_rect )
#define	msgVal1	( ((MSHAPE *) gkmsg)->sm_intval1 )
#define	msgVal2	( ((MSHAPE *) gkmsg)->sm_intval2 )
#define	msgChr1 ( ((MSHAPE *) gkmsg)->sm_chrval1 )
#define	msgChr2	( ((MSHAPE *) gkmsg)->sm_chrval2 )
#define	msgNv	( ((MPOLY  *) gkmsg)->sm_nv   )
#define	msgVs	( ((MPOLY  *) gkmsg)->sm_vs   )




