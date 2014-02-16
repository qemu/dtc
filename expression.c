/*
 * (C) Copyright David Gibson <david@gibson.dropbear.id.au>, Red Hat 2013.
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *                                                                   USA
 */
#include "srcpos.h"
#include "dtc.h"

static const char *expression_typename(enum expr_type t)
{
	switch (t) {
	case EXPR_VOID:
		return "void";

	case EXPR_INTEGER:
		return "integer";

	default:
		assert(0);
	}
}

struct operator {
	const char *name;
	unsigned nargs;
	struct expression_value (*evaluate)(struct expression *,
					    enum expr_type context);
	void (*free)(struct expression *);
};

static struct expression *__expression_build(struct srcpos *loc,
					     struct operator *op, ...)
{
	int nargs = 0;
	struct expression *expr;
	va_list ap;
	int i;

	/* Sanity check number of arguments */
	va_start(ap, op);
	while (va_arg(ap, struct expression *) != NULL)
		nargs++;
	va_end(ap);

	expr = xmalloc(sizeof(*expr) + nargs*sizeof(struct expression *));
	expr->op = op;
	expr->nargs = nargs;
	if (loc)
		expr->loc = srcpos_copy(loc);
	else
		expr->loc = NULL;

	va_start(ap, op);
	for (i = 0; i < nargs; i++)
		expr->arg[i] = va_arg(ap, struct expression *);
	va_end(ap);

	return expr;
}
#define expression_build(loc, ...)		\
	(__expression_build(loc, __VA_ARGS__, NULL))

void expression_free(struct expression *expr)
{
	int i;

	for (i = 0; i < expr->nargs; i++)
		expression_free(expr->arg[i]);
	if (expr->op->free)
		expr->op->free(expr);
	free(expr);
}

static struct expression_value type_error(struct expression *expr,
					  const char *fmt, ...)
{
	static const struct expression_value v = {
		.type = EXPR_VOID,
	};

	va_list(ap);

	va_start(ap, fmt);
	srcpos_verror(expr->loc, "Type error", fmt, ap);
	va_end(ap);

	return v;
}

struct expression_value expression_evaluate(struct expression *expr,
					    enum expr_type context)
{
	struct expression_value v = expr->op->evaluate(expr, context);

	if ((context != EXPR_VOID) && (context != v.type))
		return type_error(expr, "Expected %s expression (found %s)",
				  expression_typename(context),
				  expression_typename(v.type));

	return v;
}

#define EVALUATE(_v, _ex, _ctx) \
	do { \
		(_v) = expression_evaluate((_ex), (_ctx)); \
		if ((_v).type == EXPR_VOID) \
			return (_v); \
	} while (0)

#define EVALUATE_INT(_vi, _ex) \
	do { \
		struct expression_value _v; \
		EVALUATE(_v, (_ex), EXPR_INTEGER); \
		(_vi) = (_v).value.integer; \
	} while (0)

static struct expression_value op_eval_constant(struct expression *expr,
						enum expr_type context)
{
	assert(expr->nargs == 0);
	return expr->u.constant;
}
static struct operator op_constant = {
	.name = "constant",
	.evaluate = op_eval_constant,
};

static struct expression *__expression_constant(struct srcpos *loc,
						struct expression_value val)
{
	struct expression *expr = expression_build(loc, &op_constant);

	expr->u.constant = val;
	return expr;
}

struct expression *expression_integer_constant(struct srcpos *pos,
					       uint64_t val)
{
	struct expression_value v = {
		.type = EXPR_INTEGER,
		.value.integer = val,
	};

	return __expression_constant(pos, v);
}

#define INT_UNARY_OP(nm, cop) \
	static struct expression_value op_eval_##nm(struct expression *expr, \
						    enum expr_type context) \
	{ \
		struct expression_value v = { .type = EXPR_INTEGER, }; \
		uint64_t arg; \
		assert(expr->nargs == 1); \
		EVALUATE_INT(arg, expr->arg[0]); \
		v.value.integer = cop arg; \
		return v; \
	} \
	static struct operator op_##nm = { \
		.name = #cop, \
		.evaluate = op_eval_##nm, \
	}; \
	struct expression *expression_##nm(struct srcpos *loc, \
					   struct expression *arg) \
	{ \
		return expression_build(loc, &op_##nm, arg);	\
	}

INT_UNARY_OP(negate, -)
INT_UNARY_OP(bit_not, ~)
INT_UNARY_OP(logic_not, !)

#define INT_BINARY_OP(nm, cop) \
	static struct expression_value op_eval_##nm(struct expression *expr, \
						    enum expr_type context) \
	{ \
		struct expression_value v = { .type = EXPR_INTEGER, }; \
		uint64_t arg0, arg1; \
		assert(expr->nargs == 2); \
		EVALUATE_INT(arg0, expr->arg[0]); \
		EVALUATE_INT(arg1, expr->arg[1]); \
		v.value.integer = arg0 cop arg1; \
		return v; \
	} \
	static struct operator op_##nm = { \
		.name = #cop, \
		.evaluate = op_eval_##nm, \
	}; \
	struct expression *expression_##nm(struct srcpos *loc, \
					   struct expression *arg1, \
					   struct expression *arg2) \
	{ \
		return expression_build(loc, &op_##nm, arg1, arg2);	\
	}

INT_BINARY_OP(mod, %)
INT_BINARY_OP(div, /)
INT_BINARY_OP(mul, *)

INT_BINARY_OP(add, +)
INT_BINARY_OP(sub, -)

INT_BINARY_OP(lshift, <<)
INT_BINARY_OP(rshift, >>)

INT_BINARY_OP(lt, <)
INT_BINARY_OP(gt, >)
INT_BINARY_OP(le, <=)
INT_BINARY_OP(ge, >=)

INT_BINARY_OP(eq, ==)
INT_BINARY_OP(ne, !=)

INT_BINARY_OP(bit_and, &)
INT_BINARY_OP(bit_xor, ^)
INT_BINARY_OP(bit_or, |)

INT_BINARY_OP(logic_and, &&)
INT_BINARY_OP(logic_or, ||)

static struct expression_value op_eval_conditional(struct expression *expr,
						   enum expr_type context)
{
	uint64_t cond;

	assert(expr->nargs == 3);
	EVALUATE_INT(cond, expr->arg[0]);

	return cond ? expression_evaluate(expr->arg[1], context)
		: expression_evaluate(expr->arg[2], context);
}
static struct operator op_conditional = {
	.name = "?:",
	.evaluate = op_eval_conditional,
};
struct expression *expression_conditional(struct srcpos *loc,
					  struct expression *arg1,
					  struct expression *arg2,
					  struct expression *arg3)
{
	return expression_build(loc, &op_conditional, arg1, arg2, arg3);
}
