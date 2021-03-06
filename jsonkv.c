/*
 * Copyright (c) 2021, Tommi Leino <namhas@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "jsonv.h"
#include <err.h>
#include <stdio.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	FILE *fp;
	char str[SZ_READ + SZ_NULL];
	int n;
	jsonv_t v;
	jsonv_parser_ctx_t ctx;
	int ch;
	int debugflag = 0;

#ifdef __OpenBSD__
	if (pledge("stdio", NULL) < 0)
		err(1, "pledge");
#endif

	while ((ch = getopt(argc, argv, "hd")) != -1) {
		switch (ch) {
		case 'd':
			debugflag = 1;
			break;
		case 'h':
		default:
			fprintf(stderr, "Usage: %s [-d]\n",
			    argv[0]);
			fprintf(stderr,
			    "\t-d\tdebug\n");
			return 1;
		}
	}

	argc -= optind;
	argv += optind;

	fp = fdopen(0, "r");
	if (!fp)
		err(1, "fdopen");
	
	jsonv_init(&v, &ctx);
	while ((n = read(fileno(fp), str, sizeof(str) - 1)) > 0) {
		char *p = str;
		str[n] = '\0';

		while (
			(p = jsonv_parse(p, &v, &ctx)) &&
			ctx.state == JSONV_INPROG )
		;

		if (ctx.state == JSONV_DONE) {
			if (debugflag) {
				printf("debug: "
				    "lines %zu "
				    "types %zu "
				    "allocs %zu "
				    "reallocs %zu "
				    "memory %d kB\n",
				    ctx.line, ctx.n_types, ctx.n_allocs,
				    ctx.n_reallocs, (int)
				    (ctx.mem_used / 1024));
			}

			jsonv_print(&v);
			break;
		}
		if (ctx.state == JSONV_ERR)
			errx(1, "parse error on line %lu", ctx.line + 1);
	}
	if (n == -1)
		err(1, "read");
	
	return 0;
}
