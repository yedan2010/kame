/*	$KAME: isakmp_quick.c,v 1.59 2000/09/22 08:47:47 itojun Exp $	*/

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
/* YIPS @(#)$Id: isakmp_quick.c,v 1.59 2000/09/22 08:47:47 itojun Exp $ */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netkey/key_var.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef IPV6_INRIA_VERSION
#include <netinet/ipsec.h>
#else
#include <netinet6/ipsec.h>
#endif

#include "var.h"
#include "vmbuf.h"
#include "schedule.h"
#include "misc.h"
#include "plog.h"
#include "debug.h"

#include "localconf.h"
#include "remoteconf.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "isakmp_inf.h"
#include "oakley.h"
#include "handler.h"
#include "ipsec_doi.h"
#include "crypto_openssl.h"
#include "pfkey.h"
#include "policy.h"
#include "algorithm.h"
#include "sockmisc.h"
#include "proposal.h"
#include "sainfo.h"
#include "admin.h"
#include "strnames.h"

/* quick mode */
static vchar_t *quick_ir1sendmx __P((struct ph2handle *, vchar_t *));
static int get_sainfo_r __P((struct ph2handle *));
static int get_proposal_r __P((struct ph2handle *));

/* %%%
 * Quick Mode
 */
/*
 * begin Quick Mode as initiator.  send pfkey getspi message to kernel.
 */
int
quick_i1prep(iph2, msg)
	struct ph2handle *iph2;
	vchar_t *msg; /* must be null pointer */
{
	int error = ISAKMP_INTERNAL_ERROR;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_STATUS2) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	iph2->msgid = isakmp_newmsgid2(iph2->ph1);
	iph2->ivm = oakley_newiv2(iph2->ph1, iph2->msgid);
	if (iph2->ivm == NULL)
		return NULL;

	iph2->status = PHASE2ST_GETSPISENT;

	/* don't anything if local test mode. */
	if (f_local) {
		error = 0;
		goto end;
	}

	/* send getspi message */
	if (pk_sendgetspi(iph2) < 0)
		goto end;

	YIPSDEBUG(DEBUG_STAMP,
		plog(logp, LOCATION, NULL, "pfkey getspi sent.\n"));

	iph2->sce = sched_new(lcconf->wait_ph2complete,
				pfkey_timeover, iph2);

	error = 0;

end:
	return error;
}

/*
 * send to responder
 * 	HDR*, HASH(1), SA, Ni [, KE ] [, IDi2, IDr2 ]
 */
int
quick_i1send(iph2, msg)
	struct ph2handle *iph2;
	vchar_t *msg; /* must be null pointer */
{
	vchar_t *body = NULL;
	struct isakmp_gen *gen;
	char *p;
	int tlen;
	int error = ISAKMP_INTERNAL_ERROR;
	int pfsgroup, idci, idcr;
	int np;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_GETSPIDONE) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	/* create SA payload for my proposal */
	if (ipsecdoi_setph2proposal(iph2) < 0)
		goto end;

	/* generate NONCE value */
	iph2->nonce = eay_set_random(iph2->ph1->rmconf->nonce_size);
	if (iph2->nonce == NULL)
		goto end;

	/*
	 * DH value calculation is kicked out into cfparse.y.
	 * because pfs group can not be negotiated, it's only to be checked
	 * acceptable.
	 */
	/* generate KE value if need */
	pfsgroup = iph2->proposal->pfs_group;
	if (pfsgroup) {
		/* DH group settting if PFS is required. */
		if (oakley_setdhgroup(pfsgroup, &iph2->pfsgrp) < 0) {
			plog(logp, LOCATION, NULL,
				"failed to set DH value.\n");
			goto end;
		}
		if (oakley_dh_generate(iph2->pfsgrp,
				&iph2->dhpub, &iph2->dhpriv) < 0) {
			goto end;
		}
	}

	/* generate ID value */
	if (ipsecdoi_setid2(iph2) < 0) {
		plog(logp, LOCATION, NULL,
			"failed to get ID.\n");
		goto end;
	}
	YIPSDEBUG(DEBUG_KEY,
		plog(logp, LOCATION, NULL, "IDci:");
		PVDUMP(iph2->id););
	YIPSDEBUG(DEBUG_KEY,
		plog(logp, LOCATION, NULL, "IDcr:");
		PVDUMP(iph2->id_p););

	/* XXX we may want to skip it for transport mode */
	idci = idcr = 1;

	/* create SA;NONCE payload, and KE if need, and IDii, IDir. */
	tlen = + sizeof(*gen) + iph2->sa->l
		+ sizeof(*gen) + iph2->nonce->l;
	if (pfsgroup)
		tlen += (sizeof(*gen) + iph2->dhpub->l);
	if (idci)
		tlen += sizeof(*gen) + iph2->id->l;
	if (idcr)
		tlen += sizeof(*gen) + iph2->id_p->l;

	body = vmalloc(tlen);
	if (body == NULL) {
		plog(logp, LOCATION, NULL,
			"failed to get buffer to send.\n");
		goto end;
	}

	p = body->v;

	/* add SA payload */
	p = set_isakmp_payload(p, iph2->sa, ISAKMP_NPTYPE_NONCE);

	/* add NONCE payload */
	if (pfsgroup)
		np = ISAKMP_NPTYPE_KE;
	else if (idci || idcr)
		np = ISAKMP_NPTYPE_ID;
	else
		np = ISAKMP_NPTYPE_NONE;
	p = set_isakmp_payload(p, iph2->nonce, np);

	/* add KE payload if need. */
	np = (idci || idcr) ? ISAKMP_NPTYPE_ID : ISAKMP_NPTYPE_NONE;
	if (pfsgroup)
		p = set_isakmp_payload(p, iph2->dhpub, np);

	/* IDci */
	np = (idcr) ? ISAKMP_NPTYPE_ID : ISAKMP_NPTYPE_NONE;
	if (iph2->id)
		p = set_isakmp_payload(p, iph2->id, np);

	/* IDcr */
	if (iph2->id_p)
		p = set_isakmp_payload(p, iph2->id_p, ISAKMP_NPTYPE_NONE);

	/* generate HASH(1) */
	iph2->hash = oakley_compute_hash1(iph2->ph1, iph2->msgid, body);
	if (iph2->hash == NULL)
		goto end;

	/* send isakmp payload */
	iph2->sendbuf = quick_ir1sendmx(iph2, body);
	if (iph2->sendbuf == NULL)
		goto end;

	/* change status of isakmp status entry */
	iph2->status = PHASE2ST_MSG1SENT;

	/* add to the schedule to resend */
	iph2->retry_counter = iph2->ph1->rmconf->retry_counter;
	iph2->scr = sched_new(iph2->ph1->rmconf->retry_interval,
				isakmp_ph2resend, iph2);

	error = 0;

end:
	if (body != NULL)
		vfree(body);

	return error;
}

/*
 * receive from responder
 * 	HDR*, HASH(2), SA, Nr [, KE ] [, IDi2, IDr2 ]
 */
int
quick_i2recv(iph2, msg0)
	struct ph2handle *iph2;
	vchar_t *msg0;
{
	vchar_t *msg = NULL;
	vchar_t *hbuf = NULL;	/* for hash computing. */
	vchar_t *pbuf = NULL;	/* for payload parsing */
	struct isakmp_parse_t *pa;
	struct isakmp *isakmp = (struct isakmp *)msg0->v;
	struct isakmp_pl_hash *hash = NULL;
	int f_id;
	char *p;
	int tlen;
	int error = ISAKMP_INTERNAL_ERROR;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_MSG1SENT) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	/* decrypt packet */
	if (!ISSET(((struct isakmp *)msg0->v)->flags, ISAKMP_FLAG_E)) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"Packet wasn't encrypted.\n");
		goto end;
	}
	msg = oakley_do_decrypt(iph2->ph1, msg0, iph2->ivm->iv, iph2->ivm->ive);
	if (msg == NULL)
		goto end;

	/* create buffer for validating HASH(2) */
	/*
	 * ordering rule:
	 *	1. the first one must be HASH
	 *	2. the second one must be SA (added in isakmp-oakley-05!)
	 *	3. two IDs must be considered as IDci, then IDcr
	 */
	pbuf = isakmp_parse(msg);
	if (pbuf == NULL)
		goto end;
	pa = (struct isakmp_parse_t *)pbuf->v;

	/* HASH payload is fixed postion */
	if (pa->type != ISAKMP_NPTYPE_HASH) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"received invalid next payload type %d, "
			"expecting %d.\n",
			pa->type, ISAKMP_NPTYPE_HASH);
		goto end;
	}
	hash = (struct isakmp_pl_hash *)pa->ptr;
	pa++;

#if 0
	/*
	 * this restriction was introduced in isakmp-oakley-05.
	 * we do not check this for backward compatibility.
	 * TODO: command line/config file option to enable/disable this code
	 */
	/* HASH payload is fixed postion */
	if (pa->type != ISAKMP_NPTYPE_SA) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"received invalid next payload type %d, "
			"expecting %d.\n",
			pa->type, ISAKMP_NPTYPE_HASH);
		goto end;
	}
#endif

	/* allocate buffer for computing HASH(2) */
	tlen = iph2->nonce->l
		+ ntohl(isakmp->len) - sizeof(*isakmp);
	hbuf = vmalloc(tlen);
	if (hbuf == NULL) {
		plog(logp, LOCATION, NULL,
			"failed to get hash buffer.\n");
		goto end;
	}
	p = hbuf->v + iph2->nonce->l;	/* retain the space for Ni_b */

	/*
	 * parse the payloads.
	 * copy non-HASH payloads into hbuf, so that we can validate HASH.
	 */
	iph2->sa_ret = NULL;
	f_id = 0;	/* flag to use checking ID */
	tlen = 0;	/* count payload length except of HASH payload. */
	for (; pa->type; pa++) {

		/* copy to buffer for HASH */
		/* Don't modify the payload */
		memcpy(p, pa->ptr, pa->len);

		switch (pa->type) {
		case ISAKMP_NPTYPE_SA:
			if (iph2->sa_ret != NULL) {
				plog(logp, LOCATION, NULL,
					"Ignored, multiple SA "
					"isn't supported.\n");
				break;
			}
			if (isakmp_p2ph(&iph2->sa_ret, pa->ptr) < 0)
				goto end;
			break;

		case ISAKMP_NPTYPE_NONCE:
			if (isakmp_p2ph(&iph2->nonce_p, pa->ptr) < 0)
				goto end;
			break;

		case ISAKMP_NPTYPE_KE:
			if (isakmp_p2ph(&iph2->dhpub_p, pa->ptr) < 0)
				goto end;
			break;

		case ISAKMP_NPTYPE_ID:
		    {
			vchar_t *vp;

			/* check ID value */
			if (f_id == 0) {
				/* for IDci */
				f_id = 1;
				vp = iph2->id;
			} else {
				/* for IDcr */
				vp = iph2->id_p;
			}

			if (memcmp(vp->v, (caddr_t)pa->ptr + sizeof(struct isakmp_gen), vp->l)) {

				plog(logp, LOCATION, NULL,
					"mismatched ID was returned.\n");
				error = ISAKMP_NTYPE_ATTRIBUTES_NOT_SUPPORTED;
				goto end;
			}
		    }
			break;

		case ISAKMP_NPTYPE_N:
			plog(logp, LOCATION, iph2->ph1->remote,
				"peer transmitted Notify Message.\n");
			isakmp_check_notify(pa->ptr, iph2->ph1);
			break;

		default:
			/* don't send information, see ident_r1recv() */
			plog(logp, LOCATION, iph2->ph1->remote,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			goto end;
		}

		p += pa->len;

		/* compute true length of payload. */
		tlen += pa->len;
	}

	/* payload existency check */
	if (hash == NULL || iph2->sa_ret == NULL || iph2->nonce_p == NULL) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"few isakmp message received.\n");
		goto end;
	}

	/* Fixed buffer for calculating HASH */
	memcpy(hbuf->v, iph2->nonce->v, iph2->nonce->l);
	YIPSDEBUG(DEBUG_KEY,
		plog(logp, LOCATION, NULL,
			"HASH allocated:hbuf->l=%d actual:tlen=%d\n",
			hbuf->l, tlen + iph2->nonce->l));
	/* adjust buffer length for HASH */
	hbuf->l = iph2->nonce->l + tlen;

	/* validate HASH(2) */
    {
	char *r_hash;
	vchar_t *my_hash = NULL;
	int result;

	r_hash = (char *)hash + sizeof(*hash);

	YIPSDEBUG(DEBUG_KEY, plog(logp, LOCATION, NULL, "HASH(2) received:"));
	YIPSDEBUG(DEBUG_DKEY,
		hexdump(r_hash, ntohs(hash->h.len) - sizeof(*hash)));

	my_hash = oakley_compute_hash1(iph2->ph1, iph2->msgid, hbuf);
	if (my_hash == NULL)
		goto end;

	result = memcmp(my_hash->v, r_hash, my_hash->l);
	vfree(my_hash);

	if (result) {
		plog(logp, LOCATION, iph2->ph1->remote, "HASH(2) mismatch.\n");
		error = ISAKMP_NTYPE_INVALID_HASH_INFORMATION;
		goto end;
	}
    }

	/* validity check SA payload sent from responder */
	if (ipsecdoi_checkph2proposal(iph2) < 0) {
		error = ISAKMP_NTYPE_NO_PROPOSAL_CHOSEN;
		goto end;
	}

	/* change status of isakmp status entry */
	iph2->status = PHASE2ST_STATUS6;

	error = 0;

end:
	if (hbuf)
		vfree(hbuf);
	if (pbuf)
		vfree(pbuf);
	if (msg)
		vfree(msg);

	if (error) {
		VPTRINIT(iph2->sa_ret);
		VPTRINIT(iph2->nonce_p);
		VPTRINIT(iph2->dhpub_p);
		VPTRINIT(iph2->id);
		VPTRINIT(iph2->id_p);
	}

	return error;
}

/*
 * send to responder
 * 	HDR*, HASH(3)
 */
int
quick_i2send(iph2, msg0)
	struct ph2handle *iph2;
	vchar_t *msg0;
{
	vchar_t *msg = NULL;
	vchar_t *buf = NULL;
	char *p = NULL;
	int tlen;
	int error = ISAKMP_INTERNAL_ERROR;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_STATUS6) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	/* generate HASH(3) */
    {
	vchar_t *tmp = NULL;

	YIPSDEBUG(DEBUG_KEY, plog(logp, LOCATION, NULL, "HASH(3) generate\n"));

	tmp = vmalloc(iph2->nonce->l + iph2->nonce_p->l);
	if (tmp == NULL) { 
		plog(logp, LOCATION, NULL,
			"failed to get hash buffer.\n");
		goto end;
	}
	memcpy(tmp->v, iph2->nonce->v, iph2->nonce->l);
	memcpy(tmp->v + iph2->nonce->l, iph2->nonce_p->v, iph2->nonce_p->l);

	iph2->hash = oakley_compute_hash3(iph2->ph1, iph2->msgid, tmp);
	vfree(tmp);

	if (iph2->hash == NULL)
		goto end;
    }

	/* create buffer for isakmp payload */
	tlen = sizeof(struct isakmp)
		+ sizeof(struct isakmp_gen) + iph2->hash->l;
	buf = vmalloc(tlen);
	if (buf == NULL) { 
		plog(logp, LOCATION, NULL,
			"failed to get buffer to send.\n");
		goto end;
	}

	/* create isakmp header */
	p = set_isakmp_header2(buf, iph2, ISAKMP_NPTYPE_HASH);
	if (p == NULL)
		goto end;

	/* add HASH(3) payload */
	p = set_isakmp_payload(p, iph2->hash, ISAKMP_NPTYPE_NONE);

#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(buf, iph2->ph1->local, iph2->ph1->remote, 1);
#endif

	/* encoding */
	iph2->sendbuf = oakley_do_encrypt(iph2->ph1, buf, iph2->ivm->ive, iph2->ivm->iv);
	if (iph2->sendbuf == NULL)
		goto end;

	/* send HDR*;HASH(3) */
	if (isakmp_send(iph2->ph1, iph2->sendbuf) < 0)
		goto end;

	/* XXX: How resend ? */

	/* compute both of KEYMATs */
	if (oakley_compute_keymat(iph2, INITIATOR) < 0)
		goto end;

	iph2->status = PHASE2ST_ADDSA;

	/* don't anything if local test mode. */
	if (f_local) {
		error = 0;
		goto end;
	}

	/* if there is commit bit don't set up SA now. */
	if (ISSET(iph2->flags, ISAKMP_FLAG_C)) {
		iph2->status = PHASE2ST_COMMIT;
		error = 0;
		goto end;
	}

	/* Do UPDATE for initiator */
	YIPSDEBUG(DEBUG_PFKEY, plog(logp, LOCATION, NULL,
		"call pk_sendupdate\n"););
	if (pk_sendupdate(iph2) < 0) {
		plog(logp, LOCATION, NULL, "pfkey update failed.\n");
		goto end;
	}
	YIPSDEBUG(DEBUG_STAMP,
		plog(logp, LOCATION, NULL, "pfkey update sent.\n"));

	/* Do ADD for responder */
	if (pk_sendadd(iph2) < 0) {
		plog(logp, LOCATION, NULL, "pfkey add failed.\n");
		goto end;
	}
	YIPSDEBUG(DEBUG_STAMP,
		plog(logp, LOCATION, NULL, "pfkey add sent.\n"));

	error = 0;

end:
	if (buf != NULL)
		vfree(buf);
	if (msg != NULL)
		vfree(msg);

	return error;
}

/*
 * receive from responder
 * 	HDR#*, HASH(4), notify
 */
int
quick_i3recv(iph2, msg0)
	struct ph2handle *iph2;
	vchar_t *msg0;
{
	vchar_t *msg = NULL;
	vchar_t *pbuf = NULL;	/* for payload parsing */
	struct isakmp_parse_t *pa;
	struct isakmp_pl_hash *hash = NULL;
	vchar_t *notify = NULL;
	int error = ISAKMP_INTERNAL_ERROR;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_COMMIT) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	/* decrypt packet */
	if (!ISSET(((struct isakmp *)msg0->v)->flags, ISAKMP_FLAG_E)) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"Packet wasn't encrypted.\n");
		goto end;
	}
	msg = oakley_do_decrypt(iph2->ph1, msg0, iph2->ivm->iv, iph2->ivm->ive);
	if (msg == NULL)
		goto end;

	/* validate the type of next payload */
	pbuf = isakmp_parse(msg);
	if (pbuf == NULL)
		goto end;

	for (pa = (struct isakmp_parse_t *)pbuf->v;
	     pa->type != ISAKMP_NPTYPE_NONE;
	     pa++) {

		switch (pa->type) {
		case ISAKMP_NPTYPE_HASH:
			hash = (struct isakmp_pl_hash *)pa->ptr;
			break;
		case ISAKMP_NPTYPE_N:
			isakmp_check_notify(pa->ptr, iph2->ph1);
			notify = vmalloc(pa->len);
			if (notify == NULL) {
				plog(logp, LOCATION, NULL,
					"failed to get notify buffer.\n");
				goto end;
			}
			memcpy(notify->v, pa->ptr, notify->l);
			break;
		default:
			/* don't send information, see ident_r1recv() */
			plog(logp, LOCATION, iph2->ph1->remote,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			goto end;
		}
	}

	/* payload existency check */
	if (hash == NULL) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"few isakmp message received.\n");
		goto end;
	}

	/* validate HASH(4) */
    {
	char *r_hash;
	vchar_t *my_hash = NULL;
	vchar_t *tmp = NULL;
	int result;

	r_hash = (char *)hash + sizeof(*hash);

	YIPSDEBUG(DEBUG_KEY, plog(logp, LOCATION, NULL, "HASH(4) validate:"));
	YIPSDEBUG(DEBUG_DKEY,
		hexdump(r_hash, ntohs(hash->h.len) - sizeof(*hash)));

	my_hash = oakley_compute_hash1(iph2->ph1, iph2->msgid, notify);
	vfree(tmp);
	if (my_hash == NULL)
		goto end;

	result = memcmp(my_hash->v, r_hash, my_hash->l);
	vfree(my_hash);

	if (result) {
		plog(logp, LOCATION, iph2->ph1->remote, "HASH(4) mismatch.\n");
		error = ISAKMP_NTYPE_INVALID_HASH_INFORMATION;
		goto end;
	}
    }

	iph2->status = PHASE2ST_ADDSA;
	iph2->flags ^= ISAKMP_FLAG_C;	/* reset bit */

	/* don't anything if local test mode. */
	if (f_local) {
		error = 0;
		goto end;
	}

	/* Do UPDATE for initiator */
	YIPSDEBUG(DEBUG_PFKEY, plog(logp, LOCATION, NULL,
		"call pk_sendupdate\n"););
	if (pk_sendupdate(iph2) < 0) {
		plog(logp, LOCATION, NULL, "pfkey update failed.\n");
		goto end;
	}
	YIPSDEBUG(DEBUG_STAMP,
		plog(logp, LOCATION, NULL, "pfkey update sent.\n"));

	/* Do ADD for responder */
	if (pk_sendadd(iph2) < 0) {
		plog(logp, LOCATION, NULL, "pfkey add failed.\n");
		goto end;
	}
	YIPSDEBUG(DEBUG_STAMP,
		plog(logp, LOCATION, NULL, "pfkey add sent.\n"));

	error = 0;

end:
	if (msg != NULL)
		vfree(msg);
	if (pbuf != NULL)
		vfree(pbuf);
	if (notify != NULL)
		vfree(notify);

	return error;
}

/*
 * receive from initiator
 * 	HDR*, HASH(1), SA, Ni [, KE ] [, IDi2, IDr2 ]
 */
int
quick_r1recv(iph2, msg0)
	struct ph2handle *iph2;
	vchar_t *msg0;
{
	vchar_t *msg = NULL;
	vchar_t *hbuf = NULL;	/* for hash computing. */
	vchar_t *pbuf = NULL;	/* for payload parsing */
	struct isakmp_parse_t *pa;
	struct isakmp *isakmp = (struct isakmp *)msg0->v;
	struct isakmp_pl_hash *hash = NULL;
	char *p;
	int tlen;
	int f_id_order;	/* for ID payload detection */
	int error = ISAKMP_INTERNAL_ERROR;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_START) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	/* decrypting */
	if (!ISSET(((struct isakmp *)msg0->v)->flags, ISAKMP_FLAG_E)) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"Packet wasn't encrypted.\n");
		error = ISAKMP_NTYPE_PAYLOAD_MALFORMED;
		goto end;
	}
	/* decrypt packet */
	msg = oakley_do_decrypt(iph2->ph1, msg0, iph2->ivm->iv, iph2->ivm->ive);
	if (msg == NULL)
		goto end;

	/* create buffer for using to validate HASH(1) */
	/*
	 * ordering rule:
	 *	1. the first one must be HASH
	 *	2. the second one must be SA (added in isakmp-oakley-05!)
	 *	3. two IDs must be considered as IDci, then IDcr
	 */
	pbuf = isakmp_parse(msg);
	if (pbuf == NULL)
		goto end;
	pa = (struct isakmp_parse_t *)pbuf->v;

	/* HASH payload is fixed postion */
	if (pa->type != ISAKMP_NPTYPE_HASH) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"received invalid next payload type %d, "
			"expecting %d.\n",
			pa->type, ISAKMP_NPTYPE_HASH);
		error = ISAKMP_NTYPE_BAD_PROPOSAL_SYNTAX;
		goto end;
	}
	hash = (struct isakmp_pl_hash *)pa->ptr;
	pa++;

#if 0
	/*
	 * this restriction was introduced in isakmp-oakley-05.
	 * we do not check this for backward compatibility.
	 * TODO: command line/config file option to enable/disable this code
	 */
	/* HASH payload is fixed postion */
	if (pa->type != ISAKMP_NPTYPE_SA) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"received invalid next payload type %d, "
			"expecting %d.\n",
			pa->type, ISAKMP_NPTYPE_HASH);
		error = ISAKMP_NTYPE_BAD_PROPOSAL_SYNTAX;
		goto end;
	}
#endif

	/* allocate buffer for computing HASH(1) */
	tlen = ntohl(isakmp->len) - sizeof(*isakmp);
	hbuf = vmalloc(tlen);
	if (hbuf == NULL) {
		plog(logp, LOCATION, NULL,
			"failed to get hash buffer.\n");
		goto end;
	}
	p = hbuf->v;

	/*
	 * parse the payloads.
	 * copy non-HASH payloads into hbuf, so that we can validate HASH.
	 */
	iph2->sa = NULL;	/* we don't support multi SAs. */
	iph2->nonce_p = NULL;
	iph2->dhpub_p = NULL;
	iph2->id_p = NULL;
	iph2->id = NULL;
	tlen = 0;	/* count payload length except of HASH payload. */

	/*
	 * IDi2 MUST be immediatelly followed by IDr2.  We allowed the
	 * illegal case, but logged.  First ID payload is to be IDi2.
	 * And next ID payload is to be IDr2.
	 */
	f_id_order = 0;

	for (; pa->type; pa++) {

		/* copy to buffer for HASH */
		/* Don't modify the payload */
		memcpy(p, pa->ptr, pa->len);

		if (pa->type != ISAKMP_NPTYPE_ID)
			f_id_order = 0;

		switch (pa->type) {
		case ISAKMP_NPTYPE_SA:
			if (iph2->sa != NULL) {
				plog(logp, LOCATION, NULL,
					"Multi SAs isn't supported.\n");
				goto end;
			}
			if (isakmp_p2ph(&iph2->sa, pa->ptr) < 0)
				goto end;
			break;

		case ISAKMP_NPTYPE_NONCE:
			if (isakmp_p2ph(&iph2->nonce_p, pa->ptr) < 0)
				goto end;
			break;

		case ISAKMP_NPTYPE_KE:
			if (isakmp_p2ph(&iph2->dhpub_p, pa->ptr) < 0)
				goto end;
			break;

		case ISAKMP_NPTYPE_ID:
			if (iph2->id_p == NULL) {
				/* for IDci */
				f_id_order++;

				if (isakmp_p2ph(&iph2->id_p, pa->ptr) < 0)
					goto end;
				YIPSDEBUG(DEBUG_KEY,
					plog(logp, LOCATION, NULL,
						"received IDci:");
					PVDUMP(iph2->id_p));

			} else if (iph2->id == NULL) {
				/* for IDcr */
				if (f_id_order == 0) {
					plog(logp, LOCATION, NULL,
						"IDr2 payload is not "
						"immediatelly followed "
						"by IDi2. We allowed.\n");
					/* XXX we allowed in this case. */
				}

				if (isakmp_p2ph(&iph2->id, pa->ptr) < 0)
					goto end;
				YIPSDEBUG(DEBUG_KEY,
					plog(logp, LOCATION, NULL,
						"received IDci:");
					PVDUMP(iph2->id));
			} else {
				YIPSDEBUG(DEBUG_KEY,
					plog(logp, LOCATION, NULL,
						"received too many ID payloads.\n");
					PVDUMP(iph2->id));
				error = ISAKMP_NTYPE_INVALID_ID_INFORMATION;
				goto end;
			}
			break;

		case ISAKMP_NPTYPE_N:
			plog(logp, LOCATION, iph2->ph1->remote,
				"peer transmitted Notify Message.\n");
			isakmp_check_notify(pa->ptr, iph2->ph1);
			break;

		default:
			plog(logp, LOCATION, iph2->ph1->remote,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			error = ISAKMP_NTYPE_PAYLOAD_MALFORMED;
			goto end;
		}

		p += pa->len;

		/* compute true length of payload. */
		tlen += pa->len;
	}

	/* payload existency check */
	if (hash == NULL || iph2->sa == NULL || iph2->nonce_p == NULL) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"few isakmp message received.\n");
		error = ISAKMP_NTYPE_PAYLOAD_MALFORMED;
		goto end;
	}

	/* adjust buffer length for HASH */
	hbuf->l = tlen;

	/* validate HASH(1) */
    {
	char *r_hash;
	vchar_t *my_hash = NULL;
	int result;

	r_hash = (caddr_t)hash + sizeof(*hash);

	YIPSDEBUG(DEBUG_KEY, plog(logp, LOCATION, NULL, "HASH(1) validate:"));
	YIPSDEBUG(DEBUG_DKEY,
		hexdump(r_hash, ntohs(hash->h.len) - sizeof(*hash)));

	my_hash = oakley_compute_hash1(iph2->ph1, iph2->msgid, hbuf);
	if (my_hash == NULL)
		goto end;

	result = memcmp(my_hash->v, r_hash, my_hash->l);
	vfree(my_hash);

	if (result) {
		plog(logp, LOCATION, iph2->ph1->remote, "HASH(1) mismatch.\n");
		error = ISAKMP_NTYPE_INVALID_HASH_INFORMATION;
		goto end;
	}
    }

	/* get sainfo */
	if (get_sainfo_r(iph2) != 0) {
		plog(logp, LOCATION, NULL,
			"failed to get sainfo.\n");
		error = ISAKMP_INTERNAL_ERROR;
		goto end;
	}

	/* check the existence of ID payload and create responder's proposal */
	if (get_proposal_r(iph2) < 0) {
		plog(logp, LOCATION, NULL,
			"failed to get proposal for responder.\n");
		error = ISAKMP_INTERNAL_ERROR;
		goto end;
	}

	/* select single proposal or reject it. */
	if (ipsecdoi_selectph2proposal(iph2) < 0) {
		error = ISAKMP_NTYPE_NO_PROPOSAL_CHOSEN;
		goto end;
	}

	/* check KE and attribute of PFS */
	if ((iph2->dhpub_p != NULL && iph2->approval->pfs_group == 0)
	 || (iph2->dhpub_p == NULL && iph2->approval->pfs_group != 0)) {
		plog(logp, LOCATION, NULL,
			"KE payload and PFS attribute mismatched.\n");
		error = ISAKMP_NTYPE_NO_PROPOSAL_CHOSEN;
		goto end;
	}

	/* change status of isakmp status entry */
	iph2->status = PHASE2ST_STATUS2;

	error = 0;

end:
	if (hbuf)
		vfree(hbuf);
	if (msg)
		vfree(msg);
	if (pbuf)
		vfree(pbuf);

	if (error) {
		VPTRINIT(iph2->sa);
		VPTRINIT(iph2->nonce_p);
		VPTRINIT(iph2->dhpub_p);
		VPTRINIT(iph2->id);
		VPTRINIT(iph2->id_p);
	}

	return error;
}

/*
 * call pfkey_getspi.
 */
int
quick_r1prep(iph2, msg)
	struct ph2handle *iph2;
	vchar_t *msg;
{
	int error = ISAKMP_INTERNAL_ERROR;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_STATUS2) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	iph2->status = PHASE2ST_GETSPISENT;

	/* send getspi message */
	if (pk_sendgetspi(iph2) < 0)
		goto end;

	YIPSDEBUG(DEBUG_STAMP,
		plog(logp, LOCATION, NULL, "pfkey getspi sent.\n"));

	iph2->sce = sched_new(lcconf->wait_ph2complete,
				pfkey_timeover, iph2);

	error = 0;

end:
	return error;
}

/*
 * send to initiator
 * 	HDR*, HASH(2), SA, Nr [, KE ] [, IDi2, IDr2 ]
 */
int
quick_r2send(iph2, msg)
	struct ph2handle *iph2;
	vchar_t *msg;	/* to be zero */
{
	vchar_t *body = NULL;
	struct isakmp_gen *gen;
	char *p;
	int tlen;
	int error = ISAKMP_INTERNAL_ERROR;
	int pfsgroup;
	u_int8_t *np_p = NULL;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_GETSPIDONE) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	/* update responders SPI */
	if (ipsecdoi_updatespi(iph2) < 0) {
		plog(logp, LOCATION, NULL, "failed to update spi.\n");
		goto end;
	}

	/* generate NONCE value */
	iph2->nonce = eay_set_random(iph2->ph1->rmconf->nonce_size);
	if (iph2->nonce == NULL)
		goto end;

	/* generate KE value if need */
	pfsgroup = iph2->approval->pfs_group;
	if (iph2->dhpub_p != NULL && pfsgroup != 0) {
		/* DH group settting if PFS is required. */
		if (oakley_setdhgroup(pfsgroup, &iph2->pfsgrp) < 0) {
			plog(logp, LOCATION, NULL,
				"failed to set DH value.\n");
			goto end;
		}
		/* generate DH public value */
		if (oakley_dh_generate(iph2->pfsgrp,
				&iph2->dhpub, &iph2->dhpriv) < 0) {
			goto end;
		}
	}

	/* create SA;NONCE payload, and KE and ID if need */
	tlen = sizeof(*gen) + iph2->sa_ret->l
		+ sizeof(*gen) + iph2->nonce->l;
	if (iph2->dhpub_p != NULL && pfsgroup != 0)
		tlen += (sizeof(*gen) + iph2->dhpub->l);
	if (iph2->id_p != NULL)
		tlen += (sizeof(*gen) + iph2->id_p->l
			+ sizeof(*gen) + iph2->id->l);

	body = vmalloc(tlen);
	if (body == NULL) { 
		plog(logp, LOCATION, NULL,
			"failed to get buffer to send.\n");
		goto end;
	}
	p = body->v;

	/* make SA payload */ 
	p = set_isakmp_payload(body->v, iph2->sa_ret, ISAKMP_NPTYPE_NONCE);

	/* add NONCE payload */
	np_p = &((struct isakmp_gen *)p)->np;	/* XXX */
	p = set_isakmp_payload(p, iph2->nonce,
		(iph2->dhpub_p != NULL && pfsgroup != 0)
				? ISAKMP_NPTYPE_KE
				: (iph2->id_p != NULL
					? ISAKMP_NPTYPE_ID
					: ISAKMP_NPTYPE_NONE));

	/* add KE payload if need. */
	if (iph2->dhpub_p != NULL && pfsgroup != 0) {
		np_p = &((struct isakmp_gen *)p)->np;	/* XXX */
		p = set_isakmp_payload(p, iph2->dhpub,
			(iph2->id_p == NULL)
				? ISAKMP_NPTYPE_NONE
				: ISAKMP_NPTYPE_ID);
	}

	/* add ID payloads received. */
	if (iph2->id_p != NULL) {
		/* IDci */
		p = set_isakmp_payload(p, iph2->id_p, ISAKMP_NPTYPE_ID);
		/* IDcr */
		np_p = &((struct isakmp_gen *)p)->np;	/* XXX */
		p = set_isakmp_payload(p, iph2->id, ISAKMP_NPTYPE_NONE);
	}

	/* add a RESPONDER-LIFETIME notify payload if needed */
    {
	vchar_t *data = NULL;
	struct saprop *pp = iph2->approval;
	struct saproto *pr;

	if (pp->claim & IPSECDOI_ATTR_SA_LD_TYPE_SEC) {
		u_int32_t v = htonl((u_int32_t)pp->lifetime);
		data = isakmp_add_attr_l(data, IPSECDOI_ATTR_SA_LD_TYPE,
					IPSECDOI_ATTR_SA_LD_TYPE_SEC);
		if (!data)
			goto end;
		data = isakmp_add_attr_v(data, IPSECDOI_ATTR_SA_LD,
					(caddr_t)&v, sizeof(v));
		if (!data)
			goto end;
	}
	if (pp->claim & IPSECDOI_ATTR_SA_LD_TYPE_KB) {
		u_int32_t v = htonl((u_int32_t)pp->lifebyte);
		data = isakmp_add_attr_l(data, IPSECDOI_ATTR_SA_LD_TYPE,
					IPSECDOI_ATTR_SA_LD_TYPE_KB);
		if (!data)
			goto end;
		data = isakmp_add_attr_v(data, IPSECDOI_ATTR_SA_LD,
					(caddr_t)&v, sizeof(v));
		if (!data)
			goto end;
	}

	/*
	 * XXX Is there only single RESPONDER-LIFETIME payload in a IKE message
	 * in the case of SA bundle ?
	 */
	if (data) {
		for (pr = pp->head; pr; pr = pr->next) {
			body = isakmp_add_pl_n(body, &np_p,
					ISAKMP_NTYPE_RESPONDER_LIFETIME, pr, data);
			if (!body)
				return error;	/* XXX */
		}
		vfree(data);
	}
    }

	/* generate HASH(2) */
    {
	vchar_t *tmp;

	tmp = vmalloc(iph2->nonce_p->l + body->l);
	if (tmp == NULL) { 
		plog(logp, LOCATION, NULL,
			"failed to get hash buffer.\n");
		goto end;
	}
	memcpy(tmp->v, iph2->nonce_p->v, iph2->nonce_p->l);
	memcpy(tmp->v + iph2->nonce_p->l, body->v, body->l);

	iph2->hash = oakley_compute_hash1(iph2->ph1, iph2->msgid, tmp);
	vfree(tmp);

	if (iph2->hash == NULL)
		goto end;
    }

	/* send isakmp payload */
	iph2->sendbuf = quick_ir1sendmx(iph2, body);
	if (iph2->sendbuf == NULL)
		goto end;

	/* change status of isakmp status entry */
	iph2->status = PHASE2ST_MSG1SENT;

	/* add to the schedule to resend */
	iph2->retry_counter = iph2->ph1->rmconf->retry_counter;
	iph2->scr = sched_new(iph2->ph1->rmconf->retry_interval,
				isakmp_ph2resend, iph2);

	error = 0;

end:
	if (body != NULL)
		vfree(body);

	return error;
}

/*
 * receive from initiator
 * 	HDR*, HASH(3)
 */
int
quick_r3recv(iph2, msg0)
	struct ph2handle *iph2;
	vchar_t *msg0;
{
	vchar_t *msg = NULL;
	vchar_t *pbuf = NULL;	/* for payload parsing */
	struct isakmp_parse_t *pa;
	struct isakmp_pl_hash *hash = NULL;
	int error = ISAKMP_INTERNAL_ERROR;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_MSG1SENT) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	/* decrypt packet */
	if (!ISSET(((struct isakmp *)msg0->v)->flags, ISAKMP_FLAG_E)) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"Packet wasn't encrypted.\n");
		goto end;
	}
	msg = oakley_do_decrypt(iph2->ph1, msg0, iph2->ivm->iv, iph2->ivm->ive);
	if (msg == NULL)
		goto end;

	/* validate the type of next payload */
	pbuf = isakmp_parse(msg);
	if (pbuf == NULL)
		goto end;

	for (pa = (struct isakmp_parse_t *)pbuf->v;
	     pa->type != ISAKMP_NPTYPE_NONE;
	     pa++) {

		switch (pa->type) {
		case ISAKMP_NPTYPE_HASH:
			hash = (struct isakmp_pl_hash *)pa->ptr;
			break;
		case ISAKMP_NPTYPE_N:
			plog(logp, LOCATION, iph2->ph1->remote,
				"peer transmitted Notify Message.\n");
			isakmp_check_notify(pa->ptr, iph2->ph1);
			break;
		default:
			/* don't send information, see ident_r1recv() */
			plog(logp, LOCATION, iph2->ph1->remote,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			goto end;
		}
	}

	/* payload existency check */
	if (hash == NULL) {
		plog(logp, LOCATION, iph2->ph1->remote,
			"few isakmp message received.\n");
		goto end;
	}

	/* validate HASH(3) */
	/* HASH(3) = prf(SKEYID_a, 0 | M-ID | Ni_b | Nr_b) */
    {
	char *r_hash;
	vchar_t *my_hash = NULL;
	vchar_t *tmp = NULL;
	int result;

	r_hash = (char *)hash + sizeof(*hash);

	YIPSDEBUG(DEBUG_KEY, plog(logp, LOCATION, NULL, "HASH(3) validate:"));
	YIPSDEBUG(DEBUG_DKEY,
	    hexdump(r_hash, ntohs(hash->h.len) - sizeof(*hash)));

	tmp = vmalloc(iph2->nonce_p->l + iph2->nonce->l);
	if (tmp == NULL) { 
		plog(logp, LOCATION, NULL,
			"failed to get hash buffer.\n");
		goto end;
	}
	memcpy(tmp->v, iph2->nonce_p->v, iph2->nonce_p->l);
	memcpy(tmp->v + iph2->nonce_p->l, iph2->nonce->v, iph2->nonce->l);

	my_hash = oakley_compute_hash3(iph2->ph1, iph2->msgid, tmp);
	vfree(tmp);
	if (my_hash == NULL)
		goto end;

	result = memcmp(my_hash->v, r_hash, my_hash->l);
	vfree(my_hash);

	if (result) {
		plog(logp, LOCATION, iph2->ph1->remote, "HASH(3) mismatch.\n");
		error = ISAKMP_NTYPE_INVALID_HASH_INFORMATION;
		goto end;
	}
    }

	/* if there is commit bit, don't set up SA now. */
	if (ISSET(iph2->flags, ISAKMP_FLAG_C)) {
		iph2->status = PHASE2ST_COMMIT;
	} else
		iph2->status = PHASE2ST_STATUS6;

	error = 0;

end:
	if (pbuf != NULL)
		vfree(pbuf);
	if (msg != NULL)
		vfree(msg);

	return error;
}

/*
 * send to initiator
 * 	HDR#*, HASH(4), notify
 */
int
quick_r3send(iph2, msg0)
	struct ph2handle *iph2;
	vchar_t *msg0;
{
	vchar_t *buf = NULL;
	vchar_t *myhash = NULL;
	struct isakmp_pl_n *n;
	vchar_t *notify = NULL;
	char *p;
	int tlen;
	int error = ISAKMP_INTERNAL_ERROR;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_COMMIT) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	/* generate HASH(4) */
	/* XXX What can I do in the case of multiple different SA */
	YIPSDEBUG(DEBUG_KEY, plog(logp, LOCATION, NULL, "HASH(4) generate\n"));

	/* XXX What should I do if there are multiple SAs ? */
	tlen = sizeof(struct isakmp_pl_n) + iph2->approval->head->spisize;
	notify = vmalloc(tlen);
	if (notify == NULL) { 
		plog(logp, LOCATION, NULL,
			"failed to get notify buffer.\n");
		goto end;
	}
	n = (struct isakmp_pl_n *)notify->v;
	n->h.np = ISAKMP_NPTYPE_NONE;
	n->h.len = htons(tlen);
	n->doi = IPSEC_DOI;
	n->proto_id = iph2->approval->head->proto_id;
	n->spi_size = sizeof(iph2->approval->head->spisize);
	n->type = htons(ISAKMP_NTYPE_CONNECTED);
	memcpy(n + 1, &iph2->approval->head->spi, iph2->approval->head->spisize);

	myhash = oakley_compute_hash1(iph2->ph1, iph2->msgid, notify);
	if (myhash == NULL)
		goto end;

	/* create buffer for isakmp payload */
	tlen = sizeof(struct isakmp)
		+ sizeof(struct isakmp_gen) + myhash->l
		+ notify->l;
	buf = vmalloc(tlen);
	if (buf == NULL) { 
		plog(logp, LOCATION, NULL,
			"failed to get buffer to send.\n");
		goto end;
	}

	/* create isakmp header */
	p = set_isakmp_header2(buf, iph2, ISAKMP_NPTYPE_HASH);
	if (p == NULL)
		goto end;

	/* add HASH(4) payload */
	p = set_isakmp_payload(p, myhash, ISAKMP_NPTYPE_N);

	/* add notify payload */
	memcpy(p, notify->v, notify->l);

#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(buf, iph2->ph1->local, iph2->ph1->remote, 1);
#endif

	/* encoding */
	iph2->sendbuf = oakley_do_encrypt(iph2->ph1, buf, iph2->ivm->ive, iph2->ivm->iv);
	if (iph2->sendbuf == NULL)
		goto end;

	/* send HDR*;HASH(3) */
	if (isakmp_send(iph2->ph1, iph2->sendbuf) < 0)
		goto end;

	/* XXX: How resend ? */

	iph2->status = PHASE2ST_COMMIT;

	error = 0;

end:
	if (buf != NULL)
		vfree(buf);
	if (myhash != NULL)
		vfree(myhash);
	if (notify != NULL)
		vfree(notify);

	return error;
}

/*
 * set SA to kernel.
 */
int
quick_r3prep(iph2, msg0)
	struct ph2handle *iph2;
	vchar_t *msg0;
{
	vchar_t *msg = NULL;
	int error = ISAKMP_INTERNAL_ERROR;

	YIPSDEBUG(DEBUG_STAMP, plog(logp, LOCATION, NULL, "begin.\n"));

	/* validity check */
	if (iph2->status != PHASE2ST_STATUS6) {
		plog(logp, LOCATION, NULL,
			"status mismatched %d.\n", iph2->status);
		goto end;
	}

	/* compute both of KEYMATs */
	if (oakley_compute_keymat(iph2, RESPONDER) < 0)
		goto end;

	iph2->status = PHASE2ST_ADDSA;
	iph2->flags ^= ISAKMP_FLAG_C;	/* reset bit */

	/* don't anything if local test mode. */
	if (f_local) {
		error = 0;
		goto end;
	}

	/* Do UPDATE as responder */
	YIPSDEBUG(DEBUG_PFKEY, plog(logp, LOCATION, NULL,
		"call pk_sendupdate\n"););
	if (pk_sendupdate(iph2) < 0) {
		plog(logp, LOCATION, NULL, "pfkey update failed.\n");
		goto end;
	}
	YIPSDEBUG(DEBUG_STAMP,
		plog(logp, LOCATION, NULL, "pfkey update sent.\n"));

	/* Do ADD for responder */
	if (pk_sendadd(iph2) < 0) {
		plog(logp, LOCATION, NULL, "pfkey add failed.\n");
		goto end;
	}
	YIPSDEBUG(DEBUG_STAMP,
		plog(logp, LOCATION, NULL, "pfkey add sent.\n"));

	error = 0;

end:
	if (msg != NULL)
		vfree(msg);

	return error;
}

/*
 * create HASH, body (SA, NONCE) payload with isakmp header.
 */
static vchar_t *
quick_ir1sendmx(iph2, body)
	struct ph2handle *iph2;
	vchar_t *body;
{
	struct isakmp *isakmp;
	vchar_t *buf = NULL, *new = NULL;
	char *p;
	int tlen;
	struct isakmp_gen *gen;
	int error = ISAKMP_INTERNAL_ERROR;

	/* create buffer for isakmp payload */
	tlen = sizeof(*isakmp)
		+ sizeof(*gen) + iph2->hash->l
		+ body->l;
	buf = vmalloc(tlen);
	if (buf == NULL) { 
		plog(logp, LOCATION, NULL,
			"failed to get buffer to send.\n");
		goto end;
	}

	/* re-set encryption flag, for serurity. */
	iph2->flags |= ISAKMP_FLAG_E;

	/* set isakmp header */
	p = set_isakmp_header2(buf, iph2, ISAKMP_NPTYPE_HASH);
	if (p == NULL)
		goto end;

	/* add HASH payload */
	/* XXX is next type always SA ? */
	p = set_isakmp_payload(p, iph2->hash, ISAKMP_NPTYPE_SA);

	/* add body payload */
	memcpy(p, body->v, body->l);

#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(buf, iph2->ph1->local, iph2->ph1->remote, 1);
#endif

	/* encoding */
	new = oakley_do_encrypt(iph2->ph1, buf, iph2->ivm->ive, iph2->ivm->iv);
	if (new == NULL)
		goto end;

	vfree(buf);

	buf = new;

	/* send HDR*;HASH(1);SA;Nr to responder */
	if (isakmp_send(iph2->ph1, buf) < 0)
		goto end;

	error = 0;

end:
	if (error && buf != NULL) {
		vfree(buf);
		buf = NULL;
	}

	return buf;
}

/*
 * get remote's sainfo.
 * NOTE: this function is for responder.
 */
static int
get_sainfo_r(iph2)
	struct ph2handle *iph2;
{
	vchar_t *idsrc = NULL, *iddst = NULL;
	int prefixlen;
	int error = ISAKMP_INTERNAL_ERROR;

	if (iph2->id_p == NULL) {
		switch (iph2->src->sa_family) {
		case AF_INET:
			prefixlen = sizeof(struct in_addr) << 3;
			break;
		case AF_INET6:
			prefixlen = sizeof(struct in6_addr) << 3;
			break;
		default:
			plog(logp, LOCATION, NULL,
				"invalid family: %d\n", iph2->src->sa_family);
			goto end;
		}
		idsrc = ipsecdoi_sockaddr2id(iph2->src, prefixlen,
					IPSEC_ULPROTO_ANY);
	} else {
		idsrc = vdup(iph2->id);
	}
	if (idsrc == NULL) {
		plog(logp, LOCATION, NULL,
			"failed to set ID for source.\n");
		goto end;
	}

	if (iph2->id == NULL) {
		switch (iph2->dst->sa_family) {
		case AF_INET:
			prefixlen = sizeof(struct in_addr) << 3;
			break;
		case AF_INET6:
			prefixlen = sizeof(struct in6_addr) << 3;
			break;
		default:
			plog(logp, LOCATION, NULL,
				"invalid family: %d\n", iph2->dst->sa_family);
			goto end;
		}
		iddst = ipsecdoi_sockaddr2id(iph2->dst, prefixlen,
					IPSEC_ULPROTO_ANY);
	} else {
		iddst = vdup(iph2->id_p);
	}
	if (iddst == NULL) {
		plog(logp, LOCATION, NULL,
			"failed to set ID for destination.\n");
		goto end;
	}

	iph2->sainfo = getsainfo(idsrc, iddst);
	if (iph2->sainfo == NULL) {
		plog(logp, LOCATION, NULL,
			"failed to get sainfo.\n");
		goto end;
	}

	YIPSDEBUG(DEBUG_MISC,
		plog(logp, LOCATION, NULL,
			"get sa info: %s\n", sainfo2str(iph2->sainfo)));

	error = 0;
end:
	if (idsrc)
		vfree(idsrc);
	if (iddst)
		vfree(iddst);

	return error;
}

/*
 * Copy both IP addresses in ID payloads into [src,dst]_id if both ID types
 * are IP address and same address family.
 * Then get remote's policy from SPD copied from kernel.
 * If the type of ID payload is address or subnet type, then the index is
 * made from the payload.  If there is no ID payload, or the type of ID
 * payload is NOT address type, then the index is made from the address
 * pair of phase 1.
 * NOTE: This function is only for responder.
 */
static int
get_proposal_r(iph2)
	struct ph2handle *iph2;
{
	struct policyindex spidx;
	struct secpolicy *sp, *sp_out = NULL;
	struct ipsecrequest *req;
	struct saprop *newpp = NULL;
	int idi2type = 0;	/* switch whether copy IDs into id[src,dst]. */
	int error;

	/* check the existence of ID payload */
	if ((iph2->id_p != NULL && iph2->id == NULL)
	 || (iph2->id_p == NULL && iph2->id != NULL)) {
		plog(logp, LOCATION, NULL,
			"ERROR: Both IDs wasn't found in payload.\n");
		return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
	}

	/* make sure if id[src,dst] is null. */
	if (iph2->src_id || iph2->dst_id) {
		plog(logp, LOCATION, NULL,
			"FATAL: Why do ID[src,dst] exist already.\n");
		return ISAKMP_INTERNAL_ERROR;
	}

	memset(&spidx, 0, sizeof(spidx));

#define _XIDT(d) ((struct ipsecdoi_id_b *)(d)->v)->type

	/* make a spidx; a key to search SPD */
	/* get inbound policy */
	spidx.dir = IPSEC_DIR_INBOUND;
	spidx.ul_proto = 0;

	/* make src/dst index from ID payload or phase 1 addresses. */
	if (iph2->id != NULL
	 && (_XIDT(iph2->id) == IPSECDOI_ID_IPV4_ADDR
	  || _XIDT(iph2->id) == IPSECDOI_ID_IPV6_ADDR
	  || _XIDT(iph2->id) == IPSECDOI_ID_IPV4_ADDR_SUBNET
	  || _XIDT(iph2->id) == IPSECDOI_ID_IPV6_ADDR_SUBNET)) {
		/* get a destination address of a policy */
		error = ipsecdoi_id2sockaddr(iph2->id,
				(struct sockaddr *)&spidx.dst,
				&spidx.prefd, &spidx.ul_proto);
		if (error)
			return error;

		if (_XIDT(iph2->id) == IPSECDOI_ID_IPV4_ADDR
		 || _XIDT(iph2->id) == IPSECDOI_ID_IPV6_ADDR)
			idi2type = _XIDT(iph2->id);
	} else {

		YIPSDEBUG(DEBUG_MISC,
			plog(logp, LOCATION, NULL,
				"get a destination address of SP index "
				"from phase1 address "
				"due to no ID payloads found "
				"OR because ID type is not address.\n"));

		/*
		 * copy the SOURCE address of IKE into the DESTINATION address
		 * of the key to search the SPD because the direction of policy
		 * is inbound.
		 */
		memcpy(&spidx.dst, iph2->src, iph2->src->sa_len);
		switch (spidx.dst.ss_family) {
		case AF_INET:
			spidx.prefd = sizeof(struct in_addr) << 3;
			break;
#ifdef INET6
		case AF_INET6:
			spidx.prefd = sizeof(struct in6_addr) << 3;
			break;
#endif
		default:
			spidx.prefd = 0;
			break;
		}
	}

	/* set source address */
	if (iph2->id_p != NULL
	 && (_XIDT(iph2->id_p) == IPSECDOI_ID_IPV4_ADDR
	  || _XIDT(iph2->id_p) == IPSECDOI_ID_IPV6_ADDR
	  || _XIDT(iph2->id_p) == IPSECDOI_ID_IPV4_ADDR_SUBNET
	  || _XIDT(iph2->id_p) == IPSECDOI_ID_IPV6_ADDR_SUBNET)) {
		/* get a source address of inbound SA */
		error = ipsecdoi_id2sockaddr(iph2->id_p,
				(struct sockaddr *)&spidx.src,
				&spidx.prefs, &spidx.ul_proto);
		if (error)
			return error;

		/* make id[src,dst] if both ID types are IP address and same */
		if (_XIDT(iph2->id_p) == idi2type
		 && spidx.dst.ss_family == spidx.src.ss_family) {
			iph2->src_id = dupsaddr((struct sockaddr *)&spidx.dst);
			iph2->dst_id = dupsaddr((struct sockaddr *)&spidx.src);
		}
	} else {
		YIPSDEBUG(DEBUG_MISC,
			plog(logp, LOCATION, NULL,
				"get a source address of SP index "
				"from phase1 address "
				"due to no ID payloads found "
				"OR because ID type is not address.\n"));

		/* see above comment. */
		memcpy(&spidx.src, iph2->dst, iph2->dst->sa_len);
		switch (spidx.src.ss_family) {
		case AF_INET:
			spidx.prefs = sizeof(struct in_addr) << 3;
			break;
#ifdef INET6
		case AF_INET6:
			spidx.prefs = sizeof(struct in6_addr) << 3;
			break;
#endif
		default:
			spidx.prefs = 0;
			break;
		}
	}

#undef _XIDT(d)

	YIPSDEBUG(DEBUG_MISC,
		plog(logp, LOCATION, NULL,
			"get a src address from ID payload "
			"%s prefixlen=%u ul_proto=%u\n",
			saddr2str((struct sockaddr *)&spidx.src),
			spidx.prefs, spidx.ul_proto));
	YIPSDEBUG(DEBUG_MISC,
		plog(logp, LOCATION, NULL,
			"get dst address from ID payload "
			"%s prefixlen=%u ul_proto=%u\n",
			saddr2str((struct sockaddr *)&spidx.dst),
			spidx.prefd, spidx.ul_proto));

	sp = getsp_r(&spidx);
	if (sp == NULL) {
		plog(logp, LOCATION, NULL,
			"ERROR: no policy found: %s\n", spidx2str(&spidx));
		return ISAKMP_INTERNAL_ERROR;
	}

	/* get outbound policy */
    {
	struct sockaddr_storage addr;
	u_int8_t pref;

	spidx.dir = IPSEC_DIR_OUTBOUND;
	addr = spidx.src;
	spidx.src = spidx.dst;
	spidx.dst = addr;
	pref = spidx.prefs;
	spidx.prefs = spidx.prefd;
	spidx.prefd = pref;

	sp_out = getsp_r(&spidx);
	if (!sp_out) {
		YIPSDEBUG(DEBUG_MISC,
			plog(logp, LOCATION, NULL,
				"ERROR: no outbound policy found: %s\n",
				spidx2str(&spidx)));
	}
    }

	YIPSDEBUG(DEBUG_SA,
		plog(logp, LOCATION, NULL,
			"DEBUG: suitable SP found:%s\n", spidx2str(&spidx)));

	/* require IPsec ? */
	if (!(sp->policy == IPSEC_POLICY_IPSEC && sp_out->policy == IPSEC_POLICY_IPSEC)) {
		plog(logp, LOCATION, NULL,
			"NOTICE: policy found, but no IPsec required: %s\n",
			spidx2str(&spidx));
		return ISAKMP_INTERNAL_ERROR;
	}

	/* allocate ipsec sa proposal */
	newpp = newsaprop();
	if (newpp == NULL) {
		plog(logp, LOCATION, NULL,
			"FATAL: failed to allocate saprop.\n");
		goto err;
	}
	newpp->prop_no = 1;
	newpp->lifetime = iph2->sainfo->lifetime;
	newpp->lifebyte = iph2->sainfo->lifebyte;
	newpp->pfs_group = iph2->sainfo->pfs_group;

	/* set new saprop */
	inssaprop(&iph2->proposal, newpp);

	/* from inbound policy */
	for (req = sp->req; req; req = req->next) {
		struct saproto *newpr;
		struct sockaddr *psaddr = NULL;
		struct sockaddr *pdaddr = NULL;

		/* check if SA bundle ? */
		if (req->saidx.src.ss_len && req->saidx.dst.ss_len) {

			psaddr = (struct sockaddr *)&req->saidx.src;
			pdaddr = (struct sockaddr *)&req->saidx.dst;

			/* check end addresses of SA */
			/*
			 * NOTE: In inbound, SA's addresses in SP entry are
			 * reverse against real SA's addresses 
			 */
			if (memcmp(iph2->dst, psaddr, iph2->dst->sa_len)
			 || memcmp(iph2->src, pdaddr, iph2->src->sa_len)) {
				/* end of SA bundle */
				break;
			}
		}

		/* allocate ipsec sa protocol */
		newpr = newsaproto();
		if (newpr == NULL) {
			plog(logp, LOCATION, NULL,
				"failed to allocate saproto.\n");
			goto err;
		}

		newpr->proto_id = ipproto2doi(req->saidx.proto);
		newpr->spisize = 4;
		newpr->encmode = pfkey2ipsecdoi_mode(req->saidx.mode);
		newpr->reqid_in = req->saidx.reqid;

		if (set_satrnsbysainfo(newpr, iph2->sainfo) < 0)
			goto err;

		/* set new saproto */
		inssaproto(newpp, newpr);
	}

	/* get reqid_out from outbound policy */
	if (sp_out) {
		struct saproto *pr;

		req = sp_out->req;
		pr = newpp->head;
		while (req && pr) {
			pr->reqid_out = req->saidx.reqid;
			pr = pr->next;
			req = req->next;
		}
		if (pr || req) {
			plog(logp, LOCATION, NULL,
				"ERROR: There is a difference "
				"between the policies.\n");
			goto err;
		}
	}

	YIPSDEBUG(DEBUG_DSA,
		plog(logp, LOCATION, NULL,
			"my single bundle:\n");
			printsaprop0(newpp););

	iph2->proposal = newpp;

	return 0;

err:
	if (newpp)
		flushsaprop(newpp);

	return -1;
}
