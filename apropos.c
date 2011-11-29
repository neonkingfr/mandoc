/*	$Id$ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apropos_db.h"
#include "mandoc.h"
#include "manpath.h"

static	int	 cmp(const void *, const void *);
static	void	 list(struct res *, size_t, void *);
static	void	 usage(void);

static	char	*progname;

int
main(int argc, char *argv[])
{
	int		 ch, rc, whatis;
	struct manpaths	 paths;
	size_t		 terms;
	struct opts	 opts;
	struct expr	*e;
	char		*defpaths, *auxpaths;
	extern int	 optind;
	extern char	*optarg;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	whatis = 0 == strncmp(progname, "whatis", 6);

	memset(&paths, 0, sizeof(struct manpaths));
	memset(&opts, 0, sizeof(struct opts));

	auxpaths = defpaths = NULL;
	e = NULL;

	while (-1 != (ch = getopt(argc, argv, "M:m:S:s:")))
		switch (ch) {
		case ('M'):
			defpaths = optarg;
			break;
		case ('m'):
			auxpaths = optarg;
			break;
		case ('S'):
			opts.arch = optarg;
			break;
		case ('s'):
			opts.cat = optarg;
			break;
		default:
			usage();
			return(EXIT_FAILURE);
		}

	argc -= optind;
	argv += optind;

	if (0 == argc) 
		return(EXIT_SUCCESS);

	rc = 0;

	manpath_parse(&paths, defpaths, auxpaths);

	e = whatis ? termcomp(argc, argv, &terms) :
		     exprcomp(argc, argv, &terms);
		
	if (NULL == e) {
		fprintf(stderr, "%s: Bad expression\n", progname);
		goto out;
	}

	rc = apropos_search
		(paths.sz, paths.paths,
		 &opts, e, terms, NULL, list);

	if (0 == rc)
		fprintf(stderr, "%s: Error reading "
				"manual database\n", progname);

out:
	manpath_free(&paths);
	exprfree(e);

	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* ARGSUSED */
static void
list(struct res *res, size_t sz, void *arg)
{
	int		 i;

	qsort(res, sz, sizeof(struct res), cmp);

	for (i = 0; i < (int)sz; i++)
		printf("%s(%s%s%s) - %s\n", res[i].title,
				res[i].cat,
				*res[i].arch ? "/" : "",
				*res[i].arch ? res[i].arch : "",
				res[i].desc);
}

static int
cmp(const void *p1, const void *p2)
{

	return(strcmp(((const struct res *)p1)->title,
				((const struct res *)p2)->title));
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s "
			"[-M manpath] "
			"[-m manpath] "
			"[-S arch] "
			"[-s section] "
			"expression...\n",
			progname);
}
