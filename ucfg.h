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

#ifndef CONF_H_
#define CONF_H_

#include <stdio.h>

struct ucfg_node {
	char *name;
	char *value;

	struct ucfg_node *sub;
	struct ucfg_node *next;
};

enum {
	UCFG_OK,
	UCFG_ERR_FILE_READ,
	UCFG_ERR_FILE_WRITE,
	UCFG_ERR_SYNTAX,
	UCFG_ERR_NODE_INEXISTENT,
	UCFG_ERR_MAX
};

const char *ucfg_strerror(int error);

void ucfg_node_init(struct ucfg_node **conf);
void ucfg_node_destroy(struct ucfg_node *conf);

void ucfg_node_name_set(struct ucfg_node *conf, const char *name);
void ucfg_node_value_set(struct ucfg_node *conf, const char *value);

void ucfg_node_sub_create(struct ucfg_node *conf, struct ucfg_node **created);
void ucfg_node_next_append(struct ucfg_node *conf, struct ucfg_node **created);

int ucfg_read(struct ucfg_node **dest, FILE *stream);
int ucfg_read_file(struct ucfg_node **dest, const char *filename);

void ucfg_write(struct ucfg_node *conf, FILE *stream);
int ucfg_write_file(struct ucfg_node *conf, const char *filename);

int ucfg_lookup(struct ucfg_node **found, struct ucfg_node *root, const char *path);

#endif
