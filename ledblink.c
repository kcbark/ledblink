/*
 * Copyright (c) 2002, Kalle Carlbark
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the
 *    do *censored* entation and/or other materials provided with the 
 *    distribution.
 * 3. You may not under any circumstances change this license to GPL or dual
 *    license code released under to rootbsd license as gpl.
 * 4. Neither the name of the RootBSD nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 5. Vendors for any derivated commercial software based on code licensed by
 *    the rootbsd license must provide the original programmers with a symbolic
 *    amount of money to show their appreciation to the original programmers
 *    and/or contributors work.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 *		This program fork()'s itself into the background and checks for 
 *		new mail by using stat() to check when the mailfile is being changed. 
 *		When new mail arrives this program calls ioctl() with the
 *		KDSETLED request which'll be switching the capslock led on and off 
 *		making it blink.
 *		For FreeBSD 4.x and FreeBSD -CURRENT, probably 3.x as well.
 *
 *		Author: kc@RootBSD.net
 *		
 *		Greets goes to skypher.
 *
 *	$Id: ledblink.c,v 1.5 2002/10/03 20:49:20 kc Exp $
 *
 */

#include <sys/kbio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/capsicum.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <string.h>
#include <errno.h>
#include <sysexits.h>
#include <signal.h>
#include <time.h>
#include <err.h>

#define DURATION 	250000
#define OFF 		0	
#define ON			1
#define CONSOLE		0
#define X			1

static void usage 		__P((void));
static int daemon_init 	__P((void));
static int blink 		__P((int, int));
static int checkmail 	__P((char *));
static int checkttys 	__P((int));
static void getsig 		__P((int));

extern char *__progname;
struct stat statbuf;

char *ttys[] = {
	"/dev/ttyv0",
	"/dev/ttyv1",
	"/dev/ttyv2",
	"/dev/ttyv3",
	"/dev/ttyv4",
	"/dev/ttyv5",
	"/dev/ttyv6",
	"/dev/ttyv7",
	NULL,
};
		
		
char *xtty = "/dev/ttyv8";
int fd[9];

int
main(argc, argv)
	int 	argc;
	char	*argv[];
{
	int		ch, file, gotmail = 0, ttys, state = NULL;
	char	*mailfile;

	if ((mailfile = getenv("MAIL")) == NULL)
		bzero(mailfile, sizeof(mailfile));

	while ((ch = getopt(argc, argv, "cf:ht:x")) != -1) 
		switch (ch) {
			
			case 'c':
				state = CONSOLE;
				break;

			case 'f':
				mailfile = optarg;
				if ((file = open(mailfile, O_RDONLY)) == -1) {
					fprintf(stderr, "%s: %s: %s\n", __progname, mailfile, strerror(errno));
					exit(1);
				}
				break;

			case 'h':
				usage();
				break;
			
			case 't':
				xtty = optarg;
				break;
		
			case 'x':
				setuid(getuid());
				if (getuid() != 0) {
					fprintf(stderr, "The -x flag need to be run as root\n");
					exit(1);
				}
				state = X;
				break;

			case '?':
			default:
				usage();
				/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;
	
	if (optind < 2)
		usage();
	
	signal(SIGINT, getsig);
	signal(SIGTERM, getsig);
	
	if (daemon_init() != NULL) 
		exit(1);
	
	if ((ttys = checkttys(state)) == -1) {
		fprintf(stderr, "%s: No valid tty's\n", __progname);
		exit(EX_NOINPUT);
	}
    
    cap_enter();

	for (;;) {
		if ((checkmail(mailfile)) > 0)  {
			gotmail = 1;
		}
		else
			gotmail = 0;
		if (gotmail == 1) {
			blink(ON, ttys);
			blink(OFF, ttys);
		}
		
		usleep(50000);
	}
	
	
	return(0); /* NOTREACHED */
}

static int
blink(OnOFF, ttys)
	int		OnOFF;
	int		ttys;
{
		int i;
		if (ttys == NULL) {
			if (ioctl(fd[0], KDSETLED, OnOFF) == -1) {
				warnx("%s", strerror(errno));
				exit(1);
			}
		} else {
			for(i=0;i<ttys;i++) {
				if (ioctl(fd[i], KDSETLED, OnOFF) == -1) {
					warnx("%s", strerror(errno));
					exit(1);
				}
			}
		}

	usleep(DURATION);
	return(0);
}

static int
checkmail(mailfile)
	char *mailfile;
{

	if (mailfile == NULL) {
			fprintf(stderr, "No mailfile to check\n");
			exit(EX_NOINPUT);
	}

	if (stat(mailfile, &statbuf) == -1) {
		fprintf(stderr, "%s: %s: %s\n", __progname, mailfile, strerror(errno));
		exit(EX_DATAERR);
	}
	
	if (statbuf.st_atime < statbuf.st_mtime)
		return(statbuf.st_mtime);

	return(0);
}

static void
usage()
{
	printf("Options:\n");
	printf("-c\t\t\tBlink in Console.\n");
	printf("-f mailfile\t\tUse mailfile instead of $MAIL.\n");
	printf("-t <tty X is using>\tUse another tty than the default /dev/ttyv8\n");
	printf("-x\t\t\tBlink in X (require to be run as root).\n\n");
	fprintf(stderr, "Usage: %s [-xc] [-f mailfile] [-t /dev/tty*]\n", __progname);
	exit(EX_USAGE);
}

static int
checkttys(state)
	int		state;
{
	int 	working = 0;
	int		i		= 0;
	int		c		= 1;
	
	/* XXX: X may use another tty. */
	if (state == X) {
		close(fd[0]);
		if ((fd[0] = open(xtty, O_RDONLY)) == -1) {
   			warnx("%s", strerror(errno));
      		exit(EX_DATAERR);
		}
		return(NULL);
	}
	
	while (ttys[i] != NULL) {
		if ((fd[c] = open(ttys[i], O_RDONLY)) != -1) {
			working += 1;
		}
		i++;
		c++;
	}

	if (working == 0) {
		fprintf(stderr, "%s: No valid tty's\n", __progname);
		return(-1);
	}
	
	return(working);
}

static void
getsig(signo)
	int 	signo;
{
	exit(1);
}

/* Own version of daemon() :-) */
static int
daemon_init()
{
	pid_t pid;
	if ((pid = fork()) < 0) {
		warnx("%s", strerror(errno));
		exit(EX_UNAVAILABLE);
	} else if (pid != 0)
		exit(EX_OK);
	
	setsid();
	chdir("/");
	umask(0);
	sysconf(_SC_OPEN_MAX);

	return(0);
}
		
