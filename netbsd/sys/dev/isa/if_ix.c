/*	$NetBSD: if_ix.c,v 1.6 1999/01/08 19:22:36 augustss Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Rafal K. Boni.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/i82586reg.h>
#include <dev/ic/i82586var.h>
#include <dev/isa/if_ixreg.h>

#ifdef IX_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

int ix_media[] = {
	IFM_ETHER | IFM_10_5,
	IFM_ETHER | IFM_10_2,
	IFM_ETHER | IFM_10_T,
};
#define NIX_MEDIA       (sizeof(ix_media) / sizeof(ix_media[0]))

struct ix_softc {
	struct ie_softc sc_ie;

	bus_space_tag_t sc_regt;	/* space tag for registers */
	bus_space_handle_t sc_regh;	/* space handle for registers */

	u_int16_t	irq_encoded;	/* encoded IRQ */
	void		*sc_ih;		/* interrupt handle */
};

static void 	ix_reset __P((struct ie_softc *, int));
static void 	ix_atten __P((struct ie_softc *));
static int 	ix_intrhook __P((struct ie_softc *, int));

static void     ix_copyin __P((struct ie_softc *, void *, int, size_t));
static void     ix_copyout __P((struct ie_softc *, const void *, int, size_t));

static u_int16_t ix_read_16 __P((struct ie_softc *, int));
static void	ix_write_16 __P((struct ie_softc *, int, u_int16_t));
static void	ix_write_24 __P((struct ie_softc *, int, int));

static void	ix_mediastatus __P((struct ie_softc *, struct ifmediareq *));

static u_int16_t ix_read_eeprom __P((bus_space_tag_t, bus_space_handle_t, int));
static void	ix_eeprom_outbits __P((bus_space_tag_t, bus_space_handle_t, int, int));
static int	ix_eeprom_inbits  __P((bus_space_tag_t, bus_space_handle_t));
static void	ix_eeprom_clock   __P((bus_space_tag_t, bus_space_handle_t, int));

int ix_match __P((struct device *, struct cfdata *, void *));
void ix_attach __P((struct device *, struct device *, void *));

/*
 * EtherExpress/16 support routines
 */
static void
ix_reset(sc, why)
	struct ie_softc *sc;
	int why;
{
	struct ix_softc* isc = (struct ix_softc *) sc;

	switch (why) {
	case CHIP_PROBE:
		bus_space_write_1(isc->sc_regt, isc->sc_regh, IX_ECTRL,
				  IX_RESET_586);
		delay(100);
		bus_space_write_1(isc->sc_regt, isc->sc_regh, IX_ECTRL, 0);
		delay(100);
		break;

	case CARD_RESET:
		break;
    }
}

static void
ix_atten(sc)
	struct ie_softc *sc;
{
	struct ix_softc* isc = (struct ix_softc *) sc;
	bus_space_write_1(isc->sc_regt, isc->sc_regh, IX_ATTN, 0);
}

static u_int16_t
ix_read_eeprom(iot, ioh, location)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int location;
{
	int ectrl, edata;

	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= IX_ECTRL_MASK;
	ectrl |= IX_ECTRL_EECS;
	bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);

	ix_eeprom_outbits(iot, ioh, IX_EEPROM_READ, IX_EEPROM_OPSIZE1);
	ix_eeprom_outbits(iot, ioh, location, IX_EEPROM_ADDR_SIZE);
	edata = ix_eeprom_inbits(iot, ioh);
	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= ~(IX_RESET_ASIC | IX_ECTRL_EEDI | IX_ECTRL_EECS);
	bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);
	ix_eeprom_clock(iot, ioh, 1);
	ix_eeprom_clock(iot, ioh, 0);
	return (edata);
}

static void
ix_eeprom_outbits(iot, ioh, edata, count)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int edata, count;
{
	int ectrl, i;

	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= ~IX_RESET_ASIC;
	for (i = count - 1; i >= 0; i--) {
		ectrl &= ~IX_ECTRL_EEDI;
		if (edata & (1 << i)) {
			ectrl |= IX_ECTRL_EEDI;
		}
		bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);
		delay(1);	/* eeprom data must be setup for 0.4 uSec */
		ix_eeprom_clock(iot, ioh, 1);
		ix_eeprom_clock(iot, ioh, 0);
	}
	ectrl &= ~IX_ECTRL_EEDI;
	bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);
	delay(1);		/* eeprom data must be held for 0.4 uSec */
}

static int
ix_eeprom_inbits(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	int ectrl, edata, i;

	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= ~IX_RESET_ASIC;
	for (edata = 0, i = 0; i < 16; i++) {
		edata = edata << 1;
		ix_eeprom_clock(iot, ioh, 1);
		ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
		if (ectrl & IX_ECTRL_EEDO) {
			edata |= 1;
		}
		ix_eeprom_clock(iot, ioh, 0);
	}
	return (edata);
}

static void
ix_eeprom_clock(iot, ioh, state)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int state;
{
	int ectrl;

	ectrl = bus_space_read_1(iot, ioh, IX_ECTRL);
	ectrl &= ~(IX_RESET_ASIC | IX_ECTRL_EESK);
	if (state) {
		ectrl |= IX_ECTRL_EESK;
	}
	bus_space_write_1(iot, ioh, IX_ECTRL, ectrl);
	delay(9);		/* EESK must be stable for 8.38 uSec */
}

static int
ix_intrhook(sc, where)
	struct ie_softc *sc;
	int where;
{
	struct ix_softc* isc = (struct ix_softc *) sc;

	switch (where) {
	case INTR_ENTER:
		/* entering ISR: disable card interrupts */
		bus_space_write_1(isc->sc_regt, isc->sc_regh,
				  IX_IRQ, isc->irq_encoded);
		break;

	case INTR_EXIT:
		/* exiting ISR: re-enable card interrupts */
		bus_space_write_1(isc->sc_regt, isc->sc_regh, IX_IRQ,
    				  isc->irq_encoded | IX_IRQ_ENABLE);
	break;
    }

    return 1;
}


static void
ix_copyin (sc, dst, offset, size)
        struct ie_softc *sc;
        void *dst;
        int offset;
        size_t size;
{
	int dribble;
	u_int8_t* bptr = dst;

	bus_space_barrier(sc->bt, sc->bh, offset, size,
			  BUS_SPACE_BARRIER_READ);

	if (offset % 2) {
		*bptr = bus_space_read_1(sc->bt, sc->bh, offset);
		offset++; bptr++; size--;
	}

	dribble = size % 2;
	bus_space_read_region_2(sc->bt, sc->bh, offset, (u_int16_t *) bptr,
				size >> 1);

	if (dribble) {
		bptr += size - 1;
		offset += size - 1;
		*bptr = bus_space_read_1(sc->bt, sc->bh, offset);
	}
}

static void
ix_copyout (sc, src, offset, size)
        struct ie_softc *sc;
        const void *src;
        int offset;
        size_t size;
{
	int dribble;
	int osize = size;
	int ooffset = offset;
	const u_int8_t* bptr = src;

	if (offset % 2) {
		bus_space_write_1(sc->bt, sc->bh, offset, *bptr);
		offset++; bptr++; size--;
	}

	dribble = size % 2;
	bus_space_write_region_2(sc->bt, sc->bh, offset, (u_int16_t *)bptr,
				 size >> 1);
	if (dribble) {
		bptr += size - 1;
		offset += size - 1;
		bus_space_write_1(sc->bt, sc->bh, offset, *bptr);
	}

	bus_space_barrier(sc->bt, sc->bh, ooffset, osize,
			  BUS_SPACE_BARRIER_WRITE);
}

static u_int16_t
ix_read_16 (sc, offset)
        struct ie_softc *sc;
        int offset;
{
	bus_space_barrier(sc->bt, sc->bh, offset, 2, BUS_SPACE_BARRIER_READ);
        return bus_space_read_2(sc->bt, sc->bh, offset);
}

static void
ix_write_16 (sc, offset, value)
        struct ie_softc *sc;
        int offset;
        u_int16_t value;
{
        bus_space_write_2(sc->bt, sc->bh, offset, value);
	bus_space_barrier(sc->bt, sc->bh, offset, 2, BUS_SPACE_BARRIER_WRITE);
}

static void
ix_write_24 (sc, offset, addr)
        struct ie_softc *sc;
        int offset, addr;
{
        bus_space_write_4(sc->bt, sc->bh, offset, addr +
			  (u_long) sc->sc_maddr - (u_long) sc->sc_iobase);
	bus_space_barrier(sc->bt, sc->bh, offset, 4, BUS_SPACE_BARRIER_WRITE);
}

static void
ix_mediastatus(sc, ifmr)
        struct ie_softc *sc;
        struct ifmediareq *ifmr;
{
        struct ifmedia *ifm = &sc->sc_media;

        /*
         * The currently selected media is always the active media.
         */
        ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

int
ix_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	int i;
	int rv = 0;
	bus_addr_t maddr;
	bus_size_t msize;
	u_short checksum = 0;
	bus_space_handle_t ioh;
	bus_space_tag_t iot;
	u_int8_t val, bart_config;
	u_short pg, adjust, decode, edecode;
	u_short board_id, id_var1, id_var2, irq, irq_encoded;
	struct isa_attach_args * const ia = aux;
	short irq_translate[] = {0, 0x09, 0x03, 0x04, 0x05, 0x0a, 0x0b, 0};

	iot = ia->ia_iot;

	if (bus_space_map(iot, ia->ia_iobase,
			  IX_IOSIZE, 0, &ioh) != 0) {
		DPRINTF(("Can't map io space at 0x%x\n", ia->ia_iobase));
		return (0);
	}

	/* XXX: reset any ee16 at the current iobase */
	bus_space_write_1(iot, ioh, IX_ECTRL, IX_RESET_ASIC);
	bus_space_write_1(iot, ioh, IX_ECTRL, 0);
	delay(240);

	/* now look for ee16. */
	board_id = id_var1 = id_var2 = 0;
	for (i = 0; i < 4 ; i++) {
		id_var1 = bus_space_read_1(iot, ioh, IX_ID_PORT);
		id_var2 = ((id_var1 & 0x03) << 2);
		board_id |= (( id_var1 >> 4)  << id_var2);
	}

	if (board_id != IX_ID) {
		DPRINTF(("BART ID mismatch (got 0x%04x, expected 0x%04x)\n",
			board_id, IX_ID));
		goto out;
	}

	/*
	 * The shared RAM size and location of the EE16 is encoded into
	 * EEPROM location 6.  The location of the first set bit tells us
	 * the memory address (0xc0000 + (0x4000 * FSB)), where FSB is the
	 * number of the first set bit.  The zeroes are then shifted out,
	 * and the results is the memory size (1 = 16k, 3 = 32k, 7 = 48k,
	 * 0x0f = 64k).
	 *
	 * Examples:
	 *   0x3c -> 64k@0xc8000, 0x70 -> 48k@0xd0000, 0xc0 -> 32k@0xd8000
	 *   0x80 -> 16k@0xdc000.
	 *
	 * Side note: this comes from reading the old driver rather than
	 * from a more definitive source, so it could be out-of-whack
	 * with what the card can do...
	 */

	val = ix_read_eeprom(iot, ioh, 6) & 0xff;
	DPRINTF(("memory config: 0x%02x\n", val));

	for(i = 0; i < 8; i++) {
		if (val & 1)
			break;
		val = val >> 1;
	}

	if (i == 8) {
		DPRINTF(("Invalid or unsupported memory config\n"));
		goto out;
	}

	maddr = 0xc0000 + (i * 0x4000);

	switch (val) {
	case 0x01:
		msize = 16 * 1024;
		break;

	case 0x03:
		msize = 32 * 1024;
		break;

	case 0x07:
		msize = 48 * 1024;
		break;

	case 0x0f:
		msize = 64 * 1024;
		break;

	default:
		DPRINTF(("invalid memory size %02x\n", val));
		goto out;
	}

	if (ia->ia_maddr == ISACF_IOMEM_DEFAULT)
		ia->ia_maddr = maddr;
	else if (ia->ia_maddr != maddr) {
		DPRINTF((
		  "ix_match: memaddr of board @ 0x%x doesn't match config\n",
		  ia->ia_iobase));
		goto out;
	}

	if (ia->ia_msize == ISACF_IOSIZ_DEFAULT)
		ia->ia_msize = msize;
	else if (ia->ia_msize != msize) {
		DPRINTF((
		   "ix_match: memsize of board @ 0x%x doesn't match config\n",
		   ia->ia_iobase));
		goto out;
	}

	DPRINTF(("found %d byte memory region at %x\n",
		ia->ia_msize, ia->ia_maddr));

	/* need to put the 586 in RESET, and leave it */
	bus_space_write_1(iot, ioh, IX_ECTRL, IX_RESET_586);

	/* read the eeprom and checksum it, should == IX_ID */
	for(i = 0; i < 0x40; i++)
		checksum += ix_read_eeprom(iot, ioh, i);

	if (checksum != IX_ID) {
		DPRINTF(("checksum mismatch (got 0x%04x, expected 0x%04x\n",
			checksum, IX_ID));
		goto out;
	}

	/*
	 * Size and test the memory on the board.  The size of the memory
	 * can be one of 16k, 32k, 48k or 64k.  It can be located in the
	 * address range 0xC0000 to 0xEFFFF on 16k boundaries.
	 */
	pg = (ia->ia_maddr & 0x3C000) >> 14;
	adjust = IX_MCTRL_FMCS16 | (pg & 0x3) << 2;
	decode = ((1 << (ia->ia_msize / 16384)) - 1) << pg;
	edecode = ((~decode >> 4) & 0xF0) | (decode >> 8);

	/* ZZZ This should be checked against eeprom location 6, low byte */
	bus_space_write_1(iot, ioh, IX_MEMDEC, decode & 0xFF);

	/* ZZZ This should be checked against eeprom location 1, low byte */
	bus_space_write_1(iot, ioh, IX_MCTRL, adjust);

	/* ZZZ Now if I could find this one I would have it made */
	bus_space_write_1(iot, ioh, IX_MPCTRL, (~decode & 0xFF));

	/* ZZZ I think this is location 6, high byte */
	bus_space_write_1(iot, ioh, IX_MECTRL, edecode); /*XXX disable Exxx */

	/*
	 * Get the encoded interrupt number from the EEPROM, check it
	 * against the passed in IRQ.  Issue a warning if they do not
	 * match, and fail the probe.  If irq is 'IRQUNK' then we
	 * use the EEPROM irq, and continue.
	 */
	irq_encoded = ix_read_eeprom(iot, ioh, IX_EEPROM_CONFIG1);
	irq_encoded = (irq_encoded & IX_EEPROM_IRQ) >> IX_EEPROM_IRQ_SHIFT;
	irq = irq_translate[irq_encoded];
	if (ia->ia_irq == ISACF_IRQ_DEFAULT)
		ia->ia_irq = irq;
	else if (irq != ia->ia_irq) {
		DPRINTF(("board IRQ %d does not match config\n", irq));
		goto out;
	}

	/* disable the board interrupts */
	bus_space_write_1(iot, ioh, IX_IRQ, irq_encoded);

	bart_config = bus_space_read_1(iot, ioh, IX_CONFIG);
	bart_config |= IX_BART_LOOPBACK;
	bart_config |= IX_BART_MCS16_TEST; /* inb doesn't get bit! */
	bus_space_write_1(iot, ioh, IX_CONFIG, bart_config);
	bart_config = bus_space_read_1(iot, ioh, IX_CONFIG);

	bus_space_write_1(iot, ioh, IX_ECTRL, 0);
	delay(100);

	rv = 1;
	ia->ia_iosize = IX_IOSIZE;
	DPRINTF(("ix_match: found board @ 0x%x\n", ia->ia_iobase));

out:
	bus_space_unmap(iot, ioh, IX_IOSIZE);
	return (rv);
}

void
ix_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void   *aux;
{
	struct ix_softc *isc = (void *)self;
	struct ie_softc *sc = &isc->sc_ie;
	struct isa_attach_args *ia = aux;

	int media;
	u_short eaddrtemp;
	u_int8_t bart_config;
	bus_space_tag_t iot;
	bus_space_handle_t ioh, memh;
	u_short irq_encoded;
	u_int8_t ethaddr[ETHER_ADDR_LEN];

	iot = ia->ia_iot;

	if (bus_space_map(iot, ia->ia_iobase,
			  ia->ia_iosize, 0, &ioh) != 0) {

		DPRINTF(("\n%s: can't map i/o space 0x%x-0x%x\n",
			  sc->sc_dev.dv_xname, ia->ia_iobase,
			  ia->ia_iobase + ia->ia_iosize - 1));
		return;
	}

	if (bus_space_map(ia->ia_memt, ia->ia_maddr,
			  ia->ia_msize, 0, &memh) != 0) {

		DPRINTF(("\n%s: can't map iomem space 0x%x-0x%x\n",
			sc->sc_dev.dv_xname, ia->ia_maddr,
			ia->ia_maddr + ia->ia_msize - 1));
		bus_space_unmap(iot, ioh, ia->ia_iosize);
		return;
	}

	isc->sc_regt = iot;
	isc->sc_regh = ioh;

	/*
	 * Get the hardware ethernet address from the EEPROM and
	 * save it in the softc for use by the 586 setup code.
	 */
	eaddrtemp = ix_read_eeprom(iot, ioh, IX_EEPROM_ENET_HIGH);
	ethaddr[1] = eaddrtemp & 0xFF;
	ethaddr[0] = eaddrtemp >> 8;
	eaddrtemp = ix_read_eeprom(iot, ioh, IX_EEPROM_ENET_MID);
	ethaddr[3] = eaddrtemp & 0xFF;
	ethaddr[2] = eaddrtemp >> 8;
	eaddrtemp = ix_read_eeprom(iot, ioh, IX_EEPROM_ENET_LOW);
	ethaddr[5] = eaddrtemp & 0xFF;
	ethaddr[4] = eaddrtemp >> 8;

	sc->hwinit = NULL;
	sc->hwreset = ix_reset;
	sc->chan_attn = ix_atten;
	sc->intrhook = ix_intrhook;

	sc->memcopyin = ix_copyin;
	sc->memcopyout = ix_copyout;
	sc->ie_bus_read16 = ix_read_16;
	sc->ie_bus_write16 = ix_write_16;
	sc->ie_bus_write24 = ix_write_24;

	sc->do_xmitnopchain = 0;

	sc->sc_mediachange = NULL;
	sc->sc_mediastatus = ix_mediastatus;

	sc->bt = ia->ia_memt;
	sc->bh = memh;

	/* Map i/o space. */
	sc->sc_msize = ia->ia_msize;
	sc->sc_maddr = (void *)memh;
	sc->sc_iobase = (char *)sc->sc_maddr + sc->sc_msize - (1 << 24);

	/* set up pointers to important on-card control structures */
	sc->iscp = 0;
	sc->scb = IE_ISCP_SZ;
	sc->scp = sc->sc_msize + IE_SCP_ADDR - (1 << 24);

	sc->buf_area = sc->scb + IE_SCB_SZ;
	sc->buf_area_sz = sc->sc_msize - IE_ISCP_SZ - IE_SCB_SZ - IE_SCP_SZ;

	/* zero card memory */
	bus_space_set_region_1(sc->bt, sc->bh, 0, 0, 32);
	bus_space_set_region_1(sc->bt, sc->bh, 0, 0, sc->sc_msize);

	/* set card to 16-bit bus mode */
	bus_space_write_1(sc->bt, sc->bh, IE_SCP_BUS_USE((u_long)sc->scp), 0);

	/* set up pointers to key structures */
	ix_write_24(sc, IE_SCP_ISCP((u_long)sc->scp), (u_long) sc->iscp);
	ix_write_16(sc, IE_ISCP_SCB((u_long)sc->iscp), (u_long) sc->scb);
	ix_write_24(sc, IE_ISCP_BASE((u_long)sc->iscp), (u_long) sc->iscp);

	/* flush setup of pointers, check if chip answers */
	bus_space_barrier(sc->bt, sc->bh, 0, sc->sc_msize,
			  BUS_SPACE_BARRIER_WRITE);
	if (!i82586_proberam(sc)) {
		DPRINTF(("\n%s: Can't talk to i82586!\n",
			sc->sc_dev.dv_xname));
		bus_space_unmap(iot, ioh, ia->ia_iosize);
		bus_space_unmap(ia->ia_memt, memh, ia->ia_msize);
		return;
	}

	/* Figure out which media is being used... */
	if (ix_read_eeprom(iot, ioh, IX_EEPROM_CONFIG1) &
				IX_EEPROM_MEDIA_EXT) {
		if (ix_read_eeprom(iot, ioh, IX_EEPROM_MEDIA) &
				IX_EEPROM_MEDIA_TP)
			media = IFM_ETHER | IFM_10_T;
		else
			media = IFM_ETHER | IFM_10_2;
	} else
		media = IFM_ETHER | IFM_10_5;

	/* Take the card out of lookback */
	bart_config = bus_space_read_1(iot, ioh, IX_CONFIG);
	bart_config &= ~IX_BART_LOOPBACK;
	bart_config |= IX_BART_MCS16_TEST; /* inb doesn't get bit! */
	bus_space_write_1(iot, ioh, IX_CONFIG, bart_config);
	bart_config = bus_space_read_1(iot, ioh, IX_CONFIG);

	irq_encoded = ix_read_eeprom(iot, ioh,
				     IX_EEPROM_CONFIG1);
	irq_encoded = (irq_encoded & IX_EEPROM_IRQ) >> IX_EEPROM_IRQ_SHIFT;

	/* Enable interrupts */
	bus_space_write_1(iot, ioh, IX_IRQ,
			  irq_encoded | IX_IRQ_ENABLE);

	isc->irq_encoded = irq_encoded;

	i82586_attach(sc, "EtherExpress/16", ethaddr,
		      ix_media, NIX_MEDIA, media);

	isc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
					IPL_NET, i82586_intr, sc);
	if (isc->sc_ih == NULL)
		DPRINTF(("\n%s: can't establish interrupt\n",
			sc->sc_dev.dv_xname));
}

struct cfattach ix_ca = {
	sizeof(struct ix_softc), ix_match, ix_attach
};
