#	$NetBSD: Makefile,v 1.4 1999/02/13 02:54:43 lukem Exp $

PROG=	sdboot
SRCS=	${.CURDIR}/sdboot.s
MKMAN=	no
LIBCRT0=
INCL=	-I${.CURDIR} -I${.CURDIR}/..

CPPFLAGS+= ${INCL}

.include <bsd.prog.mk>
