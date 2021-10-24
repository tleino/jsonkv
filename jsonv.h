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

#ifndef JSONV_H
#define JSONV_H

#include <stdbool.h>
#include <stddef.h>

#define SZ_READ		8192
#define SZ_STR_CHUNK	128
#define SZ_STR_MUL	2
#define SZ_VAL		256	/* Maximum length of num, bool or null value */
#define SZ_NULL		1

typedef enum jsonv_type
{
	JSONV_UNDEF=0,
	JSONV_NULL,
	JSONV_BOOL,
	JSONV_OBJ,
	JSONV_ARR,
	JSONV_NUM,
	JSONV_STR
} jsonv_type_t;

static const char _jsonv_type_str[7][10] = {
	"undef", "null", "bool", "obj", "arr", "num", "str"
};	

typedef enum jsonv_state
{
	JSONV_DONE=0,
	JSONV_INPROG,
	JSONV_ERR
} jsonv_state_t;

typedef struct jsonv
{
	jsonv_type_t type;
	struct jsonv *first;
	struct jsonv *next;
	struct jsonv *parent;
	char *elem_name;
	union u_v {
		double num;
		char *str;
		bool flag;			
	} v;
	size_t index;	/* For path traversal, array index */
	size_t n_elem;	/* Optional for statistics */
} jsonv_t;

typedef struct jsonv_parser_ctx
{
	size_t n;			/* Position in str or buf */
	size_t len;			/* Len bytes allocated for str */
	bool escape;			/* Quote is escaped */
	bool ws;			/* Whitespace is reached */
	bool need_elem_name;		/* Elem name not yet complete */
	jsonv_state_t state;		/* Global parsing state */
	char buf[SZ_VAL + SZ_NULL];	/* Temporary buffer */
	jsonv_t *current_value;		/* Current value pointer */
	size_t line;			/* Current input line number */

	/* Statistics */
	size_t n_types;
	size_t mem_used;
	size_t n_reallocs;
	size_t n_allocs;
} jsonv_parser_ctx_t;

void		 jsonv_init(jsonv_t *, jsonv_parser_ctx_t *);
void 		 jsonv_reset(jsonv_t *, jsonv_parser_ctx_t *);
void		 jsonv_print(jsonv_t *);
char		*jsonv_parse(char *, jsonv_t *, jsonv_parser_ctx_t *);

#endif
