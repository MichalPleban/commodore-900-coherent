/*
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: MIT
 */
/*
 * common/z8001/arena0.c
 * One 60 K arena for bigheap.c, a module of its own so that ld -L gives it
 * a data segment of its own.
 */
char	bigarena0[60000];
