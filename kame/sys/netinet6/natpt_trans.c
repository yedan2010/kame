/*	$KAME: natpt_trans.c,v 1.39 2001/06/18 14:34:22 fujisawa Exp $	*/

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

#if defined(__FreeBSD__)
#include "opt_natpt.h"
#endif

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/ctype.h>

#ifdef __FreeBSD__
# include <sys/kernel.h>
#endif

#include <net/if.h>
#ifdef __bsdi__
#include <net/route.h>
#endif

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#if defined(__bsdi__) || defined(__NetBSD__)
#include <net/route.h>		/* netinet/in_pcb.h line 71 make happy.		*/
#include <netinet/in_pcb.h>
#endif

#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#if !defined(__NetBSD__) && (!defined(__FreeBSD__) || (__FreeBSD__ < 3))
#include <netinet6/tcp6.h>
#endif

#include <netinet6/natpt_defs.h>
#include <netinet6/natpt_list.h>
#include <netinet6/natpt_log.h>
#include <netinet6/natpt_var.h>


#define	recalculateTCP4Checksum		1
#define	recalculateTCP6Checksum		1
#define	recalculateUDP4Checksum		1

#define	FTP_DATA			20
#define	FTP_CONTROL			21

#if BYTE_ORDER == BIG_ENDIAN
#define	FTP4_PORT			0x504f5254
#define	FTP6_LPSV			0x4c505356
#define	FTP6_LPRT			0x4c505254
#define	FTP6_EPRT			0x45505254
#define	FTP6_EPSV			0x45505356
#else
#define	FTP4_PORT			0x54524f50
#define	FTP6_LPSV			0x5653504c
#define	FTP6_LPRT			0x5452504c
#define	FTP6_EPRT			0x54525045
#define	FTP6_EPSV			0x56535045
#endif

#define	FTPMINCMD			"CWD\r\n"
#define	FTPMINCMDLEN			strlen(FTPMINCMD)


struct ftpparam
{
    u_long		 cmd;
    caddr_t		 arg;		/* argument in mbuf if exist	*/
    caddr_t		 argend;
    struct sockaddr	*sa;		/* allocated			*/
};


/*
 *
 */

int		 errno;
int		 ip6_protocol_tr;

extern	struct in6_addr	 natpt_prefix;
extern	struct in6_addr	 natpt_prefixmask;

void		 fakeTimxceed			__P((struct _cv *, struct mbuf *));

struct mbuf	*translatingTCPUDPv4To4		__P((struct _cv *, struct pAddr *, struct _cv *));
void		 translatingPYLD4To4		__P((struct _cv *));
struct sockaddr *parsePORT			__P((caddr_t, caddr_t, struct sockaddr_in *));

void		 tr_icmp4EchoReply		__P((struct _cv *, struct _cv *));
void		 tr_icmp4Unreach		__P((struct _cv *, struct _cv *, struct pAddr *));
void		 tr_icmp4Echo			__P((struct _cv *, struct _cv *));
void		 tr_icmp4Timxceed		__P((struct _cv *, struct _cv *, struct pAddr *));
void		 tr_icmp4Paramprob		__P((struct _cv *, struct _cv *));
void		 tr_icmp4MimicPayload		__P((struct _cv *, struct _cv *, struct pAddr *));

void		 tr_icmp6DstUnreach		__P((struct _cv *, struct _cv *));
void		 tr_icmp6PacketTooBig		__P((struct _cv *, struct _cv *));
void		 tr_icmp6TimeExceed		__P((struct _cv *, struct _cv *));
void		 tr_icmp6ParamProb		__P((struct _cv *, struct _cv *));
void		 tr_icmp6EchoRequest		__P((struct _cv *, struct _cv *));
void		 tr_icmp6EchoReply		__P((struct _cv *, struct _cv *));

void		 translatingPYLD4To6		__P((struct _cv *));
int		 translatingFTP4ReplyTo6	__P((struct _cv *));

void		 translatingPYLD6To4		__P((struct _cv *));
int		 translatingFTP6CommandTo4	__P((struct _cv *));

struct ftpparam *parseFTPdialogue		__P((caddr_t, caddr_t, struct ftpparam *));
struct sockaddr *parseLPRT			__P((caddr_t, caddr_t, struct sockaddr_in6 *));
struct sockaddr	*parseEPRT			__P((caddr_t, caddr_t, struct sockaddr_in6 *));
struct sockaddr	*parse227			__P((caddr_t, caddr_t, struct sockaddr_in *));
int		 natpt_pton6			__P((caddr_t, caddr_t, struct in6_addr *));
int		 rewriteMbuf			__P((struct mbuf *, char *, int , char *,int));
void		 incrementSeq			__P((struct tcphdr *, int));
void		 decrementAck			__P((struct tcphdr *, int));

static	void	 _recalculateTCP4Checksum	__P((struct _cv *));
static	void	 _recalculateUDP4Checksum	__P((struct _cv *));

static	int	 updateTcpStatus		__P((struct _cv *));
static	int	 _natpt_tcpfsm			__P((int, int, u_short, u_char));
static	int	 _natpt_tcpfsmSessOut		__P((int, short, u_char));
static	int	 _natpt_tcpfsmSessIn		__P((int, short, u_char));

static	void	 adjustPayloadChecksum		__P((int, struct _cv *, struct _cv *));
static	void	 adjustUpperLayerChecksum	__P((int, int, struct _cv *, struct _cv *));
static	int	 adjustChecksum			__P((int, u_char *, int, u_char *, int));


#if defined(__FreeBSD__) && __FreeBSD__ >= 3
MALLOC_DECLARE(M_NATPT);
#endif


#ifdef NATPT_NAT
/*
 *	Translating From IPv4 to IPv4
 */

struct mbuf *
translatingIPv4To4(struct _cv *cv4, struct pAddr *pad)
{
    struct timeval	 atv;
    struct mbuf		*m4 = NULL;

    if (isDump(D_TRANSLATINGIPV4))
	natpt_logIp4(LOG_DEBUG, cv4->_ip._ip4, NULL);

    microtime(&atv);
    cv4->ats->tstamp = atv.tv_sec;

    switch (cv4->ip_payload)
    {
      case IPPROTO_ICMP:
	m4 = translatingICMPv4To4(cv4, pad);
	break;

      case IPPROTO_TCP:
	m4 = translatingTCPv4To4(cv4, pad);
	break;

      case IPPROTO_UDP:
	m4 = translatingUDPv4To4(cv4, pad);
	break;
    }

    if (m4)
    {
	int		 mlen;
	struct mbuf	*mm;
	struct ip	*ip4;

	ip4 = mtod(m4, struct ip *);
	ip4->ip_sum = 0;			/* Header checksum	*/
	ip4->ip_sum = in_cksum(m4, sizeof(struct ip));
	m4->m_pkthdr.rcvif = cv4->m->m_pkthdr.rcvif;

	for (mlen = 0, mm = m4; mm; mm = mm->m_next)
	{
	    mlen += mm->m_len;
	}

	m4->m_pkthdr.len = mlen;

	if (isDump(D_TRANSLATEDIPV4))
	    natpt_logIp4(LOG_DEBUG, ip4, NULL);
    }

    return (m4);
}


struct mbuf *
translatingICMPv4To4(struct _cv *cv4from, struct pAddr *pad)
{
    struct _cv		 cv4to;
    struct mbuf		*m4;
    struct ip		*ip4from, *ip4to;
    struct icmp		*icmp4from;

    ip4from = mtod(cv4from->m, struct ip *);
    icmp4from = cv4from->_payload._icmp4;

    m4 = m_copym(cv4from->m, 0, M_COPYALL, M_NOWAIT);
    ReturnEnobufs(m4);

    bzero(&cv4to, sizeof(struct _cv));
    cv4to.m = m4;
    cv4to._ip._ip4 = ip4to = mtod(m4, struct ip *);
    cv4to._payload._caddr = (caddr_t)cv4to._ip._ip4 + (ip4from->ip_hl << 2);

    ip4to->ip_dst = pad->in4dst;	/* rewrite destination address	*/

    switch (icmp4from->icmp_type)
    {
      case ICMP_ECHOREPLY:
	ip4to->ip_src = pad->in4src;	/* rewrite source address	*/
	break;

      case ICMP_UNREACH:
	switch (icmp4from->icmp_code)
	{
	  case ICMP_UNREACH_PORT:
	  case ICMP_UNREACH_NEEDFRAG:
	    fakeTimxceed(cv4from, m4);
	}
	break;

      case ICMP_TIMXCEED:
	if (icmp4from->icmp_code == ICMP_TIMXCEED_INTRANS)
	    fakeTimxceed(cv4from, m4);
	break;

      case ICMP_ECHO:
	ip4to->ip_src = pad->in4src;	/* rewrite source address	*/
	break;

      default:
	m_freem(m4);
	return (NULL);
    }

    m4->m_len = cv4from->m->m_len;
    return (m4);
}


void
fakeTimxceed(struct _cv *cv4from, struct mbuf *m4)
{
    struct ip		*ip4to;
    struct icmp		*icmp4to;
    struct ip		*innerip4to;
    struct udphdr	*innerudp4to;
    struct _tSlot	*ats;

    if (isDump(D_FAKETRACEROUTE))
	natpt_logMBuf(LOG_DEBUG, cv4from->m, "fakeTimxceed().");

    ip4to = mtod(m4, struct ip *);
    icmp4to = (struct icmp *)((caddr_t)ip4to + (ip4to->ip_hl << 2));
    innerip4to = &icmp4to->icmp_ip;
    innerudp4to = (struct udphdr *)((caddr_t)innerip4to + (innerip4to->ip_hl << 2));

    ats = cv4from->ats;
    innerip4to->ip_src = ats->local.in4dst;
    innerudp4to->uh_sport = ats->local._dport;

    {
	int	cksum;
	struct
	{
		struct in_addr a;
		u_int16_t p;
	}	Dum, Dee;

	bcopy(&ats->remote.in4src, &Dum.a, sizeof(Dum.a));
	bcopy(&ats->remote.in4dst, &Dee.a, sizeof(Dee.a));
	cksum = adjustChecksum(ntohs(innerip4to->ip_sum),
			       (u_char *)&Dum.a, sizeof(Dum.a),
			       (u_char *)&Dee.a, sizeof(Dee.a));
	innerip4to->ip_sum = htons(cksum);

	Dum.p = ats->remote._sport;
	Dum.p = ats->local._sport;	/*XXX BUG? */
	cksum = adjustChecksum(ntohs(icmp4to->icmp_cksum),
			       (u_char *)&Dum, sizeof(Dum),
			       (u_char *)&Dee, sizeof(Dee));
	icmp4to->icmp_cksum = htons(cksum);
    }
    
    {
	int	hlen = ip4to->ip_hl << 2;
	
	m4->m_data += hlen;
	m4->m_len  -= hlen;
	icmp4to->icmp_cksum = 0;
	icmp4to->icmp_cksum = in_cksum(m4, ip4to->ip_len - hlen);
	m4->m_data -= hlen;
	m4->m_len  += hlen;
    }

    if (isDump(D_FAKETRACEROUTE))
	natpt_logMBuf(LOG_DEBUG, cv4from->m, "fakeTimxceed().");
}


struct mbuf *
translatingTCPv4To4(struct _cv *cv4from, struct pAddr *pad)
{
    struct _cv		 cv4to;
    struct mbuf		*m4;

    bzero(&cv4to, sizeof(struct _cv));
    m4 = translatingTCPUDPv4To4(cv4from, pad, &cv4to);
    cv4to.ip_p  = cv4to.ip_payload = IPPROTO_TCP;
    cv4to.inout = cv4from->inout;

    updateTcpStatus(&cv4to);
    translatingPYLD4To4(&cv4to);
    adjustPayloadChecksum(IPPROTO_TCP, cv4from, &cv4to);

#ifdef recalculateTCP4Checksum
    _recalculateTCP4Checksum(&cv4to);
#endif

    return (m4);
}


struct mbuf *
translatingUDPv4To4(struct _cv *cv4from, struct pAddr *pad)
{
    struct _cv		 cv4to;
    struct mbuf		*m4;

    bzero(&cv4to, sizeof(struct _cv));
    m4 = translatingTCPUDPv4To4(cv4from, pad, &cv4to);
    cv4to.ip_p = cv4to.ip_payload = IPPROTO_UDP;

    adjustPayloadChecksum(IPPROTO_UDP, cv4from, &cv4to);

#ifdef recalculateUDP4Checksum
    _recalculateUDP4Checksum(&cv4to);
#endif

    return (m4);
}


struct mbuf *
translatingTCPUDPv4To4(struct _cv *cv4from, struct pAddr *pad, struct _cv *cv4to)
{
    struct mbuf		*m4;
    struct ip		*ip4to;
    struct tcphdr	*tcp4to;

    m4 = m_copym(cv4from->m, 0, M_COPYALL, M_NOWAIT);
    ReturnEnobufs(m4);

    ip4to = mtod(m4, struct ip *);

    ip4to->ip_src = pad->in4src;
    ip4to->ip_dst = pad->in4dst;

    tcp4to = (struct tcphdr *)((caddr_t)ip4to + (ip4to->ip_hl << 2));
    tcp4to->th_sport = pad->_sport;
    tcp4to->th_dport = pad->_dport;

    cv4to->m = m4;
    cv4to->_ip._ip4 = ip4to;
    cv4to->_payload._tcp4 = tcp4to;
    cv4to->ats = cv4from->ats;

    return (m4);
}


void
translatingPYLD4To4(struct _cv *cv4to)
{
    int			 delta = 0;
    struct tcphdr	*th4 = cv4to->_payload._tcp4;
    struct _tcpstate	*ts  = NULL;

    if (cv4to->ats
	&& (cv4to->ats->session == NATPT_OUTBOUND)
	&& (((cv4to->inout == NATPT_OUTBOUND)
	     && (htons(th4->th_dport) == FTP_CONTROL))
	    || ((cv4to->inout == NATPT_INBOUND)
		&& htons(th4->th_sport) == FTP_CONTROL)))
    {
	tcp_seq	th_seq;
	tcp_seq	th_ack;

	if ((delta = translatingFTP6CommandTo4(cv4to)) != 0)
	{
	    struct mbuf		*mbf = cv4to->m;
	    struct ip		*ip4 = cv4to->_ip._ip4;

	    ip4->ip_len += delta;
	    mbf->m_len += delta;
	    if (mbf->m_flags & M_PKTHDR)
		mbf->m_pkthdr.len += delta;
	}

	if ((ts = cv4to->ats->suit.tcp) == NULL)
	    return ;

	th_seq = th4->th_seq;
	th_ack = th4->th_ack;

	if (ts->delta[0])
	{
	    if ((cv4to->inout == NATPT_INBOUND) && (th4->th_flags & TH_ACK))
		decrementAck(th4, ts->delta[0]);
	    else if (cv4to->inout == NATPT_OUTBOUND)
		incrementSeq(th4, ts->delta[0]);
	}

	if (ts->delta[1])
	{
	    if ((cv4to->inout == NATPT_OUTBOUND) && (th4->th_flags & TH_ACK))
		decrementAck(th4, ts->delta[1]);
	    else if (cv4to->inout == NATPT_INBOUND)
		incrementSeq(th4, ts->delta[1]);
	}
	
	if ((delta != 0)
	    && ((th_seq != ts->seq[0])
		|| (th_ack != ts->ack[0])))
	{
	    ts->delta[0] += delta;
	    ts->seq[0] = th_seq;
	    ts->ack[0] = th_ack;
	}
    }
}


struct sockaddr *
parsePORT(caddr_t kb, caddr_t kk, struct sockaddr_in *sin)
{
    int		 cnt, bite;
    u_char	*d;

    bzero(sin, sizeof(struct sockaddr_in));
    sin->sin_len = sizeof(struct sockaddr_in);
    sin->sin_family = AF_INET;

    d = (u_char *)&sin->sin_addr;
    for (bite = 0, cnt = 4; (kb < kk) && (isdigit(*kb) || (*kb == ',')); kb++)
    {
	if (*kb == ',')
	{
	    *d++ = (bite & 0xff);
	    bite = 0;
	    if (--cnt <= 0)
		break;
	}
	else
	    bite = bite * 10 + *kb - '0';
    }

    if (cnt != 0)			return (NULL);

    kb++;
    d = (u_char *)&sin->sin_port;
    for (bite = 0, cnt = 2; (kb < kk) && (isdigit(*kb) || (*kb == ',')); kb++)
    {
	if (*kb == ',')
	{
	    *d++ = (bite & 0xff);
	    bite = 0;
	    if (--cnt <= 0)
		break;
	}
	else
	    bite = bite * 10 + *kb - '0';
    }

    if (cnt != 1)			return (NULL);
    if (bite > 0)
	*d = (bite & 0xff);

    return ((struct sockaddr *)sin);
}


#ifdef NATPT_FRAGMENT
struct mbuf *
translatingIPv4To4frag(struct _cv *cv4, struct pAddr *pad)
{
    struct timeval	 atv;
    struct mbuf		*m4 = NULL;
    struct ip		*ip4to;

    if (isDump(D_FRAGMENTED))
	natpt_logIp4(LOG_DEBUG, cv4->_ip._ip4, NULL);

    if (isDump(D_FRAGMENTED))
    {
	char	Wow[256];
	
	sprintf(Wow, "cv4: %p", cv4);
	natpt_logMsg(LOG_DEBUG, Wow, strlen(Wow));
	
	if (cv4)
	{
	    sprintf(Wow, "cv4->ats: %p", cv4->ats);
	    natpt_logMsg(LOG_DEBUG, Wow, strlen(Wow));
	}
    }

    microtime(&atv);
    cv4->ats->tstamp = atv.tv_sec;

    m4 = m_copym(cv4->m, 0, M_COPYALL, M_NOWAIT);
    ReturnEnobufs(m4);

    ip4to = mtod(m4, struct ip *);

    ip4to->ip_src = pad->in4src;
    ip4to->ip_dst = pad->in4dst;

    ip4to->ip_sum = 0;			/* Header checksum	*/
    ip4to->ip_sum = in_cksum(m4, sizeof(struct ip));
    m4->m_pkthdr.rcvif = cv4->m->m_pkthdr.rcvif;
    m4->m_pkthdr.len = cv4->m->m_pkthdr.len;

    if (isDump(D_FRAGMENTED))
	natpt_logIp4(LOG_DEBUG, ip4to, NULL);

    return (m4);
}
#endif	/* ifdef NATPT_FRAGMENT	*/
#endif	/* ifdef NATPT_NAT	*/


/*
 *	Translating From IPv4 To IPv6
 */

struct mbuf *
translatingIPv4To6(struct _cv *cv4, struct pAddr *pad)
{
    struct timeval	 atv;
    struct mbuf		*m6 = NULL;

    if (isDump(D_TRANSLATINGIPV4))
	natpt_logIp4(LOG_DEBUG, cv4->_ip._ip4, NULL);

    microtime(&atv);
    cv4->ats->tstamp = atv.tv_sec;

    switch (cv4->ip_payload)
    {
      case IPPROTO_ICMP:
	m6 = translatingICMPv4To6(cv4, pad);
	break;

      case IPPROTO_TCP:
	m6 = translatingTCPv4To6(cv4, pad);
	break;

      case IPPROTO_UDP:
	m6 = translatingUDPv4To6(cv4, pad);
	break;
    }

    if (m6)
	m6->m_pkthdr.rcvif = cv4->m->m_pkthdr.rcvif;

    return (m6);
}


struct mbuf *
translatingICMPv4To6(struct _cv *cv4, struct pAddr *pad)
{
    struct _cv		 cv6;
    struct mbuf		*m6;
    struct ip		*ip4;
    struct ip6_hdr	*ip6;
    struct icmp		*icmp4;
    struct icmp6_hdr	*icmp6;

    ip4 = mtod(cv4->m, struct ip *);
    icmp4 = cv4->_payload._icmp4;

    {
	caddr_t		 icmp4end;
	int		 icmp4len;

	icmp4end = (caddr_t)ip4 + cv4->m->m_pkthdr.len;
	icmp4len = icmp4end - (caddr_t)cv4->_payload._icmp4;

	if (sizeof(struct ip6_hdr) + icmp4len > MCLBYTES) {
	    errno = ENOBUFS;
	    return NULL;
	}
	MGETHDR(m6, M_NOWAIT, MT_HEADER);
	if (m6 && sizeof(struct ip6_hdr) + icmp4len > MHLEN) {
	    MCLGET(m6, M_NOWAIT);
	    if ((m6->m_flags & M_EXT) == 0) {
		m_freem(m6);
		m6 = NULL;
	    }
	}
	if (!m6) {
	    errno = ENOBUFS;
	    return (NULL);
	}
	m6->m_pkthdr.rcvif = NULL;
    }

    cv6.m = m6;
    cv6._ip._ip6 = mtod(m6, struct ip6_hdr *);
    cv6._payload._caddr = (caddr_t)cv6._ip._ip6 + sizeof(struct ip6_hdr);

    ip6 = mtod(cv6.m,  struct ip6_hdr *);
    icmp6 = cv6._payload._icmp6;;

    ip6->ip6_flow = 0;
    ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
    ip6->ip6_vfc |=  IPV6_VERSION;
    ip6->ip6_plen = 0;						/* XXX */
    ip6->ip6_nxt  = IPPROTO_ICMPV6;
    ip6->ip6_hlim = ip4->ip_ttl;
    ip6->ip6_dst  = pad->in6dst;
    ip6->ip6_src  = pad->in6src;
    if (natpt_prefix.s6_addr32[0] != 0)
    {
	ip6->ip6_src.s6_addr32[0] = natpt_prefix.s6_addr32[0];
	ip6->ip6_src.s6_addr32[1] = natpt_prefix.s6_addr32[1];
	ip6->ip6_src.s6_addr32[2] = natpt_prefix.s6_addr32[2];
    }
    else
    {
	ip6->ip6_src.s6_addr32[0] = 0;
	ip6->ip6_src.s6_addr32[1] = 0;
	ip6->ip6_src.s6_addr32[2] = 0;
    }
    ip6->ip6_src.s6_addr32[3] = ip4->ip_src.s_addr;

    switch (icmp4->icmp_type)
    {
      case ICMP_ECHOREPLY:
	tr_icmp4EchoReply(cv4, &cv6);
	break;

      case ICMP_UNREACH:
	tr_icmp4Unreach(cv4, &cv6, pad);
	tr_icmp4MimicPayload(cv4, &cv6, pad);
	break;

      case ICMP_ECHO:
	tr_icmp4Echo(cv4, &cv6);
	break;

      case ICMP_TIMXCEED:
	tr_icmp4Timxceed(cv4, &cv6, pad);
	tr_icmp4MimicPayload(cv4, &cv6, pad);
	break;

      case ICMP_PARAMPROB:
	tr_icmp4Paramprob(cv4, &cv6);
	break;

      case ICMP_REDIRECT:
      case ICMP_ROUTERADVERT:
      case ICMP_ROUTERSOLICIT:
	m_freem(m6);		/* Single hop message.	Silently drop.	*/
	return (NULL);

      case ICMP_SOURCEQUENCH:
      case ICMP_TSTAMP:
      case ICMP_TSTAMPREPLY:
      case ICMP_IREQ:
      case ICMP_IREQREPLY:
      case ICMP_MASKREQ:
      case ICMP_MASKREPLY:
	m_freem(m6);		/* Obsoleted in ICMPv6.	 Silently drop.	*/
	return (NULL);

      default:
	m_freem(m6);		/* Silently drop.			*/
	return (NULL);
    }

    icmp6->icmp6_cksum = 0;
    icmp6->icmp6_cksum = in6_cksum(cv6.m, IPPROTO_ICMPV6,
				   sizeof(struct ip6_hdr), ntohs(ip6->ip6_plen));

    return (m6);
}


void
tr_icmp4EchoReply(struct _cv *cv4, struct _cv *cv6)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp6->icmp6_type = ICMP6_ECHO_REPLY;
    icmp6->icmp6_code = 0;
    icmp6->icmp6_id   = icmp4->icmp_id;
    icmp6->icmp6_seq  = icmp4->icmp_seq;

    {
	int		 dlen;
	struct ip	*ip4 = cv4->_ip._ip4;
	struct ip6_hdr	*ip6 = cv6->_ip._ip6;
	caddr_t		 icmp4off, icmp6off;
	caddr_t		 icmp4end = (caddr_t)ip4 + cv4->m->m_pkthdr.len;
	int		 icmp4len = icmp4end - (caddr_t)cv4->_payload._icmp4;

	dlen = icmp4len - ICMP_MINLEN;
	icmp4off = (caddr_t)(cv4->_payload._icmp4) + ICMP_MINLEN;
	icmp6off = (caddr_t)(cv6->_payload._icmp6) + sizeof(struct icmp6_hdr);
	bcopy(icmp4off, icmp6off, dlen);

	ip6->ip6_plen = ntohs(sizeof(struct icmp6_hdr) + dlen);
	cv6->m->m_pkthdr.len
	  = cv6->m->m_len
	  = sizeof(struct ip6_hdr) + htons(ip6->ip6_plen);
    }
}


void
tr_icmp4Unreach(struct _cv *cv4, struct _cv *cv6, struct pAddr *pad)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp6->icmp6_type = ICMP6_DST_UNREACH;
    icmp6->icmp6_code = 0;
    icmp6->icmp6_id   = icmp4->icmp_id;
    icmp6->icmp6_seq  = icmp4->icmp_seq;

    switch (icmp4->icmp_code)
    {
      case ICMP_UNREACH_NET:
      case ICMP_UNREACH_HOST:
	icmp6->icmp6_code = ICMP6_DST_UNREACH_NOROUTE;
	break;

      case ICMP_UNREACH_PROTOCOL:					/* do more	*/
	icmp6->icmp6_type = ICMP6_PARAM_PROB;
	icmp6->icmp6_code = ICMP6_PARAMPROB_NEXTHEADER;			/* xxx		*/
	break;

      case ICMP_UNREACH_PORT:
	icmp6->icmp6_code = ICMP6_DST_UNREACH_NOPORT;
	break;

      case ICMP_UNREACH_NEEDFRAG:					/* do more	*/
	icmp6->icmp6_type = ICMP6_PACKET_TOO_BIG;
	icmp6->icmp6_code = ICMP6_PARAMPROB_HEADER;
	break;

      case ICMP_UNREACH_SRCFAIL:
	icmp6->icmp6_code = ICMP6_DST_UNREACH_NOTNEIGHBOR;
	break;

      case ICMP_UNREACH_NET_UNKNOWN:
      case ICMP_UNREACH_HOST_UNKNOWN:
	icmp6->icmp6_code = ICMP6_DST_UNREACH_NOROUTE;
	break;

      case ICMP_UNREACH_ISOLATED:
	icmp6->icmp6_code = ICMP6_DST_UNREACH_NOROUTE;
	break;

      case ICMP_UNREACH_NET_PROHIB:
      case ICMP_UNREACH_HOST_PROHIB:
	icmp6->icmp6_code = ICMP6_DST_UNREACH_ADMIN;
	break;

      case ICMP_UNREACH_TOSNET:
      case ICMP_UNREACH_TOSHOST:
	icmp6->icmp6_code = ICMP6_DST_UNREACH_NOROUTE;
	break;

      default:
	break;
    }
}


void
tr_icmp4Echo(struct _cv *cv4, struct _cv *cv6)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp6->icmp6_type = ICMP6_ECHO_REQUEST;
    icmp6->icmp6_code = 0;
    icmp6->icmp6_id   = icmp4->icmp_id;
    icmp6->icmp6_seq  = icmp4->icmp_seq;

    {
	int		 dlen;
	struct ip	*ip4 = cv4->_ip._ip4;
	struct ip6_hdr	*ip6 = cv6->_ip._ip6;
	caddr_t		 icmp4off, icmp6off;
	caddr_t		 icmp4end = (caddr_t)ip4 + cv4->m->m_pkthdr.len;
	int		 icmp4len = icmp4end - (caddr_t)cv4->_payload._icmp4;

	dlen = icmp4len - ICMP_MINLEN;
	icmp4off = (caddr_t)(cv4->_payload._icmp4) + ICMP_MINLEN;
	icmp6off = (caddr_t)(cv6->_payload._icmp6) + sizeof(struct icmp6_hdr);
	bcopy(icmp4off, icmp6off, dlen);

	ip6->ip6_plen = ntohs(sizeof(struct icmp6_hdr) + dlen);
	cv6->m->m_pkthdr.len
	  = cv6->m->m_len
	  = sizeof(struct ip6_hdr) + htons(ip6->ip6_plen);
    }
}


void
tr_icmp4Timxceed(struct _cv *cv4, struct _cv *cv6, struct pAddr *pad)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp6->icmp6_type = ICMP6_TIME_EXCEEDED;
    icmp6->icmp6_code = 0;
    icmp6->icmp6_id   = icmp4->icmp_id;
    icmp6->icmp6_seq  = icmp4->icmp_seq;
}


void
tr_icmp4Paramprob(struct _cv *cv4, struct _cv *cv6)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp6->icmp6_type = ICMP6_PARAM_PROB;
    icmp6->icmp6_code = 0;
    icmp6->icmp6_id   = icmp4->icmp_id;
    icmp6->icmp6_seq  = icmp4->icmp_seq;
}


void
tr_icmp4MimicPayload(struct _cv *cv4, struct _cv *cv6, struct pAddr *pad)
{
    int			 dgramlen;
    int			 icmp6dlen, icmp6rest;
    struct ip		*icmpip4, *ip4 = cv4->_ip._ip4;
    struct ip6_hdr	*icmpip6, *ip6 = cv6->_ip._ip6;
    caddr_t		 icmp4off, icmp4dgramoff;
    caddr_t		 icmp6off, icmp6dgramoff;
    caddr_t		 icmp4end = (caddr_t)ip4 + cv4->m->m_pkthdr.len;
    int			 icmp4len = icmp4end - (caddr_t)cv4->_payload._icmp4;

    icmp6rest = MHLEN - sizeof(struct ip6_hdr) * 2 - sizeof(struct icmp6_hdr);
    dgramlen  = icmp4len - ICMP_MINLEN - sizeof(struct ip);
    dgramlen  = min(icmp6rest, dgramlen);

    icmp4off = (caddr_t)(cv4->_payload._icmp4) + ICMP_MINLEN;
    icmp6off = (caddr_t)(cv6->_payload._icmp6) + sizeof(struct icmp6_hdr);
    icmp4dgramoff = icmp4off + sizeof(struct ip);
    icmp6dgramoff = icmp6off + sizeof(struct ip6_hdr);

    icmpip4 = (struct ip *)icmp4off;
    icmpip6 = (struct ip6_hdr *)icmp6off;
    bzero(icmpip6, sizeof(struct ip6_hdr));
    bcopy(icmp4dgramoff, icmp6dgramoff, dgramlen);

    icmpip6->ip6_flow = 0;
    icmpip6->ip6_vfc &= ~IPV6_VERSION_MASK;
    icmpip6->ip6_vfc |=	 IPV6_VERSION;
    icmpip6->ip6_plen = htons(ntohs(icmpip4->ip_len) - sizeof(struct ip6_hdr));
    icmpip6->ip6_nxt  = icmpip4->ip_p;
    icmpip6->ip6_hlim = icmpip4->ip_ttl;
    icmpip6->ip6_src  = pad->in6dst;
    icmpip6->ip6_dst  = pad->in6src;

    icmp6dlen = sizeof(struct icmp6_hdr) + sizeof(struct ip6_hdr) + dgramlen;
    ip6->ip6_plen = ntohs(icmp6dlen);
    cv6->m->m_pkthdr.len
      = cv6->m->m_len
      = sizeof(struct ip6_hdr) + htons(ip6->ip6_plen);

    switch (cv4->_payload._icmp4->icmp_type)
    {
      case ICMP_ECHO:		/* ping unreach	*/
	{
	    struct icmp6_hdr	*icmp6;

	    icmp6 = (struct icmp6_hdr *)((caddr_t)icmpip6 + sizeof(struct ip6_hdr));
	    icmp6->icmp6_type = ICMP6_ECHO_REQUEST;
	}
	break;

      case ICMP_UNREACH:
      case ICMP_TIMXCEED:	/* traceroute return */
	{
	    struct udphdr	*icmpudp6;

	    icmpudp6 = (struct udphdr *)((caddr_t)icmpip6 + sizeof(struct ip6_hdr));
	    icmpudp6->uh_sport = pad->_dport;
	    icmpudp6->uh_dport = pad->_sport;
	}
	break;
    }
}


struct mbuf *
translatingTCPv4To6(struct _cv *cv4, struct pAddr *pad)
{
    int			 cksumOrg;
    struct _cv		 cv6;
    struct mbuf		*m6;

    bzero(&cv6, sizeof(struct _cv));
    m6 = translatingTCPUDPv4To6(cv4, pad, &cv6);
    cv6.ip_p = cv6.ip_payload = IPPROTO_TCP;

    updateTcpStatus(cv4);
    translatingPYLD4To6(&cv6);
    cksumOrg = ntohs(cv4->_payload._tcp4->th_sum);
    adjustUpperLayerChecksum(IPPROTO_IPV4, IPPROTO_TCP, &cv6, cv4);

#ifdef recalculateTCP6Checksum
    {
	int		 cksumAdj, cksumCks;
	struct tcp6hdr	*th;

	cksumAdj = cv6._payload._tcp6->th_sum;

	th = cv6._payload._tcp6;
	th->th_sum = 0;
	th->th_sum = in6_cksum(cv6.m, IPPROTO_TCP, sizeof(struct ip6_hdr),
			       cv6.m->m_pkthdr.len - sizeof(struct ip6_hdr));

	cksumCks = th->th_sum;
#if	0
	printf("translatingTCPv4To6: TCP4->TCP6: %04x, %04x, %04x %d\n",
	       cksumOrg, cksumAdj, cksumCks, cv6.m->m_pkthdr.len);
#endif
    }
#endif

    return (m6);
}


void
translatingPYLD4To6(struct _cv *cv6)
{
    int			 delta = 0;
    struct tcphdr	*th6 = cv6->_payload._tcp6;
    struct _tcpstate	*ts  = NULL;

    if (cv6->ats && (cv6->ats->session == NATPT_OUTBOUND)
	&& (htons(cv6->_payload._tcp6->th_sport) == FTP_CONTROL))
    {
	if ((delta = translatingFTP4ReplyTo6(cv6)) != 0)
	{
	    struct mbuf		*mbf = cv6->m;
	    struct ip6_hdr	*ip6 = cv6->_ip._ip6;

	    ip6->ip6_plen = htons(ntohs(ip6->ip6_plen) + delta);
	    mbf->m_len += delta;
	    if (mbf->m_flags & M_PKTHDR)
		mbf->m_pkthdr.len += delta;
	}

	if ((cv6->ats == NULL)
	    || ((ts = cv6->ats->suit.tcp) == NULL))
	    return ;

	if (ts->delta[1]
	    && (cv6->inout == NATPT_INBOUND))
	    incrementSeq(th6, ts->delta[1]);

	if (ts->delta[0]
	    && (th6->th_flags & TH_ACK)
	    && (cv6->inout == NATPT_INBOUND))
	    decrementAck(th6, ts->delta[0]);

	if ((delta != 0)
	    && ((th6->th_seq != ts->seq[1])
		|| (th6->th_ack != ts ->ack[1])))
	{
	    ts->delta[1] += delta;
	    ts->seq[1] = th6->th_seq;
	    ts->ack[1] = th6->th_ack;
	}
    }
}


int
translatingFTP4ReplyTo6(struct _cv *cv6)
{
    int			 delta = 0;
    caddr_t		 kb, kk;
    struct ip6_hdr	*ip6 = cv6->_ip._ip6;
    struct tcphdr	*th6 = cv6->_payload._tcp6;
    struct _tSlot	*ats;
    struct _tcpstate	*ts;
    struct sockaddr_in	 sin;
    struct ftpparam	 ftp4;
    char		 Wow[128];

    kb = (caddr_t)th6 + (th6->th_off << 2);
    kk = (caddr_t)ip6 + sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen);
    if (((kk - kb) < FTPMINCMDLEN)
	|| (parseFTPdialogue(kb, kk, &ftp4) == NULL))
	return (0);

    ats = cv6->ats;
    ts  = ats->suit.tcp;
    switch (ts->ftpstate)
    {
      case FTP6_LPRT:
      case FTP6_EPRT:
	{
	    char	*d;

	    if (ftp4.cmd != 200)
		return (0);

	    /* getting:   200 PORT command successful.	*/
	    /* expecting: 200 EPRT command successful.	*/

	    d = ftp4.arg;
	    if ((d[0] == 'P') && (d[1] == 'O'))
	    {
		d[0] = (ts->ftpstate == FTP6_LPRT) ? 'L' : 'E';
		d[1] = 'P';
	    }
	}
	break;

      case FTP6_LPSV:
	{
	    u_char	*h, *p;

	    if (ftp4.cmd != 227)
		return (0);

	    /* getting:   227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).	*/
	    /* expecting: 228 Entering Long Passive Mode(...)			*/

	    if (parse227(ftp4.arg, kk, &sin) == NULL)
		return (0);

	    h = (char *)&ats->local.in6src;
	    p = (char *)&sin.sin_port;
	    snprintf(Wow, sizeof(Wow),
		     "228 Entering Long Passive Mode "
		     "(%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u)\r\n",
		     IPV6_VERSION >> 4, 16,
		     h[0], h[1], h[ 2], h[ 3], h[ 4], h[ 5], h[ 6], h[ 7],
		     h[8], h[9], h[10], h[11], h[12], h[13], h[14], h[15],
		     2, p[0], p[1]);
	    delta = rewriteMbuf(cv6->m, kb, (kk-kb), Wow, strlen(Wow));
	    break;
	 }
	    
      case FTP6_EPSV:
	if (ftp4.cmd != 227)
	    return (0);

	/* getting:   227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).	*/
	/* expecting: 229 Entering Extended Passive Mode (|||6446|)	*/

	if (parse227(ftp4.arg, kk, &sin) == NULL)
	    return (0);
	snprintf(Wow, sizeof(Wow), 
		 "229 Entering Extended Passive Mode (|||%d|)\r\n", ntohs(sin.sin_port));
	delta = rewriteMbuf(cv6->m, kb, (kk-kb), Wow, strlen(Wow));
	break;
    }

    return (delta);
}


struct mbuf *
translatingUDPv4To6(struct _cv *cv4, struct pAddr *pad)
{
    struct _cv		 cv6;
    struct mbuf		*m6;

    bzero(&cv6, sizeof(struct _cv));
    m6 = translatingTCPUDPv4To6(cv4, pad, &cv6);
    cv6._ip._ip6->ip6_nxt = IPPROTO_UDP;
    cv6.ip_p = cv6.ip_payload = IPPROTO_UDP;

    adjustUpperLayerChecksum(IPPROTO_IPV4, IPPROTO_UDP, &cv6, cv4);

    return (m6);
}


struct mbuf *
translatingTCPUDPv4To6(struct _cv *cv4, struct pAddr *pad, struct _cv *cv6)
{
    struct mbuf		*m6;
    struct ip		*ip4;
    struct ip6_hdr	*ip6;
    struct tcp6hdr	*tcp6;

    if (cv4->m->m_flags & M_EXT) /*XXX false assumption on m_ext.ext_siz*/
    {
	if (cv4->plen + sizeof(struct ip6_hdr) > MHLEN)
	{
	    struct mbuf	*m6next;

	    m6next = m_copym(cv4->m, 0, M_COPYALL, M_NOWAIT);
	    ReturnEnobufs(m6next);

	    m6next->m_data += cv4->poff;
	    m6next->m_len  -= cv4->poff;

	    MGETHDR(m6, M_NOWAIT, MT_HEADER);
	    ReturnEnobufs(m6);

	    m6->m_pkthdr.rcvif = NULL;
	    m6->m_next	= m6next;
	    MH_ALIGN(m6, sizeof(struct ip6_hdr));
	    m6->m_len	= sizeof(struct ip6_hdr);
	    m6->m_pkthdr.len = sizeof(struct ip6_hdr) + cv4->plen;
	    ip6 = mtod(m6, struct ip6_hdr *);

	    cv6->m = m6;
	    cv6->_ip._ip6 = mtod(m6, struct ip6_hdr *);
	    cv6->_payload._caddr = m6next->m_data;
	    cv6->plen = cv4->plen;
	    cv6->poff = 0;
	}
	else	/* (sizeof(struct ip6_hdr) + cv4->plen <= MHLEN)	*/
	{
	    caddr_t	tcp4;
	    caddr_t	tcp6;

	    MGETHDR(m6, M_NOWAIT, MT_HEADER);
	    if (m6 == NULL)
	    {
		errno = ENOBUFS;
		return (NULL);
	    }
	    m6->m_pkthdr.rcvif = NULL;

	    ip6 = mtod(m6, struct ip6_hdr *);
	    tcp4 = (caddr_t)cv4->_payload._tcp4;
	    tcp6 = (caddr_t)ip6 + sizeof(struct ip6_hdr);
	    bcopy(tcp4, tcp6, cv4->plen);

	    m6->m_pkthdr.len
		= m6->m_len
		= sizeof(struct ip6_hdr) + cv4->plen;

	    cv6->m = m6;
	    cv6->_ip._ip6 = mtod(m6, struct ip6_hdr *);
	    cv6->_payload._caddr = (caddr_t)cv6->_ip._ip6 + sizeof(struct ip6_hdr);
	    cv6->plen = cv4->plen;
	    cv6->poff = cv6->_payload._caddr - (caddr_t)cv6->_ip._ip6;
	}
    }
    else if (cv4->plen + sizeof(struct ip6_hdr) > MHLEN)
    {
	caddr_t	tcp4;
	caddr_t	tcp6;

	if (sizeof(struct ip6_hdr) + cv4->plen > MCLBYTES)
	    ReturnEnobufs(NULL);
	MGETHDR(m6, M_NOWAIT, MT_HEADER);
	if (m6 && sizeof(struct ip6_hdr) + cv4->plen > MHLEN) {
	    MCLGET(m6, M_NOWAIT);
	    if ((m6->m_flags & M_EXT) == 0) {
		m_freem(m6);
		m6 = NULL;
	    }
	}
	ReturnEnobufs(m6);

	m6->m_pkthdr.rcvif = NULL;
	m6->m_data += 128;	/* make struct ether_header{} space. -- too many?	*/
	m6->m_pkthdr.len = m6->m_len   = sizeof(struct ip6_hdr) + cv4->plen;
	ip6 = mtod(m6, struct ip6_hdr *);

	tcp4 = (caddr_t)cv4->_payload._tcp4;
	tcp6 = (caddr_t)ip6 + sizeof(struct ip6_hdr);
	bcopy(tcp4, tcp6, cv4->plen);

	cv6->m = m6;
	cv6->_ip._ip6 = mtod(m6, struct ip6_hdr *);
	cv6->_payload._caddr = tcp6;
	cv6->plen = cv4->plen;
	cv6->poff = cv6->_payload._caddr - (caddr_t)cv6->_ip._ip6;
    }
    else
    {
	caddr_t	tcp4;
	caddr_t	tcp6;

	if (sizeof(struct ip6_hdr) + cv4->plen > MHLEN) {
	    errno = ENOBUFS;
	    return NULL;
	}
	MGETHDR(m6, M_NOWAIT, MT_HEADER);
	if (m6 == NULL)
	{
	    errno = ENOBUFS;
	    return (NULL);
	}
	m6->m_pkthdr.rcvif = NULL;

	cv6->m = m6;
	ip6 = mtod(m6, struct ip6_hdr *);
	tcp4 = (caddr_t)cv4->_payload._tcp4;
	tcp6 = (caddr_t)ip6 + sizeof(struct ip6_hdr);
	bcopy(tcp4, tcp6, cv4->plen);

	m6->m_pkthdr.len
	    = m6->m_len
	    = sizeof(struct ip6_hdr) + cv4->plen;

	cv6->_ip._ip6 = mtod(m6, struct ip6_hdr *);
	cv6->_payload._caddr = (caddr_t)cv6->_ip._ip6 + sizeof(struct ip6_hdr);
	cv6->plen = cv4->plen;
	cv6->poff = cv6->_payload._caddr - (caddr_t)cv6->_ip._ip6;
    }

    cv6->ats = cv4->ats;
    cv6->inout = cv4->inout;

    ip4 = mtod(cv4->m, struct ip *);
    ip6->ip6_flow = 0;
    ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
    ip6->ip6_vfc |=  IPV6_VERSION;
    ip6->ip6_plen = htons(cv4->plen);
    ip6->ip6_nxt  = IPPROTO_TCP;
    ip6->ip6_hlim = ip4->ip_ttl -1;
    ip6->ip6_src  = pad->in6src;
    ip6->ip6_dst  = pad->in6dst;

    tcp6 = cv6->_payload._tcp6;
    tcp6->th_sport = pad->_sport;
    tcp6->th_dport = pad->_dport;

    return (m6);
}


/*
 *	Translating Form IPv6 To IPv4
 */

struct mbuf *
translatingIPv6To4(struct _cv *cv6, struct pAddr *pad)
{
    struct timeval	 atv;
    struct mbuf		*m4 = NULL;

    if (isDump(D_TRANSLATINGIPV6))
	natpt_logIp6(LOG_DEBUG, cv6->_ip._ip6, NULL);

    microtime(&atv);
    cv6->ats->tstamp = atv.tv_sec;

    switch (cv6->ip_payload)
    {
      case IPPROTO_ICMP:
	m4 = translatingICMPv6To4(cv6, pad);
	break;

      case IPPROTO_TCP:
	m4 = translatingTCPv6To4(cv6, pad);
	break;

      case IPPROTO_UDP:
	m4 = translatingUDPv6To4(cv6, pad);
	break;
    }

    if (m4)
    {
	int		 mlen;
	struct mbuf	*mm;
	struct ip	*ip4;

	ip4 = mtod(m4, struct ip *);
	ip4->ip_sum = 0;			/* Header checksum		*/
	ip4->ip_sum = in_cksum(m4, sizeof(struct ip));
	m4->m_pkthdr.rcvif = cv6->m->m_pkthdr.rcvif;

	for (mlen = 0, mm = m4; mm; mm = mm->m_next)
	{
	    mlen += mm->m_len;
	}

	m4->m_pkthdr.len = mlen;

	if (isDump(D_TRANSLATEDIPV4))
	    natpt_logIp4(LOG_DEBUG, ip4, NULL);
    }

    return (m4);
}


struct mbuf *
translatingICMPv6To4(struct _cv *cv6, struct pAddr *pad)
{
    struct _cv		 cv4;
    struct mbuf		*m4;
    struct ip		*ip4;
    struct ip6_hdr	*ip6;
    struct icmp		*icmp4;
    struct icmp6_hdr	*icmp6;

    ip6 = mtod(cv6->m, struct ip6_hdr *);
    icmp6 = cv6->_payload._icmp6;

    {
	caddr_t		 icmp6end = (caddr_t)ip6 + cv6->m->m_pkthdr.len;
	int		 icmp6len = icmp6end - (caddr_t)cv6->_payload._icmp6;

	if (sizeof(struct ip) + icmp6len > MCLBYTES) {
	    errno = ENOBUFS;
	    return NULL;
	}
	MGETHDR(m4, M_NOWAIT, MT_HEADER);
	if (m4 && sizeof(struct ip) + icmp6len > MHLEN) {
	    MCLGET(m4, M_NOWAIT);
	    if ((m4->m_flags & M_EXT) == 0) {
		m_freem(m4);
		m4 = NULL;
	    }
	}
	if (m4 == NULL)
	{
	    errno = ENOBUFS;
	    return (NULL);
	}
	m4->m_pkthdr.rcvif = NULL;
    }

    cv4.m = m4;
    cv4._ip._ip4 = mtod(m4, struct ip *);
    cv4._payload._caddr = (caddr_t)cv4._ip._ip4 + sizeof(struct ip);

    ip4 = mtod(cv4.m,  struct ip *);
    icmp4 = cv4._payload._icmp4;

    ip4->ip_v	= IPVERSION;		/* IP version				*/
    ip4->ip_hl	= 5;			/* header length (no IPv4 option)	*/
    ip4->ip_tos = 0;			/* Type Of Service			*/
    ip4->ip_len = htons(ip6->ip6_plen);	/* Payload length			*/
    ip4->ip_id	= 0;			/* Identification			*/
    ip4->ip_off = 0;			/* flag and fragment offset		*/
    ip4->ip_ttl = ip6->ip6_hlim - 1;	/* Time To Live				*/
    ip4->ip_p	= cv6->ip_payload;	/* Final Payload			*/
    ip4->ip_src = pad->in4src;		/* source addresss			*/
    ip4->ip_dst = pad->in4dst;		/* destination address			*/

    switch (icmp6->icmp6_type)
    {
      case ICMP6_DST_UNREACH:
	tr_icmp6DstUnreach(cv6, &cv4);
	break;

      case ICMP6_PACKET_TOO_BIG:
	tr_icmp6PacketTooBig(cv6, &cv4);
	break;

      case ICMP6_TIME_EXCEEDED:
	tr_icmp6TimeExceed(cv6, &cv4);
	break;

      case ICMP6_PARAM_PROB:
	tr_icmp6ParamProb(cv6, &cv4);
	break;

      case ICMP6_ECHO_REQUEST:
	tr_icmp6EchoRequest(cv6, &cv4);
	break;

      case ICMP6_ECHO_REPLY:
	tr_icmp6EchoReply(cv6, &cv4);
	break;

      case MLD6_LISTENER_QUERY:
      case MLD6_LISTENER_REPORT:
      case MLD6_LISTENER_DONE:
	m_freem(m4);		/* Single hop message.	Silently drop.	*/
	return (NULL);

      default:
	m_freem(m4);		/* Silently drop.			*/
	return (NULL);
    }

    {
	int		 hlen;
	struct mbuf	*m4  = cv4.m;
	struct ip	*ip4 = cv4._ip._ip4;

	hlen = ip4->ip_hl << 2;
	m4->m_data += hlen;
	m4->m_len  -= hlen;
	icmp4->icmp_cksum = 0;
	icmp4->icmp_cksum = in_cksum(cv4.m, ip4->ip_len - hlen);
	m4->m_data -= hlen;
	m4->m_len  += hlen;
    }

    return (m4);
}


void
tr_icmp6DstUnreach(struct _cv *cv6, struct _cv *cv4)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp4->icmp_type = ICMP_UNREACH;
    icmp4->icmp_code = 0;
    icmp4->icmp_id   = icmp6->icmp6_id;
    icmp4->icmp_seq  = icmp6->icmp6_seq;

    switch (icmp6->icmp6_code)
    {
      case ICMP6_DST_UNREACH_NOROUTE:
	icmp4->icmp_code = ICMP_UNREACH_HOST;
	break;

      case ICMP6_DST_UNREACH_ADMIN:
	icmp4->icmp_code = ICMP_UNREACH_HOST_PROHIB;
	break;

      case ICMP6_DST_UNREACH_NOTNEIGHBOR:
	icmp4->icmp_code = ICMP_UNREACH_SRCFAIL;
	break;

      case ICMP6_DST_UNREACH_ADDR:
	icmp4->icmp_code = ICMP_UNREACH_HOST;
	break;

      case ICMP6_DST_UNREACH_NOPORT:
	icmp4->icmp_code = ICMP_UNREACH_PORT;
	break;
    }
}


void
tr_icmp6PacketTooBig(struct _cv *cv6, struct _cv *cv4)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp4->icmp_type = ICMP_UNREACH;
    icmp4->icmp_code = ICMP_UNREACH_NEEDFRAG;				/* do more	*/
    icmp4->icmp_id   = icmp6->icmp6_id;
    icmp4->icmp_seq  = icmp6->icmp6_seq;
}


void
tr_icmp6TimeExceed(struct _cv *cv6, struct _cv *cv4)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp4->icmp_type = ICMP_TIMXCEED;
    icmp4->icmp_code = icmp6->icmp6_code;		/* code unchanged.	*/
    icmp4->icmp_id   = icmp6->icmp6_id;
    icmp4->icmp_seq  = icmp6->icmp6_seq;
}


void
tr_icmp6ParamProb(struct _cv *cv6, struct _cv *cv4)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp4->icmp_type = ICMP_PARAMPROB;					/* do more	*/
    icmp4->icmp_code = 0;
    icmp4->icmp_id   = icmp6->icmp6_id;
    icmp4->icmp_seq  = icmp6->icmp6_seq;

    if (icmp6->icmp6_code == ICMP6_PARAMPROB_NEXTHEADER)
    {
	icmp4->icmp_type = ICMP_UNREACH;
	icmp4->icmp_code = ICMP_UNREACH_PROTOCOL;
    }
}


void
tr_icmp6EchoRequest(struct _cv *cv6, struct _cv *cv4)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp4->icmp_type = ICMP_ECHO;
    icmp4->icmp_code = 0;
    icmp4->icmp_id   = icmp6->icmp6_id;
    icmp4->icmp_seq  = icmp6->icmp6_seq;

    {
	int	dlen;
	struct ip	*ip4 = cv4->_ip._ip4;
	struct ip6_hdr	*ip6 = cv6->_ip._ip6;
	caddr_t		 icmp6off, icmp4off;
	caddr_t		 icmp6end = (caddr_t)ip6 + cv6->m->m_pkthdr.len;
	int		 icmp6len = icmp6end - (caddr_t)cv6->_payload._icmp6;

	dlen = icmp6len - sizeof(struct icmp6_hdr);
	icmp6off = (caddr_t)(cv6->_payload._icmp6) + sizeof(struct icmp6_hdr);
	icmp4off = (caddr_t)(cv4->_payload._icmp4) + ICMP_MINLEN;
	bcopy(icmp6off, icmp4off, dlen);

	ip4->ip_len = cv4->m->m_len = sizeof(struct ip) + ICMP_MINLEN + dlen;
    }
}


void
tr_icmp6EchoReply(struct _cv *cv6, struct _cv *cv4)
{
    struct icmp		*icmp4 = cv4->_payload._icmp4;
    struct icmp6_hdr	*icmp6 = cv6->_payload._icmp6;

    icmp4->icmp_type = ICMP_ECHOREPLY;
    icmp4->icmp_code = 0;
    icmp4->icmp_id   = icmp6->icmp6_id;
    icmp4->icmp_seq  = icmp6->icmp6_seq;

    {
	int	dlen;
	struct ip	*ip4 = cv4->_ip._ip4;
	struct ip6_hdr	*ip6 = cv6->_ip._ip6;
	caddr_t		 icmp6off, icmp4off;
	caddr_t		 icmp6end = (caddr_t)ip6 + cv6->m->m_pkthdr.len;
	int		 icmp6len = icmp6end - (caddr_t)cv6->_payload._icmp6;

	dlen = icmp6len - sizeof(struct icmp6_hdr);
	icmp6off = (caddr_t)(cv6->_payload._icmp6) + sizeof(struct icmp6_hdr);
	icmp4off = (caddr_t)(cv4->_payload._icmp4) + ICMP_MINLEN;
	bcopy(icmp6off, icmp4off, dlen);

	ip4->ip_len = cv4->m->m_len = sizeof(struct ip) + ICMP_MINLEN + dlen;
    }
}


struct mbuf *
translatingTCPv6To4(struct _cv *cv6, struct pAddr *pad)
{
    int			 cksumOrg;
    struct _cv		 cv4;
    struct mbuf		*m4;

    bzero(&cv4, sizeof(struct _cv));
    m4 = translatingTCPUDPv6To4(cv6, pad, &cv4);
    cv4.ip_p = cv4.ip_payload = IPPROTO_TCP;

    updateTcpStatus(&cv4);
    translatingPYLD6To4(&cv4);
    cksumOrg = ntohs(cv6->_payload._tcp6->th_sum);
    adjustUpperLayerChecksum(IPPROTO_IPV6, IPPROTO_TCP, cv6, &cv4);

#ifdef recalculateTCP4Checksum
    _recalculateTCP4Checksum(&cv4);
#endif

    return (m4);
}


void
translatingPYLD6To4(struct _cv *cv4)
{
    int			 delta = 0;
    struct tcphdr	*th4 = cv4->_payload._tcp4;
    struct _tcpstate	*ts  = NULL;

    if (cv4->ats && (cv4->ats->session == NATPT_OUTBOUND)
	&& (htons(cv4->_payload._tcp4->th_dport) == FTP_CONTROL))
    {
	if ((delta = translatingFTP6CommandTo4(cv4)) != 0)
	{
	    struct mbuf		*mbf = cv4->m;
	    struct ip		*ip4 = cv4->_ip._ip4;

	    ip4->ip_len += delta;
	    mbf->m_len += delta;
	    if (mbf->m_flags & M_PKTHDR)
		mbf->m_pkthdr.len += delta;
	}

	if ((cv4->ats == NULL)
	    || ((ts = cv4->ats->suit.tcp) == NULL))
	    return ;

	if (ts->delta[0]
	    && (cv4->inout == NATPT_OUTBOUND))
	    incrementSeq(th4, ts->delta[0]);

	if (ts->delta[1]
	    && (th4->th_flags & TH_ACK)
	    && (cv4->inout == NATPT_OUTBOUND))
	    decrementAck(th4, ts->delta[1]);

	if ((delta != 0)
	    && ((th4->th_seq != ts->seq[0])
		|| (th4->th_ack != ts->ack[0])))
	{
	    ts->delta[0] += delta;
	    ts->seq[0] = th4->th_seq;
	    ts->ack[0] = th4->th_ack;
	}
    }
}


int
translatingFTP6CommandTo4(struct _cv *cv4)
{
    int			 delta = 0;
    char		*tstr;
    caddr_t		 kb, kk;
    struct ip		*ip4 = cv4->_ip._ip4;
    struct tcphdr	*th4 = cv4->_payload._tcp4;
    struct _tcpstate	*ts;
    struct ftpparam	 ftp6;
    struct sockaddr_in6	 sin6;
    char		 wow[128];

    kb = (caddr_t)th4 + (th4->th_off << 2);
    kk = (caddr_t)ip4 + ip4->ip_len;

    if (((kk - kb) < FTPMINCMDLEN)
	|| (parseFTPdialogue(kb, kk, &ftp6) == NULL))
	return (0);

    ts = cv4->ats->suit.tcp;
    switch(ftp6.cmd)
    {
#ifdef NATPT_NAT
      case FTP4_PORT:
	{
	    u_char		*h, *p;
	    struct _tSlot	*ats;
	    struct pAddr	 local, remote;
	    struct sockaddr_in	sin;

	    ts->ftpstate = FTP4_PORT;
	    if (parsePORT(ftp6.arg, kk, &sin) == NULL)
		return (0);

	    ats = cv4->ats;
	    local = ats->local;
	    local._sport = htons(FTP_DATA);
	    local._dport = sin.sin_port;
	    remote = ats->remote;
	    remote._sport = 0;	/* this port should be remapped	*/
	    remote._dport = htons(FTP_DATA);

	    if (ts->lport == sin.sin_port)	/* This connection opens already. */
	    {
		remote._sport = ts->rport;
	    }
	    else
	    {
		if (remapRemote4Port(ats->csl, NULL, &remote) == NULL)
		    return (0);

		if (openIncomingV4Conn(IPPROTO_TCP, &local, &remote) == NULL)
		    return (0);

		ts->lport = sin.sin_port;
		ts->rport = remote._sport;
	    }

	    h = (char *)&remote.addr[0];
	    p = (char *)&remote.port[0];
	    snprintf(wow, sizeof(wow), "PORT %u,%u,%u,%u,%u,%u\r\n",
		     h[0], h[1], h[2], h[3],
		     p[0], p[1]);

	    delta = rewriteMbuf(cv4->m, kb, (kk-kb), wow, strlen(wow));
	}
	break;
#endif

      case FTP6_LPSV:
	ts->ftpstate = FTP6_LPSV;
	tstr = "PASV\r\n";
	delta = rewriteMbuf(cv4->m, kb, (kk-kb), tstr, strlen(tstr));
	break;

      case FTP6_LPRT:
      case FTP6_EPRT:
	{
	    char		*h, *p;
	    struct _tSlot	*ats;
	    struct pAddr	 local, remote;

	    if (ftp6.cmd == FTP6_LPRT)
	    {
		ts->ftpstate = FTP6_LPRT;
		if (parseLPRT(ftp6.arg, kk, &sin6) == NULL)
		    return (0);
	    }
	    else
	    {
		ts->ftpstate = FTP6_EPRT;
		if (parseEPRT(ftp6.arg, kk, &sin6) == NULL)
		    return (0);
	    }

	    ats = cv4->ats;
	    local = ats->local;
	    local._sport = htons(FTP_DATA);
	    local._dport = sin6.sin6_port;
	    remote = ats->remote;
	    remote._sport = 0;	/* this port should be remapped	*/
	    remote._dport = htons(FTP_DATA);

	    if (remapRemote4Port(ats->csl, NULL, &remote) == NULL)
		return (0);
	    
	    if (openIncomingV4Conn(IPPROTO_TCP, &local, &remote) == NULL)
		return (0);

	    h = (char *)&remote.addr[0];
	    p = (char *)&remote.port[0];
	    snprintf(wow, sizeof(wow), "PORT %u,%u,%u,%u,%u,%u\r\n",
		     h[0], h[1], h[2], h[3],
		     p[0], p[1]);

	    delta = rewriteMbuf(cv4->m, kb, (kk-kb), wow, strlen(wow));
	}
	break;

      case FTP6_EPSV:
	ts->ftpstate = FTP6_EPSV;
	tstr = "PASV\r\n";
	delta = rewriteMbuf(cv4->m, kb, (kk-kb), tstr, strlen(tstr));
	break;
    }

    return (delta);
}


struct mbuf *
translatingUDPv6To4(struct _cv *cv6, struct pAddr *pad)
{
    struct _cv		 cv4;
    struct mbuf		*m4;

    bzero(&cv4, sizeof(struct _cv));
    m4 = translatingTCPUDPv6To4(cv6, pad, &cv4);
    cv4.ip_p = cv4.ip_payload = IPPROTO_UDP;

    adjustUpperLayerChecksum(IPPROTO_IPV6, IPPROTO_UDP, cv6, &cv4);

#if	1
    {
	int		 cksumAdj, cksumCks;
	int		 iphlen;
	struct ip	*ip4 = cv4._ip._ip4;
	struct ip	 save_ip;
	struct udpiphdr	*ui;

	cksumAdj = cv4._payload._tcp4->th_sum;

	ui = mtod(cv4.m, struct udpiphdr *);
	iphlen = ip4->ip_hl << 2;

	save_ip = *cv4._ip._ip4;
	bzero(ui, sizeof(struct ip));
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_len = htons(cv4.m->m_pkthdr.len - iphlen);
	ui->ui_src = save_ip.ip_src;
	ui->ui_dst = save_ip.ip_dst;

	ui->ui_sum = 0;
	ui->ui_sum = in_cksum(cv4.m, cv4.m->m_pkthdr.len);
	*cv4._ip._ip4 = save_ip;

	cksumCks = ui->ui_sum;
#if	0
	printf("translatingUDPv6To4: UDP6->UDP4: %04x, %04x %d\n",
	       cksumAdj, cksumCks, cv4.m->m_pkthdr.len);
#endif
    }
#endif

    return (m4);
}


struct mbuf *
translatingTCPUDPv6To4(struct _cv *cv6, struct pAddr *pad, struct _cv *cv4)
{
    struct mbuf		*m4;
    struct ip		*ip4;
    struct ip6_hdr	*ip6;
    struct tcphdr	*th;

    m4 = m_copym(cv6->m, 0, M_COPYALL, M_NOWAIT);
    ReturnEnobufs(m4);

    m4->m_data += sizeof(struct ip6_hdr) - sizeof(struct ip);
    m4->m_pkthdr.len = m4->m_len = sizeof(struct ip) + cv6->plen;

    cv4->m = m4;
    cv4->plen = cv6->plen;
    cv4->poff = sizeof(struct ip);
    cv4->_ip._ip4 = mtod(m4, struct ip *);
    cv4->_payload._caddr = (caddr_t)cv4->_ip._ip4 + sizeof(struct ip);

    cv4->ats = cv6->ats;
    cv4->inout = cv6->inout;

    ip4 = mtod(m4, struct ip *);
    ip6 = mtod(cv6->m, struct ip6_hdr *);
    ip4->ip_v	= IPVERSION;		/* IP version				*/
    ip4->ip_hl	= 5;			/* header length (no IPv4 option)	*/
    ip4->ip_tos = 0;			/* Type Of Service			*/
    ip4->ip_len = sizeof(struct ip) + ntohs(ip6->ip6_plen);
					/* Payload length			*/
    ip4->ip_id	= 0;			/* Identification			*/
    ip4->ip_off = 0;			/* flag and fragment offset		*/
    ip4->ip_ttl = ip6->ip6_hlim;	/* Time To Live				*/
    ip4->ip_p	= cv6->ip_payload;	/* Final Payload			*/
    ip4->ip_src = pad->in4src;		/* source addresss			*/
    ip4->ip_dst = pad->in4dst;		/* destination address			*/

    th = (struct tcphdr *)(ip4 + 1);
    th->th_sport = pad->_sport;
    th->th_dport = pad->_dport;

    return (m4);
}


struct ftpparam *
parseFTPdialogue(caddr_t kb, caddr_t kk, struct ftpparam *ftp6)
{
    int			 idx;
    union
    {
	char	byte[4];
	u_long	cmd;
    }	u;

    while ((kb < kk) && (*kb == ' '))
	kb++;					/* skip preceding blank	*/

    u.cmd = 0;
    if (isalpha(*kb))
    {
	/* in case FTP command	*/
	for (idx = 0; idx < 4; idx++)
	{
	    if (!isalpha(*kb) && (*kb != ' '))
		return (NULL);

	    u.byte[idx] = islower(*kb) ? toupper(*kb) : *kb;
	    if (isalpha(*kb))
		kb++;
	}
    }
    else if (isdigit(*kb))
    {
	/* in case FTP reply	*/
	for (idx = 0; idx < 3; idx++, kb++)
	{
	    if (!isdigit(*kb))
		return (NULL);

	    u.cmd = u.cmd * 10 + *kb - '0';
	}
    }
    else
	return (NULL);		/* neither ftp command nor ftp reply	*/

    while ((kb < kk) && (*kb == ' '))
	kb++;

    if (kb >= kk)
	return (NULL);		/* no end of line (<CRLF>) found	*/

    bzero(ftp6, sizeof(struct ftpparam));
    ftp6->cmd = u.cmd;
    if ((*kb != '\r') && (*kb != '\n'))
	ftp6->arg = kb;

    return (ftp6);
}


struct sockaddr *
parseLPRT(caddr_t kb, caddr_t kk, struct sockaddr_in6 *sin6)
{
    int			 port, bite;
    int			 hal = 16;
    int			 pal = 2;
    u_char		*d;

    bzero(sin6, sizeof(struct sockaddr_in6));
    sin6->sin6_len = sizeof(struct sockaddr_in6);
    sin6->sin6_family = AF_INET6;

    if (kb + 5 > kk)			return (NULL);	/* 5 for "6,16," */
    if ((kb[0] != '6') || (kb[1] != ',')
	|| (kb[2] != '1') || (kb[3] != '6') || (kb[4] != ','))
	return (NULL);
    kb += 5;

    d = (u_char *)&sin6->sin6_addr;
    for (bite = 0; (kb < kk) && (isdigit(*kb) || (*kb == ',')); kb++)
    {
	if (*kb == ',')
	{
	    *d++ = (bite & 0xff);
	    bite = 0;
	    if (--hal <= 0)
		break;
	}
	else
	    bite = bite * 10 + *kb - '0';
    }

    if (hal != 0)			return (NULL);
    if (kb + 3 > kk)			return (NULL);	/* 3 for ",2," */
    if ((kb[0] != ',') || (kb[1] != '2') || (kb[2] != ','))
	return (NULL);
    kb += 3;

    d = (u_char *)&sin6->sin6_port;
    for (port = 0; (kb < kk) && (isdigit(*kb) || (*kb == ',')); kb++)
    {
	if (*kb == ',')
	{
	    *d++ = (port & 0xff);
	    port = 0;
	    if (--pal <= 0)
		break;
	}
	else
	    port = port * 10 + *kb - '0';
    }

    if (pal != 1)			return (NULL);
    if (port > 0)
	*d = (port & 0xff);

    return ((struct sockaddr *)sin6);
}


struct sockaddr *
parseEPRT(caddr_t kb, caddr_t kk, struct sockaddr_in6 *sin6)
{
    int		port;
    caddr_t	km;

    bzero(sin6, sizeof(struct sockaddr_in6));

    if (*kb++ != '|')			return (NULL);
    switch (*kb++)
    {
      case '1':	sin6->sin6_family = AF_INET;	break;
      case '2': sin6->sin6_family = AF_INET6;	break;
      default:
	return (NULL);
    }
    if (*kb++ != '|')			return (NULL);

    km = kb;
    while ((kb < kk) && (isxdigit(*kb) || (*kb == ':')))
	kb++;
    if (*kb != '|')			return (NULL);
    if (natpt_pton6(km, kb++, &sin6->sin6_addr) == 0)
	return (NULL);

    port = 0;
    while ((kb < kk) && (isdigit(*kb)))
    {
	port = port * 10 + *kb - '0';
	kb++;
    }
    if (*kb != '|')			return (NULL);
    
    sin6->sin6_port = htons(port);
    sin6->sin6_len = sizeof(struct sockaddr_in6);
    return ((struct sockaddr *)sin6);
}


struct sockaddr *
parse227(caddr_t kb, caddr_t kk, struct sockaddr_in *sin)
{
    int				 bite;
    u_int			 byte[6];
    u_short			 inport;
    struct in_addr		 inaddr;

    while ((kb < kk) && (*kb != '(') && !isdigit(*kb))
	kb++;

    if (*kb == '(')
	kb++;

    bite = 0;
    bzero(byte, sizeof(byte));
    while ((kb < kk) && (isdigit(*kb) || (*kb == ',')))
    {
	if (isdigit(*kb))
	    byte[bite] = byte[bite] * 10 + *kb - '0';
	else if (*kb == ',')
	    bite++;
	else
	    return (NULL);

	kb++;
    }

    inaddr.s_addr  = ((byte[0] & 0xff) << 24);
    inaddr.s_addr |= ((byte[1] & 0xff) << 16);
    inaddr.s_addr |= ((byte[2] & 0xff) <<  8);
    inaddr.s_addr |= ((byte[3] & 0xff) <<  0);
    inport = ((byte[4] & 0xff) << 8) | (byte[5] & 0xff);

    bzero(sin, sizeof(struct sockaddr_in));
    sin->sin_family = AF_INET;
    sin->sin_port = htons(inport);
    sin->sin_addr = inaddr;

    return ((struct sockaddr *)sin);
}


int
natpt_pton6(caddr_t kb, caddr_t kk, struct in6_addr *addr6)
{
    int			ch, col, cols;
    u_int		v, val;
    u_char	       *d;
    struct in6_addr	bow;

    if ((*kb == ':') && (*(kb+1) != ':'))
	return (0);

    d = (u_char *)&bow;
    bzero(&bow, sizeof(bow));

    col = cols = val = 0;
    while (kb < kk)
    {
	v = 'z';
	ch = *kb++;
	if (isdigit(ch))
	    v = ch - '0';
	else if (('A' <= ch) && (ch <= 'F'))
	    v = ch - 55;
	else if (('a' <= ch) && (ch <= 'f'))
	    v = ch - 87;
	else
	    ;

	if (v != 'z')
	{
	    val = (val << 4) | v;
	    if (val > 0xffff)
		return (0);
	    col = 0;
	    continue;
	}
	else if	(ch == ':')
	{
	    if (col == 0)
	    {
		*d++ = (u_char)((val >> 8) & 0xff);
		*d++ = (u_char)( val & 0xff);
		val = 0;
		col++;
		continue;
	    }
	    else if (col == 1)
	    {
		/* count number of colon, and advance the address
		 * which begin to write.
		 */
		int	ncol;
		caddr_t	p;

		if (cols > 0)
		    return (0);	/* we've already seen "::".	*/

		for (p = kb, ncol = 0; p < kk; p++)
		    if (*p == ':')
			ncol++;

		d = (u_char *)&bow + (7-ncol)*2;
		col++;
		cols++;
		continue;
	    }
	    else
		return (0);	/* COLON continued more than 3.	*/
	}
	else
	    return (0);	/* illegal character	*/
    }

    if (val > 0)
    {
	*d++ = (u_char)((val >> 8) & 0xff);
	*d++ = (u_char)( val & 0xff);
    }
    *addr6 = bow;
    return (1);
}


int
rewriteMbuf(struct mbuf *m, char *pyld, int pyldlen, char *tstr,int tstrlen)
{
    int		i;
    caddr_t	s, d, roome;

    roome = (caddr_t)m + MSIZE;
    if (m->m_flags & M_EXT)
	roome = m->m_ext.ext_buf + MCLBYTES;

    if ((roome - pyld) < tstrlen)
	return (0xdead);				/* no room in mbuf	*/

    s = tstr;
    d = pyld;
    for (i = 0; i < tstrlen; i++)
	*d++ = *s++;

    return (tstrlen - pyldlen);
}


void
incrementSeq(struct tcphdr *th, int delta)
{
    th->th_seq = htonl(ntohl(th->th_seq) + delta);
}


void
decrementAck(struct tcphdr *th, int delta)
{
    th->th_ack = htonl(ntohl(th->th_ack) - delta);
}


/*
 * Itojun said 'code fragment in "#ifdef recalculateTCP4Checksum"
 * does not make sense to me'.  I agree, but
 * adjustUpperLayerChecksum() cause checksum error sometime but
 * not always, so I left its code.  After I fixed it, this code
 * will become vanish.
 */

static void
_recalculateTCP4Checksum(struct _cv *cv4)
{
    int			 cksumAdj, cksumCks;
    int			 iphlen;
    struct ip		*ip4 = cv4->_ip._ip4;
    struct ip		 save_ip;
    struct tcpiphdr	*ti;

    cksumAdj = cv4->_payload._tcp4->th_sum;

    ti = mtod(cv4->m, struct tcpiphdr *);
    iphlen = ip4->ip_hl << 2;

    save_ip = *cv4->_ip._ip4;
#ifdef ti_next
    ti->ti_next = ti->ti_prev = 0;
    ti->ti_x1 = 0;
#else
    bzero(ti->ti_x1, 9);
#endif
    ti->ti_pr = IPPROTO_TCP;
    ti->ti_len = htons(cv4->m->m_pkthdr.len - iphlen);
    ti->ti_src = save_ip.ip_src;
    ti->ti_dst = save_ip.ip_dst;

    ti->ti_sum = 0;
    ti->ti_sum = in_cksum(cv4->m, cv4->m->m_pkthdr.len);
    *cv4->_ip._ip4 = save_ip;

    cksumCks = ti->ti_sum;
#if	0
    printf("translatingTCPv6To4: TCP6->TCP4: %04x, %04x, %04x %d\n",
	   cksumOrg, cksumAdj, cksumCks, cv4->m->m_pkthdr.len);
#endif
}


static void
_recalculateUDP4Checksum(struct _cv *cv4)
{
    int			 cksumAdj, cksumCks;
    int			 iphlen;
    struct ip		*ip4 = cv4->_ip._ip4;
    struct ip		 save_ip;
    struct udpiphdr	*ui;

    cksumAdj = cv4->_payload._tcp4->th_sum;

    ui = mtod(cv4->m, struct udpiphdr *);
    iphlen = ip4->ip_hl << 2;

    save_ip = *cv4->_ip._ip4;
#ifdef ui_next
    ui->ui_next = ui->ui_prev = 0;
    ui->ui_x1 = 0;
#else
    bzero(ui->ui_x1, 9);
#endif
    ui->ui_pr = IPPROTO_UDP;
    ui->ui_len = htons(cv4->m->m_pkthdr.len - iphlen);
    ui->ui_src = save_ip.ip_src;
    ui->ui_dst = save_ip.ip_dst;

    ui->ui_sum = 0;
    ui->ui_sum = in_cksum(cv4->m, cv4->m->m_pkthdr.len);
    *cv4->_ip._ip4 = save_ip;

    cksumCks = ui->ui_sum;
#if	0
    printf("translatingUDPv6To4: UDP6->UDP4: %04x, %04x %d\n",
	   cksumAdj, cksumCks, cv4->m->m_pkthdr.len);
#endif
}


/*
 *
 */

static int
updateTcpStatus(struct _cv *cv)
{
    struct _tSlot	*ats = cv->ats;
    struct _tcpstate	*ts;

    if (ats->ip_payload != IPPROTO_TCP)
	return (0);							/* XXX	*/

    if ((ts = ats->suit.tcp) == NULL)
    {
	MALLOC(ts, struct _tcpstate *, sizeof(struct _tcpstate), M_NATPT, M_NOWAIT);
	if (ts == NULL)
	{
	    return (0);							/* XXX	*/
	}

	bzero(ts, sizeof(struct _tcpstate));
	
	ts->_state = TCPS_CLOSED;
	ats->suit.tcp = ts;
    }

    ts->_state
	= _natpt_tcpfsm(ats->session, cv->inout, ts->_state, cv->_payload._tcp4->th_flags);

    return (0);
}


static	int
_natpt_tcpfsm(int session, int inout, u_short state, u_char flags)
{
    int		rv;

    if (flags & TH_RST)
	return (TCPS_CLOSED);

    if (session == NATPT_OUTBOUND)
	rv = _natpt_tcpfsmSessOut(inout, state, flags);
    else
	rv = _natpt_tcpfsmSessIn (inout, state, flags);

    if (isDebug(F_TCPFSM))
    {
	int	wow[5];

	wow[0] = session;
	wow[1] = inout;
	wow[2] = state;
	wow[3] = flags;
	wow[4] = rv;
	natpt_log(LOG_TCPFSM, LOG_DEBUG, wow, sizeof(wow));
    }

    return (rv);
}


/*
//##
//#------------------------------------------------------------------------
//#	_natpt_tcpfsmSessOut

	delta(start,		eps)			-> CLOSED
	delta(CLOSED,		TH_SYN & !TH_ACK)	-> SYN_SENT
	delta(SYN_SENT,	     in	TH_SYN &  TH_ACK)	-> SYN_RCVD
	delta(SYN_RCVD,		TH_ACK)			-> ESTABLISHED
	delta(ESTABLISHED,	TH_FIN)			-> FIN_WAIT_1
	delta(FIN_WAIT_1,    in	TH_FIN | TH_ACK)	-> TIME_WAIT
	delta(FIN_WAIT_1,    in	TH_ACK)			-> FIN_WAIT_2
	delta(FIN_WAIT_1,    in	TH_FIN)			-> CLOSING
	delta(FIN_WAIT_2,    in	TH_FIN)			-> TIME_WAIT
	delta(CLOSING,		TH_ACK)			-> TIME_WAIT
	delta(TIME_WAIT,	eps)			-> CLOSED

//#------------------------------------------------------------------------
*/

static	int
_natpt_tcpfsmSessOut(int inout, short state, u_char flags)
{
    int	    rv = state;

    switch (state)
    {
      case TCPS_CLOSED:
	if ((inout == NATPT_OUTBOUND)
	    && (((flags & TH_SYN) != 0)
		&& (flags & TH_ACK) == 0))
	    rv = TCPS_SYN_SENT;
	break;

      case TCPS_SYN_SENT:
	if ((inout == NATPT_INBOUND)
	    && (flags & (TH_SYN | TH_ACK)))
	    rv = TCPS_SYN_RECEIVED;
	break;

      case TCPS_SYN_RECEIVED:
	if ((inout == NATPT_OUTBOUND)
	    && (flags & TH_ACK))
	    rv = TCPS_ESTABLISHED;
	break;

      case TCPS_ESTABLISHED:
	if ((inout == NATPT_OUTBOUND)
	    && (flags & TH_FIN))
	    rv = TCPS_FIN_WAIT_1;
	break;

      case TCPS_FIN_WAIT_1:
	if (inout == NATPT_INBOUND)
	{
	    if (flags & (TH_FIN | TH_ACK))	rv = TCPS_TIME_WAIT;
	    else if (flags & TH_ACK)		rv = TCPS_FIN_WAIT_2;
	    else if (flags & TH_FIN)		rv = TCPS_CLOSING;
	}
	break;

      case TCPS_CLOSING:
	if ((inout == NATPT_OUTBOUND)
	    && (flags & TH_ACK))
	    rv = TCPS_TIME_WAIT;
	break;

      case TCPS_FIN_WAIT_2:
	if ((inout == NATPT_INBOUND)
	    && (flags & TH_FIN))
	    rv = TCPS_TIME_WAIT;
	break;
    }

    return (rv);
}


/*
//##
//#------------------------------------------------------------------------
//#	_natpt_tcpfsmSessIn

	delta(start,		eps)			-> CLOSED
	delta(CLOSED,		TH_SYN & !TH_ACK)	-> SYN_RCVD
	delta(SYN_RCVD,		TH_ACK)			-> ESTABLISHED
	delta(ESTABLISHED,   in	TH_FIN)			-> CLOSE_WAIT
	delta(ESTABLISHED,  out	TH_FIN)			-> FIN_WAIT_1
	delta(CLOSE_WAIT,   out	TH_FIN)			-> LAST_ACK
	delta(FIN_WAIT_1,	TH_FIN & TH_ACK)	-> TIME_WAIT
	delta(FIN_WAIT_1,	TH_FIN)			-> CLOSING
	delta(FIN_WAIT_1,	TH_ACK)			-> FIN_WAIT_2
	delta(CLOSING,		TH_ACK)			-> TIME_WAIT
	delta(LAST_ACK),	TH_ACK)			-> CLOSED
	delta(FIN_WAIT_2,	TH_FIN)			-> TIME_WAIT
	delta(TIME_WAIT,	eps)			-> CLOSED

//#------------------------------------------------------------------------
*/

static	int
_natpt_tcpfsmSessIn(int inout, short state, u_char flags)
{
    int		rv = state;

    switch (state)
    {
      case TCPS_CLOSED:
	if ((inout == NATPT_INBOUND)
	    && (((flags & TH_SYN) != 0)
		&& (flags & TH_ACK) == 0))
	    rv = TCPS_SYN_RECEIVED;
	break;

      case TCPS_SYN_RECEIVED:
	if ((inout == NATPT_INBOUND)
	    && (flags & TH_ACK))
	    rv = TCPS_ESTABLISHED;
	break;

      case TCPS_ESTABLISHED:
	if ((inout == NATPT_INBOUND)
	    && (flags & TH_FIN))
	    rv = TCPS_CLOSE_WAIT;
	if ((inout == NATPT_OUTBOUND)
	    && (flags & TH_FIN))
	    rv = TCPS_FIN_WAIT_1;
	break;

      case TCPS_CLOSE_WAIT:
	if ((inout == NATPT_OUTBOUND)
	    && (flags & TH_FIN))
	    rv = TCPS_LAST_ACK;
	break;

      case TCPS_FIN_WAIT_1:
	if (inout == NATPT_INBOUND)
	{
	    if (flags & (TH_FIN | TH_ACK))	rv = TCPS_TIME_WAIT;
	    else if (flags & TH_FIN)		rv = TCPS_CLOSING;
	    else if (flags & TH_ACK)		rv = TCPS_FIN_WAIT_2;
	}
	break;

      case TCPS_CLOSING:
	if ((inout == NATPT_INBOUND)
	    && (flags & TH_ACK))
	    rv = TCPS_TIME_WAIT;
	break;

      case TCPS_LAST_ACK:
	if ((inout == NATPT_INBOUND)
	    && (flags & TH_ACK))
	    rv = TCPS_CLOSED;
	break;

      case TCPS_FIN_WAIT_2:
	if ((inout == NATPT_INBOUND)
	    && (flags & TH_FIN))
	    rv = TCPS_TIME_WAIT;
	break;
    }

    return (rv);
}


/*
 *
 */

static void
adjustPayloadChecksum(int proto, struct _cv *cv4from, struct _cv *cv4to)
{
    u_short		cksum;
    struct ipovly	ip4from, ip4to;

    bzero(&ip4from, sizeof(ip4from));
    bzero(&ip4to,   sizeof(ip4to));

    ip4from.ih_src = cv4from->_ip._ip4->ip_src;
    ip4from.ih_dst = cv4from->_ip._ip4->ip_dst;
    ip4from.ih_len = htons(cv4from->plen);
    ip4from.ih_pr  = cv4from->ip_p;

    ip4to.ih_src = cv4to->_ip._ip4->ip_src;
    ip4to.ih_dst = cv4to->_ip._ip4->ip_dst;
    ip4to.ih_len = htons(cv4to->plen);
    ip4to.ih_pr  = cv4to->ip_p;

    switch (proto)
    {
      case IPPROTO_TCP:
	cksum = adjustChecksum(ntohs(cv4from->_payload._tcp4->th_sum),
			       (u_char *)&ip4from, sizeof(struct ipovly),
			       (u_char *)&ip4to,   sizeof(struct ipovly));
	break;

      case IPPROTO_UDP:
	cksum = adjustChecksum(ntohs(cv4from->_payload._udp->uh_sum),
			       (u_char *)&ip4from, sizeof(struct ipovly),
			       (u_char *)&ip4to,   sizeof(struct ipovly));
	break;

      default:
	break;
    }

}


static void
adjustUpperLayerChecksum(int header, int proto, struct _cv *cv6, struct _cv *cv4)
{
    u_short		cksum;
    struct ipovly	ip4;
    struct ulc
    {
	struct in6_addr	ulc_src;
	struct in6_addr	ulc_dst;
	u_long		ulc_len;
	u_char		ulc_zero[3];
	u_char		ulc_nxt;
    }			ulc;

    bzero(&ulc, sizeof(struct ulc));
    bzero(&ip4, sizeof(struct ipovly));

    ulc.ulc_src = cv6->_ip._ip6->ip6_src;
    ulc.ulc_dst = cv6->_ip._ip6->ip6_dst;
    ulc.ulc_len = htonl(cv6->plen);
    ulc.ulc_nxt = cv6->ip_p;

    ip4.ih_src = cv4->_ip._ip4->ip_src;
    ip4.ih_dst = cv4->_ip._ip4->ip_dst;
    ip4.ih_len = htons(cv4->plen);
    ip4.ih_pr  = cv4->ip_p;

    switch (proto)
    {
      case IPPROTO_TCP:
	if (header == IPPROTO_IPV6)
	{
	    cksum = adjustChecksum(ntohs(cv6->_payload._tcp6->th_sum),
				   (u_char *)&ulc, sizeof(struct ulc),
				   (u_char *)&ip4, sizeof(struct ipovly));
	    cv4->_payload._tcp4->th_sum = htons(cksum);
	}
	else
	{
	    cksum = adjustChecksum(ntohs(cv4->_payload._tcp4->th_sum),
				   (u_char *)&ip4, sizeof(struct ipovly),
				   (u_char *)&ulc, sizeof(struct ulc));
	    cv6->_payload._tcp6->th_sum = htons(cksum);
	}
	break;

      case IPPROTO_UDP:
	if (header == IPPROTO_IPV6)
	{
	    cksum = adjustChecksum(ntohs(cv6->_payload._udp->uh_sum),
				   (u_char *)&ulc, sizeof(struct ulc),
				   (u_char *)&ip4, sizeof(struct ipovly));
	    cv4->_payload._udp->uh_sum = htons(cksum);
	}
	else
	{
	    cksum = adjustChecksum(ntohs(cv4->_payload._udp->uh_sum),
				   (u_char *)&ip4, sizeof(struct ipovly),
				   (u_char *)&ulc, sizeof(struct ulc));
	    cv6->_payload._udp->uh_sum = htons(cksum);
	}
	break;

      default:
    }
}


static int
adjustChecksum(int cksum, u_char *optr, int olen, u_char *nptr, int nlen)
{
    long	x, old, new;

    x = ~cksum & 0xffff;

    while (olen)
    {
	if (olen == 1)
	{
	    old = optr[0] * 256 + optr[1];
	    x -= old & 0xff00;
	    if ( x <= 0 ) { x--; x &= 0xffff; }
	    break;
	}	
	else
	{
	    old = optr[0] * 256 + optr[1];
	    x -= old & 0xffff;
	    if ( x <= 0 ) { x--; x &= 0xffff; }
	    optr += 2;
	    olen -= 2;
	}
    }

    while (nlen)
    {
	if (nlen == 1)	
	{
	    new = nptr[0] * 256 + nptr[1];
	    x += new & 0xff00;
	    if (x & 0x10000) { x++; x &= 0xffff; }
	    break;
	}
	else
	{
	    new = nptr[0] * 256 + nptr[1];
	    x += new & 0xffff;
	    if (x & 0x10000) { x++; x &= 0xffff; }
	    nptr += 2;
	    nlen -= 2;
	}
    }

    return (~x & 0xffff);
}
