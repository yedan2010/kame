#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: spic_acpi.c,v 1.1 2002/04/22 12:42:56 augustss Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/callout.h>

#include <machine/bus.h>

void	*isa_intr_establish(void *ic, int irq, int type,
	    int level, int (*ih_fun)(void *), void *ih_arg);

#include <dev/ic/spicvar.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpivar.h>

struct spic_acpi_softc {
	struct spic_softc sc_spic;	/* spic device */

	struct acpi_devnode *sc_node;	/* our ACPI devnode */

	struct acpi_resources sc_res;	/* our bus resources */

	void *sc_ih;
};

int	spic_acpi_match(struct device *, struct cfdata *, void *);
void	spic_acpi_attach(struct device *, struct device *, void *);

struct cfattach spic_acpi_ca = {
	sizeof(struct spic_acpi_softc), spic_acpi_match, spic_acpi_attach,
	/* spic_acpi_detach, spic_acpi_activate */
};

int
spic_acpi_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return (0);

	if (strcmp(aa->aa_node->ad_devinfo.HardwareId, "SNY6001") == 0)
		return (1);

	return (0);
}

void
spic_acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct spic_acpi_softc *sc = (void *) self;
	struct acpi_attach_args *aa = aux;
	struct acpi_io *io;
	struct acpi_irq *irq;
	ACPI_STATUS rv;

	printf(": Sony Programmable I/O Controller\n");

	sc->sc_node = aa->aa_node;

	/* Parse our resources. */
	rv = acpi_resource_parse(&sc->sc_spic.sc_dev, sc->sc_node, &sc->sc_res,
	    &acpi_resource_parse_ops_default);
	if (rv != AE_OK) {
		printf("%s: unable to parse resources: %d\n",
		    sc->sc_spic.sc_dev.dv_xname, rv);
		return;
	}

	sc->sc_spic.sc_iot = aa->aa_iot;
	io = acpi_res_io(&sc->sc_res, 0);
	if (io == NULL) {
		printf("%s: unable to find io resource\n",
		    sc->sc_spic.sc_dev.dv_xname);
		return;
	}
	if (bus_space_map(sc->sc_spic.sc_iot, io->ar_base, io->ar_length,
	    0, &sc->sc_spic.sc_ioh) != 0) {
		printf("%s: unable to map data register\n",
		    sc->sc_spic.sc_dev.dv_xname);
		return;
	}
	irq = acpi_res_irq(&sc->sc_res, 0);
	if (irq == NULL) {
		printf("%s: unable to find irq resource\n",
		    sc->sc_spic.sc_dev.dv_xname);
		/* XXX unmap */
		return;
	}
#if 0
	sc->sc_ih = isa_intr_establish(NULL, irq->ar_irq,
	    IST_EDGE, IPL_TTY, spic_intr, sc);
#endif

	spic_attach(&sc->sc_spic);
}

