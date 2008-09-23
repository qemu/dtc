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

#ifndef _IR_SCOPE_H_
#define _IR_SCOPE_H_

/*
 * IR Symbols and Symbol Tables
 *
 * Each ir_scope structure can have its own Symbol Table, represented
 * as a simple linked list of symbols.
 *
 * As the number of symbols and scopes is expected to be relatively
 * small (dozens total), and not large (hundreds or more), the
 * current implementation is a dead-simple brute force linear search
 * of a Symbol Table.
 *
 * Symbol Table operations (add, lookup) are implicitly performed
 * relative to the IR Scope Stack.
 *
 * During evaluation of the IR form, each symbol can have at most
 * one value, represented as an IR expression.  In the case of
 * variables (or constants), the IR expression should be a
 * literal of some type.  For function definitions, the expression
 * is the complete IR representation of the function definition.
 */

#define IRSYM_NONE	0
#define IRSYM_VAR	1
#define IRSYM_FUNCDEF	2
#define IRSYM_PARAM	3
#define IRSYM_CONST	4

#define IRSYM_NUM_TYPES	5

typedef unsigned int irsym_type;

struct ir_symbol {
	irsym_type irsym_type;
	char *irsym_name;
	struct ir *irsym_value;
	struct ir_symbol *irsym_next;
};



/*
 * IR Evaluation Scope
 */

#define IRS_NONE	0x00
#define IRS_ROOT	0x01
#define IRS_NODE	0x02
#define IRS_FOR_LOOP	0x04
#define IRS_FUNC_CALL	0x08

#define IRS_MAX_BIT	IRS_FUNC_CALL
#define IRS_ANY		0xFF

typedef unsigned int irs_type;

struct ir_scope {
	irs_type irs_type;
	struct ir_symbol *irs_symtab;
	struct ir *irs_expr;
	struct reserve_info *irs_reserve_list;
	struct property *irs_prop_list;
	struct node *irs_node_list;

	struct ir_scope *irs_next;
};

/*
 * Each entry on this stack provides an evaluation environment.
 */
extern struct ir_scope *irs_scope_stack;

extern void irs_push_scope(irs_type irs_type);
extern struct ir_scope *irs_pop_scope(void);

extern void irs_append_reserve(struct reserve_info *ri);
extern void irs_append_property(struct property *p);
extern void irs_append_node(struct node *n);
extern void irs_scope_append_node_list(struct node *nl);
extern void irs_scope_append_property_list(struct property *pl);
extern void irs_set_return_value(struct ir *ir_ret);

extern struct ir_symbol *irs_alloc_symbol(char *name, irsym_type irsym_type);
extern struct ir_symbol *irs_lookup_in_scope(struct ir_scope *irs, char *name);
extern struct ir_symbol *irs_lookup(char *name, irs_type irs_type);
extern struct ir_symbol *irs_lookup_local(char *name);
extern struct ir_symbol *irs_create_local(char *name, irsym_type irsym_type);
extern struct ir_symbol *irs_create_symbol(irs_type irs_type,
					   char *name,
					   irsym_type irsym_type);
extern void irs_dump_symbols(void);

#endif /* _IR_SCOPE_H_ */
