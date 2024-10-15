/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Fall.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"

static int lflag;
static int rval;
static const char *filename;

static void usage(void) __dead2;
static void scanfiles(char *argv[], int cooked);
static void raw_cat(int);


/*
 * Memory strategy threshold, in pages: if physmem is larger than this,
 * use a large buffer.
 */
#define	PHYSPAGES_THRESHOLD (32 * 1024)

/* Maximum buffer size in bytes - do not allow it to grow larger than this. */
#define	BUFSIZE_MAX (2 * 1024 * 1024)

/*
 * Small (default) buffer size in bytes. It's inefficient for this to be
 * smaller than MAXPHYS.
 */
#define	BUFSIZE_SMALL (MAXPHYS)


/*
 * For the bootstrapped cat binary (needed for locked appending to METALOG), we
 * disable all flags except -l and -u to avoid non-portable function calls.
 * In the future we may instead want to write a small portable bootstrap tool
 * that locks the output file before writing to it. However, for now
 * bootstrapping cat without multibyte support is the simpler solution.
 */
#define SUPPORTED_FLAGS "lu"
main(int argc, char *argv[])
{
    int ch;
    struct flock stdout_lock;

    setlocale(LC_CTYPE, "");

    while ((ch = getopt(argc, argv, SUPPORTED_FLAGS)) != -1)
	switch (ch) {
	case 'l':
	    lflag = 1;
	    break;
	case 'u':
	    setbuf(stdout, NULL);
	    break;
	default:
	    usage();
	}
    argv += optind;
    argc -= optind;

    if (lflag) {
	stdout_lock.l_len = 0;
	stdout_lock.l_start = 0;
	stdout_lock.l_type = F_WRLCK;
	stdout_lock.l_whence = SEEK_SET;
	if (fcntl(STDOUT_FILENO, F_SETLKW, &stdout_lock) == -1)
	    err(EXIT_FAILURE, "stdout");
    }

    scanfiles(argv, 0);
    if (fclose(stdout))
	err(1, "stdout");
    exit(rval);
    /* NOTREACHED */
}

static void
usage(void)
{

    fprintf(stderr, "usage: cat [-" SUPPORTED_FLAGS "] [file ...]\n");
    exit(1);
    /* NOTREACHED */
}

static void
scanfiles(char *argv[], int cooked __unused)
{
    int fd, i;
    char *path;

    i = 0;
    fd = -1;
    while ((path = argv[i]) != NULL || i == 0) {
	if (path == NULL || strcmp(path, "-") == 0) {
	    filename = "stdin";
	    fd = STDIN_FILENO;
	} else {
	    filename = path;
	    // fd = fileargs_open(fa, path);
		fd = open(path, 0);;
	}
	if (fd < 0) {
	    warn("%s", path);
	    rval = 1;
	} else {
	    raw_cat(fd);
	    if (fd != STDIN_FILENO)
		close(fd);
	}
	if (path == NULL)
	    break;
	++i;
    }
}

static void
raw_cat(int rfd)
{
    long pagesize;
    int off, wfd;
    ssize_t nr, nw;
    static size_t bsize;
    static char *buf = NULL;
    struct stat sbuf;

    wfd = fileno(stdout);
    if (buf == NULL) {
	if (fstat(wfd, &sbuf))
	    err(1, "stdout");
	if (S_ISREG(sbuf.st_mode)) {
	    /* If there's plenty of RAM, use a large copy buffer */
	    if (sysconf(_SC_PHYS_PAGES) > PHYSPAGES_THRESHOLD)
		bsize = MIN(BUFSIZE_MAX, MAXPHYS * 8);
	    else
		bsize = BUFSIZE_SMALL;
	} else {
	    bsize = sbuf.st_blksize;
	    pagesize = sysconf(_SC_PAGESIZE);
	    if (pagesize > 0)
		bsize = MAX(bsize, (size_t)pagesize);
	}
	if ((buf = malloc(bsize)) == NULL)
	    err(1, "malloc() failure of IO buffer");
    }
    while ((nr = read(rfd, buf, bsize)) > 0)
	for (off = 0; nr; nr -= nw, off += nw)
	    if ((nw = write(wfd, buf + off, (size_t)nr)) < 0)
		err(1, "stdout");
    if (nr < 0) {
	warn("%s", filename);
	rval = 1;
    }
}
