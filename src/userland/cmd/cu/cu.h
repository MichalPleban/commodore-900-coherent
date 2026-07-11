/*
 * CU downloader.
 * Packet structure.
 */

#define	NPKT	256
/* These definitions depend on table in cudld.c */
#define	STX	002			/* Start of packet */
#define	ETX	003			/* End of packet */
#define	DLE	022			/* Data line escape */

typedef	struct	PKT {
	unsigned p_len;			/* Length of packet */
	int	p_flags;		/* Flags for errors, etc */
	char	p_type;			/* Packet type: I, F, A, E, N */
	char	p_seq[2];		/* Canonical integer sequence number */
	char	p_data[NPKT];		/* Information packet data */
	char	p_crc[2];		/* Canonical CRC */
}	PKT;

/* Flags in `p_flags' */
#define	PCRC	01			/* CRC error */
#define	PTOUT	02			/* Time out error */

#define	TLEN	20			/* Timeout time */

PKT	*getpkt();
PKT	*rcvpkt();

extern	PKT	xpkt;
extern	PKT	rpkt;
