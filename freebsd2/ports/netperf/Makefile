# New ports collection makefile for: netperf
# Version required:	2.1.3
# Date created:		19 July 1996
# Whom:			koshy
#
# $Id: Makefile,v 1.2 1999/08/24 15:30:27 sumikawa Exp $
#

DISTNAME=	netperf-2.1pl3
PKGNAME=	netperf-2.1.3
CATEGORIES=	benchmarks
MASTER_SITES=	ftp://ftp.cup.hp.com/dist/networking/benchmarks/netperf/

PATCH_SITES=	ftp://ftp.kame.net/pub/kame/misc/
PATCHFILES=	netperf-21pl3-19990824.diff.gz
PATCH_DIST_STRIP=	-p

MAINTAINER=	sumikawa@kame.net

MAKEFILE=	makefile
MAN1=		netperf.1 netserver.1
MINSTALL=	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

post-install:
	strip ${PREFIX}/netperf/netperf
	strip ${PREFIX}/netperf/netserver
	${MINSTALL} ${WRKSRC}/netperf.man ${PREFIX}/man/man1/netperf.1
	${MINSTALL} ${WRKSRC}/netserver.man ${PREFIX}/man/man1/netserver.1
.if !defined(NOPORTDOCS)
	${MKDIR} ${PREFIX}/share/doc/netperf
	${MINSTALL} ${WRKSRC}/netperf.ps ${PREFIX}/share/doc/netperf
	gzip -9nf ${PREFIX}/share/doc/netperf/netperf.ps
.endif

.include <bsd.port.mk>
