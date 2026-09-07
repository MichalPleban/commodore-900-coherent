/ blkmv.t - Z8001 block move (BLKMOVE op, struct/array copy > INLINEBLK bytes).
/ The Z8000 LDIRB copies the whole byte block in ONE self-repeating instruction:
/ load the byte count into a word temp [R], then LDIRB @dst,@src,Rcount with the
/ far dst/src addresses in register PAIRS ([LR]/[RR], materialized from the BLKMOVE
/ subtrees by the address-of patterns: [RL]=left/dst pair, [RR]=right/src pair).
/ [SIZE] is the node's t_size (byte count).

BLKMOVE:
%	PEFFECT
	WORD	ANYR	PAIR	PAIR	NONE
		TREG	LPTX
		TREG	LPTX
		[ZLD]	[R],[SIZE]
		[ZLDIRB]	[RL],[RR],[R]
