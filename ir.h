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

#ifndef _IR_H_
#define _IR_H_

#include "srcpos.h"
#include "ir_builtin.h"


#define IR_UNDEF	0
#define IR_ROOT		1
#define IR_MEM_RESERVE	2
#define IR_ASSIGN	3
#define IR_PROP_DEF	4
#define IR_REF_PHANDLE	5
#define IR_REF_PATH	6
#define IR_CELL		7
#define IR_LITERAL	8
#define IR_LIT_STR	9
#define IR_LIT_BYTE	10
#define IR_LABEL	11
#define IR_LIST		12
#define IR_INCBIN	13
#define IR_BUILTIN	14
#define IR_SELECT	15
#define IR_OR		16
#define IR_AND		17
#define IR_BIT_OR	18
#define IR_BIT_XOR	19
#define IR_BIT_AND	20
#define IR_EQ		21
#define IR_LT		22
#define IR_LE		23
#define IR_GT		24
#define IR_GE		25
#define IR_NE		26
#define IR_LSHIFT	27
#define IR_RSHIFT	28
#define IR_ADD		29
#define IR_MINUS	30
#define IR_MULT		31
#define IR_DIV		32
#define IR_MOD		33
#define IR_UMINUS	34
#define IR_BIT_COMPL	35
#define IR_NOT		36
#define IR_FUNC_DEF	37
#define IR_FOR		38
#define IR_RETURN	39
#define IR_RANGE	40
#define IR_ID		41
#define IR_IF		42
#define IR_PARAMDECL	43
#define IR_FUNC_CALL	44
#define IR_NODE		45
#define IR_PROPNODENAME	46
#define IR_LIT_CELL	47
#define IR_LIT_ADDR	48
#define IR_CVT_PROPNODENAME	49
#define IR_CVT_STRING	50
#define IR_CONST_DEF	51

#define IR_NUM_TYPES	52


typedef unsigned int ir_type;

extern char const *ir_type_string(ir_type ir_type);


struct ir {
	ir_type ir_type;
	srcpos *ir_srcpos;

	long long ir_literal;
	char *ir_lit_str;
	char *ir_label_name;
	irb_id ir_builtin_id;

	struct ir *ir_name;
	struct ir *ir_label;
	struct ir *ir_expr1;
	struct ir *ir_expr2;
	struct ir *ir_expr3;

	struct ir *ir_statements;
	struct ir *ir_statements2;
	struct ir *ir_declarations;

	struct ir *ir_first;
	struct ir *ir_last;
	struct ir *ir_prev;
	struct ir *ir_next;
};


extern struct ir *the_ir_tree;

extern struct ir *ir_alloc(ir_type ir_type, srcpos *);
extern struct ir *ir_copy(struct ir *ir);	/* shallow copy */
extern void ir_free(struct ir *ir);
extern void ir_free_all(struct ir *ir);

extern struct ir *ir_alloc_unop(ir_type ir_type,
				struct ir *ir1,
				srcpos *pos);
extern struct ir *ir_alloc_binop(ir_type ir_type,
				 struct ir *ir1,
				 struct ir *ir2,
				 srcpos *pos);
extern struct ir *ir_alloc_triop(ir_type ir_type,
				 struct ir *ir1,
				 struct ir *ir2,
				 struct ir *ir3,
				 srcpos *pos);
extern struct ir *ir_list_append(struct ir *ir_list, struct ir *ir_node);
extern void ir_dump(struct ir *ir);
extern struct ir *ir_eval(struct ir *ir);
extern struct ir_scope *ir_eval_func_body(struct ir *ir_func);

extern void ir_process(void);
extern struct ir *ir_simplify(struct ir *ir, unsigned int ctxt);
extern void ir_emit(struct ir *ir);
extern void ir_emit_statement_list(struct ir *ir_list);


#define IR_EVAL_CTXT_ANY	0
#define IR_EVAL_CTXT_CELL	1

extern int ir_is_constant(struct ir *ir);
extern int ir_is_string(struct ir *ir);
extern char *ir_eval_for_label(struct ir *ir);
extern char *ir_eval_for_name(struct ir *ir);
extern uint64_t ir_eval_for_addr(struct ir *ir);
extern void ir_eval_for_data(struct ir *ir, struct data *d);
extern char *ir_eval_for_c_string(struct ir *ir);


/*
 * IR Messaging.
 */

#define IR_SEV_INFO	0
#define IR_SEV_WARN	1
#define IR_SEV_ERROR	2

typedef	unsigned int ir_severity_t;

#define ir_info(ir, fmt...)	ir_msg(IR_SEV_INFO, ir, ##fmt)
#define ir_warn(ir, fmt...)	ir_msg(IR_SEV_WARN, ir, ##fmt)
#define ir_error(ir, fmt...)	ir_msg(IR_SEV_ERROR, ir, ##fmt)

extern void ir_msg(ir_severity_t severity,
		   struct ir *ir,
		   char const *fmt, ...)
     __attribute__((format(printf, 3, 4)));

#endif /* _IR_H_ */
