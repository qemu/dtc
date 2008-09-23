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


static const char *ir_type_str[IR_NUM_TYPES] = {
	"IR_UNDEF",
	"IR_ROOT",
	"IR_MEM_RESERVE",
	"IR_ASSIGN",
	"IR_PROP_DEF",
	"IR_REF_PHANDLE",
	"IR_REF_PATH",
	"IR_CELL",
	"IR_LITERAL",
	"IR_LIT_STR",
	"IR_LIT_BYTE",
	"IR_LABEL",
	"IR_LIST",
	"IR_INCBIN",
	"IR_BUILTIN",
	"IR_SELECT",
	"IR_OR",
	"IR_AND",
	"IR_BIT_OR",
	"IR_BIT_XOR",
	"IR_BIT_AND",
	"IR_EQ",
	"IR_LT",
	"IR_LE",
	"IR_GT",
	"IR_GE",
	"IR_NE",
	"IR_LSHIFT",
	"IR_RSHIFT",
	"IR_ADD",
	"IR_MINUS",
	"IR_MULT",
	"IR_DIV",
	"IR_MOD",
	"IR_UMINUS",
	"IR_BIT_COMPL",
	"IR_NOT",
	"IR_FUNC_DEF",
	"IR_FOR",
	"IR_RETURN",
	"IR_RANGE",
	"IR_ID",
	"IR_IF",
	"IR_PARAMDECL",
	"IR_FUNC_CALL",
	"IR_NODE",
	"IR_PROPNODENAME",
	"IR_LIT_CELL",
	"IR_LIT_ADDR",
	"IR_CVT_PROPNODENAME",
	"IR_CVT_STRING",
	"IR_CONST_DEF",
};


const char *
ir_type_string(ir_type ir_type)
{
	if (0 <= ir_type && ir_type < IR_NUM_TYPES)
		return ir_type_str[ir_type];
	else
		return "<unknown>";
}


static void
ir_dump_node(struct ir *ir, unsigned int level)
{
	int indent = 4 * level;
	struct ir *iri;

	if (ir == NULL)
		return;

	/*
	 * Print node values
	 */
	printf("%*sNODE       : %p\n",
	       indent, "", ir);

	printf("%*sir_type    : %s\n",
	       indent, "", ir_type_string(ir->ir_type));

	printf("%*sir_srcpos : %s\n",
	       indent, "", srcpos_string(ir->ir_srcpos));


	printf("%*sir_literal : %lld\n", indent, "", ir->ir_literal);

	if (ir->ir_builtin_id != IRB_UNDEF) {
		printf("%*sir_builtin_id : %d\n",
		       indent, "", ir->ir_builtin_id);
	}
	if (ir->ir_lit_str) {
		printf("%*sir_lit_str : %s\n",
		       indent, "", ir->ir_lit_str);
	}
	if (ir->ir_label_name) {
		printf("%*sir_label_name : %s\n",
		       indent, "", ir->ir_label_name);
	}

	if (ir->ir_name) {
		printf("%*sir_name   : %p\n",
		       indent, "", ir->ir_name);
		ir_dump_node(ir->ir_name, level + 1);
	}

	if (ir->ir_label) {
		printf("%*sir_label   : %p\n",
		       indent, "", ir->ir_label);
		ir_dump_node(ir->ir_label, level + 1);
	}

	if (ir->ir_first)
		printf("%*sir_first   : %p\n",
		       indent, "", ir->ir_first);
	if (ir->ir_last)
		printf("%*sir_last    : %p\n",
		       indent, "", ir->ir_last);
	if (ir->ir_next)
		printf("%*sir_next    : %p\n",
		       indent, "", ir->ir_next);
	if (ir->ir_prev)
		printf("%*sir_prev    : %p\n",
		       indent, "", ir->ir_prev);

	/*
	 * Dump sub-expressions
	 */
	if (ir->ir_expr1) {
		printf("%*sir_expr1   : %p\n",
		       indent, "", ir->ir_expr1);
		ir_dump_node(ir->ir_expr1, level + 1);
	}

	if (ir->ir_expr2) {
		printf("%*sir_expr2   : %p\n",
		       indent, "", ir->ir_expr2);
		ir_dump_node(ir->ir_expr2, level + 1);
	}

	if (ir->ir_expr3) {
		printf("%*sir_expr3   : %p\n",
		       indent, "", ir->ir_expr3);
		ir_dump_node(ir->ir_expr3, level + 1);
	}

	/*
	 * Recursively dump declarations.
	 */
	if (ir->ir_declarations) {
		printf("%*sir_declarations: %p\n",
		       indent, "", ir->ir_declarations);
		ir_dump_node(ir->ir_declarations, level + 1);
	}

	/*
	 * Recursively dump statements.
	 */
	if (ir->ir_statements) {
		printf("%*sir_statements: %p\n",
		       indent, "", ir->ir_statements);
		ir_dump_node(ir->ir_statements, level + 1);
	}
	if (ir->ir_statements2) {
		printf("%*sir_statements2: %p\n",
		       indent, "", ir->ir_statements2);
		ir_dump_node(ir->ir_statements2, level + 1);
	}

	/*
	 * Recursively dump LIST chain.
	 */
	for (iri = ir->ir_first; iri != NULL; iri = iri->ir_next) {
		ir_dump_node(iri, level + 1);
	}
}


extern void
ir_dump(struct ir *ir)
{
	ir_dump_node(ir, 0);
}
