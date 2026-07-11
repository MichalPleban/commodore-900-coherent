/*
 * defines for CIO
 */

#define	MICR		0x01		/* master interrupt control	*/
#define MCCR		0x03		/* master config control	*/
#define	PAIV		0x05		/* port A interrupt vector	*/
#define PBIV		0x07		/* port B interrupt vector	*/
#define CTIV		0x09		/* counter/timer interrupt vect.*/
#define PCDPP		0x0B		/* port C data path polar.	*/
#define PCDD		0x0D		/* port C data direction	*/
#define PCSIOC		0x0F		/* port C special I/O control	*/
#define	PACS		0x11		/* port A command & status	*/
#define	PBCS		0x13		/* port B command & status	*/
#define	PAD		0x1B		/* port A data			*/
#define	PBD		0x1D		/* port B data			*/
#define PCD		0x1F		/* port C data			*/
#define	PAMS		0x41		/* port A mode spec 		*/
#define	PAHS		0x43		/* port A handshake spec 	*/
#define	PADPP		0x45		/* port A data path polar.	*/
#define	PADD		0x47		/* port A data direction	*/
#define PASIOC		0x49		/* port A special I/O control	*/
#define	PAPP		0x4B		/* port A pattern polarity	*/
#define	PAPT		0x4D		/* port A pattern transistion	*/
#define	PAPM		0x4F		/* port A pattern mask		*/
#define	PBMS		0x51
#define	PBHS		0x53
#define	PBDPP		0x55
#define	PBDD		0x57
#define	PBSIOC		0x59
#define	PBPP		0x5B
#define	PBPT		0x5D
#define	PBPM		0x5F

#define CLRIPIUS	0x20		/* Mask to clear IP and IUS */

#define CIO1		0x80		/* CIO # 1			*/

/*
 *defines for SCC
 */

#define WR0B		0x101
#define	WR1B		0x103
#define WR2B		0x105
#define WR3B		0x107
#define WR4B		0x109
#define WR5B		0x10B
#define WR6B		0x10D
#define WR7B		0x10F
#define WR8B		0x111
#define WR9B		0x113
#define WR10B		0x115
#define WR11B		0x117
#define WR12B		0x119
#define WR13B		0x11B
#define WR14B		0x11D
#define WR15B		0x11F
