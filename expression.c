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

#include "dtc.h"

struct operator {
	const char *name;
	unsigned nargs;
	uint64_t (*evaluate)(struct expression *);
	void (*free)(struct expression *);
};

static struct expression *__expression_build(struct operator *op, ...)
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

	va_start(ap, op);
	for (i = 0; i < nargs; i++)
		expr->arg[i] = va_arg(ap, struct expression *);
	va_end(ap);

	return expr;
}
#define expression_build(...) \
	(__expression_build(__VA_ARGS__, NULL))

void expression_free(struct expression *expr)
{
	int i;

	for (i = 0; i < expr->nargs; i++)
		expression_free(expr->arg[i]);
	if (expr->op->free)
		expr->op->free(expr);
	free(expr);
}

uint64_t expression_evaluate(struct expression *expr)
{
	return expr->op->evaluate(expr);
}

static uint64_t op_eval_constant(struct expression *expr)
{
	assert(expr->nargs == 0);
	return expr->u.constant;
}
static struct operator op_constant = {
	.name = "constant",
	.evaluate = op_eval_constant,
};
struct expression *expression_constant(uint64_t val)
{
	struct expression *expr = expression_build(&op_constant);

	expr->u.constant = val;
	return expr;
}

#define INT_UNARY_OP(nm, cop) \
	static uint64_t op_eval_##nm(struct expression *expr) \
	{ \
		assert(expr->nargs == 1); \
		return cop expression_evaluate(expr->arg[0]);	\
	} \
	static struct operator op_##nm = { \
		.name = #cop, \
		.evaluate = op_eval_##nm, \
	}; \
	struct expression *expression_##nm(struct expression *arg)	\
	{ \
		return expression_build(&op_##nm, arg);	\
	}

INT_UNARY_OP(negate, -)
INT_UNARY_OP(bit_not, ~)
INT_UNARY_OP(logic_not, !)

#define INT_BINARY_OP(nm, cop) \
	static uint64_t op_eval_##nm(struct expression *expr) \
	{ \
		assert(expr->nargs == 2); \
		return expression_evaluate(expr->arg[0]) \
			cop expression_evaluate(expr->arg[1]);	\
	} \
	static struct operator op_##nm = { \
		.name = #cop, \
		.evaluate = op_eval_##nm, \
	}; \
	struct expression *expression_##nm(struct expression *arg1, struct expression *arg2) \
	{ \
		return expression_build(&op_##nm, arg1, arg2);	\
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

static uint64_t op_eval_conditional(struct expression *expr)
{
	assert(expr->nargs == 3);
	return expression_evaluate(expr->arg[0])
		? expression_evaluate(expr->arg[1])
		: expression_evaluate(expr->arg[2]);
}
static struct operator op_conditional = {
	.name = "?:",
	.evaluate = op_eval_conditional,
};
struct expression *expression_conditional(struct expression *arg1, struct expression *arg2,
					  struct expression *arg3)
{
	return expression_build(&op_conditional, arg1, arg2, arg3);
}
