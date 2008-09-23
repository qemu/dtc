/*
 * Copyright 2008 Jon Loeliger, Freescale Semiconductor, Inc.
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
#include "ir.h"

extern int treesource_error;


struct ir *the_ir_tree;


extern struct ir *
ir_alloc(ir_type ir_type, srcpos *pos)
{
	struct ir *ir;

	ir = xmalloc(sizeof(struct ir));
	memset(ir, 0, sizeof(struct ir));

	ir->ir_type = ir_type;
	ir->ir_srcpos = srcpos_copy(pos);

	return ir;
}


/*
 * Shallow copy, mostly.
 *
 * Note that things like the immediate strings and source position
 * are copied, but the sub-IR-expressions are not.
 */
extern struct ir *
ir_copy(struct ir *ir)
{
	struct ir *ir_new;

	if (!ir)
		return NULL;

	ir_new = ir_alloc(ir->ir_type, ir->ir_srcpos);
	ir_new->ir_literal = ir->ir_literal;
	if (ir->ir_lit_str)
		ir_new->ir_lit_str = xstrdup(ir->ir_lit_str);
	if (ir->ir_label_name)
		ir_new->ir_label_name = xstrdup(ir->ir_label_name);

	return ir_new;
}


void
ir_free(struct ir *ir)
{
}


void
ir_free_all(struct ir *ir)
{
}


extern struct ir *
ir_alloc_unop(ir_type ir_type, struct ir *ir1, srcpos *pos)
{
    struct ir *ir;

    ir = ir_alloc(ir_type, pos);
    ir->ir_expr1 = ir1;

    return ir;
}


extern struct ir *
ir_alloc_binop(ir_type ir_type,
	       struct ir *ir1, struct ir *ir2,
	       srcpos *pos)
{
    struct ir *ir;

    ir = ir_alloc(ir_type, pos);
    ir->ir_expr1 = ir1;
    ir->ir_expr2 = ir2;

    return ir;
}


extern struct ir *
ir_alloc_triop(ir_type ir_type,
	       struct ir *ir1, struct ir *ir2, struct ir *ir3,
	       srcpos *pos)
{
    struct ir *ir;

    ir = ir_alloc(ir_type, pos);
    ir->ir_expr1 = ir1;
    ir->ir_expr2 = ir2;
    ir->ir_expr3 = ir3;

    return ir;
}


extern struct ir *
ir_list_append(struct ir *ir_list, struct ir *ir_node)
{
	if (ir_node == NULL) {
		return ir_list;
	}

	if (ir_list == NULL) {
		ir_list = ir_alloc(IR_LIST, ir_node->ir_srcpos);
	}

	ir_node->ir_prev = ir_list->ir_last;

	if (ir_list->ir_last) {
		ir_list->ir_last->ir_next = ir_node;
	} else {
		ir_list->ir_first = ir_node;
	}

	ir_list->ir_last = ir_node;

	return ir_list;
}


void
ir_process(void)
{
	/*
	 * FIXME: Fix leaking the whole orginal IR tree here.
	 */
	the_ir_tree = ir_simplify(the_ir_tree, IR_EVAL_CTXT_ANY);

	ir_emit(the_ir_tree);
}


static const char *ir_severity_str[] = {
	"Info",
	"Warning",
	"Error",
};

#define IR_NUM_SEV	ARRAY_SIZE(ir_severity_str)


void
ir_msg(ir_severity_t severity, struct ir *ir, char const *fmt, ...)
{
	srcpos *pos;
	const char *srcstr;
	va_list va;
	va_start(va, fmt);

	pos = ir ? ir->ir_srcpos : &srcpos_empty;
	srcstr = srcpos_string(pos);

	if (severity >= IR_NUM_SEV)
		severity = IR_SEV_INFO;

	fprintf(stderr, "%s: %s ", ir_severity_str[severity], srcstr);

	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");

	va_end(va);
}
