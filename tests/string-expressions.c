/*
 * Testcase for dtc string expression support
 *
 * Copyright (C) 2013 David Gibson <david@gibson.dropbear.id.au>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>


#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

struct test_expr {
	const char *expr;
	const char *result;
} expr_table[] = {
#define TE(expr, res)	{ #expr, (res) }
	TE("hello", "hello"),
	TE("hello " + "world", "hello world"),
	TE("hello" + " " + "world", "hello world"),
	TE("hello" * 2 + " world", "hellohello world"),
	TE("hello " + 2 * "world", "hello worldworld"),
	TE(("hello"), "hello"),
	TE(0 ? "hello" : "goodbye", "goodbye"),
	TE(1 ? "hello" : "goodbye", "hello"),
};

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

int main(int argc, char *argv[])
{
	void *fdt;
	const char *res;
	int reslen;
	int i;

	test_init(argc, argv);

	if ((argc == 3) && (strcmp(argv[1], "-g") == 0)) {
		FILE *f = fopen(argv[2], "w");

		if (!f)
			FAIL("Couldn't open \"%s\" for output: %s\n",
			     argv[2], strerror(errno));

		fprintf(f, "/dts-v1/;\n");
		fprintf(f, "/ {\n");
		for (i = 0; i < ARRAY_SIZE(expr_table); i++)
			fprintf(f, "\texpression-%d = %s;\n", i,
				expr_table[i].expr);
		fprintf(f, "};\n");
		fclose(f);
	} else {
		fdt = load_blob_arg(argc, argv);

		for (i = 0; i < ARRAY_SIZE(expr_table); i++) {
			char propname[16];
			int len = strlen(expr_table[i].result) + 1;

			sprintf(propname, "expression-%d", i);
			res = fdt_getprop(fdt, 0, propname, &reslen);

			if (reslen != len)
				FAIL("Incorrect length for expression %s,"
				     " %d instead of %d\n",
				     expr_table[i].expr, reslen, len);

			if (memcmp(res, expr_table[i].result, len) != 0)
				FAIL("Incorrect result for expression %s,"
				     " \"%s\" instead of \"%s\"\n",
				     expr_table[i].expr, res,
				     expr_table[i].result);
		}
	}

	PASS();
}
