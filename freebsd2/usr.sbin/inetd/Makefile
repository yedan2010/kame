#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	inetd
SRCS=	inetd.c
MAN8=	inetd.8
MLINKS=	inetd.8 inetd.conf.5

COPTS+=	-Wall -DLOGIN_CAP
#COPTS+=	-DSANITY_CHECK
COPTS+=	-DINET6

DPADD+=	${LIBUTIL}
LDADD+=	-lutil
LDADD+=	-L${.CURDIR}/../../lib/libipsec -L${.CURDIR}/../../lib/libipsec/obj \
	-L${.CURDIR}/../../lib/libinet6 -L${.CURDIR}/../../lib/libinet6/obj \
	-L/usr/local/v6/lib -lipsec -linet6
DPADD+=	${.CURDIR}/../../lib/libipsec/libipsec.a \
	${.CURDIR}/../../lib/libipsec/obj/libipsec.a \
	/usr/local/v6/lib/libipsec.a
DPADD+=	${.CURDIR}/../../lib/libinet6/libinet6.a \
	${.CURDIR}/../../lib/libinet6/obj/libinet6.a \
	/usr/local/v6/lib/libinet6.a

SRCDIR= ${.CURDIR}/../../kame/inetd
.PATH:  ${SRCDIR}
COPTS+=	-DIPSEC
SRCS+=  ipsec.c
COPTS+=-I${SRCDIR}

.include <bsd.prog.mk>
