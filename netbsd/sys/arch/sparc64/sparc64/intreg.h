/*	$NetBSD: intreg.h,v 1.1.1.1 1998/06/20 04:58:52 eeh Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)intreg.h	8.1 (Berkeley) 6/11/93
 */

#include <sparc64/sparc64/vaddrs.h>

/*
 * All sun4u interrupts arrive as interrupt packets.  These packets
 * consist of up to six (three on spitfires) quads.  The first one
 * contains the interrupt number.  This is either the device ID
 * or a pointer to the handler routine.
 * 
 * Device IDs are split into two parts: a 5-bit interrupt group number
 * and a 5-bit interrupt number.  We ignore this distinction.
 *
 */

#define MAXINTNUM	(1<<11)

/*
 * These masks are used for ienab_bis and ienab_bic, neither valid for
 * 4u machines.   We translate this to a set_softint and clear_softint.
 */
#define	IE_L14		(1<<14)	/* enable level 14 (counter 1) interrupts */
#define	IE_L10		(1<<10)	/* enable level 10 (counter 0) interrupts */
#define	IE_L8		(1<<8)	/* enable level 8 interrupts */
#define	IE_L6		(1<<6)	/* request software level 6 interrupt */
#define	IE_L4		(1<<4)	/* request software level 4 interrupt */
#define	IE_L1		(1<<1)	/* request software level 1 interrupt */

#ifndef _LOCORE
void	ienab_bis __P((int bis));	/* set given bits */
void	ienab_bic __P((int bic));	/* clear given bits */
#endif

#if 0
#if defined(SUN4M)
#ifdef notyet
#define IENAB_SYS	((_MAXNBPG * _MAXNCPU) + 0xc)
#define IENAB_P0	0x0008
#define IENAB_P1	0x1008
#define IENAB_P2	0x2008
#define IENAB_P3	0x3008
#endif /* notyet */
#endif

#if defined(SUN4M)
/*
 * Interrupt Control Registers, located in IO space.
 * (mapped to `locore' for now..)
 * There are two sets of interrupt registers called `Processor Interrupts'
 * and `System Interrupts'. The `Processor' set corresponds to the 15
 * interrupt levels as seen by the CPU. The `System' set corresponds to
 * a set of devices supported by the implementing chip-set.
 *
 * Briefly, the ICR_PI_* are per-processor interrupts; the ICR_SI_* are
 * system-wide interrupts, and the ICR_ITR selects the processor to get
 * the system's interrupts.
 */
#define ICR_PI_PEND		(PI_INTR_VA + 0x0)
#define ICR_PI_CLR		(PI_INTR_VA + 0x4)
#define ICR_PI_SET		(PI_INTR_VA + 0x8)
#define ICR_SI_PEND		(SI_INTR_VA)
#define ICR_SI_MASK		(SI_INTR_VA + 0x4)
#define ICR_SI_CLR		(SI_INTR_VA + 0x8)
#define ICR_SI_SET		(SI_INTR_VA + 0xc)
#define ICR_ITR			(SI_INTR_VA + 0x10)

/*
 * Bits in interrupt registers.  Software interrupt requests must
 * be cleared in software.  This is done in locore.s.
 * There are separate registers for reading pending interrupts and
 * setting/clearing (software) interrupts.
 */
#define PINTR_SINTRLEV(n)	(1 << (16 + (n)))
#define PINTR_IC		0x8000		/* Level 15 clear */

#define SINTR_MA		0x80000000	/* Mask All interrupts */
#define SINTR_ME		0x40000000	/* Module Error (async) */
#define SINTR_I			0x20000000	/* MSI (MBus-SBus) */
#define SINTR_M			0x10000000	/* ECC Memory controller */
#define SINTR_RSVD2		0x0f800000
#define SINTR_F			0x00400000	/* Floppy */
#define SINTR_RSVD3		0x00200000
#define SINTR_V			0x00100000	/* Video (Supersparc only) */
#define SINTR_T			0x00080000	/* Level 10 counter */
#define SINTR_SC		0x00040000	/* SCSI */
#define SINTR_RSVD4		0x00020000
#define SINTR_E			0x00010000	/* Ethernet */
#define SINTR_S			0x00008000	/* Serial port */
#define SINTR_K			0x00004000	/* Keyboard/mouse */
#define SINTR_SBUSMASK		0x00003f80	/* SBus */
#define SINTR_SBUS(n)		(((n) << 7) & 0x00003f80)
#define SINTR_RSVD5		0x0000007f

#endif
#endif
