/* $FreeBSD: src/sys/alpha/include/param.h,v 1.29 2002/09/17 01:48:54 peter Exp $ */
/* From: NetBSD: param.h,v 1.20 1997/09/19 13:52:53 leo Exp */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah $Hdr: machparam.h 1.11 89/08/14$
 *
 *	@(#)param.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Machine dependent constants for the Alpha.
 */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_long and must be cast to
 * any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#ifndef _ALIGNBYTES
#define	_ALIGNBYTES		7
#endif
#ifndef _ALIGN
#define	_ALIGN(p)		(((u_long)(p) + _ALIGNBYTES) &~ _ALIGNBYTES)
#endif
#ifndef _ALIGNED_POINTER
#define _ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)
#endif

#ifndef _MACHINE
#define	_MACHINE	alpha
#endif
#ifndef _MACHINE_ARCH
#define	_MACHINE_ARCH	alpha
#endif

#ifndef _NO_NAMESPACE_POLLUTION

#ifndef _MACHINE_PARAM_H_
#define _MACHINE_PARAM_H_

#ifndef MACHINE
#define	MACHINE		"alpha"
#endif
#ifndef MACHINE_ARCH
#define	MACHINE_ARCH	"alpha"
#endif
#define	MID_MACHINE	MID_ALPHA

#include <machine/alpha_cpu.h>
#include <machine/cpu.h>

#ifdef SMP
#define MAXCPU		32
#else
#define MAXCPU		1
#endif

#define	ALIGNBYTES		_ALIGNBYTES
#define	ALIGN(p)		_ALIGN(p)
#define ALIGNED_POINTER(p,t)	_ALIGNED_POINTER(p,t)

#define	PAGE_SIZE	(1 << ALPHA_PGSHIFT)		/* bytes/page */
#define PAGE_SHIFT	ALPHA_PGSHIFT
#define PAGE_MASK	(PAGE_SIZE-1)
#define NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))

#define	KERNBASE	0xfffffc0000300000LL	/* start of kernel virtual */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	CLSIZE		1
#define	CLSIZELOG2	0

/* NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE */
#define	SSIZE		1		/* initial stack size/NBPG */
#define	SINCR		1		/* increment of stack/NBPG */

#define	KSTACK_PAGES	2		/* pages of kstack (with pcb) */
#define	UAREA_PAGES	1		/* pages of u-area */

#define	KSTACK_GUARD			/* compile in kstack guard page */

/*
 * Mach derived conversion macros
 */
#define	round_page(x)	((((unsigned long)(x)) + PAGE_MASK) & ~(PAGE_MASK))
#define	trunc_page(x)	((unsigned long)(x) & ~(PAGE_MASK))

#define atop(x)			((unsigned long)(x) >> PAGE_SHIFT)
#define ptoa(x)			((unsigned long)(x) << PAGE_SHIFT)

#define	alpha_btop(x)		((unsigned long)(x) >> PAGE_SHIFT)
#define	alpha_ptob(x)		((unsigned long)(x) << PAGE_SHIFT)

#define pgtok(x)                ((x) * (PAGE_SIZE / 1024)) 

#endif /* !_MACHINE_PARAM_H_ */
#endif /* !_NO_NAMESPACE_POLLUTION */
