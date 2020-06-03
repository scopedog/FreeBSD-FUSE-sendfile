/******************************************************************************
*
*	PROGRAM  : log.c
*	LANGUAGE : C
*	REMARKS  : Functions regarding log.
*	AUTHOR   : Hiroshi Nishida
*	COPYRIGHT: Copyright (C) 2006- ASUSA, ASJ
*
*******************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#ifndef __WIN32__
#include <grp.h>
#endif
#include "util.h"

static int	LogFd;
static char	LogFile[PATH_MAX + 1];

#ifndef	True
#define True		1
#endif
#ifndef	False
#define False		0
#endif
#define	LOGPERM		( S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP )
#define MAXLOGSZ	100000

/*****************************************************************/

#if 0
/* Vfprintf error messages */
static void
Cmnerr(const char *fmt, va_list ap)
{
	int		err;

	/* Save errno */
	err = errno;

	/* Output error message */
	vfprintf(stderr, fmt, ap);
	if (err != 0)
		fprintf(stderr, ": %s", strerror(err));
	putc('\n', stderr);
}

/* Output error message and exit */
void
Fatal(const char *fmt,...)
{
	va_list		ap;

	va_start(ap, fmt);
	Cmnerr(fmt, ap);
	va_end(ap);
	exit(1);
}
#endif

/* Initialize log file */
int
InitLog(char *logfile, char *command, int daemonflg)
{
	struct stat	sbuf;
	struct group   *gp;
	int		fl;

	//Daemon = daemonflg;

	/* Set log file */
	if (logfile == NULL) {
		if (command == NULL) {
			ErrorExit("Error: InitLog(): arg 2 must be non NULL");
		}

		// Automatically set file name from command
		snprintf(LogFile, sizeof(LogFile), "/var/log/%s.log", command);
		logfile = LogFile;
	}
	else {
		strncpy(LogFile, logfile, sizeof(LogFile));
		LogFile[PATH_MAX] = '\0';
	}

	/* Open log file */
	if ((LogFd = open(logfile, O_WRONLY | O_APPEND | O_CREAT, LOGPERM)) < 0) {
		snprintf(LogFile, sizeof(LogFile), "%s.log", command);
		logfile = LogFile;
		if ((LogFd = open(logfile, O_WRONLY | O_APPEND | O_CREAT, LOGPERM)) < 0) {
			ErrorExit("Error: InitLog: open %s: %s\n", logfile, strerror(errno));
		}
	}

	/* Set "close-on-exec" flag */
	if ((fl = fcntl(LogFd, F_GETFD, 0)) < 0 ||
	    (fcntl(LogFd, F_SETFD, fl | FD_CLOEXEC) < 0)) {
		close(LogFd);
		LogFd = -1;
		return -1;
	}
	if (!daemonflg)
		return 1;

	/* Get "daemon" group ID */
	if ((gp = getgrnam("daemon")) == NULL) {
		close(LogFd);
		LogFd = -1;
		errno = EINVAL;
		return (-1);
	}
	/*
	 * Change user ID and group ID if the owner of the log file is not
	 * root or its group is not daemon
	 */
	if (fstat(LogFd, &sbuf) < 0) {
		close(LogFd);
		LogFd = -1;
		return (-1);
	}
	if ((sbuf.st_uid != 0) || (sbuf.st_gid != gp->gr_gid))
		fchown(LogFd, 0, gp->gr_gid);

	/* Set "close-on-exec" flag */
	if ((fl = fcntl(LogFd, F_GETFD, 0)) < 0 ||
	    (fcntl(LogFd, F_SETFD, fl | FD_CLOEXEC) < 0)) {
		close(LogFd);
		LogFd = -1;
		return (-1);
	}
	if (!daemonflg)
		return (1);

	/* Get "daemon" group ID */
	if ((gp = getgrnam("daemon")) == NULL) {
		close(LogFd);
		LogFd = -1;
		errno = EINVAL;
		return (-1);
	}
	/*
	 * Change user ID and group ID if the owner of the log file is not
	 * root or its group is not daemon
	 */
	if (fstat(LogFd, &sbuf) < 0) {
		close(LogFd);
		LogFd = -1;
		return (-1);
	}
	if ((sbuf.st_uid != 0) || (sbuf.st_gid != gp->gr_gid))
		fchown(LogFd, 0, gp->gr_gid);

	return (1);
}

void
Log(const char *fmt,...)
{
	time_t		t;
	char           *tm, *p;
	struct stat	sbuf;
	va_list		ap;
	char		buf[BUFSIZ];

	/* Obtain the current time and convert to string */
	time(&t);
	tm = ctime(&t);

	/* Conver '\n' to space and copy to buffer */
	tm[24] = ':';
	strcat(tm, " ");
	strncpy(buf, tm, 26);

	/* Concatenate fmt to buf */
	va_start(ap, fmt);
	p = buf + 26;
	vsnprintf(p, sizeof(buf) - 26, fmt, ap);
	va_end(ap);

	/* Concatenate '\n' */
	strcat(buf, "\n");

	/* If the size of log file is too big, save it as a gzipped file */
	if (fstat(LogFd, &sbuf) == 0) {
		if (sbuf.st_size > MAXLOGSZ) {
			time_t		_t = time(NULL);
			struct tm	*t = localtime(&_t);
			char		bkup_file[PATH_MAX], buf[256];

			/* Set file name */
			snprintf(bkup_file, sizeof(bkup_file),
				"%s-%04d%02d%02d-%02d%02d%02d",
				LogFile, t->tm_year + 1900, t->tm_mon + 1,
				t->tm_mday, t->tm_hour, t->tm_min,
				t->tm_sec);

			/* Copy LogFile to bkup_file */
			snprintf(buf, sizeof(buf), "cp %s %s",
				 LogFile, bkup_file);
			system(buf);

			/* Gzip bkup_file */
			snprintf(buf, sizeof(buf), "gzip -f -q %s &",bkup_file);
			system(buf);

			/* Reset log file */
			ftruncate(LogFd, 0);
		}
	}

	/* Write message to log file */
	write(LogFd, buf, strlen(buf));

	/* Write message to stderr */
	if (!Daemon)
		write(2, p, strlen(p));
}
