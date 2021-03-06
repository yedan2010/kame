# New ports collection makefile for:    fwtk
# Date created:                         15th June 1996
# Whom:                                 gpalmer
#
# $KAME: Makefile,v 1.2 2000/06/25 10:58:32 sumikawa Exp $
#

PORTNAME=	fwtk2.1
PORTVERSION=	2.1
CATEGORIES=	security net
EXTRACT_SUFX=	.tar.Z
DISTFILES=	fwtk2.1${EXTRACT_SUFX} fwtk-doc-only${EXTRACT_SUFX}

PATCH_SITES=	http://www.imasy.or.jp/~ume/ipv6/
PATCHFILES=	fwtk2.1-ipv6-19990423.diff.gz
PATCH_DIST_STRIP=	-p1

MAINTAINER=	ume@mahoroba.org

RESTRICTED=	"No form of redistribution is allowed"

WRKSRC=		${WRKDIR}/fwtk
HAS_CONFIGURE=	YES
CONFIGURE_SCRIPT=	fixmake

MAN3=		auth.3
MAN5=		netperm-table.5
MAN8=		authmgr.8 authsrv.8 ftp-gw.8 http-gw.8 login-sh.8 netacl.8 \
		plug-gw.8 rlogin-gw.8 smap.8 smapd.8 tn-gw.8 x-gw.8
DOCS=		sample-report.txt manpages.ps admin_guide.ps user_guide.ps \
		overview.ps presentation.ps
SBIN_BINS=	authdump authload authmgr
LIBEXEC_BINS=	authsrv ftp-gw http-gw netacl plug-gw rlogin-gw smap smapd \
		tn-gw x-gw

PREFIX=		/usr/local/v6

.include <bsd.port.pre.mk>

.if !exists(${DISTDIR}/fwtk2.1${EXTRACT_SUFX}) \
	&& !exists(${DISTDIR}/fwtk-doc-only${EXTRACT_SUFX})
IGNORE='Please read ftp://ftp.tis.com/pub/firewalls/toolkit/README for details of how to obtain the FTWK source.  Put the files fwtk2.1${EXTRACT_SUFX} and fwtk-doc-only${EXTRACT_SUFX} into the directory ${DISTDIR} and run make again.'
.endif

pre-configure:
	@${MV} ${WRKSRC}/Makefile.config ${WRKSRC}/Makefile.config.in
	@${SED} -e s:%%FWTKSRCDIR%%:${WRKSRC}: \
		-e s:%%BSD_PREFIX%%:${PREFIX}: \
		${WRKSRC}/Makefile.config.in > ${WRKSRC}/Makefile.config
	@${MV} ${WRKSRC}/firewall.h ${WRKSRC}/firewall.h.in
	@${SED} -e s:%%FWTKSRCDIR%%:${WRKSRC}: \
		-e s:%%BSD_PREFIX%%:${PREFIX}: \
		${WRKSRC}/firewall.h.in > ${WRKSRC}/firewall.h

post-install:
	( cd ${PREFIX}/libexec ; strip ${SBIN_BINS} ${LIBEXEC_BINS} )
	( cd ${PREFIX}/libexec ; ${MV} ${SBIN_BINS} mqueue ../sbin )
	${MKDIR} ${PREFIX}/share/doc/fwtk
	${INSTALL_MAN} ${WRKSRC}/LICENSE ${PREFIX}/share/doc/fwtk
	( cd ${WRKSRC}/doc/man ; ${INSTALL_MAN} ${MAN3} ${PREFIX}/man/man3 )
	( cd ${WRKSRC}/doc/man ; ${INSTALL_MAN} ${MAN5} ${PREFIX}/man/man5 )
	( cd ${WRKSRC}/doc/man ; ${INSTALL_MAN} ${MAN8} ${PREFIX}/man/man8 )
.if !defined(NOPORTDOCS)
	( cd ${WRKSRC}/doc ; ${INSTALL_MAN} ${DOCS} ${PREFIX}/share/doc/fwtk )
.endif
	${INSTALL_DATA} ${FILESDIR}/netperm-table.v6-sample ${PREFIX}/etc

.include <bsd.port.post.mk>
