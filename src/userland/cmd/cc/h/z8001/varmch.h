/*
 * h/z8001/varmch.h
 * Machine-specific variant bits for the segmented Z8000 (Z8001) compiler.
 * Template: h/i386/varmch.h. Slots VMBASE..VMAXIM (34..47, see h/var.h).
 *
 * The two memory models map onto the Z8000 segmentation:
 *   VSEG  (LARGE)  = segmented: default pointer is a 2-word seg:offset (LPTR),
 *                   the Z8001/Z8003 23-bit address space (the C900 default).
 *   VNSEG (SMALL)  = non-segmented: flat 16-bit pointers (SPTR), Z8002/Z8004.
 * Keep the SMALL/LARGE names too so the shared cc driver selects a model with
 * the same flags it uses for the i8086/i386 compilers.
 */

#define	VSMALL	(VMBASE+0)	/* non-segmented (Z8002/4): 1-word pointers	*/
#define	VNSEG	(VMBASE+0)	/* synonym for VSMALL				*/
#define	VLARGE	(VMBASE+1)	/* segmented (Z8001/3): 2-word seg:off pointers	*/
#define	VSEG	(VMBASE+1)	/* synonym for VLARGE				*/
#define	VEPU	(VMBASE+2)	/* emit Z8070 EPU (EPA) floating-point templates */
#define	VEMUFP	(VMBASE+3)	/* emit Extended-Instruction-trap soft-fp calls	*/
#define	VALIGN	(VMBASE+4)	/* word-align the stack (Z8000 wants even SP)	*/
#define	VBUSLOCK (VMBASE+5)	/* emit bus-lock (Z8003/4) for atomic sequences	*/
#define	VXSTAT	(VMBASE+6)	/* output static external items			*/
#define	VRAM	(VMBASE+7)	/* place pure/const data in the data space (RAM) */
#define	VTPA	(VMBASE+8)	/* CP/M-8000 transient program: frame/auto
				 * addresses carry the TPA segment (0x32)
				 * instead of Coherent's flat-model segment 0
				 * or the kernel's system stack (VKERN, 0x3F) */

/* end of h/z8001/varmch.h */
