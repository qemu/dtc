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
#include "ir_scope.h"
#include "nv.h"

extern struct boot_info *the_boot_info;


void ir_emit_node(struct ir *ir);


void
ir_emit_prop_def(struct ir *irp)
{
	struct ir *ir_lhs;
	struct property *p;
	char *prop_name;
	char *lab;
	struct data d;

	debug("ir_emit_prop_def(");

	lab = ir_eval_for_label(irp->ir_label);
	if (lab) {
		debug("%s : ", lab);
	}

	ir_lhs = ir_eval(irp->ir_expr1);
	prop_name = ir_eval_for_name(ir_lhs);
	debug("%s = <expr>)\n", prop_name);

	if (prop_name) {
		d = empty_data;
		ir_eval_for_data(irp->ir_expr2, &d);
		p =  build_property(prop_name, d, lab);
		irs_append_property(p);
	}

	debug("ir_emit_prop_def(): Done\n");
}


void
ir_emit_assign(struct ir *ir_assign)
{
	char *var_name;
	struct ir_symbol *irsym;
	struct ir *ir_val;
	struct ir *ir_pos;

	ir_pos = ir_assign->ir_expr1 ? ir_assign->ir_expr1 : ir_assign;

	var_name = ir_eval_for_name(ir_assign->ir_expr1);

	debug("ir_emit_assign(%s)\n", var_name);

	if (!var_name) {
		ir_error(ir_pos, "Can't determine LHS name\n");
		return;
	}

	irsym = irs_lookup_local(var_name);
	if (irsym != NULL) {
		if (irsym->irsym_type == IRSYM_CONST) {
			ir_error(ir_pos,
				 "Can't assign to constant \"%s\"\n",
				 var_name);
		}
	} else {
		/*
		 * FIXME: Debate on-the-fly creation or pre-declaration.
		 */
		irsym = irs_create_local(var_name, IRSYM_VAR);
	}

	ir_val = ir_eval(ir_assign->ir_expr2);
	irsym->irsym_value = ir_val;
}


void
ir_emit_for(struct ir *ir_for)
{
	char *var_name;
	struct ir_symbol *irsym;
	struct ir *ir_id;
	struct ir *ir_val;
	struct ir *ir_range;
	unsigned long long var;
	unsigned long long start;
	unsigned long long stop;

	irs_push_scope(IRS_FOR_LOOP);

	/*
	 * Introduce for-loop variable into FOR_LOOP scope.
	 */
	ir_id = ir_for->ir_expr1;
	var_name = ir_eval_for_name(ir_id);
	irsym = irs_create_local(var_name, IRSYM_VAR);

	ir_val = ir_alloc(IR_LIT_ADDR, ir_id->ir_srcpos);
	irsym->irsym_value = ir_val;

	ir_range = ir_for->ir_expr2;
	start = ir_eval_for_addr(ir_range->ir_expr1);
	stop = ir_eval_for_addr(ir_range->ir_expr2);

	debug("Range appears to be %llu to %llu\n", start, stop);

	var = start;
	while (var <= stop ) {
	    ir_val->ir_literal = var;
	    ir_emit_statement_list(ir_for->ir_statements);
	    var++;
	}

	irs_pop_scope();
}


void
ir_emit_if(struct ir *ir_if)
{
	uint64_t lit;

	debug("ir_if()\n");
	lit = ir_eval_for_addr(ir_if->ir_expr1);
	if (lit) {
		ir_emit_statement_list(ir_if->ir_statements);
	} else {
		ir_emit_statement_list(ir_if->ir_statements2);
	}
}


void
ir_emit_return(struct ir *ir_return)
{
	struct ir *ir_ret_expr;

	ir_ret_expr = ir_eval(ir_return->ir_expr1);
	irs_set_return_value(ir_ret_expr);
}


void
ir_emit_func_call(struct ir *ir_func)
{
	struct ir_scope *irs_scope;

	/*
	 * Perform function body.
	 *
	 * Returned scope has node and property "side effects".
	 * Function return value is thrown to /dev/null.
	 */
	irs_scope = ir_eval_func_body(ir_func);

	/*
	 * Propagate any nodes or properties into parent scope.
	 */
	irs_scope_append_property_list(irs_scope->irs_prop_list);
	irs_scope_append_node_list(irs_scope->irs_node_list);
}


void
ir_emit_statement(struct ir *ir)
{
	if (ir == NULL)
		return;

	switch (ir->ir_type) {
	case IR_NODE:
		ir_emit_node(ir);
		break;

	case IR_PROP_DEF:
		ir_emit_prop_def(ir);
		break;

	case IR_FOR:
		ir_emit_for(ir);
		break;

	case IR_IF:
		ir_emit_if(ir);
		break;

	case IR_RETURN:
		ir_emit_return(ir);
		break;

	case IR_ASSIGN:
		ir_emit_assign(ir);
		break;

	case IR_FUNC_CALL:
		ir_emit_func_call(ir);
		break;

	case IR_LIST:
		/*
		 * FIXME: LIST within a LIST.  Optimize out earlier?
		 */
		ir_emit_statement_list(ir);
		break;

	default:
		ir_error(ir, "Unknown statement with ir_type %s\n",
			 ir_type_string(ir->ir_type));
	}
}


void
ir_emit_statement_list(struct ir *ir_list)
{
	struct ir *ir;

	if (ir_list == NULL)
		return;

	if (ir_list->ir_type != IR_LIST)
		return;

	for (ir = ir_list->ir_first; ir != NULL; ir = ir->ir_next) {
		ir_emit_statement(ir);
	}
}


/*
 * Enter a /define/ function definitin into IRS_ROOT symtab.
 */
void
ir_emit_func_def(struct ir *ir_func_def)
{
	char *func_name;
	struct ir_symbol *irsym;
	struct ir *ir_pos;

	ir_pos = ir_func_def->ir_expr1
		? ir_func_def->ir_expr1 : ir_func_def;

	func_name = ir_eval_for_name(ir_func_def->ir_name);


	irsym = irs_lookup(func_name, IRS_ROOT);
	if (irsym != NULL) {
		ir_error(ir_pos,
			 "Redefinintion of \"%s\" ignored\n",
			 func_name);
		return;
	}

	irsym = irs_create_symbol(IRS_ROOT, func_name, IRSYM_FUNCDEF);

	irsym->irsym_value = ir_func_def;

	debug("ir_emit_func_def(): Defined %s\n", func_name);
}


/*
 * Enter a /define/ function definitin into IRS_ROOT symtab.
 */
void
ir_emit_const_def(struct ir *ir_const_def)
{
	char *const_name;
	struct ir_symbol *irsym;
	struct ir *ir_val;
	struct ir *ir_pos;

	ir_pos = ir_const_def->ir_expr1
		? ir_const_def->ir_expr1 : ir_const_def;

	const_name = ir_eval_for_name(ir_const_def->ir_expr1);

	debug("ir_const_def(%s)\n", const_name);

	if (!const_name) {
		ir_error(ir_pos, "Can't determine LHS constant name\n");
		return;
	}

	irsym = irs_lookup(const_name, IRS_ANY);
	if (irsym != NULL) {
		ir_warn(ir_pos,
			"Redefinintion of \"%s\" ignored\n",
		        const_name);
		return;
	}

	irsym = irs_create_symbol(IRS_ROOT, const_name, IRSYM_CONST);

	ir_val = ir_eval(ir_const_def->ir_expr2);
	irsym->irsym_value = ir_val;
}


void
ir_emit_mem_reserve(struct ir *ir_mem)
{
	struct reserve_info *ri;
	uint64_t addr;
	uint64_t size;
	char *lab;

	if (ir_mem == NULL)
		return;

	addr = ir_eval_for_addr(ir_mem->ir_expr1);
	size = ir_eval_for_addr(ir_mem->ir_expr2);
	lab = ir_eval_for_label(ir_mem->ir_label);

	debug("ir_emit_mem_reserve(0x%llx, 0x%llx, ",
	      (unsigned long long)addr, (unsigned long long)size);
	if (lab) {
		debug("%s)\n", lab);
	} else {
		debug("<no-label>)\n");
	}

	ri = build_reserve_entry(addr, size, lab);
	irs_append_reserve(ri);
}


void
ir_emit_declaration_list(struct ir *ir_list)
{
	struct ir *ir;

	if (ir_list == NULL)
		return;

	if (ir_list->ir_type != IR_LIST)
		return;

	for (ir = ir_list->ir_first; ir != NULL; ir = ir->ir_next) {
		switch (ir->ir_type) {
		case IR_CONST_DEF:
			ir_emit_const_def(ir);
			break;

		case IR_FUNC_DEF:
			ir_emit_func_def(ir);
			break;

		case IR_MEM_RESERVE:
			ir_emit_mem_reserve(ir);
			break;

		default:
			ir_error(ir,
				 "Unknown declaration type %s\n",
				 ir_type_string(ir->ir_type));
			break;
		}
	}
}


void
ir_emit_node(struct ir *ir)
{
	struct ir *ir_name;
	struct ir *ir_label;
	char *name;
	char *label;
	struct node *node;
	struct node *node_list;
	struct ir_scope *irs_scope;

	if (ir == NULL)
		return;

	if (ir->ir_type != IR_NODE)
		return;

	ir_name = ir_eval(ir->ir_name);
	name = ir_eval_for_name(ir_name);

	ir_label = ir_eval(ir->ir_label);
	label = ir_eval_for_label(ir_label);

	debug("ir_emit_node(): Making node %s : %s\n", label, name);

	irs_push_scope(IRS_NODE);
	ir_emit_statement_list(ir->ir_statements);
	irs_scope = irs_pop_scope();

	node_list = reverse_nodes(irs_scope->irs_node_list);
	node = build_node(irs_scope->irs_prop_list, node_list);
	name_node(node, name, label);

	irs_append_node(node);
}


void
ir_add_cmd_line_constant_defs(void)
{
	struct nv_pair *nv;
	struct ir_symbol *irsym;
	struct ir *ir_const;

	for (nv = nv_list; nv != NULL; nv = nv->nv_next) {
		irsym = irs_create_symbol(IRS_ROOT, nv->nv_name, IRSYM_CONST);
		if (nv->nv_value) {
			ir_const = ir_alloc(IR_LITERAL, &srcpos_empty);
			ir_const->ir_lit_str = xstrdup(nv->nv_value);
			irsym->irsym_value = ir_const;
		}
	}
}


void
ir_emit_root(struct ir *ir)
{
	struct reserve_info *ri_list;
	struct node *node_list;

	if (ir == NULL)
		return;

	if (ir->ir_type != IR_ROOT) {
		ir_error(ir, "Bad root node\n");
		return;
	}

	irs_push_scope(IRS_ROOT);

	/*
	 * Establish constant definitions from command line.
	 */
	ir_add_cmd_line_constant_defs();

	/*
	 * Fast-and-loose... These are definitions, not declarations!
	 */
	ir_emit_declaration_list(ir->ir_declarations);

	/*
	 * Emit the root IR_NODE.
	 */
	ir_emit_node(ir->ir_statements);

	/*
	 * Build the_boot_info.
	 */
	ri_list = irs_scope_stack->irs_reserve_list;
	node_list = irs_scope_stack->irs_node_list;
	the_boot_info = build_boot_info(ri_list, node_list, 0);

	irs_pop_scope();
}


extern void
ir_emit(struct ir *ir)
{
	ir_emit_root(ir);
	fflush(stdout);
}
