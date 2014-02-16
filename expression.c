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

	case EXPR_STRING:
		return "string";

	case EXPR_BYTESTRING:
		return "bytestring";

	default:
		assert(0);
	}
}

static struct expression_value value_clone(struct expression_value val)
{
	struct expression_value clone = val;

	switch (val.type) {
	case EXPR_STRING:
	case EXPR_BYTESTRING:
		clone.value.d = data_clone(val.value.d);
		break;

	default:
		/* nothing more to do */
		;
	}

	return clone;
}

static void value_free(struct expression_value val)
{
	switch (val.type) {
	case EXPR_STRING:
	case EXPR_BYTESTRING:
		data_free(val.value.d);
		break;

	default:
		/* nothing to do */
		;
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

	/* Strings can be promoted to bytestrings */
	if ((v.type == EXPR_STRING)
	    && (context == EXPR_BYTESTRING))
		v.type = EXPR_BYTESTRING;

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

#define EVALUATE_STR(_vs, _ex) \
	do { \
		struct expression_value _v; \
		EVALUATE(_v, (_ex), EXPR_STRING); \
		(_vs) = (_v).value.d; \
	} while (0)

#define EVALUATE_BS(_vd, _ex) \
	do { \
		struct expression_value _v; \
		EVALUATE(_v, (_ex), EXPR_BYTESTRING); \
		(_vd) = (_v).value.d; \
	} while (0)

static struct expression_value op_eval_constant(struct expression *expr,
						enum expr_type context)
{
	assert(expr->nargs == 0);
	return value_clone(expr->u.constant);
}
static void op_free_constant(struct expression *expr)
{
	value_free(expr->u.constant);
}

static struct operator op_constant = {
	.name = "constant",
	.evaluate = op_eval_constant,
	.free = op_free_constant,
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

struct expression *expression_string_constant(struct srcpos *pos,
					      struct data val)
{
	struct expression_value v = {
		.type = EXPR_STRING,
		.value.d = val,
	};

	return __expression_constant(pos, v);
}

struct expression *expression_bytestring_constant(struct srcpos *pos,
						  struct data val)
{
	struct expression_value v = {
		.type = EXPR_BYTESTRING,
		.value.d = val,
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

/*
 * We need to write out add and mul in full, since they can be used on
 * both integer and string arguments with different meanings
 */

static struct expression_value op_eval_mul(struct expression *expr,
					   enum expr_type context)
{
	struct expression_value arg0, arg1;
	struct expression_value v;
	uint64_t n, i;
	struct data s;
	struct data d = empty_data;

	assert(expr->nargs == 2);
	EVALUATE(arg0, expr->arg[0], EXPR_VOID);
	EVALUATE(arg1, expr->arg[1], EXPR_VOID);

	if ((arg0.type == EXPR_INTEGER) && (arg1.type == EXPR_INTEGER)) {
		v.type = EXPR_INTEGER;
		v.value.integer = arg0.value.integer * arg1.value.integer;
		return v;
	} else if ((arg0.type != EXPR_INTEGER) && (arg0.type != EXPR_STRING)) {
		return type_error(expr->arg[0], "Expected integer or string"
				  " expression (found %s)",
				  expression_typename(arg0.type));
	} else if (arg0.type == EXPR_INTEGER) {
		if (arg1.type != EXPR_STRING)
			return type_error(expr->arg[1], "Expected string"
					  " expression (found %s)",
					  expression_typename(arg1.type));
		n = arg0.value.integer; 
		s = arg1.value.d;
	} else {
		assert(arg0.type == EXPR_STRING);
		if (arg1.type != EXPR_INTEGER)
			return type_error(expr->arg[1], "Expected integer"
					  " expression (found %s)",
					  expression_typename(arg1.type));
		n = arg1.value.integer;
		s = arg0.value.d;
	}

	for (i = 0; i < n; i++)
		d = data_append_data(d, s.val, s.len - 1);

	v.type = EXPR_STRING;
	v.value.d = data_append_byte(d, 0); /* Terminating \0 */

	return v;
}
static struct operator op_mul = {
	.name = "*",
	.evaluate = op_eval_mul,
};
struct expression *expression_mul(struct srcpos *loc,
				  struct expression *arg0,
				  struct expression *arg1)
{
	return expression_build(loc, &op_mul, arg0, arg1);
}

static struct expression_value op_eval_add(struct expression *expr,
					   enum expr_type context)
{
	struct expression_value arg0, arg1;
	struct expression_value v;

	assert(expr->nargs == 2);
	EVALUATE(arg0, expr->arg[0], EXPR_VOID);
	EVALUATE(arg1, expr->arg[1], EXPR_VOID);
	if ((arg0.type != EXPR_INTEGER) && (arg0.type != EXPR_STRING))
		return type_error(expr->arg[0], "Expected integer or string"
				  " expression (found %s)",
				  expression_typename(arg0.type));
	if ((arg1.type != EXPR_INTEGER) && (arg1.type != EXPR_STRING))
		return type_error(expr->arg[0], "Expected integer or string"
				  " expression (found %s)",
				  expression_typename(arg1.type));

	if (arg0.type != arg1.type)
		return type_error(expr, "Operand types to + (%s, %s) don't match",
				  expression_typename(arg0.type),
				  expression_typename(arg1.type));

	v.type = arg0.type;

	switch (v.type) {
	case EXPR_INTEGER:
		v.value.integer = arg0.value.integer + arg1.value.integer;
		break;

	case EXPR_STRING:
		v.value.d = data_copy_mem(arg0.value.d.val,
					  arg0.value.d.len - 1);
		v.value.d = data_append_data(v.value.d, arg1.value.d.val,
					     arg1.value.d.len);
		break;

	default:
		assert(0);
	}
	return v;
}
static struct operator op_add = {
	.name = "+",
	.evaluate = op_eval_add,
};
struct expression *expression_add(struct srcpos *loc,
				  struct expression *arg0,
				  struct expression *arg1)
{
	return expression_build(loc, &op_add, arg0, arg1);
}

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


static struct expression_value op_eval_incbin(struct expression *expr,
					      enum expr_type context)
{
	struct data filename;
	uint64_t offset, len;
	FILE *f;
	struct expression_value v = {
		.type = EXPR_BYTESTRING,
	};

	EVALUATE_STR(filename, expr->arg[0]);
	EVALUATE_INT(offset, expr->arg[1]);
	EVALUATE_INT(len, expr->arg[2]);

	f = srcfile_relative_open(filename.val, NULL);

	if (offset != 0)
		if (fseek(f, offset, SEEK_SET) != 0)
			die("Couldn't seek to offset %llu in \"%s\": %s",
			    (unsigned long long)offset, filename.val,
			    strerror(errno));

	v.value.d = data_copy_file(f, len);

	fclose(f);
	return v;
}
static struct operator op_incbin = {
	.name = "/incbin/",
	.nargs = 3,
	.evaluate = op_eval_incbin,
};
struct expression *expression_incbin(struct srcpos *loc,
				     struct expression *file,
				     struct expression *off,
				     struct expression *len)
{
	return expression_build(loc, &op_incbin, file, off, len);
}

static struct expression_value op_eval_arraycell(struct expression *expr,
						 enum expr_type context)
{
	uint64_t cellval;
	struct expression_value v = {
		.type = EXPR_BYTESTRING,
	};
	int bits = expr->u.bits;

	assert(expr->nargs == 1);
	EVALUATE_INT(cellval, expr->arg[0]);

	v.value.d = data_append_integer(empty_data, cellval, bits);
	return v;
}
static struct operator op_arraycell = {
	.name = "< >",
	.evaluate = op_eval_arraycell,
};
struct expression *expression_arraycell(struct srcpos *loc, int bits,
					struct expression *cell)
{
	struct expression *expr = expression_build(loc, &op_arraycell, cell);

	expr->u.bits = bits;
	return expr;
}

static struct expression_value op_eval_join(struct expression *expr,
					    enum expr_type context)
{
	struct data arg0, arg1;
	struct expression_value v = {
		.type = EXPR_BYTESTRING,
	};

	assert(expr->nargs == 2);
	EVALUATE_BS(arg0, expr->arg[0]);
	EVALUATE_BS(arg1, expr->arg[1]);

	v.value.d = data_merge(arg0, arg1);
	return v;
}
static struct operator op_join = {
	.name = ",",
	.evaluate = op_eval_join,
};
struct expression *expression_join(struct srcpos *loc,
				   struct expression *arg0,
				   struct expression *arg1)
{
	return expression_build(loc, &op_join, arg0, arg1);
}
