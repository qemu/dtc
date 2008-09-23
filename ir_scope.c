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


struct ir_scope *irs_scope_stack;

static const char *irs_scope_bits_str[] = {
	"ROOT",
	"NODE",
	"FOR_LOOP",
	"FUNC_CALL",
};


static const char *irsym_type_str[IRSYM_NUM_TYPES] = {
	"NONE",
	"VAR",
	"FUNCDEF",
	"PARAM",
	"CONST",
};


void
irs_push_scope(irs_type irs_type)
{
	struct ir_scope *irs_new;

	irs_new = xmalloc(sizeof(struct ir_scope));
	memset(irs_new, 0, sizeof(struct ir_scope));

	irs_new->irs_type = irs_type;
	irs_new->irs_next = irs_scope_stack;

	irs_scope_stack = irs_new;
}


struct ir_scope *
irs_pop_scope()
{
	struct ir_scope *irs_top = irs_scope_stack;

	irs_scope_stack = irs_top->irs_next;
	irs_top->irs_next = NULL;

	return irs_top;
}


void
irs_free_scope(struct ir_scope *irs)
{
	free(irs);
}


struct ir_scope *
irs_find_scope(irs_type irs_match)
{
	struct ir_scope *irs;

	for (irs = irs_scope_stack; irs != NULL; irs = irs->irs_next) {
		if (irs->irs_type & irs_match)
			return irs;
	}
	return NULL;
}


void
irs_append_reserve(struct reserve_info *ri)
{
	struct ir_scope *irs;

	irs = irs_find_scope(IRS_ROOT);
	irs->irs_reserve_list = add_reserve_entry(irs->irs_reserve_list, ri);
}


void
irs_scope_append_property_list(struct property *pl)
{
	struct ir_scope *irs;
	struct property *p;

	irs = irs_find_scope(IRS_NODE);
	for (p = pl; p != NULL; p = p->next) {
		irs->irs_prop_list = chain_property(p, irs->irs_prop_list);
	}
}

void
irs_scope_append_node_list(struct node *nl)
{
	struct ir_scope *irs;
	struct node *n;

	irs = irs_find_scope(IRS_NODE);
	for (n = nl; n != NULL; n = n->parent) {
		irs->irs_node_list = chain_node(n, irs->irs_node_list);
	}
}


void
irs_append_property(struct property *p)
{
	struct ir_scope *irs;

	irs = irs_find_scope(IRS_NODE);
	irs->irs_prop_list = chain_property(p, irs->irs_prop_list);
}


void
irs_append_node(struct node *n)
{
	struct ir_scope *irs;

	irs = irs_find_scope(IRS_NODE | IRS_ROOT);
	irs->irs_node_list = chain_node(n, irs->irs_node_list);
}


void
irs_set_return_value(struct ir *ir_ret)
{
	struct ir_scope *irs;

	/*
	 * FIXME: If a  previous irs_expr existed, it just leaked.
	 */
	irs = irs_find_scope(IRS_FUNC_CALL);
	irs->irs_expr = ir_ret;
}



struct ir_symbol *
irs_alloc_symbol(char *name, irsym_type irsym_type)
{
	struct ir_symbol *irsym;

	irsym = xmalloc(sizeof(struct ir_symbol));
	memset(irsym, 0, sizeof(struct ir_symbol));

	irsym->irsym_type = irsym_type;
	irsym->irsym_name = xstrdup(name);

	return irsym;
}


void
irs_add_symbol(struct ir_scope *irs, struct ir_symbol *irsym)
{
	irsym->irsym_next = irs->irs_symtab;
	irs->irs_symtab = irsym;
}


struct ir_symbol *
irs_lookup_in_scope(struct ir_scope *irs, char *name)
{
	struct ir_symbol *irsym;

	for (irsym = irs->irs_symtab; irsym; irsym = irsym->irsym_next)
		if (strcmp(irsym->irsym_name, name) == 0)
			return irsym;

	return NULL;
}


struct ir_symbol *
irs_lookup(char *name, irs_type irs_type)
{
	struct ir_scope *irs;
	struct ir_symbol *irsym;

	/*
	 * Look through scope stack finding matching scopes.
	 */
	for (irs = irs_scope_stack; irs != NULL; irs = irs->irs_next)
		if (irs->irs_type & irs_type) {
			irsym = irs_lookup_in_scope(irs, name);
			if (irsym != NULL)
				return irsym;
		}

	return NULL;
}


/*
 * Try to find a symbol that is local to the innermost function.
 *
 * Look through scope stack finding matching scopes.
 * Peer into FUNC_CALL, FOR_LOOP and IR_ROOT symbol tables,
 * but bail at first FUNC_CALL to make them be "local".
 */
struct ir_symbol *
irs_lookup_local(char *name)
{
	struct ir_scope *irs;
	struct ir_symbol *irsym;

	for (irs = irs_scope_stack; irs != NULL; irs = irs->irs_next) {
		if (irs->irs_type & (IRS_FUNC_CALL|IRS_FOR_LOOP|IRS_ROOT)) {
			irsym = irs_lookup_in_scope(irs, name);
			if (irsym != NULL)
				return irsym;
			if (irs->irs_type & IRS_FUNC_CALL)
				return NULL;
		}
	}

	return NULL;
}


struct ir_symbol *
irs_create_local(char *name, irsym_type irsym_type)
{
	struct ir_scope *irs;
	struct ir_symbol *irsym;

	for (irs = irs_scope_stack; irs != NULL; irs = irs->irs_next) {
		if (irs->irs_type & (IRS_FUNC_CALL|IRS_ROOT)) {
			break;
		}
	}

	irsym = irs_alloc_symbol(name, irsym_type);
	irs_add_symbol(irs_scope_stack, irsym);

	return irsym;
}


struct ir_symbol *
irs_create_symbol(irs_type irs_type, char *name, irsym_type irsym_type)
{
	struct ir_scope *irs;
	struct ir_symbol *irsym;

	/*
	 * Check for prior existence of symbol first.
	 */
	irsym = irs_lookup(name, irs_type);
	if (irsym != NULL)
		return irsym;

	/*
	 * Create the symbol.
	 */
	irsym = irs_alloc_symbol(name, irsym_type);

	/*
	 * Locate the right scope and add symbol.
	 */
	irs = irs_find_scope(irs_type);
	irs_add_symbol(irs, irsym);

	return irsym;
}


void
irs_dump_symbols(void)
{
	struct ir_scope *irs;
	struct ir_symbol *irsym;
	int i;

	/*
	 * Look through scope stack.
	 */
	for (irs = irs_scope_stack; irs != NULL; irs = irs->irs_next) {
		printf("Type: 0x%02x : ", irs->irs_type);
		for (i = 0; i < IRS_MAX_BIT; i++) {
			if (irs->irs_type & (1 << i))
				printf("%s ", irs_scope_bits_str[i]);
		}
		printf("\n");
		for (irsym = irs->irs_symtab;
		     irsym != NULL;
		     irsym = irsym->irsym_next) {
			printf("    %s : %s\n",
			       irsym_type_str[irsym->irsym_type],
			       irsym->irsym_name);
		}
	}
}
