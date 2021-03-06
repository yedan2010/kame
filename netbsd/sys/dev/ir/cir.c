/*	$NetBSD: cir.c,v 1.2 2001/12/12 15:33:53 augustss Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/vnode.h>

#include <dev/ir/ir.h>
#include <dev/ir/cirio.h>
#include <dev/ir/cirvar.h>

cdev_decl(cir);

int cir_match(struct device *parent, struct cfdata *match, void *aux);
void cir_attach(struct device *parent, struct device *self, void *aux);
int cir_activate(struct device *self, enum devact act);
int cir_detach(struct device *self, int flags);

struct cfattach cir_ca = {
	sizeof(struct cir_softc), cir_match, cir_attach,
	cir_detach, cir_activate
};

extern struct cfdriver cir_cd;

#define CIRUNIT(dev) (minor(dev))

int
cir_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct ir_attach_args *ia = aux;

	return (ia->ia_type == IR_TYPE_CIR);
}

void
cir_attach(struct device *parent, struct device *self, void *aux)
{
	struct cir_softc *sc = (struct cir_softc *)self;
	struct ir_attach_args *ia = aux;

	sc->sc_methods = ia->ia_methods;
	sc->sc_handle = ia->ia_handle;

#ifdef DIAGNOSTIC
	if (sc->sc_methods->im_read == NULL ||
	    sc->sc_methods->im_write == NULL ||
	    sc->sc_methods->im_setparams == NULL)
		panic("%s: missing methods\n", sc->sc_dev.dv_xname);
#endif
	printf("\n");
}

int
cir_activate(struct device *self, enum devact act)
{
	/*struct cir_softc *sc = (struct cir_softc *)self;*/

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		break;
	}
	return (0);
}

int
cir_detach(struct device *self, int flags)
{
	/*struct cir_softc *sc = (struct cir_softc *)self;*/
	int maj, mn;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == ciropen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}

int
ciropen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct cir_softc *sc;
	int error;

	sc = device_lookup(&cir_cd, CIRUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if ((sc->sc_dev.dv_flags & DVF_ACTIVE) == 0)
		return (EIO);
	if (sc->sc_open)
		return (EBUSY);
	if (sc->sc_methods->im_open != NULL) {
		error = sc->sc_methods->im_open(sc->sc_handle, flag, mode, p);
		if (error)
			return (error);
	}
	sc->sc_open = 1;
	return (0);
}

int
circlose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct cir_softc *sc;
	int error;

	sc = device_lookup(&cir_cd, CIRUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if (sc->sc_methods->im_close != NULL)
		error = sc->sc_methods->im_close(sc->sc_handle, flag, mode, p);
	else
		error = 0;
	sc->sc_open = 0;
	return (error);
}

int
cirread(dev_t dev, struct uio *uio, int flag)
{
	struct cir_softc *sc;

	sc = device_lookup(&cir_cd, CIRUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if ((sc->sc_dev.dv_flags & DVF_ACTIVE) == 0)
		return (EIO);
	return (sc->sc_methods->im_read(sc->sc_handle, uio, flag));
}

int
cirwrite(dev_t dev, struct uio *uio, int flag)
{
	struct cir_softc *sc;

	sc = device_lookup(&cir_cd, CIRUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if ((sc->sc_dev.dv_flags & DVF_ACTIVE) == 0)
		return (EIO);
	return (sc->sc_methods->im_write(sc->sc_handle, uio, flag));
}

int
cirioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct cir_softc *sc;
	int error;

	sc = device_lookup(&cir_cd, CIRUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if ((sc->sc_dev.dv_flags & DVF_ACTIVE) == 0)
		return (EIO);

	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		error = 0;
		break;
	case CIR_GET_PARAMS:
		*(struct cir_params *)addr = sc->sc_params;
		break;
	case CIR_SET_PARAMS:
		error = sc->sc_methods->im_setparams(sc->sc_handle,
			    (struct cir_params *)addr);
		if (!error)
			sc->sc_params = *(struct cir_params *)addr;
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

int
cirpoll(dev_t dev, int events, struct proc *p)
{
	struct cir_softc *sc;
	int revents;
	int s;

	sc = device_lookup(&cir_cd, CIRUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if ((sc->sc_dev.dv_flags & DVF_ACTIVE) == 0)
		return (EIO);

	revents = 0;
	s = splir();
#if 0
	if (events & (POLLIN | POLLRDNORM))
		if (sc->sc_rdframes > 0)
			revents |= events & (POLLIN | POLLRDNORM);
#endif

#if 0
	/* How about write? */
	if (events & (POLLOUT | POLLWRNORM))
		if (???)
			revents |= events & (POLLOUT | POLLWRNORM);
#endif

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &sc->sc_rdsel);

#if 0
		if (events & (POLLOUT | POLLWRNORM))
			selrecord(p, &sc->sc_wrsel);
#endif
	}

	splx(s);
	return (revents);
}
