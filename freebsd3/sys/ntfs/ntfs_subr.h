/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: ntfs_subr.h,v 1.2.2.1 1999/03/14 09:47:05 semenu Exp $
 */

#define	VA_LOADED		0x0001
#define	VA_PRELOADED		0x0002

struct ntvattr {
	struct ntvattr *va_nextp;

	u_int32_t		va_vflag;
	struct vnode	       *va_vp;
	struct ntnode 	       *va_ip;

	u_int32_t		va_flag;
	u_int32_t		va_type;
	u_int8_t		va_namelen;
	char			va_name[NTFS_MAXATTRNAME];

	u_int32_t		va_compression;
	u_int32_t		va_compressalg;
	u_int32_t		va_datalen;
	u_int32_t		va_allocated;
	cn_t	 		va_vcnstart;
	cn_t	 		va_vcnend;
	u_int16_t		va_index;
	union {
		struct {
			cn_t *		cn;
			cn_t *		cl;
			u_long		cnt;
		} vrun;
		caddr_t		datap;
		struct attr_name *name;
		struct attr_indexroot *iroot;
		struct attr_indexalloc *ialloc;
	} va_d;
};
#define	va_vruncn	va_d.vrun.cn
#define va_vruncl	va_d.vrun.cl
#define va_vruncnt	va_d.vrun.cnt
#define va_datap	va_d.datap
#define va_a_name	va_d.name
#define va_a_iroot	va_d.iroot
#define va_a_ialloc	va_d.ialloc


#define uastrcmp(a,b,c,d)	ntfs_uastrcmp(ntmp,a,b,c,d)

#ifndef NTFS_DEBUG
#define ntfs_ntref(i)	(i)->i_usecount++
#else
#define ntfs_ntref(i)   {						\
	printf("ntfs_ntref: ino %d, usecount: %d\n",			\
		(i)->i_number, (i)->i_usecount++);			\
}
#endif

int ntfs_procfixups __P(( struct ntfsmount *, u_int32_t, caddr_t, size_t ));
int ntfs_parserun __P(( cn_t *, cn_t *, u_int8_t *, u_long, u_long *));
int ntfs_runtocn __P(( cn_t *, struct ntfsmount *, u_int8_t *, u_long, cn_t));
int ntfs_readntvattr_plain __P(( struct ntfsmount *, struct ntnode *, struct ntvattr *, off_t, size_t, void *,size_t *));
int ntfs_readattr_plain __P(( struct ntfsmount *, struct ntnode *, u_int32_t, char *, off_t, size_t, void *,size_t *));
int ntfs_readattr __P(( struct ntfsmount *, struct ntnode *, u_int32_t, char *, off_t, size_t, void *));
int ntfs_filesize __P(( struct ntfsmount *, struct fnode *, u_int64_t *, u_int64_t *));
int ntfs_times __P(( struct ntfsmount *, struct ntnode *, ntfs_times_t *));
struct timespec	ntfs_nttimetounix __P(( u_int64_t ));
int ntfs_ntreaddir __P(( struct ntfsmount *, struct fnode *, u_int32_t, struct attr_indexentry **));
wchar ntfs_toupper __P(( struct ntfsmount *, wchar ));
int ntfs_uustricmp __P(( struct ntfsmount *, wchar *, int, wchar *, int ));
int ntfs_uastricmp __P(( struct ntfsmount *, wchar *, int, char *, int ));
int ntfs_uastrcmp __P(( struct ntfsmount *, wchar *, int, char *, int ));
int ntfs_runtovrun __P(( cn_t **, cn_t **, u_long *, u_int8_t *));
int ntfs_attrtontvattr __P(( struct ntfsmount *, struct ntvattr **, struct attr * ));
void ntfs_freentvattr __P(( struct ntvattr * ));
int ntfs_loadntvattrs __P(( struct ntfsmount *, struct vnode *, caddr_t, struct ntvattr **));
struct ntvattr * ntfs_findntvattr __P(( struct ntfsmount *, struct ntnode *, u_int32_t, cn_t ));
int ntfs_ntlookup __P(( struct ntfsmount *, struct vnode *, struct componentname *, struct vnode **));
int ntfs_isnamepermitted __P(( struct ntfsmount *, struct attr_indexentry * ));
int ntfs_ntvattrrele __P(( struct ntvattr * ));
int ntfs_ntvattrget __P(( struct ntfsmount *, struct ntnode *, u_int32_t, char *, cn_t , struct ntvattr **));
int ntfs_ntget __P(( struct ntfsmount *, ino_t, struct ntnode **));
void ntfs_ntrele __P(( struct ntnode *));
int ntfs_loadntnode __P(( struct ntfsmount *, struct ntnode * ));
int ntfs_ntlookupattr(struct ntfsmount *, char *, int, int *, char **);
int ntfs_writentvattr_plain(struct ntfsmount *, struct ntnode *, struct ntvattr *, off_t, size_t, void *, size_t *);
int ntfs_writeattr_plain(struct ntfsmount *, struct ntnode *, u_int32_t, char *, off_t, size_t, void *, size_t *);
