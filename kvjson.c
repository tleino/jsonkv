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
			fprintf(stderr, "Usage: %s [-v]\n",
			    argv[0]);
			fprintf(stderr,
			    "\t-d\tdebug\n");
			break;
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
