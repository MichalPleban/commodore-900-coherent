/*
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: MIT
 */
/*
 * stdint.h
 * Exact-width integer types.
 * Z8001 Coherent: char is 8 bits, int 16, long 32.
 */

#ifndef	__STDINT_H__
#define	__STDINT_H__

typedef	char		int8_t;
typedef	unsigned char	uint8_t;
typedef	int		int16_t;
typedef	unsigned int	uint16_t;
typedef	long		int32_t;
typedef	unsigned long	uint32_t;

/* A pointer is a segmented seg:offset pair, 32 bits. */
typedef	long		intptr_t;
typedef	unsigned long	uintptr_t;

#endif
