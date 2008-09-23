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

%option noyywrap nounput noinput yylineno stack

%x BYTESTRING
%x PROPNODENAME
%x COMMENT
%s V1

NOTPROPNAMES	[+?-]
PROPNODECHAR	[a-zA-Z0-9,._+*#?@-]
PATHCHAR	({PROPNODECHAR}|[/])
LABEL		[a-zA-Z_][a-zA-Z0-9_]*
ID		\\[a-zA-Z_][a-zA-Z0-9_]*
STRING		\"([^\\"]|\\.)*\"
WS		[[:blank:]]
LINECOMMENT	"//".*\n

%{
#include "dtc.h"
#include "srcpos.h"
#include "dtc-parser.tab.h"

/*#define LEXDEBUG	1*/

#ifdef LEXDEBUG
#define DPRINT(fmt, ...)	fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define DPRINT(fmt, ...)	do { } while (0)
#endif

static int dts_version = 1;

/*
 * Track column positions too.  Brute force.
 */
int yycolno = 1;

#define YY_USER_ACTION	{ \
		yylloc.file = srcpos_file; \
		yylloc.first_line = yylineno; \
		yylloc.last_line = yylineno; \
		yylloc.first_column = yycolno; \
		yycolno += yyleng; \
		yylloc.last_column = yycolno - 1; \
		DPRINT("Set srcpos: %s\n", srcpos_string(&yylloc)); \
	}

#define BEGIN_DEFAULT()		DPRINT("<V1>\n"); \
				BEGIN(V1); \

static void push_input_file(const char *filename);
static int pop_input_file(void);
%}

%%
<*>"/include/"{WS}*{STRING} {
			char *name = strchr(yytext, '\"') + 1;
			yytext[yyleng-1] = '\0';
			push_input_file(name);
		}

<*><<EOF>>	{
			if (!pop_input_file()) {
				yyterminate();
			}
		}

<*>{STRING}	{
			DPRINT("String: %s\n", yytext);
			yytext[yyleng-1] = 0; /* remove close quote */
			yylval.litstr = xstrdup(yytext + 1);
			return DT_STRING;
		}

<*>"/dts-v1/"	{
			DPRINT("Keyword: /dts-v1/\n");
			dts_version = 1;
			BEGIN_DEFAULT();
			return DT_V1;
		}

<*>"/memreserve/"	{
			DPRINT("Keyword: /memreserve/\n");
			BEGIN_DEFAULT();
			return DT_MEMRESERVE;
		}

<*>{LABEL}:	{
			DPRINT("Label: %s\n", yytext);
			yylval.labelref = xstrdup(yytext);
			yylval.labelref[yyleng-1] = '\0';
			return DT_LABEL;
		}

<*>\&{LABEL}	{
			/* label reference */
			DPRINT("Ref: %s\n", yytext+1);
			yylval.labelref = xstrdup(yytext+1);
			return DT_REF;
		}

<*>"&{/"{PATHCHAR}+\}	{
			/* new-style path reference */
			yytext[yyleng-1] = '\0';
			DPRINT("Ref: %s\n", yytext+2);
			yylval.labelref = xstrdup(yytext+2);
			return DT_REF;
		}

<BYTESTRING>[0-9a-fA-F]{2} {
			yylval.byte = strtol(yytext, NULL, 16);
			DPRINT("Byte: %02x\n", (int)yylval.byte);
			return DT_BYTE;
		}

<BYTESTRING>"]"	{
			DPRINT("/BYTESTRING\n");
			BEGIN_DEFAULT();
			return ']';
		}

<*>[0-9]+|0[xX][0-9a-fA-F]+      {
			yylval.literal = xstrdup(yytext);
			DPRINT("Literal: '%s'\n", yylval.literal);
			return DT_LITERAL;
		}

<*>"for"	{
			DPRINT("token: for\n");
			BEGIN_DEFAULT();
			return DT_FOR;
		}

<*>"if"		{
			DPRINT("token: if\n");
			BEGIN_DEFAULT();
			return DT_IF;
		}

<*>"void"		{
			DPRINT("token: void\n");
			return DT_VOID;
		}

<*>"in"	{
			DPRINT("token: in\n");
			BEGIN_DEFAULT();
			return DT_IN;
		}

<*>"else"	{
			DPRINT("token: else\n");
			BEGIN_DEFAULT();
			return DT_ELSE;
		}

<*>"return"	{
			DPRINT("token: return\n");
			return DT_RETURN;
		}

<*>"/define/"	{
			DPRINT("token: define\n");
			BEGIN_DEFAULT();
			return DT_DEFINE;
		}

<*>"/const/"	{
			DPRINT("token: const\n");
			return DT_CONST;
}

<*>"/incbin/"	{
			DPRINT("Binary Include\n");
			return DT_INCBIN;
		}

<*>{ID}		{
			DPRINT("Id: %s\n", yytext);
			yylval.id = xstrdup(yytext + 1); /* drop \ */
			BEGIN_DEFAULT();
			return DT_ID;
		}

<PROPNODENAME>{NOTPROPNAMES} {
			/*
			 * Lone +, - and ? chars shouldn't be
			 * property names.
			 */
			BEGIN_DEFAULT();
			return yytext[0];
		}


<PROPNODENAME>{PROPNODECHAR}+ {
			DPRINT("PropNodeName: %s\n", yytext);
			yylval.propnodename = xstrdup(yytext);
			BEGIN_DEFAULT();
			return DT_PROPNODENAME;
		}

<*>{WS}+	{
			/* eat whitespace, but not newline */
		}

<*>"/*"		{
			yy_push_state(COMMENT);
		}

<COMMENT>{
[^*\n]*		{
			/* munch */
		}

[^*\n]*\n	{
			yycolno = 1;
		}

"*"+[^*/\n]*	{
			/* munch */
		}

"*"+[^*/\n]*\n {
			yycolno = 1;
		}

"*"+"/"		{
			yy_pop_state();
		}
}


<*>{LINECOMMENT}+ {
			/* eat C++-style comments including newline */
			yycolno = 1;
		}

<*>":="		{
			DPRINT("token: <<\n");
			return DT_ASSIGN;
		}
<*>"<<"		{
			DPRINT("token: <<\n");
			return DT_LSHIFT;
		}
<*>">>"		{
			DPRINT("token: >>\n");
			return DT_RSHIFT;
		}
<*>"<="		{
			DPRINT("token: <=\n");
			return DT_LE;
		}
<*>">="		{
			DPRINT("token: >=\n");
			return DT_GE;
		}
<*>"=="		{
			DPRINT("token: ==\n");
			return DT_EQ;
		}
<*>"!="		{
			DPRINT("token: !=\n");
			return DT_NE;
		}
<*>"&&"		{
			DPRINT("token: &&\n");
			return DT_AND;
		}
<*>"||"		{
			DPRINT("token: ||\n");
			return DT_OR;
		}
<*>".."		{
			DPRINT("token: ..\n");
			return DT_RANGE;
		}

<*>\n		{
			yycolno = 1;
		}

<*>.		{
			DPRINT("Char: %c (\\x%02x)\n", yytext[0],
				(unsigned)yytext[0]);
			if (yytext[0] == '[') {
				DPRINT("<BYTESTRING>\n");
				BEGIN(BYTESTRING);
			}
			if ((yytext[0] == '{')
			    || (yytext[0] == ';')
			    || (yytext[0] == ':')
			) {
				DPRINT("<PROPNODENAME>\n");
				BEGIN(PROPNODENAME);
			}
			return yytext[0];
		}

%%


/*
 * Stack of nested include file contexts.
 */

struct incl_file {
	struct dtc_file *file;
	YY_BUFFER_STATE yy_prev_buf;
	int yy_prev_lineno;
	int yy_prev_colno;
	struct incl_file *prev;
};

static struct incl_file *incl_file_stack;


/*
 * Detect infinite include recursion.
 */
#define MAX_INCLUDE_DEPTH	(100)

static int incl_depth = 0;


static void push_input_file(const char *filename)
{
	struct incl_file *incl_file;
	struct dtc_file *newfile;
	struct search_path search, *searchptr = NULL;

	assert(filename);

	if (incl_depth++ >= MAX_INCLUDE_DEPTH)
		die("Includes nested too deeply");

	if (srcpos_file) {
		search.dir = srcpos_file->dir;
		search.next = NULL;
		search.prev = NULL;
		searchptr = &search;
	}

	newfile = dtc_open_file(filename, searchptr);

	incl_file = xmalloc(sizeof(struct incl_file));

	/*
	 * Save current context.
	 */
	incl_file->yy_prev_buf = YY_CURRENT_BUFFER;
	incl_file->yy_prev_lineno = yylineno;
	incl_file->yy_prev_lineno = yycolno;
	incl_file->file = srcpos_file;
	incl_file->prev = incl_file_stack;

	incl_file_stack = incl_file;

	/*
	 * Establish new context.
	 */
	srcpos_file = newfile;
	yylineno = 1;
	yycolno = 1;
	yyin = newfile->file;
	yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
}


static int pop_input_file(void)
{
	struct incl_file *incl_file;

	if (incl_file_stack == 0)
		return 0;

	dtc_close_file(srcpos_file);

	/*
	 * Pop.
	 */
	--incl_depth;
	incl_file = incl_file_stack;
	incl_file_stack = incl_file->prev;

	/*
	 * Recover old context.
	 */
	yy_delete_buffer(YY_CURRENT_BUFFER);
	yy_switch_to_buffer(incl_file->yy_prev_buf);
	yylineno = incl_file->yy_prev_lineno;
	yycolno = incl_file->yy_prev_colno;
	srcpos_file = incl_file->file;
	yyin = incl_file->file ? incl_file->file->file : NULL;

	/*
	 * Free old state.
	 */
	free(incl_file);

	return 1;
}
