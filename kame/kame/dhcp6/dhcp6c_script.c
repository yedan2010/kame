/*	$KAME: dhcp6c_script.c,v 1.1 2003/04/11 07:13:22 jinmei Exp $	*/

/*
 * Copyright (C) 2003 WIDE Project.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/stat.h>

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

#include <netinet/in.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "dhcp6.h"
#include "config.h"
#include "common.h"

static char dnsserver_str[] = "new_domain_name_servers";

static int safefile __P((const char *));

void
client6_script(ifp, state, optinfo)
	struct dhcp6_if *ifp;
	int state;
	struct dhcp6_optinfo *optinfo;
{
	int i, dnsservers, envc, elen;
	char **envp, *s, *scriptpath;
	char reason[] = "REASON=NBI";
	struct dhcp6_listval *v;
	pid_t pid, wpid;

	/* if a script is not specified, do nothing */
	if (ifp->scriptpath == NULL)
		return;
	scriptpath = ifp->scriptpath;

	/* initialize counters */
	dnsservers = 0;
	envc = 2;     /* we always include the reason and the terminator*/

	/* count the number of variables */
	for (v = TAILQ_FIRST(&optinfo->dns_list); v; v = TAILQ_NEXT(v, link))
		dnsservers++;
	envc += dnsservers ? 1 : 0;

	/* allocate an environments array */
	if ((envp = malloc(sizeof (char *) * envc)) == NULL) {
		dprintf(LOG_NOTICE, FNAME,
		    "failed to allocate environment buffer");
		return;
	}
	memset(envp, 0, sizeof (char *) * envc);

	/*
	 * Copy the parameters as environment variables
	 */
	i = 0;
	/* reason */
	if ((envp[i++] = strdup(reason)) == NULL) {
		dprintf(LOG_NOTICE, FNAME,
		    "failed to allocate reason strings");
		goto clean;
	}
	/* "var=addr1 addr2 ... addrN" + null char for termination */
	elen = sizeof (dnsserver_str) +
	    (INET6_ADDRSTRLEN + 1) * dnsservers + 1;
	if ((s = envp[i++] = malloc(elen)) == NULL) {
		dprintf(LOG_NOTICE, FNAME,
		    "failed to allocate strings for DNS servers");
		goto clean;
	}
	memset(s, 0, elen);
	strcpy(s, dnsserver_str);
	s += strlen(dnsserver_str);
	*s++ = '=';
	for (v = TAILQ_FIRST(&optinfo->dns_list); v; v = TAILQ_NEXT(v, link)) {
		char *addr;

		addr = in6addr2str(&v->val_addr6, 0);
		strcpy(s, addr);
		s += strlen(addr);
		*s++ = ' ';
	}
	*s = '\0';

	/* launch the script */
	pid = fork();
	if (pid < 0) {
		dprintf(LOG_ERR, FNAME, "failed to fork: %s", strerror(errno));
		goto clean;
	} else if (pid) {
		int wstatus;

		do {
			wpid = wait(&wstatus);
		} while (wpid != pid && wpid > 0);

		if (wpid < 0)
			dprintf(LOG_ERR, FNAME, "wait: %s", strerror(errno));
		else {
			dprintf(LOG_DEBUG, FNAME,
			    "script \"%s\" terminated", scriptpath);
		}
	} else {
		char *argv[2];
		int fd;

		argv[0] = scriptpath;
		argv[1] = NULL;

		if (safefile(scriptpath)) {
			dprintf(LOG_ERR, FNAME,
			    "script \"%s\" cannot be executed safely",
			    scriptpath);
			exit(1);
		}

		if ((fd = open("/dev/null", O_RDWR)) != -1) {
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO)
				close(fd);
		}

		execve(scriptpath, argv, envp);

		dprintf(LOG_ERR, FNAME, "child: exec failed: %s",
		    strerror(errno));
		exit(0);
	}

  clean:
	for (i = 0; i < envc; i++)
		free(envp[i]);
	free(envp);

	return;
}

static int
safefile(path)
	const char *path;
{
	struct stat s;
	uid_t myuid;

	/* no setuid */
	if (getuid() != geteuid()) {
		dprintf(LOG_NOTICE, FNAME,
		    "setuid'ed execution not allowed\n");
		return (-1);
	}

	if (lstat(path, &s) != 0) {
		dprintf(LOG_NOTICE, FNAME, "lstat failed: %s",
		    strerror(errno));
		return (-1);
	}

	/* the file must be owned by the running uid */
	myuid = getuid();
	if (s.st_uid != myuid) {
		dprintf(LOG_NOTICE, FNAME, "%s has invalid owner uid\n", path);
		return (-1);
	}

	switch (s.st_mode & S_IFMT) {
	case S_IFREG:
		break;
	default:
		dprintf(LOG_NOTICE, FNAME, "%s is an invalid file type 0x%o\n",
		    path, (s.st_mode & S_IFMT));
		return (-1);
	}

	return (0);
}