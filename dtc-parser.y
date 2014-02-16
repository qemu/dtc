/*
 * (C) Copyright David Gibson <dwg@au1.ibm.com>, IBM Corporation.  2005.
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
%{
#include <stdio.h>

#include "dtc.h"
#include "srcpos.h"

extern int yylex(void);
extern void yyerror(char const *s);
#define ERROR(loc, ...) \
	do { \
		srcpos_error((loc), "Error", __VA_ARGS__); \
		treesource_error = true; \
	} while (0)

extern struct boot_info *the_boot_info;
extern bool treesource_error;

static uint64_t expr_int(struct expression *expr);
static struct data expr_bytestring(struct expression *expr);

#define UNOP(loc, op, a)	(expression_##op(&loc, (a)))
#define BINOP(loc, op, a, b)	(expression_##op(&loc, (a), (b)))
%}

%union {
	char *propnodename;
	char *labelref;
	uint8_t byte;
	struct data data;

	struct {
		int bits;
		struct expression *expr;
	} array;

	struct property *prop;
	struct property *proplist;
	struct node *node;
	struct node *nodelist;
	struct reserve_info *re;
	uint64_t integer;
	struct expression *expr;
}

%token DT_V1
%token DT_MEMRESERVE
%token DT_LSHIFT DT_RSHIFT DT_LE DT_GE DT_EQ DT_NE DT_AND DT_OR
%token DT_BITS
%token DT_DEL_PROP
%token DT_DEL_NODE
%token <propnodename> DT_PROPNODENAME
%token <integer> DT_LITERAL
%token <integer> DT_CHAR_LITERAL
%token <byte> DT_BYTE
%token <data> DT_STRING
%token <labelref> DT_LABEL
%token <labelref> DT_REF
%token DT_INCBIN

%type <re> memreserve
%type <re> memreserves
%type <array> array
%type <array> arrayprefix
%type <data> bytestring_literal
%type <prop> propdef
%type <proplist> proplist

%type <node> devicetree
%type <node> nodedef
%type <node> subnode
%type <nodelist> subnodes

%type <expr> expr_incbin
%type <expr> expr_prim
%type <expr> expr_prelabel
%type <expr> expr_unary
%type <expr> expr_mul
%type <expr> expr_add
%type <expr> expr_shift
%type <expr> expr_rela
%type <expr> expr_eq
%type <expr> expr_bitand
%type <expr> expr_bitxor
%type <expr> expr_bitor
%type <expr> expr_and
%type <expr> expr_or
%type <expr> expr_conditional
%type <expr> expr_postlabel
%type <expr> expr_join
%type <expr> expr

%%

sourcefile:
	  DT_V1 ';' memreserves devicetree
		{
			the_boot_info = build_boot_info($3, $4,
							guess_boot_cpuid($4));
		}
	;

memreserves:
	  /* empty */
		{
			$$ = NULL;
		}
	| memreserve memreserves
		{
			$$ = chain_reserve_entry($1, $2);
		}
	;

memreserve:
	  DT_MEMRESERVE expr_prim expr_prim ';'
		{
			$$ = build_reserve_entry(expr_int($2), expr_int($3));
		}
	| DT_LABEL memreserve
		{
			add_label(&$2->labels, $1);
			$$ = $2;
		}
	;

devicetree:
	  '/' nodedef
		{
			$$ = name_node($2, "");
		}
	| devicetree '/' nodedef
		{
			$$ = merge_nodes($1, $3);
		}
	| devicetree DT_REF nodedef
		{
			struct node *target = get_node_by_ref($1, $2);

			if (target)
				merge_nodes(target, $3);
			else
				ERROR(&@2, "Label or path %s not found", $2);
			$$ = $1;
		}
	| devicetree DT_DEL_NODE DT_REF ';'
		{
			struct node *target = get_node_by_ref($1, $3);

			if (target)
				delete_node(target);
			else
				ERROR(&@3, "Label or path %s not found", $3);


			$$ = $1;
		}
	;

nodedef:
	  '{' proplist subnodes '}' ';'
		{
			$$ = build_node($2, $3);
		}
	;

proplist:
	  /* empty */
		{
			$$ = NULL;
		}
	| proplist propdef
		{
			$$ = chain_property($2, $1);
		}
	;

propdef:
	  DT_PROPNODENAME '=' expr ';'
		{
			struct data d = expr_bytestring($3);

			$$ = build_property($1, d);
		}
	| DT_PROPNODENAME ';'
		{
			$$ = build_property($1, empty_data);
		}
	| DT_DEL_PROP DT_PROPNODENAME ';'
		{
			$$ = build_property_delete($2);
		}
	| DT_LABEL propdef
		{
			add_label(&$2->labels, $1);
			$$ = $2;
		}
	;

array:
	  arrayprefix '>'	{ $$ = $1; }
	;

arrayprefix:
	DT_BITS DT_LITERAL '<'
		{
			unsigned long long bits;

			bits = $2;

			if ((bits !=  8) && (bits != 16) &&
			    (bits != 32) && (bits != 64)) {
				ERROR(&@2, "Array elements must be"
				      " 8, 16, 32 or 64-bits");
				bits = 32;
			}

			$$.bits = bits;
			$$.expr = expression_bytestring_constant(&@$, empty_data);
		}
	| '<'
		{
			$$.bits = 32;
			$$.expr = expression_bytestring_constant(&@$, empty_data);
		}
	| arrayprefix expr_prim
		{
			struct expression *cell = expression_arraycell(&@2,
								       $1.bits,
								       $2);
			$$.bits = $1.bits;
			$$.expr = expression_join(&@$, $1.expr, cell);
		}
	| arrayprefix DT_REF
		{
			uint64_t val = ~0ULL >> (64 - $1.bits);
			struct data d = empty_data;
			struct expression *cell;

			if ($1.bits == 32)
				d = data_add_marker(d, REF_PHANDLE, $2);
			else
				ERROR(&@2, "References are only allowed in "
					    "arrays with 32-bit elements.");

			d = data_append_integer(d, val, $1.bits);
			cell = expression_bytestring_constant(&@2, d);

			$$.bits = $1.bits;
			$$.expr = expression_join(&@$, $1.expr, cell);
		}
	| arrayprefix DT_LABEL
		{
			struct data d = data_add_marker(empty_data, LABEL, $2);
			struct expression *label;

			label = expression_bytestring_constant(&@2, d);
			$$.bits = $1.bits;
			$$.expr = expression_join(&@$, $1.expr, label);
		}
	;

expr_incbin:
	  DT_INCBIN '(' expr_conditional ',' expr_conditional ',' expr_conditional ')'
		{
			$$ = expression_incbin(&@$, $3, $5, $7);
		}
	| DT_INCBIN '(' expr_conditional ')'
		{
			$$ = expression_incbin(&@$, $3,
					       expression_integer_constant(NULL, 0),
					       expression_integer_constant(NULL, -1));
		}
	;

expr_prim:
	  DT_LITERAL 		{ $$ = expression_integer_constant(&yylloc, $1); }
	| DT_CHAR_LITERAL	{ $$ = expression_integer_constant(&yylloc, $1); }
	| DT_STRING
		{
			$$ = expression_string_constant(&yylloc, $1);
		}
	| '[' bytestring_literal ']'
		{
			$$ = expression_bytestring_constant(&@2, $2);
		}
	| expr_incbin
	| array			{ $$ = $1.expr; }
	| '(' expr ')'
		{
			$$ = $2;
		}
	;

expr:
	  expr_join
	;

expr_join:
	  expr_postlabel
	| expr_join ',' expr_postlabel
		{
			$$ = expression_join(&@$, $1, $3);
		}
	;

expr_postlabel:
	  expr_conditional
	| expr_conditional DT_LABEL
		{
			struct data d = data_add_marker(empty_data, LABEL, $2);
			struct expression *label;

			label = expression_bytestring_constant(&@2, d);
			$$ = expression_join(&@$, $1, label);
		}
	;


expr_conditional:
	  expr_or
	| DT_REF
		{
			struct data d = data_add_marker(empty_data, REF_PATH, $1);
			$$ = expression_bytestring_constant(&@$, d);
		}
	| expr_or '?' expr ':' expr_conditional
		{
			$$ = expression_conditional(&yylloc, $1, $3, $5);
		}
	;

expr_or:
	  expr_and
	| expr_or DT_OR expr_and { $$ = BINOP(@$, logic_or, $1, $3); }
	;

expr_and:
	  expr_bitor
	| expr_and DT_AND expr_bitor { $$ = BINOP(@$, logic_and, $1, $3); }
	;

expr_bitor:
	  expr_bitxor
	| expr_bitor '|' expr_bitxor { $$ = BINOP(@$, bit_or, $1, $3); }
	;

expr_bitxor:
	  expr_bitand
	| expr_bitxor '^' expr_bitand { $$ = BINOP(@$, bit_xor, $1, $3); }
	;

expr_bitand:
	  expr_eq
	| expr_bitand '&' expr_eq { $$ = BINOP(@$, bit_and, $1, $3); }
	;

expr_eq:
	  expr_rela
	| expr_eq DT_EQ expr_rela { $$ = BINOP(@$, eq, $1, $3); }
	| expr_eq DT_NE expr_rela { $$ = BINOP(@$, ne, $1, $3); }
	;

expr_rela:
	  expr_shift
	| expr_rela '<' expr_shift { $$ = BINOP(@$, lt, $1, $3); }
	| expr_rela '>' expr_shift { $$ = BINOP(@$, gt, $1, $3); }
	| expr_rela DT_LE expr_shift { $$ = BINOP(@$, le, $1, $3); }
	| expr_rela DT_GE expr_shift { $$ = BINOP(@$, ge, $1, $3); }
	;

expr_shift:
	  expr_shift DT_LSHIFT expr_add { $$ = BINOP(@$, lshift, $1, $3); }
	| expr_shift DT_RSHIFT expr_add { $$ = BINOP(@$, rshift, $1, $3); }
	| expr_add
	;

expr_add:
	  expr_add '+' expr_mul { $$ = BINOP(@$, add, $1, $3); }
	| expr_add '-' expr_mul { $$ = BINOP(@$, sub, $1, $3); }
	| expr_mul
	;

expr_mul:
	  expr_mul '*' expr_unary { $$ = BINOP(@$, mul, $1, $3); }
	| expr_mul '/' expr_unary { $$ = BINOP(@$, div, $1, $3); }
	| expr_mul '%' expr_unary { $$ = BINOP(@$, mod, $1, $3); }
	| expr_unary
	;

expr_unary:
	  expr_prelabel
	| '-' expr_unary { $$ = UNOP(@$, negate, $2); }
	| '~' expr_unary { $$ = UNOP(@$, bit_not, $2); }
	| '!' expr_unary { $$ = UNOP(@$, logic_not, $2); }
	;

expr_prelabel:
	  expr_prim
	| DT_LABEL expr_prelabel
		{
			struct data d = data_add_marker(empty_data, LABEL, $1);
			struct expression *label;

			label = expression_bytestring_constant(&@1, d);
			$$ = expression_join(&@$, label, $2);
		}
	;

bytestring_literal:
	  /* empty */
		{
			$$ = empty_data;
		}
	| bytestring_literal DT_BYTE
		{
			$$ = data_append_byte($1, $2);
		}
	| bytestring_literal DT_LABEL
		{
			$$ = data_add_marker($1, LABEL, $2);
		}
	;

subnodes:
	  /* empty */
		{
			$$ = NULL;
		}
	| subnode subnodes
		{
			$$ = chain_node($1, $2);
		}
	| subnode propdef
		{
			ERROR(&@2, "Properties must precede subnodes");
			YYERROR;
		}
	;

subnode:
	  DT_PROPNODENAME nodedef
		{
			$$ = name_node($2, $1);
		}
	| DT_DEL_NODE DT_PROPNODENAME ';'
		{
			$$ = name_node(build_node_delete(), $2);
		}
	| DT_LABEL subnode
		{
			add_label(&$2->labels, $1);
			$$ = $2;
		}
	;

%%

void yyerror(char const *s)
{
	ERROR(&yylloc, "%s", s);
}

static uint64_t expr_int(struct expression *expr)
{
	struct expression_value v = expression_evaluate(expr, EXPR_INTEGER);

	if (v.type == EXPR_VOID) {
		treesource_error = true;
		return -1;
	}
	assert(v.type == EXPR_INTEGER);
	return v.value.integer;
}

static struct data expr_bytestring(struct expression *expr)
{
	struct expression_value v = expression_evaluate(expr, EXPR_BYTESTRING);

	if (v.type == EXPR_VOID) {
		treesource_error = true;
		return empty_data;
	}
	assert(v.type == EXPR_BYTESTRING);
	return v.value.d;
}
