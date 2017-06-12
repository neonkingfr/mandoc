/*	$Id$ */
/*
 * Copyright (c) 2009, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011,2012,2014,2015,2017 Ingo Schwarze <schwarze@openbsd.org>
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
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "term.h"

static	size_t	term_tbl_len(size_t, void *);
static	size_t	term_tbl_strlen(const char *, void *);
static	size_t	term_tbl_sulen(const struct roffsu *, void *);
static	void	tbl_char(struct termp *, char, size_t);
static	void	tbl_data(struct termp *, const struct tbl_opts *,
			const struct tbl_dat *,
			const struct roffcol *);
static	void	tbl_literal(struct termp *, const struct tbl_dat *,
			const struct roffcol *);
static	void	tbl_number(struct termp *, const struct tbl_opts *,
			const struct tbl_dat *,
			const struct roffcol *);
static	void	tbl_hrule(struct termp *, const struct tbl_span *, int);
static	void	tbl_word(struct termp *, const struct tbl_dat *);


static size_t
term_tbl_sulen(const struct roffsu *su, void *arg)
{
	return term_hspan((const struct termp *)arg, su) / 24;
}

static size_t
term_tbl_strlen(const char *p, void *arg)
{
	return term_strlen((const struct termp *)arg, p);
}

static size_t
term_tbl_len(size_t sz, void *arg)
{
	return term_len((const struct termp *)arg, sz);
}

void
term_tbl(struct termp *tp, const struct tbl_span *sp)
{
	const struct tbl_cell	*cp;
	const struct tbl_dat	*dp;
	static size_t		 offset;
	size_t			 coloff, tsz;
	int			 ic, horiz, spans, vert, more;
	char			 fc;

	/* Inhibit printing of spaces: we do padding ourselves. */

	tp->flags |= TERMP_NOSPACE | TERMP_NONOSPACE;

	/*
	 * The first time we're invoked for a given table block,
	 * calculate the table widths and decimal positions.
	 */

	if (tp->tbl.cols == NULL) {
		tp->tbl.len = term_tbl_len;
		tp->tbl.slen = term_tbl_strlen;
		tp->tbl.sulen = term_tbl_sulen;
		tp->tbl.arg = tp;

		tblcalc(&tp->tbl, sp, tp->tcol->offset, tp->tcol->rmargin);

		/* Center the table as a whole. */

		offset = tp->tcol->offset;
		if (sp->opts->opts & TBL_OPT_CENTRE) {
			tsz = sp->opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX)
			    ? 2 : !!sp->opts->lvert + !!sp->opts->rvert;
			for (ic = 0; ic < sp->opts->cols; ic++)
				tsz += tp->tbl.cols[ic].width + 3;
			tsz -= 3;
			if (offset + tsz > tp->tcol->rmargin)
				tsz -= 1;
			tp->tcol->offset = offset + tp->tcol->rmargin > tsz ?
			    (offset + tp->tcol->rmargin - tsz) / 2 : 0;
		}

		/* Horizontal frame at the start of boxed tables. */

		if (sp->opts->opts & TBL_OPT_DBOX)
			tbl_hrule(tp, sp, 2);
		if (sp->opts->opts & (TBL_OPT_DBOX | TBL_OPT_BOX))
			tbl_hrule(tp, sp, 1);
	}

	/* Set up the columns. */

	tp->flags |= TERMP_MULTICOL;
	horiz = 0;
	switch (sp->pos) {
	case TBL_SPAN_HORIZ:
	case TBL_SPAN_DHORIZ:
		horiz = 1;
		term_setcol(tp, 1);
		break;
	case TBL_SPAN_DATA:
		term_setcol(tp, sp->opts->cols + 2);
		coloff = tp->tcol->offset;

		/* Set up a column for a left vertical frame. */

		if (sp->opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX) ||
		    sp->opts->lvert)
			coloff++;
		tp->tcol->rmargin = coloff;

		/* Set up the data columns. */

		dp = sp->first;
		spans = 0;
		for (ic = 0; ic < sp->opts->cols; ic++) {
			if (spans == 0) {
				tp->tcol++;
				tp->tcol->offset = coloff;
			}
			coloff += tp->tbl.cols[ic].width;
			tp->tcol->rmargin = coloff;
			coloff++;
			if (ic + 1 < sp->opts->cols)
				coloff += 2;
			if (spans) {
				spans--;
				continue;
			}
			if (dp == NULL)
				continue;
			spans = dp->spans;
			dp = dp->next;
		}

		/* Set up a column for a right vertical frame. */

		tp->tcol++;
		tp->tcol->offset = coloff;
		if (sp->opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX) ||
		    sp->opts->rvert)
			coloff++;
		tp->tcol->rmargin = coloff;

		/* Spans may have reduced the number of columns. */

		tp->lasttcol = tp->tcol - tp->tcols;

		/* Fill the buffers for all data columns. */

		tp->tcol = tp->tcols;
		dp = sp->first;
		spans = 0;
		for (ic = 0; ic < sp->opts->cols; ic++) {
			if (spans) {
				spans--;
				continue;
			}
			tp->tcol++;
			tp->col = 0;
			tbl_data(tp, sp->opts, dp, tp->tbl.cols + ic);
			if (dp == NULL)
				continue;
			spans = dp->spans;
			dp = dp->next;
		}
		break;
	}

	do {
		/* Print the vertical frame at the start of each row. */

		tp->tcol = tp->tcols;
		fc = '\0';
		if (sp->layout->vert ||
		    (sp->prev != NULL && sp->prev->layout->vert) ||
		    sp->opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX))
			fc = horiz ? '+' : '|';
		else if (horiz && sp->opts->lvert)
			fc = '-';
		if (fc != '\0') {
			(*tp->advance)(tp, tp->tcols->offset);
			(*tp->letter)(tp, fc);
			tp->viscol = tp->tcol->offset + 1;
		}

		/* Print the data cells. */

		more = 0;
		if (horiz) {
			tbl_hrule(tp, sp, 0);
			term_flushln(tp);
		} else {
			cp = sp->layout->first;
			dp = sp->first;
			spans = 0;
			for (ic = 0; ic < sp->opts->cols; ic++) {
				if (spans == 0) {
					tp->tcol++;
					if (dp != NULL) {
						spans = dp->spans;
						dp = dp->next;
					}
					if (tp->tcol->col < tp->tcol->lastcol)
						term_flushln(tp);
					if (tp->tcol->col < tp->tcol->lastcol)
						more = 1;
					if (tp->tcol + 1 ==
					    tp->tcols + tp->lasttcol)
						continue;
				} else
					spans--;

				/* Vertical frames between data cells. */

				if (cp != NULL) {
					vert = cp->vert;
					cp = cp->next;
				} else
					vert = 0;
				if (vert == 0)
					continue;

				if (tp->tcol->rmargin + 1 > tp->viscol) {
					(*tp->advance)(tp, tp->tcol->rmargin
					   + 1 - tp->viscol);
					tp->viscol = tp->tcol->rmargin + 1;
				}
				while (vert--) {
					(*tp->letter)(tp, '|');
					tp->viscol++;
				}
			}
		}

		/* Print the vertical frame at the end of each row. */

		fc = '\0';
		if (sp->layout->last->vert ||
		    (sp->prev != NULL && sp->prev->layout->last->vert) ||
		    (sp->opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX)))
			fc = horiz ? '+' : '|';
		else if (horiz && sp->opts->rvert)
			fc = '-';
		if (fc != '\0') {
			if (horiz == 0) {
				tp->tcol++;
				(*tp->advance)(tp,
				    tp->tcol->offset > tp->viscol ?
				    tp->tcol->offset - tp->viscol : 1);
			}
			(*tp->letter)(tp, fc);
		}
		(*tp->endline)(tp);
		tp->viscol = 0;
	} while (more);

	/*
	 * If we're the last row, clean up after ourselves: clear the
	 * existing table configuration and set it to NULL.
	 */

	term_setcol(tp, 1);
	tp->flags &= ~TERMP_MULTICOL;
	tp->tcol->rmargin = tp->maxrmargin;
	if (sp->next == NULL) {
		if (sp->opts->opts & (TBL_OPT_DBOX | TBL_OPT_BOX)) {
			tbl_hrule(tp, sp, 1);
			tp->skipvsp = 1;
		}
		if (sp->opts->opts & TBL_OPT_DBOX) {
			tbl_hrule(tp, sp, 2);
			tp->skipvsp = 2;
		}
		assert(tp->tbl.cols);
		free(tp->tbl.cols);
		tp->tbl.cols = NULL;
		tp->tcol->offset = offset;
	}
	tp->flags &= ~TERMP_NONOSPACE;
}

/*
 * Kinds of horizontal rulers:
 * 0: inside the table (single or double line with crossings)
 * 1: inner frame (single line with crossings and ends)
 * 2: outer frame (single line without crossings with ends)
 */
static void
tbl_hrule(struct termp *tp, const struct tbl_span *sp, int kind)
{
	const struct tbl_cell *c1, *c2;
	int	 vert;
	char	 line, cross;

	line = (kind == 0 && TBL_SPAN_DHORIZ == sp->pos) ? '=' : '-';
	cross = (kind < 2) ? '+' : '-';

	if (kind)
		term_word(tp, "+");
	c1 = sp->layout->first;
	c2 = sp->prev == NULL ? NULL : sp->prev->layout->first;
	if (c2 == c1)
		c2 = NULL;
	for (;;) {
		tbl_char(tp, line, tp->tbl.cols[c1->col].width + 1);
		vert = c1->vert;
		if ((c1 = c1->next) == NULL)
			 break;
		if (c2 != NULL) {
			if (vert < c2->vert)
				vert = c2->vert;
			c2 = c2->next;
		}
		if (vert)
			tbl_char(tp, cross, vert);
		if (vert < 2)
			tbl_char(tp, line, 2 - vert);
	}
	if (kind) {
		term_word(tp, "+");
		term_flushln(tp);
	}
}

static void
tbl_data(struct termp *tp, const struct tbl_opts *opts,
	const struct tbl_dat *dp,
	const struct roffcol *col)
{

	if (dp == NULL) {
		tbl_char(tp, ASCII_NBRSP, col->width);
		return;
	}

	switch (dp->pos) {
	case TBL_DATA_NONE:
		tbl_char(tp, ASCII_NBRSP, col->width);
		return;
	case TBL_DATA_HORIZ:
	case TBL_DATA_NHORIZ:
		tbl_char(tp, '-', col->width);
		return;
	case TBL_DATA_NDHORIZ:
	case TBL_DATA_DHORIZ:
		tbl_char(tp, '=', col->width);
		return;
	default:
		break;
	}

	switch (dp->layout->pos) {
	case TBL_CELL_HORIZ:
		tbl_char(tp, '-', col->width);
		break;
	case TBL_CELL_DHORIZ:
		tbl_char(tp, '=', col->width);
		break;
	case TBL_CELL_LONG:
	case TBL_CELL_CENTRE:
	case TBL_CELL_LEFT:
	case TBL_CELL_RIGHT:
		tbl_literal(tp, dp, col);
		break;
	case TBL_CELL_NUMBER:
		tbl_number(tp, opts, dp, col);
		break;
	case TBL_CELL_DOWN:
		tbl_char(tp, ASCII_NBRSP, col->width);
		break;
	default:
		abort();
	}
}

static void
tbl_char(struct termp *tp, char c, size_t len)
{
	size_t		i, sz;
	char		cp[2];

	cp[0] = c;
	cp[1] = '\0';

	sz = term_strlen(tp, cp);

	for (i = 0; i < len; i += sz)
		term_word(tp, cp);
}

static void
tbl_literal(struct termp *tp, const struct tbl_dat *dp,
		const struct roffcol *col)
{
	size_t		 len, padl, padr, width;
	int		 ic, spans;

	assert(dp->string);
	len = term_strlen(tp, dp->string);
	width = col->width;
	ic = dp->layout->col;
	spans = dp->spans;
	while (spans--)
		width += tp->tbl.cols[++ic].width + 3;

	padr = width > len ? width - len : 0;
	padl = 0;

	switch (dp->layout->pos) {
	case TBL_CELL_LONG:
		padl = term_len(tp, 1);
		padr = padr > padl ? padr - padl : 0;
		break;
	case TBL_CELL_CENTRE:
		if (2 > padr)
			break;
		padl = padr / 2;
		padr -= padl;
		break;
	case TBL_CELL_RIGHT:
		padl = padr;
		padr = 0;
		break;
	default:
		break;
	}

	tbl_char(tp, ASCII_NBRSP, padl);
	tbl_word(tp, dp);
	tbl_char(tp, ASCII_NBRSP, padr);
}

static void
tbl_number(struct termp *tp, const struct tbl_opts *opts,
		const struct tbl_dat *dp,
		const struct roffcol *col)
{
	char		*cp;
	char		 buf[2];
	size_t		 sz, psz, ssz, d, padl;
	int		 i;

	/*
	 * See calc_data_number().  Left-pad by taking the offset of our
	 * and the maximum decimal; right-pad by the remaining amount.
	 */

	assert(dp->string);

	sz = term_strlen(tp, dp->string);

	buf[0] = opts->decimal;
	buf[1] = '\0';

	psz = term_strlen(tp, buf);

	if ((cp = strrchr(dp->string, opts->decimal)) != NULL) {
		for (ssz = 0, i = 0; cp != &dp->string[i]; i++) {
			buf[0] = dp->string[i];
			ssz += term_strlen(tp, buf);
		}
		d = ssz + psz;
	} else
		d = sz + psz;

	if (col->decimal > d && col->width > sz) {
		padl = col->decimal - d;
		if (padl + sz > col->width)
			padl = col->width - sz;
		tbl_char(tp, ASCII_NBRSP, padl);
	} else
		padl = 0;
	tbl_word(tp, dp);
	if (col->width > sz + padl)
		tbl_char(tp, ASCII_NBRSP, col->width - sz - padl);
}

static void
tbl_word(struct termp *tp, const struct tbl_dat *dp)
{
	int		 prev_font;

	prev_font = tp->fonti;
	if (dp->layout->flags & TBL_CELL_BOLD)
		term_fontpush(tp, TERMFONT_BOLD);
	else if (dp->layout->flags & TBL_CELL_ITALIC)
		term_fontpush(tp, TERMFONT_UNDER);

	term_word(tp, dp->string);

	term_fontpopq(tp, prev_font);
}
