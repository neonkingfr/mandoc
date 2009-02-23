/* $Id$ */
/*
 * Copyright (c) 2008 Kristaps Dzonsons <kristaps@kth.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <assert.h>
#include <stdlib.h>

#include "private.h"

/*
 * Pre- and post-validate macros as they're parsed.  Pre-validation
 * occurs when the macro has been detected and its arguments parsed.
 * Post-validation occurs when all child macros have also been parsed.
 * In the ELEMENT case, this is simply the parameters of the macro; in
 * the BLOCK case, this is the HEAD, BODY, TAIL and so on.
 */

typedef	int	(*v_pre)(struct mdoc *, struct mdoc_node *);
typedef	int	(*v_post)(struct mdoc *);

/* FIXME: some sections should only occur in specific msecs. */
/* FIXME: ignoring Pp. */
/* FIXME: math symbols. */
/* FIXME: valid character-escape checks. */
/* FIXME: .Fd only in synopsis section. */

struct	valids {
	v_pre	*pre;
	v_post	*post;
};

/* Utility checks. */

static	int	check_parent(struct mdoc *, struct mdoc_node *, 
			int, enum mdoc_type);
static	int	check_msec(struct mdoc *, struct mdoc_node *, 
			int, enum mdoc_msec *);
static	int	check_stdarg(struct mdoc *, struct mdoc_node *);
static	int	err_child_lt(struct mdoc *, const char *, int);
static	int	err_child_gt(struct mdoc *, const char *, int);
static	int	warn_child_gt(struct mdoc *, const char *, int);
static	int	err_child_eq(struct mdoc *, const char *, int);
static	int	warn_child_eq(struct mdoc *, const char *, int);

/* Utility auxiliaries. */

static	inline int count_child(struct mdoc *);
static	inline int warn_count(struct mdoc *, const char *, 
			int, const char *, int);
static	inline int err_count(struct mdoc *, const char *, 
			int, const char *, int);

/* Specific pre-child-parse routines. */

static	int	pre_display(struct mdoc *, struct mdoc_node *);
static	int	pre_sh(struct mdoc *, struct mdoc_node *);
static	int	pre_ss(struct mdoc *, struct mdoc_node *);
static	int	pre_bd(struct mdoc *, struct mdoc_node *);
static	int	pre_bl(struct mdoc *, struct mdoc_node *);
static	int	pre_it(struct mdoc *, struct mdoc_node *);
static	int	pre_cd(struct mdoc *, struct mdoc_node *);
static	int	pre_er(struct mdoc *, struct mdoc_node *);
static	int	pre_ex(struct mdoc *, struct mdoc_node *);
static	int	pre_rv(struct mdoc *, struct mdoc_node *);
static	int	pre_an(struct mdoc *, struct mdoc_node *);
static	int	pre_st(struct mdoc *, struct mdoc_node *);
static	int	pre_prologue(struct mdoc *, struct mdoc_node *);
static	int	pre_prologue(struct mdoc *, struct mdoc_node *);
static	int	pre_prologue(struct mdoc *, struct mdoc_node *);

/* Specific post-child-parse routines. */

static	int	herr_ge1(struct mdoc *);
static	int	herr_le1(struct mdoc *);
static	int	herr_eq0(struct mdoc *);
static	int	eerr_eq0(struct mdoc *);
static	int	eerr_le1(struct mdoc *);
static	int	eerr_le2(struct mdoc *);
static	int	eerr_eq1(struct mdoc *);
static	int	eerr_ge1(struct mdoc *);
static	int	ewarn_eq0(struct mdoc *);
static	int	ewarn_eq1(struct mdoc *);
static	int	bwarn_ge1(struct mdoc *);
static	int	ewarn_ge1(struct mdoc *);
static	int	ebool(struct mdoc *);
static	int	post_sh(struct mdoc *);
static	int	post_sh_body(struct mdoc *);
static	int	post_sh_head(struct mdoc *);
static	int	post_bl(struct mdoc *);
static	int	post_it(struct mdoc *);
static	int	post_ex(struct mdoc *);
static	int	post_an(struct mdoc *);
static	int	post_at(struct mdoc *);
static	int	post_xr(struct mdoc *);
static	int	post_nm(struct mdoc *);
static	int	post_bf(struct mdoc *);
static	int	post_root(struct mdoc *);

/* Collections of pre-child-parse routines. */

static	v_pre	pres_prologue[] = { pre_prologue, NULL };
static	v_pre	pres_d1[] = { pre_display, NULL };
static	v_pre	pres_bd[] = { pre_display, pre_bd, NULL };
static	v_pre	pres_bl[] = { pre_bl, NULL };
static	v_pre	pres_it[] = { pre_it, NULL };
static	v_pre	pres_ss[] = { pre_ss, NULL };
static	v_pre	pres_sh[] = { pre_sh, NULL };
static	v_pre	pres_cd[] = { pre_cd, NULL };
static	v_pre	pres_er[] = { pre_er, NULL };
static	v_pre	pres_ex[] = { pre_ex, NULL };
static	v_pre	pres_rv[] = { pre_rv, NULL };
static	v_pre	pres_an[] = { pre_an, NULL };
static	v_pre	pres_st[] = { pre_st, NULL };

/* Collections of post-child-parse routines. */

static	v_post	posts_bool[] = { eerr_eq1, ebool, NULL };
static	v_post	posts_bd[] = { herr_eq0, bwarn_ge1, NULL };
static	v_post	posts_text[] = { eerr_ge1, NULL };
static	v_post	posts_wtext[] = { ewarn_ge1, NULL };
static	v_post	posts_notext[] = { eerr_eq0, NULL };
static	v_post	posts_wline[] = { bwarn_ge1, herr_eq0, NULL };
static	v_post	posts_sh[] = { herr_ge1, bwarn_ge1, post_sh, NULL };
static	v_post	posts_bl[] = { herr_eq0, bwarn_ge1, post_bl, NULL };
static	v_post	posts_it[] = { post_it, NULL };
static	v_post	posts_in[] = { ewarn_eq1, NULL };
static	v_post	posts_ss[] = { herr_ge1, NULL };
static	v_post	posts_pf[] = { eerr_eq1, NULL };
static	v_post	posts_pp[] = { ewarn_eq0, NULL };
static	v_post	posts_ex[] = { eerr_le1, post_ex, NULL };
static	v_post	posts_an[] = { post_an, NULL };
static	v_post	posts_at[] = { post_at, NULL };
static	v_post	posts_xr[] = { eerr_ge1, eerr_le2, post_xr, NULL };
static	v_post	posts_nm[] = { post_nm, NULL };
static	v_post	posts_bf[] = { herr_le1, post_bf, NULL };
static	v_post	posts_rs[] = { herr_eq0, bwarn_ge1, NULL };
static	v_post	posts_fo[] = { bwarn_ge1, NULL };
static	v_post	posts_bk[] = { herr_eq0, bwarn_ge1, NULL };

/* Per-macro pre- and post-child-check routine collections. */

const	struct valids mdoc_valids[MDOC_MAX] = {
	{ NULL, NULL }, 			/* \" */
	{ pres_prologue, posts_text },		/* Dd */
	{ pres_prologue, NULL },		/* Dt */
	{ pres_prologue, NULL },		/* Os */
	{ pres_sh, posts_sh },			/* Sh */ 
	{ pres_ss, posts_ss },			/* Ss */ 
	{ NULL, posts_pp },			/* Pp */ 
	{ pres_d1, posts_wline },		/* D1 */
	{ pres_d1, posts_wline },		/* Dl */
	{ pres_bd, posts_bd },			/* Bd */
	{ NULL, NULL },				/* Ed */
	{ pres_bl, posts_bl },			/* Bl */ 
	{ NULL, NULL },				/* El */
	{ pres_it, posts_it },			/* It */
	{ NULL, posts_text },			/* Ad */ 
	{ pres_an, posts_an },			/* An */ 
	{ NULL, NULL },				/* Ar */
	{ pres_cd, posts_text },		/* Cd */ 
	{ NULL, NULL },				/* Cm */
	{ NULL, posts_text },			/* Dv */ 
	{ pres_er, posts_text },		/* Er */ 
	{ NULL, posts_text },			/* Ev */ 
	{ pres_ex, posts_ex },			/* Ex */ 
	{ NULL, posts_text },			/* Fa */ 
	{ NULL, posts_wtext },			/* Fd */
	{ NULL, NULL },				/* Fl */
	{ NULL, posts_text },			/* Fn */ 
	{ NULL, posts_wtext },			/* Ft */ 
	{ NULL, posts_text },			/* Ic */ 
	{ NULL, posts_in },			/* In */ 
	{ NULL, posts_text },			/* Li */
	{ NULL, posts_wtext },			/* Nd */
	{ NULL, posts_nm },			/* Nm */
	{ NULL, posts_wline },			/* Op */
	{ NULL, NULL },				/* Ot */
	{ NULL, NULL },				/* Pa */
	{ pres_rv, posts_notext },		/* Rv */
	{ pres_st, posts_notext },		/* St */ 
	{ NULL, posts_text },			/* Va */
	{ NULL, posts_text },			/* Vt */ 
	{ NULL, posts_xr },			/* Xr */ 
	{ NULL, posts_text },			/* %A */
	{ NULL, posts_text },			/* %B */
	{ NULL, posts_text },			/* %D */
	{ NULL, posts_text },			/* %I */
	{ NULL, posts_text },			/* %J */
	{ NULL, posts_text },			/* %N */
	{ NULL, posts_text },			/* %O */
	{ NULL, posts_text },			/* %P */
	{ NULL, posts_text },			/* %R */
	{ NULL, posts_text },			/* %T */
	{ NULL, posts_text },			/* %V */
	{ NULL, NULL },				/* Ac */
	{ NULL, NULL },				/* Ao */
	{ NULL, posts_wline },			/* Aq */
	{ NULL, posts_at },			/* At */ 
	{ NULL, NULL },				/* Bc */
	{ NULL, posts_bf },			/* Bf */
	{ NULL, NULL },				/* Bo */
	{ NULL, posts_wline },			/* Bq */
	{ NULL, NULL },				/* Bsx */
	{ NULL, NULL },				/* Bx */
	{ NULL, posts_bool },			/* Db */
	{ NULL, NULL },				/* Dc */
	{ NULL, NULL },				/* Do */
	{ NULL, posts_wline },			/* Dq */
	{ NULL, NULL },				/* Ec */
	{ NULL, NULL },				/* Ef */ 
	{ NULL, posts_text },			/* Em */ 
	{ NULL, NULL },				/* Eo */
	{ NULL, NULL },				/* Fx */
	{ NULL, posts_text },			/* Ms */ 
	{ NULL, posts_notext },			/* No */
	{ NULL, posts_notext },			/* Ns */
	{ NULL, NULL },				/* Nx */
	{ NULL, NULL },				/* Ox */
	{ NULL, NULL },				/* Pc */
	{ NULL, posts_pf },			/* Pf */
	{ NULL, NULL },				/* Po */
	{ NULL, posts_wline },			/* Pq */
	{ NULL, NULL },				/* Qc */
	{ NULL, posts_wline },			/* Ql */
	{ NULL, NULL },				/* Qo */
	{ NULL, posts_wline },			/* Qq */
	{ NULL, NULL },				/* Re */
	{ NULL, posts_rs },			/* Rs */
	{ NULL, NULL },				/* Sc */
	{ NULL, NULL },				/* So */
	{ NULL, posts_wline },			/* Sq */
	{ NULL, posts_bool },			/* Sm */ 
	{ NULL, posts_text },			/* Sx */
	{ NULL, posts_text },			/* Sy */
	{ NULL, posts_text },			/* Tn */
	{ NULL, NULL },				/* Ux */
	{ NULL, NULL },				/* Xc */
	{ NULL, NULL },				/* Xo */
	{ NULL, posts_fo },			/* Fo */ 
	{ NULL, NULL },				/* Fc */ 
	{ NULL, NULL },				/* Oo */
	{ NULL, NULL },				/* Oc */
	{ NULL, posts_bk },			/* Bk */
	{ NULL, NULL },				/* Ek */
	{ NULL, posts_notext },			/* Bt */
	{ NULL, NULL },				/* Hf */
	{ NULL, NULL },				/* Fr */
	{ NULL, posts_notext },			/* Ud */
};


static inline int
warn_count(struct mdoc *m, const char *k, 
		int want, const char *v, int has)
{

	return(mdoc_warn(m, WARN_SYNTAX, "suggests %s %d %s "
				"(has %d)", v, want, k, has));
}


static inline int
err_count(struct mdoc *m, const char *k,
		int want, const char *v, int has)
{

	return(mdoc_err(m, "requires %s %d %s (has %d)",
				v, want, k, has));
}


static inline int
count_child(struct mdoc *mdoc)
{
	int		  i;
	struct mdoc_node *n;

	for (i = 0, n = mdoc->last->child; n; n = n->next, i++)
		/* Do nothing */ ;

	return(i);
}


/*
 * Build these up with macros because they're basically the same check
 * for different inequalities.  Yes, this could be done with functions,
 * but this is reasonable for now.
 */

#define CHECK_CHILD_DEFN(lvl, name, ineq) 			\
static int 							\
lvl##_child_##name(struct mdoc *mdoc, const char *p, int sz) 	\
{ 								\
	int i; 							\
	if ((i = count_child(mdoc)) ineq sz) 			\
		return(1); 					\
	return(lvl##_count(mdoc, #ineq, sz, p, i)); 		\
}

#define CHECK_BODY_DEFN(name, lvl, func, num) 			\
static int 							\
b##lvl##_##name(struct mdoc *mdoc) 				\
{ 								\
	if (MDOC_BODY != mdoc->last->type) 			\
		return(1); 					\
	return(func(mdoc, "multiline parameters", (num))); 	\
}

#define CHECK_ELEM_DEFN(name, lvl, func, num) 			\
static int							\
e##lvl##_##name(struct mdoc *mdoc) 				\
{ 								\
	assert(MDOC_ELEM == mdoc->last->type); 			\
	return(func(mdoc, "line parameters", (num))); 		\
}

#define CHECK_HEAD_DEFN(name, lvl, func, num)			\
static int 							\
h##lvl##_##name(struct mdoc *mdoc) 				\
{ 								\
	if (MDOC_HEAD != mdoc->last->type) 			\
		return(1); 					\
	return(func(mdoc, "multiline parameters", (num))); 	\
}


CHECK_CHILD_DEFN(warn, gt, >)			/* warn_child_gt() */
CHECK_CHILD_DEFN(err, gt, >)			/* err_child_gt() */
CHECK_CHILD_DEFN(warn, eq, ==)			/* warn_child_eq() */
CHECK_CHILD_DEFN(err, eq, ==)			/* err_child_eq() */
CHECK_CHILD_DEFN(err, lt, <)			/* err_child_lt() */
CHECK_BODY_DEFN(ge1, warn, warn_child_gt, 0)	/* bwarn_ge1() */
CHECK_ELEM_DEFN(eq1, warn, warn_child_eq, 1)	/* ewarn_eq1() */
CHECK_ELEM_DEFN(eq0, warn, warn_child_eq, 0)	/* ewarn_eq0() */
CHECK_ELEM_DEFN(ge1, warn, warn_child_gt, 0)	/* ewarn_gt1() */
CHECK_ELEM_DEFN(eq1, err, err_child_eq, 1)	/* eerr_eq1() */
CHECK_ELEM_DEFN(le2, err, err_child_lt, 3)	/* eerr_le2() */
CHECK_ELEM_DEFN(le1, err, err_child_lt, 2)	/* eerr_le1() */
CHECK_ELEM_DEFN(eq0, err, err_child_eq, 0)	/* eerr_eq0() */
CHECK_ELEM_DEFN(ge1, err, err_child_gt, 0)	/* eerr_ge1() */
CHECK_HEAD_DEFN(eq0, err, err_child_eq, 0)	/* herr_eq0() */
CHECK_HEAD_DEFN(le1, err, err_child_lt, 2)	/* herr_le1() */
CHECK_HEAD_DEFN(ge1, err, err_child_gt, 0)	/* herr_ge1() */


static int
check_stdarg(struct mdoc *mdoc, struct mdoc_node *node)
{

	if (MDOC_Std == node->data.elem.argv[0].arg && 
			1 == node->data.elem.argc)
		return(1);

	return(mdoc_nwarn(mdoc, node, WARN_COMPAT, 
				"one argument suggested"));
}


static int
check_msec(struct mdoc *mdoc, struct mdoc_node *node, 
		int sz, enum mdoc_msec *msecs)
{
	int		 i;

	for (i = 0; i < sz; i++)
		if (msecs[i] == mdoc->meta.msec)
			return(1);
	return(mdoc_nwarn(mdoc, node, WARN_COMPAT, 
				"wrong manual section"));
}


static int
pre_display(struct mdoc *mdoc, struct mdoc_node *node)
{
	struct mdoc_node *n;

	/* Display elements (`Bd', `D1'...) cannot be nested. */

	if (MDOC_BLOCK != node->type)
		return(1);

	/* LINTED */
	for (n = mdoc->last->parent; n; n = n->parent) 
		if (MDOC_BLOCK == n->type)
			if (MDOC_Bd == n->tok)
				break;
	if (NULL == n)
		return(1);

	return(mdoc_nerr(mdoc, node, "displays may not be nested"));
}


static int
pre_bl(struct mdoc *mdoc, struct mdoc_node *node)
{
	int		 type, err, i;
	struct mdoc_arg	*argv;
	size_t		 argc;

	if (MDOC_BLOCK != node->type)
		return(1);

	argc = node->data.block.argc; 

	/* Make sure that only one type of list is specified.  */

	/* LINTED */
	for (i = 0, type = err = 0; i < (int)argc; i++) {
		argv = &node->data.block.argv[i];

		switch (argv->arg) {
		case (MDOC_Bullet):
			/* FALLTHROUGH */
		case (MDOC_Dash):
			/* FALLTHROUGH */
		case (MDOC_Enum):
			/* FALLTHROUGH */
		case (MDOC_Hyphen):
			/* FALLTHROUGH */
		case (MDOC_Item):
			/* FALLTHROUGH */
		case (MDOC_Tag):
			/* FALLTHROUGH */
		case (MDOC_Diag):
			/* FALLTHROUGH */
		case (MDOC_Hang):
			/* FALLTHROUGH */
		case (MDOC_Ohang):
			/* FALLTHROUGH */
		case (MDOC_Inset):
			/* FALLTHROUGH */
		case (MDOC_Column):
			if (0 == type++)
				break;
			return(mdoc_perr(mdoc, argv->line, argv->pos, 
					"multiple types specified"));
		default:
			break;
		}
	}

	if (type)
		return(1);
	return(mdoc_err(mdoc, "no type specified"));
}


static int
pre_bd(struct mdoc *mdoc, struct mdoc_node *node)
{
	int		 type, err, i;
	struct mdoc_arg	*argv;
	size_t		 argc;

	if (MDOC_BLOCK != node->type)
		return(1);

	argc = node->data.block.argc;

	/* Make sure that only one type of display is specified.  */

	/* LINTED */
	for (i = 0, err = type = 0; ! err && i < (int)argc; i++) {
		argv = &node->data.block.argv[i];

		switch (argv->arg) {
		case (MDOC_Ragged):
			/* FALLTHROUGH */
		case (MDOC_Unfilled):
			/* FALLTHROUGH */
		case (MDOC_Filled):
			/* FALLTHROUGH */
		case (MDOC_Literal):
			/* FALLTHROUGH */
		case (MDOC_File):
			if (0 == type++) 
				break;
			return(mdoc_perr(mdoc, argv->line, argv->pos, 
					"multiple types specified"));
		default:
			break;
		}
	}

	if (type)
		return(1);
	return(mdoc_err(mdoc, "no type specified"));
}


static int
pre_ss(struct mdoc *mdoc, struct mdoc_node *node)
{

	if (MDOC_BLOCK != node->type)
		return(1);
	return(check_parent(mdoc, node, MDOC_Sh, MDOC_BODY));
}


static int
pre_sh(struct mdoc *mdoc, struct mdoc_node *node)
{

	if (MDOC_BLOCK != node->type)
		return(1);
	return(check_parent(mdoc, node, -1, MDOC_ROOT));
}


static int
pre_it(struct mdoc *mdoc, struct mdoc_node *node)
{

	/* TODO: -width attribute must be specified for -tag. */
	/* TODO: children too big for -width? */

	if (MDOC_BLOCK != node->type)
		return(1);
	return(check_parent(mdoc, node, MDOC_Bl, MDOC_BODY));
}


static int
pre_st(struct mdoc *mdoc, struct mdoc_node *node)
{

	if (1 == node->data.elem.argc)
		return(1);
	return(mdoc_nerr(mdoc, node, "one argument required"));
}


static int
pre_an(struct mdoc *mdoc, struct mdoc_node *node)
{

	if (1 >= node->data.elem.argc)
		return(1);
	return(mdoc_nerr(mdoc, node, "one argument allowed"));
}


static int
pre_rv(struct mdoc *mdoc, struct mdoc_node *node)
{
	enum mdoc_msec msecs[] = { MSEC_2, MSEC_3 };

	if ( ! check_msec(mdoc, node, 2, msecs))
		return(0);
	return(check_stdarg(mdoc, node));
}


static int
pre_ex(struct mdoc *mdoc, struct mdoc_node *node)
{
	enum mdoc_msec msecs[] = { MSEC_1, MSEC_6, MSEC_8 };

	if ( ! check_msec(mdoc, node, 3, msecs))
		return(0);
	return(check_stdarg(mdoc, node));
}


static int
pre_er(struct mdoc *mdoc, struct mdoc_node *node)
{
	enum mdoc_msec msecs[] = { MSEC_2 };

	return(check_msec(mdoc, node, 1, msecs));
}


static int
pre_cd(struct mdoc *mdoc, struct mdoc_node *node)
{
	enum mdoc_msec msecs[] = { MSEC_4 };

	return(check_msec(mdoc, node, 1, msecs));
}


static int
pre_prologue(struct mdoc *mdoc, struct mdoc_node *node)
{

	if (SEC_PROLOGUE != mdoc->lastnamed)
		return(mdoc_nerr(mdoc, node, "prologue only"));

	/* Check for ordering. */

	switch (node->tok) {
	case (MDOC_Os):
		if (mdoc->meta.title && mdoc->meta.date)
			break;
		return(mdoc_nerr(mdoc, node, 
				"prologue out-of-order"));
	case (MDOC_Dt):
		if (NULL == mdoc->meta.title && mdoc->meta.date)
			break;
		return(mdoc_nerr(mdoc, node, 
				"prologue out-of-order"));
	case (MDOC_Dd):
		if (NULL == mdoc->meta.title && 0 == mdoc->meta.date)
			break;
		return(mdoc_nerr(mdoc, node, 
				"prologue out-of-order"));
	default:
		abort();
		/* NOTREACHED */
	}

	/* Check for repetition. */

	switch (node->tok) {
	case (MDOC_Os):
		if (NULL == mdoc->meta.os)
			return(1);
		break;
	case (MDOC_Dd):
		if (0 == mdoc->meta.date)
			return(1);
		break;
	case (MDOC_Dt):
		if (NULL == mdoc->meta.title)
			return(1);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(mdoc_nerr(mdoc, node, "prologue repetition"));
}


static int
post_bf(struct mdoc *mdoc)
{
	char		 *p;
	struct mdoc_node *head;

	if (MDOC_BLOCK != mdoc->last->type)
		return(1);

	head = mdoc->last->data.block.head;

	if (0 == mdoc->last->data.block.argc) {
		if (NULL == head->child)
			return(mdoc_err(mdoc, "argument expected"));

		p = head->child->data.text.string;
		if (xstrcmp(p, "Em"))
			return(1);
		else if (xstrcmp(p, "Li"))
			return(1);
		else if (xstrcmp(p, "Sm"))
			return(1);
		return(mdoc_nerr(mdoc, head->child, "invalid font"));
	}

	if (head->child)
		return(mdoc_err(mdoc, "argument expected"));

	if (1 == mdoc->last->data.block.argc)
		return(1);
	return(mdoc_err(mdoc, "argument expected"));
}


static int
post_nm(struct mdoc *mdoc)
{

	if (mdoc->last->child)
		return(1);
	if (mdoc->meta.name)
		return(1);
	return(mdoc_err(mdoc, "not yet invoked with name"));
}


static int
post_xr(struct mdoc *mdoc)
{
	struct mdoc_node *n;

	if (NULL == (n = mdoc->last->child->next))
		return(1);
	if (MSEC_DEFAULT != mdoc_atomsec(n->data.text.string))
		return(1);
	return(mdoc_nerr(mdoc, n, "invalid manual section"));
}


static int
post_at(struct mdoc *mdoc)
{

	if (NULL == mdoc->last->child)
		return(1);
	if (ATT_DEFAULT != mdoc_atoatt(mdoc->last->child->data.text.string))
		return(1);
	return(mdoc_err(mdoc, "require valid symbol"));
}


static int
post_an(struct mdoc *mdoc)
{

	if (0 != mdoc->last->data.elem.argc) {
		if (NULL == mdoc->last->child)
			return(1);
		return(mdoc_err(mdoc, "argument(s) expected"));
	}

	if (mdoc->last->child)
		return(1);
	return(mdoc_err(mdoc, "argument(s) expected"));
}


static int
post_ex(struct mdoc *mdoc)
{

	if (0 == mdoc->last->data.elem.argc) {
		if (mdoc->last->child)
			return(1);
		return(mdoc_err(mdoc, "argument(s) expected"));
	}
	if (mdoc->last->child)
		return(mdoc_err(mdoc, "argument(s) expected"));
	if (1 != mdoc->last->data.elem.argc)
		return(mdoc_err(mdoc, "argument(s) expected"));
	if (MDOC_Std != mdoc->last->data.elem.argv[0].arg)
		return(mdoc_err(mdoc, "argument(s) expected"));

	return(1);
}


static int
post_it(struct mdoc *mdoc)
{
	int		  type, sv, i;
#define	TYPE_NONE	 (0)
#define	TYPE_BODY	 (1)
#define	TYPE_HEAD	 (2)
#define	TYPE_OHEAD	 (3)
	size_t		  argc;
	struct mdoc_node *n;

	if (MDOC_BLOCK != mdoc->last->type)
		return(1);

	n = mdoc->last->parent->parent;

	argc = n->data.block.argc;
	type = TYPE_NONE;
	sv = -1;
	
	/* Some types require block-head, some not. */

	/* LINTED */
	for (i = 0; TYPE_NONE == type && i < (int)argc; i++)
		switch (n->data.block.argv[i].arg) {
		case (MDOC_Tag):
			/* FALLTHROUGH */
		case (MDOC_Diag):
			/* FALLTHROUGH */
		case (MDOC_Hang):
			/* FALLTHROUGH */
		case (MDOC_Ohang):
			/* FALLTHROUGH */
		case (MDOC_Inset):
			type = TYPE_HEAD;
			sv = n->data.block.argv[i].arg;
			break;
		case (MDOC_Bullet):
			/* FALLTHROUGH */
		case (MDOC_Dash):
			/* FALLTHROUGH */
		case (MDOC_Enum):
			/* FALLTHROUGH */
		case (MDOC_Hyphen):
			/* FALLTHROUGH */
		case (MDOC_Item):
			type = TYPE_BODY;
			sv = n->data.block.argv[i].arg;
			break;
		case (MDOC_Column):
			type = TYPE_OHEAD;
			sv = n->data.block.argv[i].arg;
			break;
		default:
			break;
		}

	assert(TYPE_NONE != type);

	n = mdoc->last->data.block.head;

	if (TYPE_HEAD == type) {
		if (NULL == n->child)
			if ( ! mdoc_warn(mdoc, WARN_SYNTAX, 
					"argument(s) suggested"))
				return(0);

		n = mdoc->last->data.block.body;
		if (NULL == n->child)
			if ( ! mdoc_warn(mdoc, WARN_SYNTAX, 
					"multiline body suggested"))
				return(0);

	} else if (TYPE_BODY == type) {
		if (n->child)
			if ( ! mdoc_warn(mdoc, WARN_SYNTAX, 
					"no argument suggested"))
				return(0);
	
		n = mdoc->last->data.block.body;
		if (NULL == n->child)
			if ( ! mdoc_warn(mdoc, WARN_SYNTAX, 
					"multiline body suggested"))
				return(0);
	} else {
		if (NULL == n->child)
			if ( ! mdoc_warn(mdoc, WARN_SYNTAX, 
					"argument(s) suggested"))
				return(0);
	
		n = mdoc->last->data.block.body;
		if (n->child)
			if ( ! mdoc_warn(mdoc, WARN_SYNTAX, 
					"no multiline body suggested"))
				return(0);
	}

	if (MDOC_Column != sv)
		return(1);

	argc = mdoc->last->parent->parent->data.block.argv->sz;
	n = mdoc->last->data.block.head->child;

	for (i = 0; n; n = n->next)
		i++;

	if (i == (int)argc)
		return(1);

	return(mdoc_err(mdoc, "need %zu columns (have %d)", argc, i));
#undef	TYPE_NONE
#undef	TYPE_BODY
#undef	TYPE_HEAD
#undef	TYPE_OHEAD
}


static int
post_bl(struct mdoc *mdoc)
{
	struct mdoc_node	*n;

	if (MDOC_BODY != mdoc->last->type)
		return(1);

	/* LINTED */
	for (n = mdoc->last->child; n; n = n->next) {
		if (MDOC_BLOCK == n->type) 
			if (MDOC_It == n->tok)
				continue;
		break;
	}

	if (NULL == n)
		return(1);

	return(mdoc_nerr(mdoc, n, "bad child of parent list"));
}


static int
ebool(struct mdoc *mdoc)
{
	struct mdoc_node *n;

	/* LINTED */
	for (n = mdoc->last->child; n; n = n->next) {
		if (MDOC_TEXT != n->type)
			break;
		if (xstrcmp(n->data.text.string, "on"))
			continue;
		if (xstrcmp(n->data.text.string, "off"))
			continue;
		break;
	}

	if (NULL == n)
		return(1);
	return(mdoc_nerr(mdoc, n, "expected boolean"));
}


static int
post_root(struct mdoc *mdoc)
{

	if (NULL == mdoc->first->child)
		return(mdoc_err(mdoc, "document lacks data"));
	if (SEC_PROLOGUE == mdoc->lastnamed)
		return(mdoc_err(mdoc, "document lacks prologue"));

	if (MDOC_BLOCK != mdoc->first->child->type)
		return(mdoc_err(mdoc, "lacking post-prologue `%s'", 
					mdoc_macronames[MDOC_Sh]));
	if (MDOC_Sh != mdoc->first->child->tok)
		return(mdoc_err(mdoc, "lacking post-prologue `%s'", 
					mdoc_macronames[MDOC_Sh]));

	return(1);
}


static int
post_sh(struct mdoc *mdoc)
{

	if (MDOC_HEAD == mdoc->last->type)
		return(post_sh_head(mdoc));
	if (MDOC_BODY == mdoc->last->type)
		return(post_sh_body(mdoc));

	return(1);
}


static int
post_sh_body(struct mdoc *mdoc)
{
	struct mdoc_node *n;

	if (SEC_NAME != mdoc->lastnamed)
		return(1);

	/*
	 * Warn if the NAME section doesn't contain the `Nm' and `Nd'
	 * macros (can have multiple `Nm' and one `Nd').  Note that the
	 * children of the BODY declaration can also be "text".
	 */

	if (NULL == (n = mdoc->last->child))
		return(mdoc_warn(mdoc, WARN_COMPAT, "section NAME "
					"should contain %s and %s", 
					mdoc_macronames[MDOC_Nm],
					mdoc_macronames[MDOC_Nd]));

	for ( ; n && n->next; n = n->next) {
		if (MDOC_ELEM == n->type && MDOC_Nm == n->tok)
			continue;
		if (MDOC_TEXT == n->type)
			continue;
		if ( ! (mdoc_nwarn(mdoc, n, WARN_COMPAT, "section "
					"NAME should contain %s as "
					"initial body child", 
					mdoc_macronames[MDOC_Nm])))
			return(0);
	}

	if (MDOC_ELEM == n->type && MDOC_Nd == n->tok)
		return(1);

	return(mdoc_warn(mdoc, WARN_COMPAT, "section NAME should "
				"contain %s as the last child",
				mdoc_macronames[MDOC_Nd]));
}


static int
post_sh_head(struct mdoc *mdoc)
{
	char		  buf[64];
	enum mdoc_sec	  sec;

	assert(MDOC_Sh == mdoc->last->tok);

	if ( ! xstrlcats(buf, mdoc->last->child, 64))
		return(mdoc_err(mdoc, "macro parameters too long"));

	sec = mdoc_atosec(buf);

	if (SEC_BODY == mdoc->lastnamed && SEC_NAME != sec)
		return(mdoc_err(mdoc, "section NAME must be first"));
	if (SEC_CUSTOM == sec)
		return(1);
	if (sec == mdoc->lastnamed)
		return(mdoc_warn(mdoc, WARN_SYNTAX, "section repeated"));
	if (sec < mdoc->lastnamed)
		return(mdoc_warn(mdoc, WARN_SYNTAX, "section out of conventional order"));

	return(1);
}


int
mdoc_valid_pre(struct mdoc *mdoc, struct mdoc_node *node)
{
	v_pre		*p;

	if (MDOC_TEXT == node->type)
		return(1);
	assert(MDOC_ROOT != node->type);

	if (NULL == mdoc_valids[node->tok].pre)
		return(1);
	for (p = mdoc_valids[node->tok].pre; *p; p++)
		if ( ! (*p)(mdoc, node)) 
			return(0);
	return(1);
}


int
mdoc_valid_post(struct mdoc *mdoc)
{
	v_post		*p;

	if (MDOC_VALID & mdoc->last->flags)
		return(1);
	mdoc->last->flags |= MDOC_VALID;

	if (MDOC_TEXT == mdoc->last->type)
		return(1);
	if (MDOC_ROOT == mdoc->last->type)
		return(post_root(mdoc));

	if (NULL == mdoc_valids[mdoc->last->tok].post)
		return(1);
	for (p = mdoc_valids[mdoc->last->tok].post; *p; p++)
		if ( ! (*p)(mdoc)) 
			return(0);

	return(1);
}

