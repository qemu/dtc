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

%locations

%{
#include <stdio.h>

#include "dtc.h"
#include "srcpos.h"
#include "ir.h"

extern int yylex(void);
extern void yyerror(char const *s);

extern int treesource_error;

#define YYERROR_VERBOSE

%}

%union {
	struct ir *ir;
	char *propnodename;
	char *id;
	char *litstr;
	char *literal;
	char *labelref;
	uint8_t byte;
}

%token DT_V1
%token DT_MEMRESERVE
%token DT_INCBIN
%token DT_DEFINE
%token DT_CONST
%token DT_FOR
%token DT_IN
%token DT_RANGE
%token DT_VOID
%token DT_IF
%token DT_ELSE
%token DT_RETURN

%token <propnodename> DT_PROPNODENAME
%token <id> DT_ID
%token <literal> DT_LITERAL
%token <byte> DT_BYTE
%token <litstr> DT_STRING
%token <labelref> DT_LABEL
%token <labelref> DT_REF

%token DT_OR
%token DT_AND
%token DT_EQ DT_NE
%token DT_LE DT_GE
%token DT_LSHIFT DT_RSHIFT
%token DT_ASSIGN

%type <ir> sourcefile
%type <ir> memreserve
%type <ir> devicetree
%type <ir> declaration_list
%type <ir> declaration
%type <ir> funcdef
%type <ir> constdef
%type <ir> errordef
%type <ir> subnode
%type <ir> paramdecl_list
%type <ir> paramdecls
%type <ir> paramdecl

%type <ir> statement_block
%type <ir> statement_list
%type <ir> statement
%type <ir> for_statement
%type <ir> if_statement
%type <ir> return_statement
%type <ir> assign_statement
%type <ir> trivial_statement
%type <ir> error_statement

%type <ir> propdef
%type <ir> celllist
%type <ir> cellval
%type <ir> literal
%type <ir> string
%type <ir> addr
%type <ir> byte
%type <ir> propnodename
%type <ir> label
%type <ir> opt_label
%type <ir> node_label
%type <ir> propdata
%type <ir> propdataitem
%type <ir> propdataprefix
%type <ir> bytestring

%type <ir> param_list

%type <ir> expr
%type <ir> expr_primary
%type <ir> expr_postfix
%type <ir> expr_unary
%type <ir> expr_mul
%type <ir> expr_add
%type <ir> expr_shift
%type <ir> expr_rela
%type <ir> expr_eq
%type <ir> expr_bitand
%type <ir> expr_bitxor
%type <ir> expr_bitor
%type <ir> expr_and
%type <ir> expr_or
%type <ir> expr_conditional

%type <ir> range
%type <ir> identifier

%%

sourcefile:
	  DT_V1 ';' declaration_list devicetree
		{
			the_ir_tree = ir_alloc(IR_ROOT, &@4);
			the_ir_tree->ir_declarations = $3;
			the_ir_tree->ir_statements = $4;
		}
	;

declaration_list:
	  /* empty */
		{
			$$ = NULL;
		}
	| declaration_list declaration
		{
			$$ = ir_list_append($1, $2);
		}
	;

declaration:
	  memreserve
	| constdef
	| funcdef
	| errordef
	;

memreserve:
	  opt_label DT_MEMRESERVE addr addr ';'
		{
			$$ = ir_alloc_binop(IR_MEM_RESERVE, $3, $4, &@2);
			$$->ir_label = $1;
		}
	;


constdef:
	  DT_CONST identifier '=' expr ';'
		{
			$$ = ir_alloc_binop(IR_CONST_DEF, $2, $4, &@1);
		}
	;

funcdef:
	  DT_DEFINE identifier paramdecls statement_block
		{
			$$ = ir_alloc(IR_FUNC_DEF, &@1);
			$$->ir_name = $2;
			$$->ir_declarations = $3;
			$$->ir_statements = $4;
		}
	;

errordef:
	  error
		{
			$$ = NULL
		}
	;

paramdecls:
	  '(' paramdecl_list ')'
		{
			$$ = $2;
		}
	;

paramdecl_list:
	  /* empty */
		{
			$$ = NULL;
		}
	| paramdecl
		{
			$$ = ir_list_append(NULL, $1);
		}
	| paramdecl_list ',' paramdecl
		{
			$$ = ir_list_append($1, $3);
		}
	;

paramdecl:
	  identifier
	;


devicetree:
	  '/' statement_block ';'
		{
			$$ = ir_alloc(IR_NODE, &@2);
			$$->ir_statements = $2;
			$$->ir_name = ir_alloc(IR_PROPNODENAME, &@1);
			$$->ir_name->ir_lit_str = "";
			$$->ir_label = NULL;
		}
	;


statement_block:
	  '{' statement_list '}'
		{
			$$ = $2;
		}
	;

statement_list:
	  /* empty */
		{
			$$ = NULL;
		}
	| statement_list statement
		{
			$$ = ir_list_append($1, $2);
		}
	;

statement:
	  for_statement
	| if_statement
	| return_statement
	| assign_statement
	| propdef
	| subnode
	| statement_block
	| trivial_statement
	| error_statement
	;


subnode:
	  node_label expr statement_block ';'
		{
			$$ = ir_alloc(IR_NODE, &@3);
			$$->ir_statements = $3;
			$$->ir_label = $1;
			$$->ir_name = $2;
		}
	| label expr statement_block ';'
		{
			$$ = ir_alloc(IR_NODE, &@3);
			$$->ir_statements = $3;
			$$->ir_label = $1;
			$$->ir_name = $2;
		}
	| expr statement_block ';'
		{
			$$ = ir_alloc(IR_NODE, &@2);
			$$->ir_statements = $2;
			$$->ir_label = NULL;
			$$->ir_name = $1;
		}
	;

for_statement:
	  DT_FOR identifier DT_IN range statement_block
		{
			$$ = ir_alloc_binop(IR_FOR, $2, $4, &@1);
			$$->ir_statements = $5;
		}
	;

range:
	  expr DT_RANGE expr
		{
			$$ = ir_alloc_binop(IR_RANGE, $1, $3, &@2);
		}
	;

if_statement:
	  DT_IF '(' expr ')' statement_block
		{
			$$ = ir_alloc_unop(IR_IF, $3, &@1);
			$$->ir_statements = $5;
		}
	| DT_IF '(' expr ')' statement_block DT_ELSE statement_block
		{
			$$ = ir_alloc_unop(IR_IF, $3, &@1);
			$$->ir_statements = $5;
			$$->ir_statements2 = $7;
		}
	;

return_statement:
	  DT_RETURN expr ';'
		{
			$$ = ir_alloc_unop(IR_RETURN, $2, &@1);
		}
	;

assign_statement:
	  identifier DT_ASSIGN expr ';'
		{
			$$ = ir_alloc_binop(IR_ASSIGN, $1, $3, &@2);
		}
	;

trivial_statement:
	  ';'
		{
			$$ = NULL;
		}
	;

error_statement:
	  error
		{
			$$ = NULL;
		}
	;

propdef:
	  expr ';'
		{
			$$ = ir_alloc_unop(IR_PROP_DEF,
					   ir_alloc_unop(IR_CVT_PROPNODENAME,
							 $1,
							 &@1),
					   &@1);
			$$->ir_label = NULL;
		}
	| expr '=' propdata ';'
		{
			$$ = ir_alloc_binop(IR_PROP_DEF,
					    ir_alloc_unop(IR_CVT_PROPNODENAME,
							  $1,
							  &@1),
					    $3,
					    &@2);
			$$->ir_label = NULL;
		}
	| label expr ';'
		{
			$$ = ir_alloc_unop(IR_PROP_DEF,
					   ir_alloc_unop(IR_CVT_PROPNODENAME,
							 $2,
							 &@2),
					   &@2);
			$$->ir_label = $1;
		}
	| label expr '=' propdata ';'
		{
			$$ = ir_alloc_binop(IR_PROP_DEF,
					    ir_alloc_unop(IR_CVT_PROPNODENAME,
							  $2,
							  &@2),
					    $4,
					    &@3);
			$$->ir_label = $1;
		}
	;

propdata:
	  propdataprefix propdataitem
		{
			$$ = ir_list_append($1, $2);
		}
	| propdata label
		{
			$$ = ir_list_append($1, $2);
		}
	;

propdataitem:
	  string
		{
			$$ = $1;
		}
	| '<' celllist '>'
		{
			$$ = $2;
		}
	| '[' bytestring ']'
		{
			$$ = $2;
		}
	| DT_REF
		{
			$$ = ir_alloc(IR_REF_PATH, &@1);
			$$->ir_label_name = $1;
		}
	| DT_INCBIN '(' expr ')'
		{
			$$ = ir_alloc_unop(IR_INCBIN, $3, &@1);
		}
	| DT_INCBIN '(' expr ',' expr ',' expr ')'
		{
			$$ = ir_alloc_triop(IR_INCBIN, $3, $5, $7, &@1);
		}
	;

propdataprefix:
	  /* empty */
		{
			$$ = NULL;
		}
	| propdata ','
		{
			$$ = $1;
		}
	| propdataprefix label
		{
			$$ = ir_list_append($1, $2);
		}
	;

celllist:
	  /* empty */
		{
			$$ = NULL;
		}
	| celllist cellval
		{
			$$ = ir_list_append($1, $2);
		}
	| celllist '&' '(' expr ')'
		{
			$$ = ir_alloc(IR_REF_PHANDLE, &@2);
			$$->ir_label = $4;
			$$ = ir_list_append($1, $$);

		}
	| celllist DT_REF
		{
			$$ = ir_alloc(IR_REF_PHANDLE, &@2);
			$$->ir_label_name = $2;
			$$ = ir_list_append($1, $$);
		}
	| celllist label
		{
			$$ = ir_list_append($1, $2);
		}
	;

cellval:
	  expr_primary
		{
			$$ = ir_alloc_unop(IR_CELL, $1, &@1);
		}
	;


expr:
	  expr_conditional
	;

expr_conditional:
	  expr_or
	| expr_or '?' expr_or ':' expr_conditional
		{
			$$ = ir_alloc_triop(IR_SELECT, $1, $3, $5, &@2);
		}
	;

expr_or:
	  expr_and
	| expr_or DT_OR expr_and
		{
			$$ = ir_alloc_binop(IR_OR, $1, $3, &@2);
		};

expr_and:
	  expr_bitor
	| expr_and DT_AND expr_bitor
		{
			$$ = ir_alloc_binop(IR_AND, $1, $3, &@2);
		};
	;

expr_bitor:
	  expr_bitxor
	| expr_bitor '|' expr_bitxor
		{
			$$ = ir_alloc_binop(IR_BIT_OR, $1, $3, &@2);
		};
	;

expr_bitxor:
	  expr_bitand
	| expr_bitxor '^' expr_bitand
		{
			$$ = ir_alloc_binop(IR_BIT_XOR, $1, $3, &@2);
		};
	;

expr_bitand:
	  expr_eq
	| expr_bitand '&' expr_eq
		{
			$$ = ir_alloc_binop(IR_BIT_AND, $1, $3, &@2);
		};
	;

expr_eq:
	  expr_rela
	| expr_eq DT_EQ expr_rela
		{
			$$ = ir_alloc_binop(IR_EQ, $1, $3, &@2);
		}
	| expr_eq DT_NE expr_rela
		{
			$$ = ir_alloc_binop(IR_NE, $1, $3, &@2);
		}
	;

expr_rela:
	  expr_shift
	| expr_rela '<' expr_shift
		{
			$$ = ir_alloc_binop(IR_LT, $1, $3, &@2);
		}
	| expr_rela '>' expr_shift
		{
			$$ = ir_alloc_binop(IR_GT, $1, $3, &@2);
		}
	| expr_rela DT_LE expr_shift
		{
			$$ = ir_alloc_binop(IR_LE, $1, $3, &@2);
		}
	| expr_rela DT_GE expr_shift
		{
			$$ = ir_alloc_binop(IR_GE, $1, $3, &@2);
		}
	;

expr_shift:
	  expr_add
	| expr_shift DT_LSHIFT expr_add
		{
			$$ = ir_alloc_binop(IR_LSHIFT, $1, $3, &@2);
		}
	| expr_shift DT_RSHIFT expr_add
		{
			$$ = ir_alloc_binop(IR_RSHIFT, $1, $3, &@2);
		}
	;

expr_add:
	  expr_mul
	| expr_add '+' expr_mul
		{
			$$ = ir_alloc_binop(IR_ADD, $1, $3, &@2);
		}
	| expr_add '-' expr_mul
		{
			$$ = ir_alloc_binop(IR_MINUS, $1, $3, &@2);
		}
	;

expr_mul:
	  expr_unary
	| expr_mul '*' expr_unary
		{
			$$ = ir_alloc_binop(IR_MULT, $1, $3, &@2);
		}
	| expr_mul '/' expr_unary
		{
			$$ = ir_alloc_binop(IR_DIV, $1, $3, &@2);
		}
	| expr_mul '%' expr_unary
		{
			$$ = ir_alloc_binop(IR_MOD, $1, $3, &@2);
		}
	;

expr_unary:
	  expr_postfix
	| '-' expr_unary
		{
			$$ = ir_alloc_unop(IR_UMINUS, $2, &@1);
		}
	| '~' expr_unary
		{
			$$ = ir_alloc_unop(IR_BIT_COMPL, $2, &@1);
		}
	| '!' expr_unary
		{
			$$ = ir_alloc_unop(IR_NOT, $2, &@1);
		}
	;

expr_postfix:
	  expr_primary
	| expr_postfix '(' ')'
		{
			$$ = ir_alloc_binop(IR_FUNC_CALL, $1, NULL, &@2);
		}
	| expr_postfix '(' param_list ')'
		{
			$$ = ir_alloc_binop(IR_FUNC_CALL, $1, $3, &@2);
		}
	;

param_list:
	  expr
		{
			$$ = ir_list_append(NULL, $1);
		}
	| param_list ',' expr
		{
			$$ = ir_list_append($1, $3);
		}
	;



expr_primary:
	  literal
	| string
	| propnodename
	| identifier
	| '(' expr ')'
		{
			$$ = $2;
		}
	;

addr:
	  expr_primary
	;


bytestring:
	  /* empty */
		{
			$$ = NULL;
		}
	| bytestring byte
		{
			$$ = ir_list_append($1, $2);
		}
	| bytestring label
		{
			$$ = ir_list_append($1, $2);
		}
	;

propnodename:
	  DT_PROPNODENAME
		{
			$$ = ir_alloc(IR_PROPNODENAME, &@1);
			$$->ir_lit_str = $1;
		}
	;

identifier:
	  DT_ID
		{
			$$ = ir_alloc(IR_ID, &@1);
			$$->ir_lit_str = $1;
		}
	;

literal:
	  DT_LITERAL
		{
			$$ = ir_alloc(IR_LITERAL, &@1);
			$$->ir_lit_str = $1;
		}
	;

byte:
	  DT_BYTE
		{
			$$ = ir_alloc(IR_LIT_BYTE, &@1);
			$$->ir_literal = $1;
		}
	;

string:
	  DT_STRING
		{
			$$ = ir_alloc(IR_LIT_STR, &@1);
			$$->ir_lit_str = $1;
		}
	;

opt_label:
	  /* empty */
		{
			$$ = NULL;
		}
	| label
		{
			$$ = $1;
		}
	;

label:
	  DT_LABEL
		{
			$$ = ir_alloc(IR_LABEL, &@1);
			$$->ir_label_name = $1;
		}
	;

node_label:
	  expr ':'
	;


%%

void yyerror(char const *s)
{
	srcpos_error(&yylloc, "%s", s);
	treesource_error = 1;
}
