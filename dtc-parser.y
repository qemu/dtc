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

#define UNOP(op, a)	(expression_##op((a)))
#define BINOP(op, a, b)	(expression_##op((a), (b)))
%}

%union {
	char *propnodename;
	char *labelref;
	uint8_t byte;
	struct data data;

	struct {
		struct data	data;
		int		bits;
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

%type <data> propdata
%type <data> propdataprefix
%type <re> memreserve
%type <re> memreserves
%type <array> arrayprefix
%type <data> bytestring
%type <prop> propdef
%type <proplist> proplist

%type <node> devicetree
%type <node> nodedef
%type <node> subnode
%type <nodelist> subnodes

%type <expr> expr_prim
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
			$$ = build_reserve_entry(expr_int($2),
						 expr_int($3));
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
	  DT_PROPNODENAME '=' propdata ';'
		{
			$$ = build_property($1, $3);
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

propdata:
	  propdataprefix DT_STRING
		{
			$$ = data_merge($1, $2);
		}
	| propdataprefix arrayprefix '>'
		{
			$$ = data_merge($1, $2.data);
		}
	| propdataprefix '[' bytestring ']'
		{
			$$ = data_merge($1, $3);
		}
	| propdataprefix DT_REF
		{
			$$ = data_add_marker($1, REF_PATH, $2);
		}
	| propdataprefix DT_INCBIN '(' DT_STRING ',' expr_prim ',' expr_prim ')'
		{
			FILE *f = srcfile_relative_open($4.val, NULL);
			off_t offset = expr_int($6);
			struct data d;

			if (offset != 0)
				if (fseek(f, offset, SEEK_SET) != 0)
					die("Couldn't seek to offset %llu in \"%s\": %s",
					    (unsigned long long)offset, $4.val,
					    strerror(errno));

			d = data_copy_file(f, expr_int($8));

			$$ = data_merge($1, d);
			fclose(f);
		}
	| propdataprefix DT_INCBIN '(' DT_STRING ')'
		{
			FILE *f = srcfile_relative_open($4.val, NULL);
			struct data d = empty_data;

			d = data_copy_file(f, -1);

			$$ = data_merge($1, d);
			fclose(f);
		}
	| propdata DT_LABEL
		{
			$$ = data_add_marker($1, LABEL, $2);
		}
	;

propdataprefix:
	  /* empty */
		{
			$$ = empty_data;
		}
	| propdata ','
		{
			$$ = $1;
		}
	| propdataprefix DT_LABEL
		{
			$$ = data_add_marker($1, LABEL, $2);
		}
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

			$$.data = empty_data;
			$$.bits = bits;
		}
	| '<'
		{
			$$.data = empty_data;
			$$.bits = 32;
		}
	| arrayprefix expr_prim
		{
			uint64_t val = expr_int($2);

			if ($1.bits < 64) {
				uint64_t mask = (1ULL << $1.bits) - 1;
				/*
				 * Bits above mask must either be all zero
				 * (positive within range of mask) or all one
				 * (negative and sign-extended). The second
				 * condition is true if when we set all bits
				 * within the mask to one (i.e. | in the
				 * mask), all bits are one.
				 */
				if ((val > mask) && ((val | mask) != -1ULL))
					ERROR(&@2, "Value out of range for"
					      " %d-bit array element", $1.bits);
			}

			$$.data = data_append_integer($1.data, val, $1.bits);
		}
	| arrayprefix DT_REF
		{
			uint64_t val = ~0ULL >> (64 - $1.bits);

			if ($1.bits == 32)
				$1.data = data_add_marker($1.data,
							  REF_PHANDLE,
							  $2);
			else
				ERROR(&@2, "References are only allowed in "
					    "arrays with 32-bit elements.");

			$$.data = data_append_integer($1.data, val, $1.bits);
		}
	| arrayprefix DT_LABEL
		{
			$$.data = data_add_marker($1.data, LABEL, $2);
		}
	;

expr_prim:
	  DT_LITERAL 		{ $$ = expression_constant($1); }
	| DT_CHAR_LITERAL	{ $$ = expression_constant($1); }
	| '(' expr ')'
		{
			$$ = $2;
		}
	;

expr:
	expr_conditional
	;

expr_conditional:
	  expr_or
	| expr_or '?' expr ':' expr_conditional
		{
			$$ = expression_conditional($1, $3, $5);
		}
	;

expr_or:
	  expr_and
	| expr_or DT_OR expr_and { $$ = BINOP(logic_or, $1, $3); }
	;

expr_and:
	  expr_bitor
	| expr_and DT_AND expr_bitor { $$ = BINOP(logic_and, $1, $3); }
	;

expr_bitor:
	  expr_bitxor
	| expr_bitor '|' expr_bitxor { $$ = BINOP(bit_or, $1, $3); }
	;

expr_bitxor:
	  expr_bitand
	| expr_bitxor '^' expr_bitand { $$ = BINOP(bit_xor, $1, $3); }
	;

expr_bitand:
	  expr_eq
	| expr_bitand '&' expr_eq { $$ = BINOP(bit_and, $1, $3); }
	;

expr_eq:
	  expr_rela
	| expr_eq DT_EQ expr_rela { $$ = BINOP(eq, $1, $3); }
	| expr_eq DT_NE expr_rela { $$ = BINOP(ne, $1, $3); }
	;

expr_rela:
	  expr_shift
	| expr_rela '<' expr_shift { $$ = BINOP(lt, $1, $3); }
	| expr_rela '>' expr_shift { $$ = BINOP(gt, $1, $3); }
	| expr_rela DT_LE expr_shift { $$ = BINOP(le, $1, $3); }
	| expr_rela DT_GE expr_shift { $$ = BINOP(ge, $1, $3); }
	;

expr_shift:
	  expr_shift DT_LSHIFT expr_add { $$ = BINOP(lshift, $1, $3); }
	| expr_shift DT_RSHIFT expr_add { $$ = BINOP(rshift, $1, $3); }
	| expr_add
	;

expr_add:
	  expr_add '+' expr_mul { $$ = BINOP(add, $1, $3); }
	| expr_add '-' expr_mul { $$ = BINOP(sub, $1, $3); }
	| expr_mul
	;

expr_mul:
	  expr_mul '*' expr_unary { $$ = BINOP(mul, $1, $3); }
	| expr_mul '/' expr_unary { $$ = BINOP(div, $1, $3); }
	| expr_mul '%' expr_unary { $$ = BINOP(mod, $1, $3); }
	| expr_unary
	;

expr_unary:
	  expr_prim
	| '-' expr_unary { $$ = UNOP(negate, $2); }
	| '~' expr_unary { $$ = UNOP(bit_not, $2); }
	| '!' expr_unary { $$ = UNOP(logic_not, $2); }
	;

bytestring:
	  /* empty */
		{
			$$ = empty_data;
		}
	| bytestring DT_BYTE
		{
			$$ = data_append_byte($1, $2);
		}
	| bytestring DT_LABEL
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
	uint64_t val;
	val = expression_evaluate(expr);
	expression_free(expr);
	return val;
}
