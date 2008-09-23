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

#include "dtc.h"
#include "nv.h"

struct nv_pair *nv_list;


struct nv_pair *
nv_alloc(void)
{
	struct nv_pair *nv;

	nv = xmalloc(sizeof(struct nv_pair));
	memset(nv, 0, sizeof(struct nv_pair));

	return nv;
}


int
nv_is_present(char *name)
{
	struct nv_pair *nv;

	for (nv = nv_list; nv != NULL; nv = nv->nv_next) {
		if (strcmp(nv->nv_name, name) == 0) {
			return 1;
		}
	}
	return 0;
}


void
nv_dump(void)
{
	struct nv_pair *nv;

	for (nv = nv_list; nv != NULL; nv = nv->nv_next) {
		printf("NV: %s = \"%s\"\n", nv->nv_name, nv->nv_value);
	}
}


/*
 * Accept a string like "foo=123", or "cpu=mpc8548".
 * Split it on the = for name and value parts.
 * Record it in a name-value pairing list for later
 * use when setting up the IR evaluation environment.
 */

void
nv_note_define(char *defstr)
{
	struct nv_pair *nv;
	char *name;
	char *value;

	if (!defstr || ! *defstr)
		return;

	name = xstrdup(defstr);

	/*
	 * Separate name and value at equal sign.
	 */
	value = strchr(name, '=');
	if (value) {
		*value = 0;
		value++;
		if (! *value) {
			value = NULL;
		}
	}

	if (nv_is_present(name)) {
		printf("Warning: Ignored duplicate value %s for %s\n",
		       value, name);
		return;
	}

	debug("nv_note_define(): %s = \"%s\"\n", name, value);

	nv = nv_alloc();
	nv->nv_name = name;
	nv->nv_value = value;

	nv->nv_next = nv_list;
	nv_list = nv;
}
