/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
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

static void usage(void);
static int scanfiles(char *paths[], int path_count);
static int raw_cat(int, char *);

int main(int argc, char *argv[])
{
    setlocale(LC_CTYPE, "");

    int ch;
    while ((ch = getopt(argc, argv, "h")) != -1)
        if (ch == 'h')
            usage();
    argv += optind;
    int path_count = argc - optind;

    int rval = scanfiles(argv, argc);
    return rval;
}

static void
usage(void)
{
    fprintf(stderr, "usage: cat [-] [file ...]\n");
    exit(1);
}

static int
scanfiles(char *paths[], int path_count)
{
    int rval = 0;
    for (int i = 0; i < path_count; i++)
    {
        char *path = paths[i];
        if (path == NULL)
            break;

        if (path == NULL || strcmp(path, "-") == 0)
        {
            rval = rval || raw_cat(STDIN_FILENO, "stdin");
        }
        else
        {
            int fd = open(path, 0);
            if (fd < 0)
            {
                warn("%s", path);
                rval = 1;
            }
            else
            {
                rval = rval || raw_cat(fd, path);
                if (fd != STDIN_FILENO)
                    close(fd);
            }
        }
    }

    return rval;
}

static int
raw_cat(int rfd, char *filename)
{
    long pagesize;
    int off, wfd;
    ssize_t nr, nw;
    static size_t bsize;
    static char *buf = NULL;
    struct stat sbuf;

    wfd = fileno(stdout);
    if (fstat(wfd, &sbuf))
        err(1, "stdout");

    bsize = sbuf.st_blksize;
    pagesize = sysconf(_SC_PAGESIZE);
    bsize = MAX(bsize, (size_t)pagesize);
    if ((buf = malloc(bsize)) == NULL)
        err(1, "malloc() failure of IO buffer");
    while ((nr = read(rfd, buf, bsize)) > 0)
        for (off = 0; nr; nr -= nw, off += nw)
            if ((nw = write(wfd, buf + off, (size_t)nr)) < 0)
                err(1, "stdout");
    if (nr < 0)
    {
        warn("%s", filename);
        return 1;
    }
    return 0;
}
