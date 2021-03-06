/*-
 * Copyright (c) 2001 Jake Burkholder.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sparc64/sparc64/db_interface.c,v 1.9 2002/03/13 04:59:01 jake Exp $
 */

#include <sys/param.h> 
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/cons.h>
#include <sys/ktr.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <machine/cpu.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

#include <machine/atomic.h>
#include <machine/setjmp.h>

static jmp_buf *db_nofault = 0;
extern jmp_buf db_jmpbuf;

int db_active;
db_regs_t ddb_regs;

static jmp_buf db_global_jmpbuf;
static int db_global_jmpbuf_valid;

int
kdb_trap(struct trapframe *tf)
{

	if (db_global_jmpbuf_valid)
		longjmp(db_global_jmpbuf, 1);
	flushw();
	ddb_regs = *tf;
	critical_enter();
	setjmp(db_global_jmpbuf);
	db_global_jmpbuf_valid = TRUE;
	atomic_add_acq_int(&db_active, 1);
#ifdef SMP
	stop_cpus(PCPU_GET(other_cpus));
#endif
	cndbctl(TRUE);
	db_trap(tf->tf_type, 0);
	cndbctl(FALSE);
	db_active--;
#ifdef SMP
	restart_cpus(stopped_cpus);
#endif
	db_global_jmpbuf_valid = FALSE;
	critical_exit();
	*tf = ddb_regs;
	TF_DONE(tf);
	return (1);
}

void
db_read_bytes(vm_offset_t addr, size_t size, char *data)
{
	char *src;

	db_nofault = &db_jmpbuf;
	src = (char *)addr;
	while (size-- > 0)
		*data++ = *src++;
	db_nofault = NULL;
}

void
db_write_bytes(vm_offset_t addr, size_t size, char *data)
{
	char *dst;

	db_nofault = &db_jmpbuf;
	dst = (char *)addr;
	while (size-- > 0)
		*dst++ = *data++;
	db_nofault = NULL;
}

void
db_show_mdpcpu(struct pcpu *pc)
{
}

DB_COMMAND(reboot, db_reboot)
{
	cpu_reset();
}

DB_COMMAND(halt, db_halt)
{
	cpu_halt();
}
