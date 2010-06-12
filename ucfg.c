/* Copyright (c) 2010, Torbjörn Lönnemark <tobbez@ryara.net>
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
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>

#include "ucfg.h"

/* oom handler */
static void oom(void) __attribute__((noreturn));
static void oom(void)
{
	static const char msg[] = "libucfg: Not enough memory\n";
	fputs(msg, stderr);
	abort();
}

/* malloc that aborts on oom */
static void *ucfg_xmalloc(size_t size)
{
	void *p;

	if (!(p = malloc(size))) {
		oom();
	}

	return p;
}

/* realloc that aborts on oom */
static void *ucfg_xrealloc(void *ptr, size_t size)
{
	void *p;

	if (!(p = realloc(ptr, size))) {
		oom();
	}

	return p;
}

/* convert return values to strings */
const char *ucfg_strerror(int error)
{
	static const char *messages[UCFG_ERR_MAX] = {
		"libucfg: OK",					/* UCFG_OK */ 
		"libucfg: Error when opening file for reading",	/* UCFG_ERR_FILE_READ */
		"libucfg: Error when opening file for writing", /* UCFG_ERR_FILE_WRITE */
		"libucfg: Syntax error in provided config",	/* UCFG_ERR_SYNTAX */
		"libucfg: Requested node does not exist"	/* UCFG_ERR_NODE_INEXISTENT */
	};

	if (error >= UCFG_ERR_MAX) {
		return NULL;
	}

	return messages[error];
}

/* initialize a config structure */
void ucfg_node_init(struct ucfg_node **conf)
{
	*conf = ucfg_xmalloc(sizeof(**conf));

	(*conf)->name = NULL;
	(*conf)->value = NULL;

	(*conf)->sub = NULL;
	(*conf)->next = NULL;
}

/* destroy a config structure */
void ucfg_node_destroy(struct ucfg_node *conf)
{
	if (conf != NULL) {
		ucfg_node_destroy(conf->sub);
		conf->sub = NULL;

		ucfg_node_destroy(conf->next);
		conf->next = NULL;

		if (conf->name != NULL) {
			free(conf->name);
			conf->name = NULL;
		}

		if (conf->value != NULL) {
			free(conf->value);
			conf->value = NULL;
		}

		free(conf);
	}
}


/* make the current node contain a new subsection */
void ucfg_node_sub_create(struct ucfg_node *conf, struct ucfg_node **created)
{
	ucfg_node_init(&conf->sub);
	if (created != NULL) {
		*created = conf->sub;
	}
}

/* append a config node onto a section */
void ucfg_node_next_append(struct ucfg_node *conf, struct ucfg_node **created)
{
	struct ucfg_node *tmp;

	tmp = conf;
	while (tmp->next != NULL) {
		tmp = tmp->next;
	}

	ucfg_node_init(&tmp->next);

	if (created != NULL) {
		*created = tmp->next;
	}
}

/* set the name of a config node */
void ucfg_node_name_set(struct ucfg_node *conf, const char *name)
{
	conf->name = ucfg_xmalloc(strlen(name) + 1);
	strcpy(conf->name, name);
}

/* set the value of a config node */
void ucfg_node_value_set(struct ucfg_node *conf, const char *value)
{
	conf->value = ucfg_xmalloc(strlen(value) + 1);
	strcpy(conf->value, value);
}


/* escape double-quotes in a string */
static char *ucfg_string_escape(char *str)
{
	char *tmp, *res, *old_pos;

	tmp = old_pos = str;
	res = ucfg_xmalloc(1);
	*res = '\0';

	while ((tmp = strchr(tmp, '"')) != NULL) {
		int res_old_len = strlen(res) + 1;
		char *res_old_end;
		int new_len = res_old_len + (tmp - old_pos + 1) + 1;
		res = ucfg_xrealloc(res, new_len);
		res_old_end = res + strlen(res);
		strncpy(res_old_end, old_pos, tmp - old_pos + 1);
		*(res + new_len - 2) = '"';
		*(res + new_len - 1) = '\0';

		++tmp;
		old_pos = tmp;
	}

	{
		char *res_old_end;
		int new_len = strlen(res) + strlen(old_pos) + 1;
		res = ucfg_xrealloc(res, new_len);
		res_old_end = res + strlen(res);
		strcpy(res_old_end, old_pos);
	}

	return res;
}

/* serialize a config structure onto a file stream, with indentation */
static void ucfg_write_indented(struct ucfg_node *conf, int indent, FILE *stream)
{
	fprintf(stream, "%*s", indent, "");
	if (conf->name)
		fprintf(stream, "%s: ", conf->name);

	if (conf->value) {
		char *esc = ucfg_string_escape(conf->value);
		fprintf(stream, "\"%s\";\n", esc);
		free(esc);
	} else if (conf->sub) {
		fprintf(stream, "{\n");
		ucfg_write_indented(conf->sub, indent + 2, stream);
		fprintf(stream, "%*s", indent, "");
		fprintf(stream, "}\n");
	}

	if (conf->next != NULL) {
		ucfg_write_indented(conf->next, indent, stream);
	}
}

/* serialize a config structure onto a file stream */
void ucfg_write(struct ucfg_node *conf, FILE *stream)
{
	ucfg_write_indented(conf, 0, stream);
}

/* serialize a config structure into a config file */
int ucfg_write_file(struct ucfg_node *conf, const char *filename)
{
	FILE *f = fopen(filename, "w");
	if (!f) {
		return UCFG_ERR_FILE_WRITE;
	} else {
		ucfg_write(conf, f);
		fflush(f);
		fclose(f); 
	}

	return UCFG_OK;
}


/* size of steps to increase strings by while reading into them */
#define ALLOC_CHUNK_SIZE 32

/* discard whitespace from a file stream */
static void parse_read_whitespace(FILE *stream)
{
	int tmp;
	while ((tmp = getc(stream)) == ' ' || tmp == '\t' || tmp == '\n');
	if(tmp != EOF) {
		ungetc(tmp, stream);
	}
}


/* parse and read a value entity from a file stream */
static char *parse_read_value(FILE *stream)
{
	int tmp;
	int len = 0;
	int max_len = ALLOC_CHUNK_SIZE;
	char *ret = ucfg_xmalloc(max_len);

	while (1) {
		if (len == max_len) {
			max_len += ALLOC_CHUNK_SIZE;
			ret = ucfg_xrealloc(ret, max_len);
		}

		if ((tmp = getc(stream)) == EOF) {
			fprintf(stderr, "parse_read_value: expected character, got EOF\n");
			free(ret);
			ret = NULL;
		} else if (tmp == '"') { /* escaped double-quote or end of value */
			if ((tmp = getc(stream)) == '"') { /* escaped double-quote */
				ret[len++] = '"';
			} else if (tmp == ';') { /* end of value */
				break;
			} else {
				fprintf(stderr, "parse_read_value: expected ';', got '%c'\n", tmp);
				free(ret);
				ret = NULL;
				break;
			}
		} else {
			ret[len++] = tmp;
		}
	}

	if (ret != NULL) {
		ret = ucfg_xrealloc(ret, len + 1);
		ret[len] = '\0';
	}

	return ret;
}

/* parse and read a name entity from a file stream */
static char *parse_read_name(FILE *stream)
{
	int len = 0;
	int max_len = ALLOC_CHUNK_SIZE;
	char *ret;
	int tmp;

	ret = ucfg_xmalloc(max_len);

	while (1) {
		if (len == max_len) {
			max_len += ALLOC_CHUNK_SIZE;
			ret = ucfg_xrealloc(ret, max_len);
		}

		if ((tmp = getc(stream)) == EOF) {
			fprintf(stderr, "parse_read_name:"
					"expected character, got EOF\n");
			free(ret);
			ret = NULL;
		} else if (tmp == ':') { /* end of name */
			break;
		} else {
			ret[len++] = tmp;
		}
	}

	if (ret != NULL) {
		ret = ucfg_xrealloc(ret, len + 1);
		ret[len] = '\0';
	}

	return ret;
}

/* create a config structure from a file stream */
int ucfg_read(struct ucfg_node **dest, FILE *stream)
{
	struct ucfg_node *cur;
	int retval = UCFG_OK;
	int tmpc;
	char *tmps;

	ucfg_node_init(dest);
	cur = *dest;

	do {
		parse_read_whitespace(stream);

		tmpc = getc(stream);
		if (tmpc != EOF) {
			if (tmpc == '{') { /* read list, recurse  */
				if (cur->sub != NULL) {
					ucfg_node_init(&cur->next);
					cur = cur->next;
				}
				if ((retval = ucfg_read(&cur->sub, stream)) != UCFG_OK) {
					break;
				}
			} else if (tmpc == '}') { /* end of list */
				break;
			} else if (tmpc == '"') {
				if (cur->value != NULL) {
					ucfg_node_init(&cur->next);
					cur = cur->next;
				}
				if ((tmps = parse_read_value(stream)) != NULL) {
					cur->value = tmps;
				} else {
					retval = UCFG_ERR_SYNTAX;
					break;
				}
			} else { /* read name */
				ungetc(tmpc, stream);
				if (cur->name != NULL && cur->sub == NULL && cur->value == NULL) {
					retval = UCFG_ERR_SYNTAX;
					break;
				} else if (cur->sub != NULL || cur->value != NULL) {
					ucfg_node_init(&cur->next);
					cur = cur->next;
				}
				if ((tmps = parse_read_name(stream)) != NULL) {
					cur->name = tmps;
				} else {
					retval = UCFG_ERR_SYNTAX;
				}
			}
		}

	} while (retval == UCFG_OK && !feof(stream));

	if (retval != UCFG_OK) {
		ucfg_node_destroy(*dest);
		*dest = NULL;
	}

	return retval;
}

/* create a config structure from a config file */
int ucfg_read_file(struct ucfg_node **dest, const char *filename)
{
	int retval;
	FILE *f = fopen(filename, "r");

	if (!f) {
		retval = UCFG_ERR_FILE_READ;
	} else {
		retval = ucfg_read(dest, f);
		fclose(f); 
	}

	return retval;
}

/* split a colon-delimeted string. function modifies its first argument. */
static char *strsepc(char **stringp) 
{
	if (*stringp == NULL) {
		return NULL;
	} else {
		char *ret;
		char *pos;
		ret = *stringp;

		if ((pos = strchr(*stringp, ':')) != NULL) {
			*pos = '\0';
			*stringp = pos + 1;
		} else {
			*stringp = NULL;
		}

		return ret;
	}
}

/* lookup a section or value in a config structure */
int ucfg_lookup(struct ucfg_node **found, struct ucfg_node *root, const char *path)
{
	char *lpath;
	char *curpos;
	char *curstr;

	lpath = ucfg_xmalloc(strlen(path) + 1);
	strcpy(lpath, path);

	curpos = lpath;
	*found = root;

	curstr = strsepc(&curpos);
	while (curstr != NULL) {
		while (*found != NULL) {
			if (strcmp(curstr, "") == 0) {
				break;
			} else if ((*found)->name != NULL && 
					strcmp((*found)->name, curstr) == 0) {
				break;
			}
			*found = (*found)->next;
		}

		if (*found == NULL) {
			break;
		}

		curstr = strsepc(&curpos);
		if (curstr != NULL) {
			*found = (*found)->sub;
		}
	}

	free(lpath);

	if (*found == NULL) {
		return UCFG_ERR_NODE_INEXISTENT;
	}

	return UCFG_OK;
}
