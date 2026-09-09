/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * C900 I/O chip port map.
 */
#ifndef CIOPORTS_H
#define CIOPORTS_H

/*
 * Zilog Z8036 Z-CIO (counter/timer + parallel I/O) register offsets.
 */
#define CIO_MICR    0x01    /* master interrupt control register */
#define CIO_MCCR    0x03    /* master configuration control register */

/* Port A registers */
#define CIO_PACAS   0x11    /* command & status */
#define CIO_PAMS    0x41    /* mode specification */
#define CIO_PAHS    0x43    /* handshake specification */
#define CIO_PADPP   0x45    /* data path polarity */
#define CIO_PADD    0x47    /* data direction */
#define CIO_PASIOC  0x49    /* special I/O control */
#define CIO_PAPP    0x4b    /* pattern polarity */
#define CIO_PAPT    0x4d    /* pattern transition */
#define CIO_PAPM    0x4f    /* pattern mask */
#define CIO_PAIV    0x05    /* interrupt vector */
#define CIO_PADATA  0x1b    /* port A data */

/* Port C registers */
#define CIO_PCDD    0x0d    /* data direction */
#define CIO_PCSIOC  0x0f    /* special I/O control */
#define CIO_PCDPP   0x0b    /* data path polarity */
#define CIO_PCDATA  0x1f    /* port C data */

/* Control/flag bits */
#define CIO_MIE     0x80    /* master interrupt enable (in MICR) */
#define CIO_PAE     0x04    /* port A enable (in MCCR) */
#define CIO_C_IPIUS 0x20    /* clear IP & IUS command (to PACAS) */
#define CIO_PC2     0x04    /* port C bit 2 */
#define CIO_PC3     0x08    /* port C bit 3 */

/* Keyboard CIO */
#define ZCIO1       0x0000  /* base address of the keyboard CIO */
#define PKBD        0x0205  /* keyboard enable port */
#define KBDEN       0x02    /* keyboard enable bit */
#define HRVECTOR    8       /* port A interrupt vector */
#define LASTKEY     0xfe

/*
 * Zilog Z8030 SCC (serial communications controller) register offsets.
 */
#define SCC_WR0   0x01  /* command register */
#define SCC_WR1   0x03  /* Tx & Rx interrupt enables */
#define SCC_WR2   0x05  /* interrupt vector */
#define SCC_WR3   0x07  /* Rx parameters & control */
#define SCC_WR4   0x09  /* Tx/Rx misc parameters & modes */
#define SCC_WR5   0x0b  /* Tx parameters & control */
#define SCC_WR6   0x0d  /* sync byte 1 / SDLC */
#define SCC_WR7   0x0f  /* sync byte 2 / SDLC */
#define SCC_WR8   0x11  /* Tx buffer */
#define SCC_WR9   0x13  /* master interrupt control */
#define SCC_WR10  0x15  /* misc Tx/Rx control */
#define SCC_WR11  0x17  /* clock mode control */
#define SCC_WR12  0x19  /* baud-rate generator low */
#define SCC_WR13  0x1b  /* baud-rate generator high */
#define SCC_WR14  0x1d  /* misc control */
#define SCC_WR15  0x1f  /* external/status interrupt control */
#define SCC_RR0   0x01  /* Tx/Rx buffer & misc status */

/*
 * 6845 CRTC (character display).
 */
#define INDX6845    0x0410  /* index/address register */
#define DATA6845    0x0412  /* data register */
#define CRTCNTRL    0x0418  /* CRT control register */
#define CRTON       0x28    /* enable CRT video & blink */
#define CRTOFF      0x00    /* disable CRT video & blink */

/*
 * I/O port base addresses.
 */
#define SCC_BASE    0x0100  /* Z8030 SCC serial */
#define PDMAC_PORT  0x0500  /* PDMAC command/data port */
#define FDC_BASE    0x4000  /* floppy controller window */
#define MOUSE_XPORT 0x0400  /* mouse relative-X + key states */
#define MOUSE_YPORT 0x0402  /* mouse relative-Y */
#define MMU_IO      0x00FC  /* Z8010 MMU special I/O */
#define PSA_IO      0x00F8  /* program status area / MMU control */

#endif /* CIOPORTS_H */
