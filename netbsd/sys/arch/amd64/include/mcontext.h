/*	$NetBSD: mcontext.h,v 1.4.2.1 2004/11/12 06:33:30 jmc Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AMD64_MCONTEXT_H_
#define _AMD64_MCONTEXT_H_

/*
 * General register state
 */
#define _NGREG		26
typedef	unsigned long	__greg_t;
typedef	__greg_t	__gregset_t[_NGREG];

/*
 * This is laid out to match trapframe and intrframe (see <machine/frame.h>).
 * Hence, memcpy between gregs and a trapframe is possible.
 */
#define _REG_RDI	0
#define _REG_RSI	1
#define _REG_RDX	2
#define _REG_RCX	3
#define _REG_R8		4
#define _REG_R9		5
#define _REG_R10	6
#define _REG_R11	7
#define _REG_R12	8
#define _REG_R13	9
#define _REG_R14	10
#define _REG_R15	11
#define _REG_RBP	12
#define _REG_RBX	13
#define _REG_RAX	14
#define _REG_GS		15
#define _REG_FS		16
#define _REG_ES		17
#define _REG_DS		18
#define _REG_TRAPNO	19
#define _REG_ERR	20
#define _REG_RIP	21
#define _REG_CS		22
#define _REG_RFL	23
#define _REG_URSP	24
#define _REG_SS		25

/*
 * Floating point register state
 */
typedef char __fpregset_t[512];

/*
 * The padding below is to make __fpregs have a 16-byte aligned offset
 * within ucontext_t.
 */

typedef struct {
	__gregset_t	__gregs;
	long 		__pad;
	__fpregset_t	__fpregs;
} mcontext_t;

#define _UC_UCONTEXT_ALIGN	(~0xf)

#define _UC_MACHINE_SP(uc)	((uc)->uc_mcontext.__gregs[_REG_URSP] - 128)
#define _UC_MACHINE_PC(uc)	((uc)->uc_mcontext.__gregs[_REG_RIP])
#define _UC_MACHINE_INTRV(uc)	((uc)->uc_mcontext.__gregs[_REG_RAX])

#define	_UC_MACHINE_SET_PC(uc, pc)	_UC_MACHINE_PC(uc) = (pc)

/*
 * mcontext extensions to handle signal delivery.
 */
#define _UC_SETSTACK	0x00010000
#define _UC_CLRSTACK	0x00020000


#ifdef _KERNEL

/*
 * 32bit context definitions.
 */

#define _NGREG32	19
typedef unsigned int	__greg32_t;
typedef __greg32_t	__gregset32_t[_NGREG32];

#define _REG32_GS	0
#define _REG32_FS	1
#define _REG32_ES	2
#define _REG32_DS	3
#define _REG32_EDI	4
#define _REG32_ESI	5
#define _REG32_EBP	6
#define _REG32_ESP	7
#define _REG32_EBX	8
#define _REG32_EDX	9
#define _REG32_ECX	10
#define _REG32_EAX	11
#define _REG32_TRAPNO	12
#define _REG32_ERR	13
#define _REG32_EIP	14
#define _REG32_CS	15
#define _REG32_EFL	16
#define _REG32_UESP	17
#define _REG32_SS	18

/*
 * Floating point register state
 */
typedef struct fxsave64 __fpregset32_t;

typedef struct {
	__gregset32_t	__gregs;
	__fpregset32_t	__fpregs;
} mcontext32_t;

#define _UC_MACHINE_PAD32	5

struct trapframe;
int check_mcontext(const mcontext_t *, struct trapframe *);

#endif /* _KERNEL */

#endif	/* !_AMD64_MCONTEXT_H_ */
