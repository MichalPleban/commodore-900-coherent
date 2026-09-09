/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * Trap handler.  The common trap dispatch pushes the full trap frame
 * (trap code, R0-R15, normal SP, MMU status) before the call, so the
 * arguments arrive with the trap code deepest.  Enter the debugger.
 */

extern debugcmd();

trap(mmu, nsp, w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15, code)
long mmu, nsp;
int  w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15;
int  code;
{
        register int a, b, c;
        long e;

        debugcmd(code);
}
