/*	$OpenBSD: cacvar.h,v 1.1 2000/12/17 21:35:06 mickey Exp $	*/
/*	$NetBSD: cacvar.h,v 1.7 2000/10/19 14:28:47 ad Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#ifndef _IC_CACVAR_H_
#define	_IC_CACVAR_H_

#define	CAC_MAX_CCBS	20
#define	CAC_MAX_XFER	(0xffff * 512)
#define	CAC_SG_SIZE	32
#define	CAC_SECTOR_SIZE	512

#define	cac_inb(sc, port) \
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, port)
#define	cac_inw(sc, port) \
	bus_space_read_2((sc)->sc_iot, (sc)->sc_ioh, port)
#define	cac_inl(sc, port) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, port)
#define	cac_outb(sc, port, val) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, port, val)
#define	cac_outw(sc, port, val) \
	bus_space_write_2((sc)->sc_iot, (sc)->sc_ioh, port, val)
#define	cac_outl(sc, port, val) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, port, val)

/*
 * Stupid macros to deal with alignment/endianness issues.
 */

#define	CAC_GET1(x)							\
	(((u_char *)&(x))[0])
#define	CAC_GET2(x)							\
	(((u_char *)&(x))[0] | (((u_char *)&(x))[1] << 8))
#define	CAC_GET4(x)							\
	((((u_char *)&(x))[0] | (((u_char *)&(x))[1] << 8)) |		\
	(((u_char *)&(x))[0] << 16 | (((u_char *)&(x))[1] << 24)))

struct cac_ccb {
	/* Data the controller will touch - 276 bytes */
	struct cac_hdr	ccb_hdr;
	struct cac_req	ccb_req;
	struct cac_sgb	ccb_seg[CAC_SG_SIZE];

	/* Data the controller won't touch */
	int		ccb_flags;
	int		ccb_datasize;
	paddr_t		ccb_paddr;
	bus_dmamap_t	ccb_dmamap_xfer;
	SIMPLEQ_ENTRY(cac_ccb) ccb_chain;
	struct scsi_xfer *ccb_xs;
};

#define	CAC_CCB_DATA_IN		0x0001	/* Map describes inbound xfer */
#define	CAC_CCB_DATA_OUT	0x0002	/* Map describes outbound xfer */
#define	CAC_CCB_ACTIVE		0x0004	/* Command submitted to controller */

struct cac_softc;

struct cac_linkage {
	struct	cac_ccb *(*cl_completed)(struct cac_softc *);
	int	(*cl_fifo_full)(struct cac_softc *);
	void	(*cl_intr_enable)(struct cac_softc *, int);
	int	(*cl_intr_pending)(struct cac_softc *);
	void	(*cl_submit)(struct cac_softc *, struct cac_ccb *);
};

struct cac_softc {
	struct device		sc_dv;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;
	bus_dmamap_t		sc_dmamap;
	int			sc_nunits;
	void			*sc_ih;
	struct scsi_link	sc_link;
	const struct cac_linkage	*sc_cl;
	caddr_t			sc_ccbs;
	paddr_t			sc_ccbs_paddr;
	SIMPLEQ_HEAD(, cac_ccb)	sc_ccb_free;	
	SIMPLEQ_HEAD(, cac_ccb)	sc_ccb_queue;
	struct cac_drive_info	*sc_dinfos;
};

int	cac_init __P((struct cac_softc *, int));
int	cac_intr __P((void *));

extern const struct	cac_linkage cac_l0;

#endif	/* !_IC_CACVAR_H_ */
