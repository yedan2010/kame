/*
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 * $FreeBSD: src/sys/powerpc/include/clock.h,v 1.9 2002/03/20 23:17:49 alfred Exp $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef _KERNEL

extern	int	disable_rtc_set;
extern	int	wall_cmos_clock;
extern	int	adjkerntz;

int	sysbeep(int pitch, int period);
int	acquire_timer2(int mode);
int	release_timer2(void);

void	decr_intr(struct clockframe *);

#endif

#endif /* !_MACHINE_CLOCK_H_ */
