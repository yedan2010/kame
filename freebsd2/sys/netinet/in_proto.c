/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in_proto.c	8.2 (Berkeley) 2/9/95
 *	$Id: in_proto.c,v 1.33.2.1 1997/09/16 18:37:00 joerg Exp $
 */

#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/radix.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>
#include <netinet/igmp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_encap.h>
/*
 * TCP/IP protocol family: IP, ICMP, UDP, TCP.
 */

#ifdef IPSEC
#include <netinet6/ah.h>
#ifdef IPSEC_ESP
#include <netinet6/esp.h>
#endif
#include <netinet6/ipcomp.h>
#endif /* IPSEC */

#include "gif.h"
#if NGIF > 0
#include <netinet/in_gif.h>
#endif

#include "stf.h"
#if NSTF > 0
#include <net/if_stf.h>
#endif

#ifdef IPXIP
#include <netipx/ipx.h>
#include <netipx/ipx_ip.h>
#endif

#ifdef NSIP
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef TPIP
void	tpip_input(), tpip_ctlinput(), tp_init(), tp_slowtimo(), tp_drain();
int	tp_ctloutput(), tp_usrreq();
#endif

#ifdef EON
void	eoninput(), eonctlinput(), eonprotoinit();
#endif /* EON */

#if defined(PM)
void	pm_init		__P((void));
void	pm_input	__P((struct mbuf *, int));
int	pm_ctloutput	__P((int, struct socket *, int, int, struct mbuf **));
int	pm_usrreq	__P((struct socket *,
			     int, struct mbuf *, struct mbuf *, struct mbuf *));
#endif

#if defined(NATPT)
void	natpt_init	__P((void));
int	natpt_ctloutput	__P((int, struct socket *, int, int, struct mbuf **));
struct pr_usrreqs natpt_usrreqs;
#endif

extern	struct domain inetdomain;

struct protosw inetsw[] = {
{ 0,		&inetdomain,	0,		0,
  0,		0,		0,		0,
  0,
  ip_init,	0,		ip_slowtimo,	ip_drain
},
{ SOCK_DGRAM,	&inetdomain,	IPPROTO_UDP,	PR_ATOMIC|PR_ADDR,
  udp_input,	0,		udp_ctlinput,	ip_ctloutput,
  udp_usrreq,
  udp_init
},
{ SOCK_STREAM,	&inetdomain,	IPPROTO_TCP,
	PR_CONNREQUIRED|PR_IMPLOPCL|PR_WANTRCVD,
  tcp_input,	0,		tcp_ctlinput,	tcp_ctloutput,
  0,
  tcp_init,	tcp_fasttimo,	tcp_slowtimo,	tcp_drain,
  &tcp_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  rip_input,	0,		0,		rip_ctloutput,
  rip_usrreq,
  0,		0,		0,		0,
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_ICMP,	PR_ATOMIC|PR_ADDR,
  icmp_input,	0,		0,		rip_ctloutput,
  rip_usrreq
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_IGMP,	PR_ATOMIC|PR_ADDR,
  igmp_input,	0,		0,		rip_ctloutput,
  rip_usrreq,
  igmp_init,	igmp_fasttimo,	igmp_slowtimo
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_RSVP,	PR_ATOMIC|PR_ADDR,
  rsvp_input,	0,		0,		rip_ctloutput,
  rip_usrreq,
  0,		0,		0,		0,
},
#ifdef IPSEC
{ SOCK_RAW,	&inetdomain,	IPPROTO_AH,	PR_ATOMIC|PR_ADDR,
  ah4_input,	0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
},
#ifdef IPSEC_ESP
{ SOCK_RAW,	&inetdomain,	IPPROTO_ESP,	PR_ATOMIC|PR_ADDR,
  esp4_input,	0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
},
#endif
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPCOMP,	PR_ATOMIC|PR_ADDR,
  ipcomp4_input, 0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
},
#endif /* IPSEC */
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV4,	PR_ATOMIC|PR_ADDR,
  encap4_input,	0,	 	0,		rip_ctloutput,
  rip_usrreq,	  
  encap_init,	0,		0,		0,
},
#ifdef INET6
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR,
  encap4_input,	0,	 	0,		rip_ctloutput,
  rip_usrreq,	  
  0,		0,		0,		0,
},
#endif /* INET6 */
#if 0
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPIP,	PR_ATOMIC|PR_ADDR,
  ipip_input,	0,	 	0,		rip_ctloutput,
  rip_usrreq,
  0,		0,		0,		0,
},
#endif
#ifdef IPDIVERT
{ SOCK_RAW,	&inetdomain,	IPPROTO_DIVERT,	PR_ATOMIC|PR_ADDR,
  div_input,	0,	 	0,		ip_ctloutput,
  div_usrreq,
  div_init,	0,		0,		0,
},
#endif
#ifdef TPIP
{ SOCK_SEQPACKET,&inetdomain,	IPPROTO_TP,	PR_CONNREQUIRED|PR_WANTRCVD,
  tpip_input,	0,		tpip_ctlinput,	tp_ctloutput,
  tp_usrreq,
  tp_init,	0,		tp_slowtimo,	tp_drain,
},
#endif
/* EON (ISO CLNL over IP) */
#ifdef EON
{ SOCK_RAW,	&inetdomain,	IPPROTO_EON,	0,
  eoninput,	0,		eonctlinput,		0,
  0,
  eonprotoinit,	0,		0,		0,
},
#endif
#ifdef IPXIP
{ SOCK_RAW,	&inetdomain,	IPPROTO_IDP,	PR_ATOMIC|PR_ADDR,
  ipxip_input,	0,		ipxip_ctlinput,	0,
  rip_usrreq,
  0,		0,		0,		0,
},
#endif
#ifdef NSIP
{ SOCK_RAW,	&inetdomain,	IPPROTO_IDP,	PR_ATOMIC|PR_ADDR,
  idpip_input,	0,		nsip_ctlinput,	0,
  rip_usrreq,
  0,		0,		0,		0,
},
#endif
#if defined(PM)
{ SOCK_RAW,	&inetdomain,	IPPROTO_PM,	PR_ATOMIC|PR_ADDR,
  pm_input,	0,		0,		pm_ctloutput,
  pm_usrreq,
  pm_init,	0,		0,		0,
},
#endif
#if defined(NATPT)
{ SOCK_RAW,	&inetdomain,	IPPROTO_AHIP,	PR_ATOMIC|PR_ADDR,
  0,		0,		0,		0,
  0,
  natpt_init,	0,		0,		0,
 &natpt_usrreqs
},
#endif
	/* raw wildcard */
{ SOCK_RAW,	&inetdomain,	0,		PR_ATOMIC|PR_ADDR,
  rip_input,	0,		0,		rip_ctloutput,
  rip_usrreq,
  rip_init,	0,		0,		0,
},
};

#if NGIF > 0
struct protosw in_gif_protosw =
{ SOCK_RAW,	&inetdomain,	0/*IPPROTO_IPV[46]*/,	PR_ATOMIC|PR_ADDR,
  in_gif_input, rip_output,	0,		rip_ctloutput,
  rip_usrreq,
  0,            0,              0,              0,
};
#endif /*NGIF*/

#if NSTF > 0
struct protosw in_stf_protosw =
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR,
  in_stf_input, rip_output,	0,		rip_ctloutput,
  rip_usrreq,
  0,            0,              0,              0
};
#endif /*NSTF*/

extern int in_inithead(void **, int);

struct domain inetdomain =
    { AF_INET, "internet", 0, 0, 0, 
      inetsw, &inetsw[sizeof(inetsw)/sizeof(inetsw[0])], 0,
      in_inithead, 32, sizeof(struct sockaddr_in)
    };

DOMAIN_SET(inet);

SYSCTL_NODE(_net,      PF_INET,		inet,	CTLFLAG_RW, 0,
	"Internet Family");

SYSCTL_NODE(_net_inet, IPPROTO_IP,	ip,	CTLFLAG_RW, 0,	"IP");
SYSCTL_NODE(_net_inet, IPPROTO_ICMP,	icmp,	CTLFLAG_RW, 0,	"ICMP");
SYSCTL_NODE(_net_inet, IPPROTO_UDP,	udp,	CTLFLAG_RW, 0,	"UDP");
SYSCTL_NODE(_net_inet, IPPROTO_TCP,	tcp,	CTLFLAG_RW, 0,	"TCP");
SYSCTL_NODE(_net_inet, IPPROTO_IGMP,	igmp,	CTLFLAG_RW, 0,	"IGMP");
#ifdef IPSEC
SYSCTL_NODE(_net_inet, IPPROTO_AH,	ipsec,	CTLFLAG_RW, 0,	"IPSEC");
#endif /* IPSEC */
#ifdef IPDIVERT
SYSCTL_NODE(_net_inet, IPPROTO_DIVERT,	div,	CTLFLAG_RW, 0,	"DIVERT");
#endif

