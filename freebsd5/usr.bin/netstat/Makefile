#	@(#)Makefile	8.1 (Berkeley) 6/12/93
# $FreeBSD: src/usr.bin/netstat/Makefile,v 1.23.2.1 2005/02/13 07:23:39 obrien Exp $

PROG=	netstat
SRCS=	if.c inet.c inet6.c main.c mbuf.c mcast.c mroute.c ipx.c route.c \
	unix.c atalk.c netgraph.c mroute6.c ipsec.c sctp.c
CFLAGS+=-Wall -I${.CURDIR}
CFLAGS+=-DIPSEC
CFLAGS+=-DINET6
CFLAGS+=-DDCCP
CFLAGS+=-DSCTP
WARNS?=	2
NO_WERROR=

BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBIPX} ${LIBNETGRAPH} ${LIBUTIL}
LDADD=	-lkvm -lipx -lnetgraph -lutil

LDADD+=	-L${.OBJDIR}/../../lib/libinet6 \
	-L/usr/local/v6/lib -linet6
DPADD+= ${.OBJDIR}/../lib/libinet6/libinet6.a \
	/usr/local/v6/lib/libinet6.a

.PATH:	${.CURDIR}/../../kame/netstat

.include <bsd.prog.mk>
