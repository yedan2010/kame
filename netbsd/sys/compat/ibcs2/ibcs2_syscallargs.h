/*	$NetBSD: ibcs2_syscallargs.h,v 1.24 1999/02/09 20:48:20 christos Exp $	*/

/*
 * System call argument lists.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.20 1999/02/09 20:22:37 christos Exp 
 */

#ifndef _IBCS2_SYS__SYSCALLARGS_H_
#define _IBCS2_SYS__SYSCALLARGS_H_

#ifdef	syscallarg
#undef	syscallarg
#endif

#define	syscallarg(x)								\
		union {								\
			register_t pad;						\
			struct { x datum; } le;					\
			struct {						\
				int8_t pad[ (sizeof (register_t) < sizeof (x))	\
					? 0					\
					: sizeof (register_t) - sizeof (x)];	\
				x datum;					\
			} be;							\
		}

struct ibcs2_sys_read_args {
	syscallarg(int) fd;
	syscallarg(char *) buf;
	syscallarg(u_int) nbytes;
};

struct ibcs2_sys_open_args {
	syscallarg(const char *) path;
	syscallarg(int) flags;
	syscallarg(int) mode;
};

struct ibcs2_sys_waitsys_args {
	syscallarg(int) a1;
	syscallarg(int) a2;
	syscallarg(int) a3;
};

struct ibcs2_sys_creat_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
};

struct ibcs2_sys_unlink_args {
	syscallarg(const char *) path;
};

struct ibcs2_sys_execv_args {
	syscallarg(const char *) path;
	syscallarg(char **) argp;
};

struct ibcs2_sys_chdir_args {
	syscallarg(const char *) path;
};

struct ibcs2_sys_time_args {
	syscallarg(ibcs2_time_t *) tp;
};

struct ibcs2_sys_mknod_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
	syscallarg(int) dev;
};

struct ibcs2_sys_chmod_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
};

struct ibcs2_sys_chown_args {
	syscallarg(const char *) path;
	syscallarg(int) uid;
	syscallarg(int) gid;
};

struct ibcs2_sys_stat_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_stat *) st;
};

struct ibcs2_sys_mount_args {
	syscallarg(char *) special;
	syscallarg(char *) dir;
	syscallarg(int) flags;
	syscallarg(int) fstype;
	syscallarg(char *) data;
	syscallarg(int) len;
};

struct ibcs2_sys_umount_args {
	syscallarg(char *) name;
};

struct ibcs2_sys_setuid_args {
	syscallarg(int) uid;
};

struct ibcs2_sys_stime_args {
	syscallarg(long *) timep;
};

struct ibcs2_sys_alarm_args {
	syscallarg(unsigned) sec;
};

struct ibcs2_sys_fstat_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_stat *) st;
};

struct ibcs2_sys_utime_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_utimbuf *) buf;
};

struct ibcs2_sys_access_args {
	syscallarg(const char *) path;
	syscallarg(int) flags;
};

struct ibcs2_sys_nice_args {
	syscallarg(int) incr;
};

struct ibcs2_sys_statfs_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_statfs *) buf;
	syscallarg(int) len;
	syscallarg(int) fstype;
};

struct ibcs2_sys_kill_args {
	syscallarg(int) pid;
	syscallarg(int) signo;
};

struct ibcs2_sys_fstatfs_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_statfs *) buf;
	syscallarg(int) len;
	syscallarg(int) fstype;
};

struct ibcs2_sys_pgrpsys_args {
	syscallarg(int) type;
	syscallarg(caddr_t) dummy;
	syscallarg(int) pid;
	syscallarg(int) pgid;
};

struct ibcs2_sys_times_args {
	syscallarg(struct tms *) tp;
};

struct ibcs2_sys_plock_args {
	syscallarg(int) cmd;
};

struct ibcs2_sys_setgid_args {
	syscallarg(int) gid;
};

struct ibcs2_sys_sigsys_args {
	syscallarg(int) sig;
	syscallarg(ibcs2_sig_t) fp;
};

struct ibcs2_sys_msgsys_args {
	syscallarg(int) which;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
	syscallarg(int) a6;
};

struct ibcs2_sys_sysi86_args {
	syscallarg(int) cmd;
	syscallarg(int) arg;
};

struct ibcs2_sys_shmsys_args {
	syscallarg(int) which;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
};

struct ibcs2_sys_semsys_args {
	syscallarg(int) which;
	syscallarg(int) a2;
	syscallarg(int) a3;
	syscallarg(int) a4;
	syscallarg(int) a5;
};

struct ibcs2_sys_ioctl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(caddr_t) data;
};

struct ibcs2_sys_uadmin_args {
	syscallarg(int) cmd;
	syscallarg(int) func;
	syscallarg(caddr_t) data;
};

struct ibcs2_sys_utssys_args {
	syscallarg(int) a1;
	syscallarg(int) a2;
	syscallarg(int) flag;
};

struct ibcs2_sys_execve_args {
	syscallarg(const char *) path;
	syscallarg(char **) argp;
	syscallarg(char **) envp;
};

struct ibcs2_sys_fcntl_args {
	syscallarg(int) fd;
	syscallarg(int) cmd;
	syscallarg(char *) arg;
};

struct ibcs2_sys_ulimit_args {
	syscallarg(int) cmd;
	syscallarg(int) newlimit;
};

struct ibcs2_sys_rmdir_args {
	syscallarg(const char *) path;
};

struct ibcs2_sys_mkdir_args {
	syscallarg(const char *) path;
	syscallarg(int) mode;
};

struct ibcs2_sys_getdents_args {
	syscallarg(int) fd;
	syscallarg(char *) buf;
	syscallarg(int) nbytes;
};

struct ibcs2_sys_sysfs_args {
	syscallarg(int) cmd;
	syscallarg(caddr_t) d1;
	syscallarg(char *) buf;
};

struct ibcs2_sys_getmsg_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_stropts *) ctl;
	syscallarg(struct ibcs2_stropts *) dat;
	syscallarg(int *) flags;
};

struct ibcs2_sys_putmsg_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_stropts *) ctl;
	syscallarg(struct ibcs2_stropts *) dat;
	syscallarg(int) flags;
};

struct ibcs2_sys_symlink_args {
	syscallarg(const char *) path;
	syscallarg(const char *) link;
};

struct ibcs2_sys_lstat_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_stat *) st;
};

struct ibcs2_sys_readlink_args {
	syscallarg(const char *) path;
	syscallarg(char *) buf;
	syscallarg(int) count;
};

struct ibcs2_sys_sigaltstack_args {
	syscallarg(const struct ibcs2_sigaltstack *) nss;
	syscallarg(struct ibcs2_sigaltstack *) oss;
};

struct ibcs2_sys_statvfs_args {
	syscallarg(const char *) path;
	syscallarg(struct ibcs2_statvfs *) buf;
};

struct ibcs2_sys_fstatvfs_args {
	syscallarg(int) fd;
	syscallarg(struct ibcs2_statvfs *) buf;
};

struct ibcs2_sys_mmap_args {
	syscallarg(ibcs2_caddr_t) addr;
	syscallarg(ibcs2_size_t) len;
	syscallarg(int) prot;
	syscallarg(int) flags;
	syscallarg(int) fd;
	syscallarg(ibcs2_off_t) off;
};

struct ibcs2_sys_memcntl_args {
	syscallarg(ibcs2_caddr_t) addr;
	syscallarg(ibcs2_size_t) len;
	syscallarg(int) cmd;
	syscallarg(ibcs2_caddr_t) arg;
	syscallarg(int) attr;
	syscallarg(int) mask;
};

struct ibcs2_sys_gettimeofday_args {
	syscallarg(struct timeval *) tp;
};

struct ibcs2_sys_settimeofday_args {
	syscallarg(struct timeval *) tp;
};

struct xenix_sys_locking_args {
	syscallarg(int) fd;
	syscallarg(int) blk;
	syscallarg(int) size;
};

struct xenix_sys_rdchk_args {
	syscallarg(int) fd;
};

struct xenix_sys_chsize_args {
	syscallarg(int) fd;
	syscallarg(long) size;
};

struct xenix_sys_ftime_args {
	syscallarg(struct xenix_timeb *) tp;
};

struct xenix_sys_nap_args {
	syscallarg(long) millisec;
};

struct ibcs2_sys_eaccess_args {
	syscallarg(const char *) path;
	syscallarg(int) flags;
};

struct ibcs2_sys_sigaction_args {
	syscallarg(int) signum;
	syscallarg(const struct ibcs2_sigaction *) nsa;
	syscallarg(struct ibcs2_sigaction *) osa;
};

struct ibcs2_sys_sigprocmask_args {
	syscallarg(int) how;
	syscallarg(const ibcs2_sigset_t *) set;
	syscallarg(ibcs2_sigset_t *) oset;
};

struct ibcs2_sys_sigpending_args {
	syscallarg(ibcs2_sigset_t *) set;
};

struct ibcs2_sys_sigsuspend_args {
	syscallarg(const ibcs2_sigset_t *) set;
};

struct ibcs2_sys_getgroups_args {
	syscallarg(int) gidsetsize;
	syscallarg(ibcs2_gid_t *) gidset;
};

struct ibcs2_sys_setgroups_args {
	syscallarg(int) gidsetsize;
	syscallarg(ibcs2_gid_t *) gidset;
};

struct ibcs2_sys_sysconf_args {
	syscallarg(int) name;
};

struct ibcs2_sys_pathconf_args {
	syscallarg(char *) path;
	syscallarg(int) name;
};

struct ibcs2_sys_fpathconf_args {
	syscallarg(int) fd;
	syscallarg(int) name;
};

struct ibcs2_sys_rename_args {
	syscallarg(const char *) from;
	syscallarg(const char *) to;
};

struct ibcs2_sys_scoinfo_args {
	syscallarg(struct scoutsname *) bp;
	syscallarg(int) len;
};

/*
 * System call prototypes.
 */

int	sys_nosys	__P((struct proc *, void *, register_t *));
int	sys_exit	__P((struct proc *, void *, register_t *));
int	sys_fork	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_read	__P((struct proc *, void *, register_t *));
int	sys_write	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_open	__P((struct proc *, void *, register_t *));
int	sys_close	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_waitsys	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_creat	__P((struct proc *, void *, register_t *));
int	sys_link	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_unlink	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_execv	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_chdir	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_time	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_mknod	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_chmod	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_chown	__P((struct proc *, void *, register_t *));
int	sys_obreak	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_stat	__P((struct proc *, void *, register_t *));
int	compat_43_sys_lseek	__P((struct proc *, void *, register_t *));
int	sys_getpid	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_mount	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_umount	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_setuid	__P((struct proc *, void *, register_t *));
int	sys_getuid	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_stime	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_alarm	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_fstat	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_pause	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_utime	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_access	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_nice	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_statfs	__P((struct proc *, void *, register_t *));
int	sys_sync	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_kill	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_fstatfs	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_pgrpsys	__P((struct proc *, void *, register_t *));
int	sys_dup	__P((struct proc *, void *, register_t *));
int	sys_pipe	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_times	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_plock	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_setgid	__P((struct proc *, void *, register_t *));
int	sys_getgid	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_sigsys	__P((struct proc *, void *, register_t *));
#ifdef SYSVMSG
int	ibcs2_sys_msgsys	__P((struct proc *, void *, register_t *));
#else
#endif
int	ibcs2_sys_sysi86	__P((struct proc *, void *, register_t *));
#ifdef SYSVSHM
int	ibcs2_sys_shmsys	__P((struct proc *, void *, register_t *));
#else
#endif
#ifdef SYSVSEM
int	ibcs2_sys_semsys	__P((struct proc *, void *, register_t *));
#else
#endif
int	ibcs2_sys_ioctl	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_uadmin	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_utssys	__P((struct proc *, void *, register_t *));
int	sys_fsync	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_execve	__P((struct proc *, void *, register_t *));
int	sys_umask	__P((struct proc *, void *, register_t *));
int	sys_chroot	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_fcntl	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_ulimit	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_rmdir	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_mkdir	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_getdents	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_sysfs	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_getmsg	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_putmsg	__P((struct proc *, void *, register_t *));
int	sys_poll	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_symlink	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_lstat	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_readlink	__P((struct proc *, void *, register_t *));
int	sys_fchmod	__P((struct proc *, void *, register_t *));
int	sys___posix_fchown	__P((struct proc *, void *, register_t *));
int	sys___sigreturn14	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_sigaltstack	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_statvfs	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_fstatvfs	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_mmap	__P((struct proc *, void *, register_t *));
int	sys_mprotect	__P((struct proc *, void *, register_t *));
int	sys_munmap	__P((struct proc *, void *, register_t *));
int	sys_fchdir	__P((struct proc *, void *, register_t *));
int	sys_readv	__P((struct proc *, void *, register_t *));
int	sys_writev	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_memcntl	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_gettimeofday	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_settimeofday	__P((struct proc *, void *, register_t *));
int	compat_43_sys_truncate	__P((struct proc *, void *, register_t *));
int	compat_43_sys_ftruncate	__P((struct proc *, void *, register_t *));
int	xenix_sys_locking	__P((struct proc *, void *, register_t *));
int	xenix_sys_rdchk	__P((struct proc *, void *, register_t *));
int	xenix_sys_chsize	__P((struct proc *, void *, register_t *));
int	xenix_sys_ftime	__P((struct proc *, void *, register_t *));
int	xenix_sys_nap	__P((struct proc *, void *, register_t *));
int	sys_select	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_eaccess	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_sigaction	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_sigprocmask	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_sigpending	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_sigsuspend	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_getgroups	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_setgroups	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_sysconf	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_pathconf	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_fpathconf	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_rename	__P((struct proc *, void *, register_t *));
int	ibcs2_sys_scoinfo	__P((struct proc *, void *, register_t *));
#endif /* _IBCS2_SYS__SYSCALLARGS_H_ */
