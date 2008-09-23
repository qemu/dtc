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
#include "srcpos.h"
#include "ir.h"
#include "ir_scope.h"


/*
 * Returns:
 *    0 on success, with *val filled in
 *    -1 == bad characters in literal number
 *    -2 == literal out of range
 *    -3 == bad literal
 */

int
ir_eval_literal_guessing(const char *s, int base, int bits,
			 unsigned long long *val)
{
	char *e;

	errno = 0;
	*val = strtoull(s, &e, base);

	if (*e)
		return -1;
	else if ((errno == ERANGE)
		 || ((bits < 64) && (*val >= (1ULL << bits))))
		return -2;
	else if (errno != 0)
		return -3;

	return 0;
}

unsigned long long
ir_eval_literal_str(const char *s, int base, int bits)
{
	unsigned long long val;
	char *e;

	errno = 0;
	val = strtoull(s, &e, base);

	if (*e)
		die("bad characters in literal");
	else if ((errno == ERANGE)
		 || ((bits < 64) && (val >= (1ULL << bits))))
		die("literal %s out of range", s);
	else if (errno != 0)
		die("bad literal");

	return val;
}


struct ir *
ir_eval_cvt_to_string(struct ir *ir)
{
	char buf[30];
	unsigned long long lit1;
	struct ir *ir_new;

	ir_new = ir_alloc(IR_LIT_STR, ir->ir_srcpos);
	if (ir_is_constant(ir)) {
		lit1 = ir_eval_for_addr(ir);
		snprintf(buf, 30, "%llu", lit1);
		ir_new->ir_lit_str = xstrdup(buf);
	} else {
		ir_error(ir,
			 "Can't convert %s to a string\n",
			 ir_type_string(ir->ir_type));
	}

	return ir_new;
}


/*
 * FIXME: This should be named ir_is_constant_number()
 */
int
ir_is_constant(struct ir *ir)
{
	return ir &&
		(ir->ir_type == IR_LITERAL
		 || ir->ir_type == IR_LIT_BYTE
		 || ir->ir_type == IR_LIT_CELL
		 || ir->ir_type == IR_LIT_ADDR);
}


int
ir_is_string(struct ir *ir)
{
	return ir && ir->ir_type == IR_LIT_STR;
}


char *
ir_eval_for_label(struct ir *ir)
{
	char *str;

	if (ir == NULL)
		return NULL;

	if (ir->ir_type == IR_LABEL
	    || ir->ir_type == IR_REF_PATH
	    || ir->ir_type == IR_REF_PHANDLE) {
		str = xstrdup(ir->ir_label_name);
	} else if (ir->ir_type == IR_LIT_STR) {
		str = xstrdup(ir->ir_lit_str);
	} else {
		str = NULL;
	}

	return str;
}


char *
ir_eval_for_name(struct ir *ir)
{
	struct ir *ir_val;
	char *str;

	if (ir == NULL)
		return NULL;

	ir_val = ir;

	if (ir_val == NULL) {
		ir_error(ir, "Expected a name\n");
		return NULL;
	}

	if (ir_val->ir_type != IR_PROPNODENAME
	    && ir_val->ir_type != IR_ID
	    && ir_val->ir_type != IR_LIT_STR)
		return NULL;

	str = xstrdup(ir->ir_lit_str);

	return str;
}


/*
 * FIXME: This is misnamed.  Should be more like ir_eval_for_const()
 */
uint64_t
ir_eval_for_addr(struct ir *ir)
{
	unsigned long long a = 0;

	struct ir *ir_val;

	ir_val = ir_eval(ir);
	if (ir_val == NULL) {
		ir_error(ir, "Expected a const expression\n");
		return 0;
	}

	/*
	 * FIXME: UH, ir_is_constant() check or something?
	 */

	a  = ir_val->ir_literal;
	ir_free(ir_val);
	debug("eval_for_addr() is 0x%08llx\n", a);

	return a;
}


char *
ir_eval_for_c_string(struct ir *ir)
{
	struct data dtmp;
	char *p;

	if (ir == NULL)
		return NULL;


	if (!ir_is_string(ir))
		return NULL;

	p = ir->ir_lit_str;
	dtmp = data_copy_escape_string(p, strlen(p));

	return xstrdup(dtmp.val);
}


void
ir_eval_for_data(struct ir *ir, struct data *d)
{
	struct ir *ir_val;
	struct ir *iri;
	struct data dtmp;
	char *lab;
	cell_t c;
	unsigned long long ulit64;

	if (ir == NULL)
		return;

	ir_val = ir_eval(ir);

	switch (ir_val->ir_type) {
	case IR_LIST:
		for (iri = ir_val->ir_first; iri != NULL; iri = iri->ir_next) {
			ir_eval_for_data(iri, d);
		}
		break;

	case IR_LIT_STR:
		dtmp = data_copy_escape_string(ir_val->ir_lit_str,
					       strlen(ir_val->ir_lit_str));
		*d = data_merge(*d, dtmp);
		break;

	case IR_LIT_BYTE:
		*d = data_append_byte(*d, ir_val->ir_literal);
		break;

	case IR_LIT_ADDR:
		ulit64 = ir_val->ir_literal;
		*d = data_append_addr(*d, ulit64);
		break;

	case IR_LIT_CELL:
		c = (cell_t) ir_val->ir_literal;
		*d = data_append_cell(*d, c);
		break;

	case IR_CELL:
		ir_eval_for_data(ir_val->ir_expr1, d);
		break;

	case IR_LABEL:
		lab = ir_eval_for_label(ir);
		*d = data_add_marker(*d, LABEL, lab);
		break;

	case IR_REF_PATH:
		lab = ir_eval_for_label(ir);
		*d = data_add_marker(*d, REF_PATH, lab);
		break;

	case IR_REF_PHANDLE:
		lab = ir_eval_for_label(ir);
		*d = data_add_marker(*d, REF_PHANDLE, lab);
		*d = data_append_cell(*d, -1);
		break;

	case IR_INCBIN:	{
		struct search_path path = { srcpos_file->dir, NULL, NULL };
		struct data dinc = empty_data;
		char *file_name;
		struct dtc_file *file;
		unsigned long long start;
		unsigned long long len;
		struct ir *ir_pos;

		/*
		 * expr1 is file_name
		 * expr2 is start, NULL implies start of file
		 * expr3 is length, NULL implies whole file
		 */
		file_name = ir_eval_for_c_string(ir_val->ir_expr1);
		file = dtc_open_file(file_name, &path);

		ir_pos = ir_val->ir_expr2;
		start = ir_eval_for_addr(ir_val->ir_expr2);
		if (ir_val->ir_expr3)
			len = ir_eval_for_addr(ir_val->ir_expr3);
		else
			len = -1;

		if (start != 0) {
			if (fseek(file->file, start, SEEK_SET) != 0) {
				ir_error(ir_pos,
					 "Couldn't seek to offset %llu in \"%s\": %s",
					 (unsigned long long)start,
					 file_name,
					 strerror(errno));
			}
		}

		dinc = data_copy_file(file->file, len);
		*d = data_merge(*d, dinc);
		dtc_close_file(file);
		break;
	}

	default:
		ir_error(ir,
			 "Can't convert IR type %s to data\n",
			 ir_type_string(ir_val->ir_type));
		break;
	}
}


struct ir_scope *
ir_eval_func_body(struct ir *ir_func)
{
	char *func_name;
	struct ir *ir_func_def;
	struct ir *ir_parameters;
	struct ir *ir_statements;
	struct ir_symbol *irsym;
	struct ir *ir_p;
	struct ir *ir_f;
	char *param_name;
	struct ir_scope *irs_scope;
	struct ir *ir_next;
	struct ir *ir_pos;

	if (ir_func == NULL)
		return NULL;

	if (ir_func->ir_type != IR_FUNC_CALL)
		return NULL;

	/*
	 * Lookup the function definition.
	 */
	ir_pos = ir_func->ir_expr1;
	func_name = ir_eval_for_name(ir_func->ir_expr1);
	debug("ir_eval_func_body(): Looking up %s\n", func_name);

	irsym = irs_lookup(func_name, IRS_ANY);
	if (irsym == NULL || irsym->irsym_type != IRSYM_FUNCDEF) {
		ir_error(ir_pos,
			 "%s isn't a function definition\n",
			 func_name);
		return NULL;
	}

	ir_func_def = irsym->irsym_value;
	ir_statements = ir_func_def->ir_statements;
	ir_parameters = ir_func_def->ir_declarations;

	debug("ir_eval_func_body(): Found definition for %s\n",
	      irsym->irsym_name);

	/*
	 * Set up parameter binding via eval-and-copy-in.
	 *
	 * First pass evaluates each parameter expression and
	 * builds a temporary list of each eval() parameter.
	 * These evaluations need to be done before the function
	 * scope is opened.
	 *
	 * Remember to dodge a possible parent IR_LIST node.
	 */
	debug("ir_eval_func_body(): Evaluating parameters\n");
	ir_p = ir_eval(ir_func->ir_expr2);
	if (ir_p != NULL && ir_p->ir_type == IR_LIST) {
		ir_p = ir_p->ir_first;
	}

	/*
	 * Open an evaluation scope and symbol table for
	 * the function.
	 */
	irs_push_scope(IRS_FUNC_CALL);

	/*
	 * Second pass loops over each formal parameter
	 * and each actual expression simultaneously.
	 *
	 * Again remember to dodge a possible parent IR_LIST node.
	 */
	ir_f = ir_parameters;
	if (ir_f != NULL && ir_f->ir_type == IR_LIST) {
		ir_f = ir_f->ir_first;
	}

	debug("ir_eval_func_body(): Binding parameter to formals\n");

	ir_pos = ir_p;
	while (ir_f != NULL && ir_p != NULL) {
		param_name = ir_f->ir_lit_str;

		debug("ir_eval_func_body(): Binding parameter %s\n",
		      param_name);

		irsym = irs_create_local(param_name, IRSYM_VAR);

		irsym->irsym_value = ir_p;
		ir_next = ir_p->ir_next;
		ir_p->ir_next = ir_p->ir_prev = NULL;

		ir_f = ir_f->ir_next;
		ir_p = ir_next;
		ir_pos = ir_p;
	}

	if (ir_f != NULL && ir_p == NULL) {
		ir_error(ir_pos,
			 "Not enough parameters to %s (%s)\n",
			 func_name,
			 srcpos_string(ir_func_def->ir_srcpos));
	}

	if (ir_f == NULL && ir_p != NULL) {
		ir_error(ir_pos,
			 "Too many parameters to %s (%s)\n",
			 func_name,
			 srcpos_string(ir_func_def->ir_srcpos));
	}

	/*
	 * And "invoke" it.
	 */
	ir_emit_statement_list(ir_statements);

	/*
	 * FIXME:  Do parameter copy-out here?
	 */
	irs_scope = irs_pop_scope();

	/*
	 * FIXME: This is a bit dodgy perhaps.
	 */
	return irs_scope;
}


struct ir *
ir_eval_func_call(struct ir *ir_func)
{
	struct ir_scope *irs_scope;

	/*
	 * Perform function body.
	 * Returned scope has "side effects".
	 *
	 * This context really just wants the return value,
	 * but we could debate using nodes and properties too?
	 */
	irs_scope = ir_eval_func_body(ir_func);

	if (!irs_scope)
		return NULL;

	return irs_scope->irs_expr;
}


struct ir *
ir_eval(struct ir *ir)
{
	struct ir *ir_new;
	struct ir *iri;
	struct ir *ir1;
	struct ir *ir2;
	struct ir_symbol *irsym;
	unsigned long long lit1;
	unsigned long long lit2;
	char *str;
	int len;

	if (ir == NULL)
		return NULL;

	ir_new = NULL;

	/*
	 * Perform IR node-specific evaluations.
	 */
	switch (ir->ir_type) {
	case IR_LIT_STR:
	case IR_LIT_BYTE:
	case IR_LIT_CELL:
	case IR_LIT_ADDR:
	case IR_PROPNODENAME:
	case IR_REF_PATH:
		/*
		 * Values already present in the IR node.
		 */
		ir_new = ir_copy(ir);
		break;

	case IR_REF_PHANDLE:
		/*
		 * Promote a REF_PHANDLE of a LIT_STR to a
		 * direct REF_PHANDLE.
		 */
		ir_new = ir_copy(ir);
		if (ir->ir_label) {
			iri = ir_eval(ir->ir_label);
			if (ir_is_string(iri)) {
				ir_new->ir_label_name = iri->ir_lit_str;
			}
		}
		break;

	case IR_LABEL:
		ir_new = ir_copy(ir);
		break;

	case IR_CVT_PROPNODENAME:
		iri = ir_eval(ir->ir_expr1);
		str = ir_eval_for_name(iri);
		if (str) {
			ir_new = ir_alloc(IR_PROPNODENAME, ir->ir_srcpos);
			ir_new->ir_lit_str = str;
		}
		break;

	case IR_CVT_STRING:
		iri = ir_eval(ir->ir_expr1);
		ir_new = ir_eval_cvt_to_string(iri);
		break;

	case IR_ID:
		irsym = irs_lookup(ir->ir_lit_str, IRS_ANY);
		if (irsym != NULL) {
			ir_new = ir_eval(irsym->irsym_value);
		} else {
			ir_error(ir,
				 "Unknown value for \"%s\"\n",
				 ir->ir_lit_str);
		}
		break;

	case IR_LIST:
		ir_new = ir_alloc(IR_LIST, ir->ir_srcpos);
		for (iri = ir->ir_first; iri != NULL; iri = iri->ir_next) {
			ir_list_append(ir_new, ir_eval(iri));
		}
		break;

	case IR_SELECT:
		/*
		 * Pick the ? or the : side.
		 */
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		if (lit1) {
			ir_new = ir_eval(ir->ir_expr2);
		} else {
			ir_new = ir_eval(ir->ir_expr3);
		}
		break;

	case IR_OR:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		if (!lit1) {
			lit1 = ir_eval_for_addr(ir->ir_expr2);
		}
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = (lit1 != 0);
		break;

	case IR_AND:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		if (lit1) {
			lit1 = ir_eval_for_addr(ir->ir_expr2);
		}
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = (lit1 != 0);

		break;

	case IR_BIT_OR:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 | lit2;
		break;

	case IR_BIT_XOR:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 ^ lit2;
		break;

	case IR_BIT_AND:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 & lit2;
		break;

	case IR_EQ:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 == lit2;
		break;

	case IR_LT:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 < lit2;
		break;

	case IR_LE:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 <= lit2;
		break;

	case IR_GT:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 > lit2;
		break;

	case IR_GE:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 >= lit2;
		break;

	case IR_NE:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 != lit2;
		break;

	case IR_LSHIFT:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 << lit2;
		break;

	case IR_RSHIFT:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 >> lit2;
		break;

	case IR_ADD:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 + lit2;
		break;

	case IR_MINUS:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 - lit2;
		break;

	case IR_MULT:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 * lit2;
		break;

	case IR_DIV:
		/* FIXME: check for division by const 0 */
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		lit2 = ir_eval_for_addr(ir->ir_expr2);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = lit1 / lit2;
		break;

	case IR_MOD:
		/*
		 * This is really a bit upside down due to not having
		 * a real typing system.  Cope for now.
		 */
		ir1 = ir_eval(ir->ir_expr1);
		ir2 = ir_eval(ir->ir_expr2);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			/*
			 * FIXME: check for division by const 0.
			 */
			lit1 = ir_eval_for_addr(ir->ir_expr1);
			lit2 = ir_eval_for_addr(ir->ir_expr2);
			ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
			ir_new->ir_literal = lit1 % lit2;

		} else if (ir_is_string(ir1) || ir_is_string(ir2)) {
			if (!ir_is_string(ir1))
				ir1 = ir_eval_cvt_to_string(ir1);
			if (!ir_is_string(ir2))
				ir2 = ir_eval_cvt_to_string(ir2);
			len = strlen(ir1->ir_lit_str)
				+ strlen(ir2->ir_lit_str) + 1;
			str = xmalloc(len);
			strcpy(str, ir1->ir_lit_str);
			strcat(str, ir2->ir_lit_str);
			str[len - 1] = 0;
			ir_new = ir_alloc(IR_LIT_STR, ir->ir_srcpos);
			ir_new->ir_lit_str = str;
		}
		break;

	case IR_BIT_COMPL:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = ~lit1;
		break;

	case IR_NOT:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = !lit1;
		break;

	case IR_UMINUS:
		lit1 = ir_eval_for_addr(ir->ir_expr1);
		ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		ir_new->ir_literal = -lit1;
		break;

	case IR_FUNC_CALL:
		ir_new = ir_eval_func_call(ir);
		break;

	case IR_BUILTIN:
		ir_new = ir_eval_builtin(ir);
		break;

	case IR_RANGE:
		ir_new = ir_copy(ir);
		ir_new->ir_expr1 = ir_eval(ir->ir_expr1);
		ir_new->ir_expr2 = ir_eval(ir->ir_expr2);
		break;

	case IR_CELL:
		ir_new = ir_eval(ir->ir_expr1);
		if (ir_is_constant(ir_new)) {
			/* FIXME: Check for 32-bit range here? */
			ir_new->ir_type = IR_LIT_CELL;
		} else if (ir_is_string(ir_new)) {
			/* empty */
		} else if (ir_new->ir_type == IR_LIST) {
			/*
			 * FIXME: Bah.  should verify each list item
			 * is constant.
			 */
		} else {
			ir_error(ir, "Can't determine CELL value\n");
		}
		break;

	case IR_LITERAL:
		lit1 = 0;
		if (ir_eval_literal_guessing(ir->ir_lit_str,
					     0, 64, &lit1) == 0) {
			/*
			 * Smells like an number.
			 */
			ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
			ir_new->ir_literal = lit1;
		} else {
			/*
			 * Dunno what it is.  Must be a string.
			 * FIXME: ir_eval_for_c_string() here?
			 */
			ir_new = ir_alloc(IR_LIT_STR, ir->ir_srcpos);
			ir_new->ir_lit_str = xstrdup(ir->ir_lit_str);
		}
		break;

	case IR_INCBIN:
		ir_new = ir_copy(ir);
		ir_new->ir_expr1 = ir_eval(ir->ir_expr1);
		ir_new->ir_expr2 = ir_eval(ir->ir_expr2);
		ir_new->ir_expr3 = ir_eval(ir->ir_expr3);
		break;

	case IR_PROP_DEF:
		ir_error(ir, "Can't evaluate IR_PROP_DEF here.\n");
		break;

	case IR_ROOT:
	case IR_RETURN:
	case IR_IF:
	case IR_FOR:
	case IR_ASSIGN:
	case IR_MEM_RESERVE:
	case IR_FUNC_DEF:
	case IR_PARAMDECL:
	case IR_NODE:
		ir_error(ir,
			 "Can't evaluate %s statements in expressions\n",
			 ir_type_string(ir->ir_type));
		break;

	default:
		ir_error(ir,
			 "Unknown expression ir_type %s\n",
			 ir_type_string(ir->ir_type));
	}

	return ir_new;
}


struct ir *
ir_simplify_func_call(struct ir *ir, unsigned int ctxt)
{
	char *name;
	irb_id irb;
	struct ir *ir_new;
	struct ir *ir1;
	struct ir *ir2;

	ir_new = ir_copy(ir);
	ir1 = ir_simplify(ir->ir_expr1, ctxt);
	ir2 = ir_simplify(ir->ir_expr2, IR_EVAL_CTXT_ANY);
	name = ir_eval_for_name(ir1);
	if (name) {
		irb = ir_lookup_builtin_by_name(name);
		if (irb != IRB_UNDEF) {
			debug("ir_simplify(): Use builtin %s\n",
			      name);
			ir_new->ir_type = IR_BUILTIN;
			ir_new->ir_builtin_id = irb;
			ir_new->ir_expr1 = ir2;
		} else {
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
	} else {
		ir_error(ir1, "Unknown function %s\n", name);
	}

	return ir_new;
}



struct ir *
ir_simplify(struct ir *ir, unsigned int ctxt)
{
	struct ir *ir1;
	struct ir *ir2;
	ir_type ir_type;
	struct ir *ir_new;
	unsigned long long lit1;
	unsigned long long lit2;
	unsigned long long ulit64;

	if (ir == NULL)
		return NULL;

	/*
	 * First determine what the evaluation context will be
	 * for any sub-expression based on the current IR node.
	 */
	switch (ir->ir_type) {
	case IR_CELL:
		/*
		 * Pass new context down.
		 */
		ctxt = IR_EVAL_CTXT_CELL;
		break;

	case IR_INCBIN:
	case IR_IF:
	case IR_FOR:
	case IR_MEM_RESERVE:
		/*
		 * These are always done in an ANY context.
		 */
		ctxt = IR_EVAL_CTXT_ANY;
		break;

	default:
		/*
		 * Use the supplied (parameter) context.
		 */
		break;
	}


	/*
	 * Perform IR node-specific optimizations.
	 */
	switch (ir->ir_type) {
	case IR_ID:
	case IR_LIT_STR:
	case IR_LIT_BYTE:
	case IR_LIT_CELL:
	case IR_LIT_ADDR:
	case IR_PROPNODENAME:
	case IR_LABEL:
	case IR_REF_PATH:
		/*
		 * Already as simple as they can be.
		 */
		ir_new = ir_copy(ir);
		break;

	case IR_REF_PHANDLE:
		ir_new = ir_copy(ir);
		ir_new->ir_label = ir_simplify(ir->ir_label, ctxt);
		break;

	case IR_RETURN:
		ir_new = ir_copy(ir);
		ir_new->ir_expr1 = ir_simplify(ir->ir_expr1, ctxt);
		break;

	case IR_MEM_RESERVE:
	case IR_RANGE:
		ir_new = ir_copy(ir);
		ir_new->ir_expr1 = ir_simplify(ir->ir_expr1, ctxt);
		ir_new->ir_expr2 = ir_simplify(ir->ir_expr2, ctxt);
		break;

	case IR_CELL:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir1 && ir1->ir_type == IR_LIT_CELL) {
			ir_new = ir1;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
		}
		break;

	case IR_PROP_DEF:
		ir_new = ir_copy(ir);
		ir_new->ir_label = ir_simplify(ir->ir_label, ctxt);
		ir_new->ir_expr1 = ir_simplify(ir->ir_expr1, ctxt);
		ir_new->ir_expr2 = ir_simplify(ir->ir_expr2, ctxt);
		break;

	case IR_LITERAL:
		/*
		 * Based on context, evaluate literal into 32 or 64 bits.
		 * LIT_ADDR could be a lie; it just means 64-bit.  Feh.
		 */
		if (ctxt == IR_EVAL_CTXT_CELL) {
			ulit64 = ir_eval_literal_str(ir->ir_lit_str, 0, 32);
			ir_new = ir_alloc(IR_LIT_CELL, ir->ir_srcpos);
		} else {
			ulit64 = ir_eval_literal_str(ir->ir_lit_str, 0, 64);
			ir_new = ir_alloc(IR_LIT_ADDR, ir->ir_srcpos);
		}
		ir_new->ir_literal = ulit64;
		break;

	case IR_FUNC_CALL:
		ir_new = ir_simplify_func_call(ir, ctxt);
		break;

	case IR_BUILTIN:
		ir_new = ir_copy(ir);
		ir_new->ir_expr1 = ir_simplify(ir->ir_expr1, ctxt);
		ir_new->ir_expr2 = ir_simplify(ir->ir_expr2, ctxt);
		break;

	case IR_LIST:
		ir_new = ir_alloc(IR_LIST, ir->ir_srcpos);
		for (ir1 = ir->ir_first; ir1 != NULL; ir1 = ir1->ir_next) {
			ir_list_append(ir_new,
				       ir_simplify(ir1, ctxt));
		}
		break;

	case IR_INCBIN:
		/*
		 * Ponder loading and caching files?
		 */
		ir_new = ir_copy(ir);
		ir_new->ir_expr1 = ir_simplify(ir->ir_expr1, ctxt);
		ir_new->ir_expr2 = ir_simplify(ir->ir_expr2, ctxt);
		ir_new->ir_expr3 = ir_simplify(ir->ir_expr3, ctxt);
		break;

	case IR_ASSIGN:
		ir_new = ir_copy(ir);
		ir_new->ir_expr1 = ir_simplify(ir->ir_expr1, ctxt);
		ir_new->ir_expr2 = ir_simplify(ir->ir_expr2, ctxt);
		break;

	case IR_IF:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir_is_constant(ir1)) {
			/*
			 * Eliminate the IR_IF.
			 * Pick the THEN or ELSE statements only.
			 * FIXME: Fix leaking ir1 here.
			 */
			ulit64 = ir_eval_for_addr(ir1);
			if (ulit64) {
				/*
				 * Keep the THEN statements.
				 */
				ir_new = ir_simplify(ir->ir_statements, ctxt);
			} else {
				/*
				 * Keep the ELSE statements.
				 */
				ir_new = ir_simplify(ir->ir_statements2, ctxt);
			}
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir1 = ir_simplify(ir->ir_statements, ctxt);
			ir_new->ir_statements = ir1;
			ir1 = ir_simplify(ir->ir_statements2, ctxt);
			ir_new->ir_statements2 = ir1;
		}
		break;

	case IR_FOR:
		/*
		 * Lots of optimizations possible here based on
		 * empty statements and trivial ranges.  Later.
		 * FIXME: Do "for" simplification optimizations.
		 */
		ir_new = ir_copy(ir);
		ir_new->ir_expr1 = ir_simplify(ir->ir_expr1, ctxt);
		ir_new->ir_expr2 = ir_simplify(ir->ir_expr2, ctxt);
		ir_new->ir_expr3 = ir_simplify(ir->ir_expr3, ctxt);
		ir_new->ir_statements = ir_simplify(ir->ir_statements, ctxt);
		break;

	case IR_SELECT:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir_is_constant(ir1)) {
			/*
			 * Pick the ? or the : side.
			 * FIXME: Fix leaking ir1.
			 */
			ulit64 = ir_eval_for_addr(ir1);
			if (ulit64) {
				ir_new = ir_simplify(ir->ir_expr2, ctxt);
			} else {
				ir_new = ir_simplify(ir->ir_expr3, ctxt);
			}
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir_simplify(ir->ir_expr1, ctxt);
			ir_new->ir_expr2 = ir_simplify(ir->ir_expr2, ctxt);
			ir_new->ir_expr3 = ir_simplify(ir->ir_expr3, ctxt);
		}
		break;

	case IR_CVT_PROPNODENAME:
		/*
		 * IR_CVT_PROPNODENAME(IR_PROPNODENAME) == IR_PROPNODENAME,
		 * so drop the CVT.
		 */
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir1 && ir1->ir_type == IR_PROPNODENAME) {
			ir_new = ir1;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
		}
		break;

	case IR_CVT_STRING:
		/*
		 * IR_CVT_STRING(IR_LIT_STR) == IR_LIT_STR,
		 * so drop the CVT.
		 */
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir1 && ir1->ir_type == IR_LIT_STR) {
			ir_new = ir1;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
		}
		break;

	case IR_OR:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir_is_constant(ir1)) {
			ulit64 = ir_eval_for_addr(ir1);
			if (ulit64) {
				ir_type = (ctxt == IR_EVAL_CTXT_CELL)
					? IR_LIT_CELL : IR_LIT_ADDR;
				ir_new = ir_alloc(ir_type, ir->ir_srcpos);
				ir_new->ir_literal = 1;
				break;
			}
		}

		ir2 = ir_simplify(ir->ir_expr2, ctxt);

		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new->ir_literal = lit1 || lit2;

		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir_simplify(ir1, ctxt);
			ir_new->ir_expr2 = ir_simplify(ir2, ctxt);
		}
		break;

	case IR_AND:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir_is_constant(ir1)) {
			ulit64 = ir_eval_for_addr(ir1);
			if (ulit64 == 0) {
				ir_type = (ctxt == IR_EVAL_CTXT_CELL)
					? IR_LIT_CELL : IR_LIT_ADDR;
				ir_new = ir_alloc(ir_type, ir->ir_srcpos);
				ir_new->ir_literal = 0;
				break;
			}
		}

		ir2 = ir_simplify(ir->ir_expr2, ctxt);

		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new->ir_literal = lit1 && lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_BIT_OR:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 | lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_BIT_XOR:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 ^ lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_BIT_AND:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 & lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_EQ:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 == lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_LT:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 < lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_LE:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 <= lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_GT:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 > lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_GE:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 >= lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_NE:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 != lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_LSHIFT:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 << lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_RSHIFT:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 >> lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_ADD:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 + lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_MINUS:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 - lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_MULT:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 * lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_DIV:
		/* FIXME: check for division by const 0 */
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 / lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_MOD:
		/* FIXME: check for division by const 0 */
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		ir2 = ir_simplify(ir->ir_expr2, ctxt);
		if (ir_is_constant(ir1) && ir_is_constant(ir2)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			lit2 = ir_eval_for_addr(ir2);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = lit1 % lit2;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
			ir_new->ir_expr2 = ir2;
		}
		break;

	case IR_BIT_COMPL:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir_is_constant(ir1)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = ~lit1;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
		}
		break;

	case IR_NOT:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir_is_constant(ir1)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = !lit1;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
		}
		break;

	case IR_UMINUS:
		ir1 = ir_simplify(ir->ir_expr1, ctxt);
		if (ir_is_constant(ir1)) {
			ir_type = (ctxt == IR_EVAL_CTXT_CELL)
				? IR_LIT_CELL : IR_LIT_ADDR;
			lit1 = ir_eval_for_addr(ir1);
			ir_new = ir_alloc(ir_type, ir->ir_srcpos);
			ir_new->ir_literal = -lit1;
		} else {
			ir_new = ir_copy(ir);
			ir_new->ir_expr1 = ir1;
		}
		break;

	case IR_ROOT:
		ir_new = ir_copy(ir);
		ir_new->ir_declarations =
			ir_simplify(ir->ir_declarations, ctxt);
		ir_new->ir_statements =
			ir_simplify(ir->ir_statements, ctxt);
		break;

	case IR_NODE:
		ir_new = ir_copy(ir);
		ir_new->ir_label = ir_simplify(ir->ir_label, ctxt);
		ir_new->ir_name = ir_simplify(ir->ir_name, ctxt);
		ir_new->ir_statements =
			ir_simplify(ir->ir_statements, ctxt);
		break;

	case IR_CONST_DEF:
		ir_new = ir_copy(ir);
		ir_new->ir_expr1 = ir_simplify(ir->ir_expr1, ctxt);
		ir_new->ir_expr2 = ir_simplify(ir->ir_expr2, ctxt);
		break;

	case IR_FUNC_DEF:
		ir_new = ir_copy(ir);
		ir_new->ir_name = ir_simplify(ir->ir_name, ctxt);
		ir_new->ir_declarations =
			ir_simplify(ir->ir_declarations, ctxt);
		ir_new->ir_statements =
			ir_simplify(ir->ir_statements, ctxt);
		break;

	case IR_PARAMDECL:
	default:
		ir_new = NULL;
		ir_error(ir,
			 "Can't simplify unknown ir_type %s\n",
			 ir_type_string(ir->ir_type));
	}

	return ir_new;
}
