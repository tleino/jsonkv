#include "jsonv.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <err.h>

static void		 _value_print(jsonv_t *, int, char *);
static char		*_value_done(char *, jsonv_t *, jsonv_parser_ctx_t *);
static void		 _add_elem(jsonv_parser_ctx_t *);
static char		*_skip_ws(char *, jsonv_parser_ctx_t *);
static char		*_read_until_ws(char *, jsonv_parser_ctx_t *);
static char		*_parse_type(char *, jsonv_t *, jsonv_parser_ctx_t *);

void
jsonv_init(jsonv_t *v, jsonv_parser_ctx_t *ctx)
{
	memset(v, 0, sizeof(jsonv_t));
	memset(ctx, 0, sizeof(jsonv_parser_ctx_t));
}

void
jsonv_reset(jsonv_t *v, jsonv_parser_ctx_t *ctx)
{
	if (v->type == JSONV_STR && v->v.str)
		free(v->v.str);
	if (v->elem_name)
		free(v->elem_name);

	jsonv_init(v, ctx);
}

void jsonv_print(jsonv_t *v)
{
	_value_print(v, 0, NULL);

}

char* jsonv_parse(char *p, jsonv_t *v, jsonv_parser_ctx_t *ctx)
{
	if (ctx->current_value == NULL)
		ctx->current_value = v;

	v = ctx->current_value;

	ctx->state = JSONV_INPROG;

	if (*p == '\0' || p == NULL) {
		return NULL;
	}

	if (v->type == JSONV_UNDEF) {
		if ((p = _skip_ws(p, ctx)) == NULL)
			return NULL;

		p = _parse_type(p, v, ctx);
		if (ctx->state == JSONV_ERR) {
#if 0
			printf("ERROR: Unrecognized type\n");
#endif
			return p;
		}
		ctx->n_types++;
	}

	/* If we are parsing an object element and we're still needing
	 * its name, make sure the data we are parsing is a string
	 */
	if (ctx->need_elem_name && v->type != JSONV_STR) {
#if 0
		printf("ERROR: Need elem name but wrong type\n");
#endif
		ctx->state = JSONV_ERR;
		return p;
	}

	/* At this point we should have type */
	switch (v->type) {
		case JSONV_BOOL:
			p = _read_until_ws(p, ctx);

			if (!strcmp(ctx->buf, "true")) {
				v->v.flag = true;
				return _value_done(p, v, ctx);
			}
			else if (!strcmp(ctx->buf, "false")) {
				v->v.flag = false;
				return _value_done(p, v, ctx);
			}
			else if (ctx->ws) {
#if 0
				printf("ERROR: Reached ws but value is not "
				    "true or false: %s\n", ctx->buf);
#endif
				ctx->state = JSONV_ERR;
				return NULL;
			}

			return p;
		case JSONV_NULL:
			p = _read_until_ws(p, ctx);

			if (!strcmp(ctx->buf, "null")) {
				return _value_done(p, v, ctx);
			}
			else if (ctx->ws) {
				ctx->state = JSONV_ERR;
				return NULL;
			}

			return p;
		case JSONV_NUM:
			p = _read_until_ws(p, ctx);

			if (ctx->ws) {
				v->v.num = atof(ctx->buf);
				return _value_done(p, v, ctx);
			}

			return p;
		case JSONV_ARR:
			if ((p = _skip_ws(p, ctx)) == NULL)
				return NULL;

			if (*p == ']') {
				return _value_done(++p, v, ctx);
			}
			if (*p == ',' || v->first == NULL) {
				_add_elem(ctx);
				if (*p == ',') return ++p;
				return p;
			} else {
#if 0
				printf("ERROR: Expected ',' got '%c'\n", *p);
#endif
				ctx->state = JSONV_ERR;
				return NULL;
			}
			return p;
		case JSONV_OBJ:
			if ((p = _skip_ws(p, ctx)) == NULL)
				return NULL;

			if (*p == '}') {
				return _value_done(++p, v, ctx);
			}
			if (*p == ',' || v->first == NULL) {
				_add_elem(ctx);
				ctx->need_elem_name = true;
				if (*p == ',') return ++p;
				return p;
			} else {
#if 0
				printf("ERROR: Expected ',' got '%c' (%d)\n",
				    *p, *p);
#endif
				ctx->state = JSONV_ERR;
				return NULL;
			}
			return p;
		case JSONV_STR: {
			char *s;
			if (v->v.str == NULL) {
				ctx->len = SZ_STR_CHUNK;
				v->v.str = malloc(ctx->len);

				ctx->n_allocs++;
				ctx->mem_used += ctx->len;

				ctx->n = 0;
			}
			s = v->v.str;
			for (; p && *p != '\0'; p++) {
				if (*p == '\\') {
					ctx->escape = true;
					continue;
				}
				if (ctx->n == ctx->len) {
					ctx->mem_used -= ctx->len;

					ctx->len *= SZ_STR_MUL;
					v->v.str = realloc(s, ctx->len);

					ctx->n_reallocs++;
					ctx->mem_used += ctx->len;

					s = v->v.str;
				} 
				if (*p == '\"' && ctx->escape == false) {
					s[ctx->n] = '\0';
					if (ctx->need_elem_name) {
						/* Don't copy,
						 * switch pointers, because
						 * we needed the str for
						 * elem name but we still
						 * need v.str later if the
						 * elem is a str
						 */
						v->elem_name = s;
						v->v.str = NULL;
						ctx->need_elem_name = false;
						ctx->n = 0;
						v->type = JSONV_UNDEF;
						return ++p;
					}
					return _value_done(++p, v, ctx);
				}
				s[ctx->n++] = *p;
				if (ctx->escape) ctx->escape = false;
			}
			return NULL;
		}
		default:
			break;
	}

	ctx->state = JSONV_ERR;
	return NULL;
}

static void
_value_print(jsonv_t *v, int indent_level, char *path)
{
	jsonv_t *n;
	char *npath = NULL;

	if (v->elem_name) {
		if (path == NULL) {
			npath = malloc(sizeof(char) * 1024);
			npath[0] = 0;
		} else {
			npath = strdup(path);
		}
		strcat(npath, "/");
		strcat(npath, v->elem_name);
	}
	if (npath && npath[0] != 0) {
		printf("%s: ", npath);
	}

	switch (v->type) {
		case JSONV_BOOL:
			printf("%s\n", v->v.flag ? "true" : "false");
			break;
		case JSONV_NULL:
			printf("null\n");
			break;
		case JSONV_NUM:
			printf("%f\n", v->v.num);
			break;
		case JSONV_STR:
			printf("\"%s\"\n", v->v.str);
			break;
		case JSONV_ARR:
			n = v->first;
			printf("array (%zu elements)\n", v->n_elem);
			_value_print(n, indent_level + 1, npath);
			break;
		case JSONV_OBJ:
			n = v->first;
			printf("object\n");
			while (n) {
				_value_print(n, indent_level + 1, npath);
				n = n->next;
			}
			break;
		default:
			printf("<unhandled type>\n");
			break;
	}
}

static char *
_value_done(char *p, jsonv_t *v, jsonv_parser_ctx_t *ctx)
{
	/* Go up to parent or just signal completion */
	if (v->parent) {
		ctx->current_value = v->parent;
		ctx->n = 0;
		ctx->buf[0] = 0;
		ctx->ws = false;
		ctx->need_elem_name = false;
		return _skip_ws(p, ctx);
	}
	ctx->state = JSONV_DONE;
	return p;
}

static void
_add_elem(jsonv_parser_ctx_t *ctx)
{
	jsonv_t *nv;

	nv = calloc(1, sizeof(jsonv_t));
	if (nv == NULL)
		err(1, "calloc");

	ctx->n_allocs++;
	ctx->mem_used += sizeof(jsonv_t);

	nv->parent = ctx->current_value;
	nv->parent->n_elem++;

	if (nv->parent->first == NULL) {
		nv->parent->first = nv;
	}
	else {
		nv->next = nv->parent->first;
		nv->parent->first = nv;
	}
	ctx->current_value = nv;
}

static char *
_skip_ws(char *p, jsonv_parser_ctx_t *ctx)
{
	for (; p && *p != '\0'; p++) {
		if (*p == '\n') {
			ctx->line++;
			continue;
		}
		if (*p == ' ' || *p == '\t' || *p == '\r' || *p == ':') {
			continue;
		}
		else {
			break;
		}
	}
	if (*p == '\0') return NULL;
	return p;
}

static char *
_read_until_ws(char *p, jsonv_parser_ctx_t *ctx)
{
	while (ctx->ws == false && ctx->n < SZ_VAL && *p != '\0' ) {
		if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'
		|| *p == ',' || *p == ']' || *p == '}') {
			if (*p == '\n') ctx->line++;
			ctx->ws = true;
			break;
		}
		ctx->buf[ctx->n++] = *(p++);
	}
	ctx->buf[ctx->n] = '\0';
	return p;
}

static char*
_parse_type(char *p, jsonv_t *v, jsonv_parser_ctx_t *ctx)
{
	/* Simple one-letter identification */
	switch (*p) {
		case '\"':
			v->type = JSONV_STR;
			return ++p;
		case '{':
			v->type = JSONV_OBJ;
			return ++p;
		case '[':
			v->type = JSONV_ARR;
			return ++p;
		case 't': case 'f':
			v->type = JSONV_BOOL;
			return p;
		case 'n':
			v->type = JSONV_NULL;
			return p;
		case '-': case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			v->type = JSONV_NUM;
			return p;
	}

	ctx->state = JSONV_ERR;
	return NULL;
}
