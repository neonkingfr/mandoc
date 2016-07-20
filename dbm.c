/*	$Id$ */
/*
 * Copyright (c) 2016 Ingo Schwarze <schwarze@openbsd.org>
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
 *
 * Map-based version of the mandoc database, for read-only access.
 * The interface is defined in "dbm.h".
 */
#include "config.h"

#include <assert.h>
#include <endian.h>
#if HAVE_ERR
#include <err.h>
#endif
#include <errno.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mansearch.h"
#include "dbm_map.h"
#include "dbm.h"

struct macro {
	int32_t	value;
	int32_t	pages;
};

struct page {
	int32_t	name;
	int32_t	sect;
	int32_t	arch;
	int32_t	desc;
	int32_t	file;
};

enum iter {
	ITER_NONE = 0,
	ITER_NAME,
	ITER_SECT,
	ITER_ARCH,
	ITER_DESC,
	ITER_MACRO
};

static struct macro	*macros[MACRO_MAX];
static int32_t		 nvals[MACRO_MAX];
static struct page	*pages;
static int32_t		 npages;
static enum iter	 iteration;

static struct dbm_res	 page_bytitle(enum iter, const struct dbm_match *);
static struct dbm_res	 page_byarch(const struct dbm_match *);
static struct dbm_res	 page_bymacro(int32_t, const struct dbm_match *);
static char		*macro_bypage(int32_t, int32_t);


/*** top level functions **********************************************/

/*
 * Open a disk-based mandoc database for read-only access.
 * Map the pages and macros[] arrays.
 * Return 0 on success.  Return -1 and set errno on failure.
 */
int
dbm_open(const char *fname)
{
	const int32_t	*mp, *ep;
	int32_t		 im;

	if (dbm_map(fname) == -1)
		return -1;

	if ((npages = be32toh(*dbm_getint(4))) < 0) {
		warnx("dbm_open(%s): Invalid number of pages: %d",
		    fname, npages);
		goto fail;
	}
	pages = (struct page *)dbm_getint(5);

	if ((mp = dbm_get(*dbm_getint(2))) == NULL) {
		warnx("dbm_open(%s): Invalid offset of macros array", fname);
		goto fail;
	}
	if (be32toh(*mp) != MACRO_MAX) {
		warnx("dbm_open(%s): Invalid number of macros: %d",
		    fname, be32toh(*mp));
		goto fail;
	}
	for (im = 0; im < MACRO_MAX; im++) {
		if ((ep = dbm_get(*++mp)) == NULL) {
			warnx("dbm_open(%s): Invalid offset of macro %d",
			    fname, im);
			goto fail;
		}
		nvals[im] = be32toh(*ep);
		macros[im] = (struct macro *)++ep;
	}
	return 0;

fail:
	dbm_unmap();
	errno = EFTYPE;
	return -1;
}

void
dbm_close(void)
{
	dbm_unmap();
}


/*** functions for handling pages *************************************/

int32_t
dbm_page_count(void)
{
	return npages;
}

/*
 * Give the caller pointers to the data for one manual page.
 */
struct dbm_page *
dbm_page_get(int32_t ip)
{
	static struct dbm_page	 res;

	assert(ip >= 0);
	assert(ip < npages);
	res.name = dbm_get(pages[ip].name);
	res.sect = dbm_get(pages[ip].sect);
	res.arch = pages[ip].arch ? dbm_get(pages[ip].arch) : NULL;
	res.desc = dbm_get(pages[ip].desc);
	res.file = dbm_get(pages[ip].file);
	res.addr = dbm_addr(pages + ip);
	return &res;
}

/*
 * Functions to start filtered iterations over manual pages.
 */
void
dbm_page_byname(const struct dbm_match *match)
{
	assert(match != NULL);
	page_bytitle(ITER_NAME, match);
}

void
dbm_page_bysect(const struct dbm_match *match)
{
	assert(match != NULL);
	page_bytitle(ITER_SECT, match);
}

void
dbm_page_byarch(const struct dbm_match *match)
{
	assert(match != NULL);
	page_byarch(match);
}

void
dbm_page_bydesc(const struct dbm_match *match)
{
	assert(match != NULL);
	page_bytitle(ITER_DESC, match);
}

void
dbm_page_bymacro(int32_t im, const struct dbm_match *match)
{
	assert(im >= 0);
	assert(im < MACRO_MAX);
	assert(match != NULL);
	page_bymacro(im, match);
}

/*
 * Return the number of the next manual page in the current iteration.
 */
struct dbm_res
dbm_page_next(void)
{
	struct dbm_res			 res = {-1, 0};

	switch(iteration) {
	case ITER_NONE:
		return res;
	case ITER_ARCH:
		return page_byarch(NULL);
	case ITER_MACRO:
		return page_bymacro(0, NULL);
	default:
		return page_bytitle(iteration, NULL);
	}
}

/*
 * Functions implementing the iteration over manual pages.
 */
static struct dbm_res
page_bytitle(enum iter arg_iter, const struct dbm_match *arg_match)
{
	static const struct dbm_match	*match;
	static const char		*cp;	
	static int32_t			 ip;
	struct dbm_res			 res = {-1, 0};

	assert(arg_iter == ITER_NAME || arg_iter == ITER_DESC ||
	    arg_iter == ITER_SECT);

	/* Initialize for a new iteration. */

	if (arg_match != NULL) {
		iteration = arg_iter;
		match = arg_match;
		switch (iteration) {
		case ITER_NAME:
			cp = dbm_get(pages[0].name);
			break;
		case ITER_SECT:
			cp = dbm_get(pages[0].sect);
			break;
		case ITER_DESC:
			cp = dbm_get(pages[0].desc);
			break;
		default:
			abort();
		}
		ip = 0;
		return res;
	}

	/* Search for a name. */

	while (ip < npages) {
		if (iteration == ITER_NAME)
			cp++;
		if (dbm_match(match, cp))
			break;
		cp = strchr(cp, '\0') + 1;
		if (iteration == ITER_DESC)
			ip++;
		else if (*cp == '\0') {
			cp++;
			ip++;
		}
	}

	/* Reached the end without a match. */

	if (ip == npages) {
		iteration = ITER_NONE;
		match = NULL;
		cp = NULL;
		return res;
	}

	/* Found a match; save the quality for later retrieval. */

	res.page = ip;
	res.bits = iteration == ITER_NAME ? cp[-1] : 0;

	/* Skip the remaining names of this page. */

	if (++ip < npages) {
		do {
			cp++;
		} while (cp[-1] != '\0' ||
		    (iteration != ITER_DESC && cp[-2] != '\0'));
	}
	return res;
}

static struct dbm_res
page_byarch(const struct dbm_match *arg_match)
{
	static const struct dbm_match	*match;
	struct dbm_res			 res = {-1, 0};
	static int32_t			 ip;
	const char			*cp;	

	/* Initialize for a new iteration. */

	if (arg_match != NULL) {
		iteration = ITER_ARCH;
		match = arg_match;
		ip = 0;
		return res;
	}

	/* Search for an architecture. */

	for ( ; ip < npages; ip++)
		if (pages[ip].arch)
			for (cp = dbm_get(pages[ip].arch);
			    *cp != '\0';
			    cp = strchr(cp, '\0') + 1)
				if (dbm_match(match, cp)) {
					res.page = ip++;
					return res;
				}

	/* Reached the end without a match. */

	iteration = ITER_NONE;
	match = NULL;
	return res;
}

static struct dbm_res
page_bymacro(int32_t im, const struct dbm_match *match)
{
	static const int32_t	*pp;
	struct dbm_res		 res = {-1, 0};
	const char		*cp;
	int32_t			 iv;

	assert(im >= 0);
	assert(im < MACRO_MAX);

	/* Initialize for a new iteration. */

	if (match != NULL) {
		iteration = ITER_MACRO;
		cp = nvals[im] ? dbm_get(macros[im]->value) : NULL;
		for (iv = 0; iv < nvals[im]; iv++) {
			if (dbm_match(match, cp))
				break;
			cp = strchr(cp, '\0') + 1;
		}
		pp = iv == nvals[im] ? NULL : dbm_get(macros[im][iv].pages);
		return res;
	}
	if (iteration != ITER_MACRO)
		return res;

	/* No more matches. */

	if (pp == NULL || *pp == 0) {
		iteration = ITER_NONE;
		pp = NULL;
		return res;
	}

	/* Found a match. */

	res.page = (struct page *)dbm_get(*pp++) - pages;
	return res;
}


/*** functions for handling macros ************************************/

int32_t
dbm_macro_count(int32_t im)
{
	assert(im >= 0);
	assert(im < MACRO_MAX);
	return nvals[im];
}

struct dbm_macro *
dbm_macro_get(int32_t im, int32_t iv)
{
	static struct dbm_macro macro;

	assert(im >= 0);
	assert(im < MACRO_MAX);
	assert(iv >= 0);
	assert(iv < nvals[im]);
	macro.value = dbm_get(macros[im][iv].value);
	macro.pp = dbm_get(macros[im][iv].pages);
	return &macro;
}

/*
 * Filtered iteration over macro entries.
 */
void
dbm_macro_bypage(int32_t im, int32_t ip)
{
	assert(im >= 0);
	assert(im < MACRO_MAX);
	assert(ip != 0);
	macro_bypage(im, ip);
}

char *
dbm_macro_next(void)
{
	return macro_bypage(MACRO_MAX, 0);
}

static char *
macro_bypage(int32_t arg_im, int32_t arg_ip)
{
	static const int32_t	*pp;
	static int32_t		 im, ip, iv;

	/* Initialize for a new iteration. */

	if (arg_im < MACRO_MAX && arg_ip != 0) {
		im = arg_im;
		ip = arg_ip;
		pp = dbm_get(macros[im]->pages);
		iv = 0;
		return NULL;
	}
	if (im >= MACRO_MAX)
		return NULL;

	/* Search for the next value. */

	while (iv < nvals[im]) {
		if (*pp == ip)
			break;
		if (*pp == 0)
			iv++;
		pp++;
	}

	/* Reached the end without a match. */

	if (iv == nvals[im]) {
		im = MACRO_MAX;
		ip = 0;
		pp = NULL;
		return NULL;
	}

	/* Found a match; skip the remaining pages of this entry. */

	if (++iv < nvals[im])
		while (*pp++ != 0)
			continue;

	return dbm_get(macros[im][iv - 1].value);
}
