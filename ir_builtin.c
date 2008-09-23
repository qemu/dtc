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

#include <stdio.h>

#include "dtc.h"
#include "ir.h"


typedef struct ir * (*irb_impl_func)(struct ir *ir_params);

struct builtin_func {
	char *name;
	irb_impl_func implementation;
};

struct ir *ir_builtin_join(struct ir *ir_builtin);
struct ir *ir_builtin_hexstr(struct ir *ir_builtin);
struct ir *ir_builtin_list(struct ir *ir_builtin);
struct ir *ir_builtin_cell(struct ir *ir_builtin);

static const struct builtin_func builtin_table[] = {
	{ "join", ir_builtin_join },
	{ "hexstr", ir_builtin_hexstr },
	{ "list", ir_builtin_list },
	{ "cell", ir_builtin_cell },
};

#define IRB_NUM_BUILTINS	ARRAY_SIZE(builtin_table)


irb_id
ir_lookup_builtin_by_name(char *str_name)
{
	irb_id irb;

	for (irb = 0; irb < IRB_NUM_BUILTINS; irb++) {
		if (strcmp(builtin_table[irb].name, str_name) == 0) {
			return irb;
		}
	}

	return IRB_UNDEF;
}


struct ir *
ir_eval_builtin(struct ir *ir)
{
	irb_id irb;
	const struct builtin_func *bf;

	if (ir == NULL)
		return NULL;

	if (ir->ir_type != IR_BUILTIN)
		return NULL;

	irb = ir->ir_builtin_id;

	if (irb <= IRB_UNDEF || irb >= IRB_NUM_BUILTINS)
		return NULL;

	bf = &builtin_table[irb];

	return (*bf->implementation)(ir);
}


struct ir *
ir_builtin_join(struct ir *ir_builtin)
{
	struct ir *ir_new;
	struct ir *irp;
	struct ir *ir;
	char *s;
	char *str;
	int len;
	char buf[30];

	debug("ir_builtin_impl_join():\n");

	irp = ir_builtin->ir_expr1;
	if (irp->ir_type == IR_LIST)
		irp = irp->ir_first;

	len = 1;
	str = xmalloc(1);
	*str  = 0;

	while (irp != NULL) {
		ir = ir_eval(irp);

		if (ir_is_string(ir)) {
			s = ir_eval_for_c_string(ir);
		} else if (ir_is_constant(ir)) {
			unsigned long long a;
			a = ir_eval_for_addr(ir);
			snprintf(buf, sizeof(buf), "%llu", a);
			s = buf;
		} else {
			ir_error(ir,
				 "Can't handle %s in join()\n",
				 ir_type_string(ir->ir_type));
			s = 0;
		}

		len += strlen(s);

		str = xrealloc(str, len);
		strcat(str, s);

		irp = irp->ir_next;
	}

	ir_new = ir_alloc(IR_LIT_STR, ir_builtin->ir_srcpos);
	ir_new->ir_lit_str = str;

	return ir_new;
}



struct ir *
ir_builtin_hexstr(struct ir *ir_builtin)
{
	struct ir *ir_new;
	struct ir *irp;
	struct ir *ir;
	char *s;
	char *str;
	int len;
	char buf[30];

	irp = ir_builtin->ir_expr1;
	if (irp->ir_type == IR_LIST)
		irp = irp->ir_first;

	len = 1;
	str = xmalloc(1);
	*str  = 0;

	while (irp != NULL) {
		ir = ir_eval(irp);
		if (ir_is_constant(ir)) {
			unsigned long long a;
			a = ir_eval_for_addr(ir);
			snprintf(buf, 30, "%llx", a);
			s = buf;
		} else {
			ir_error(ir,
				 "Can't handle %s in hexstr()\n",
				 ir_type_string(ir->ir_type));
			s = 0;
		}
		len += strlen(s);

		str = xrealloc(str, len);
		strcat(str, s);

		irp = irp->ir_next;
	}

	ir_new = ir_alloc(IR_LIT_STR, ir_builtin->ir_srcpos);
	ir_new->ir_lit_str = str;

	return ir_new;
}


struct ir *
ir_builtin_list(struct ir *ir_builtin)
{
	struct ir *ir_new;

	ir_new = ir_eval(ir_builtin->ir_expr1);

	if (ir_new && ir_new->ir_type != IR_LIST) {
		ir_new = ir_alloc_unop(IR_LIST, ir_new, ir_builtin->ir_srcpos);
	}

	return ir_new;
}


struct ir *
ir_builtin_cell(struct ir *ir_builtin)
{
	struct ir *irp;
	struct ir *ir_new;

	irp = ir_builtin->ir_expr1;
	if (irp->ir_type == IR_LIST)
		irp = irp->ir_first;

	ir_new = ir_eval(ir_alloc_unop(IR_CELL, irp, ir_builtin->ir_srcpos));

	return ir_new;
}
