/*
 * Copyright (c) 2012, Shixin Zeng
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "iniconfig.h"

#ifdef _DEBUG
#define debug(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif //__DEBUG

typedef enum{
	INITIAL,
	SECTION,
	AFTER_SECTION,
	ENTRY,
	AFTER_ENTRY
} state_t;

static char msg[4096];

static void
line_break_r(const char content[], unsigned long len, 
			 unsigned long *i, int *line, int *col)
{
	unsigned long j = *i;
	if (j + 1 < len && content[j + 1] == '\n')
		j += 2;
	else
		j ++;

	(*line) ++;
	*col = 1;

	*i = j;
}

static void
line_break_n(const char content[], unsigned long len, 
			 unsigned long *i, int *line, int *col)
{
	(*i) ++;
	(*line) ++;
	*col = 1;
}

static void
comment(const char content[], unsigned long len, 
		 unsigned long *i, int *line, int *col)
{
	unsigned long j = *i;
	j ++;
	while(j < len 
		  && content[j] != '\r'
		  && content[j] != '\n') {
		j ++;
	}
	if(j < len && content[j] == '\r')
		line_break_r(content, len, &j, line, col);
	else if(j < len && content[j] == '\n')
		line_break_n(content, len, &j, line, col);

	*i = j;
}

static void
spaces(const char content[], unsigned long len, 
	  unsigned long *i, int *line, int *col)
{
	unsigned long j = *i;
	while(j < len 
		  && (content[j] == ' ' || content[j] == '\t')){
		j ++;
	}
	(*col) += j - (*i);
	*i = j;
}

static char
escape(char c)
{
	switch (c){
		case '\\':
			return '\\';
		case '0':
			return '\0';
		case 'a':
			return '\a';
		case 'b':
			return '\b';
		case 't':
			return '\t';
		case 'r':
			return '\r';
		case '#':
			return '#';
		case ';':
			return ';';
		case '=':
			return '=';
		case ':':
			return ':';
		default:
			return c;
	}
}

static char *
string_delimiter(const char content[], unsigned long len, 
			   unsigned long *i, int *line, int *col,
			   iniconfig_error_report_func_t report,
			   char d)
{
	unsigned long j = *i;
	unsigned long k = 0;
	unsigned size = 0;
	char *value = NULL;
	int iline = *line;
	int icol = *col;
	for(j = *i; j < len; j ++){
		if(content[j] == '\\'){
			j ++;
		}else if(content[j] == d){
			break;
		}
		size ++;
	}
	if(j == len){
		if(report != NULL){
			snprintf(msg, sizeof(msg), "ERROR: line %d, col %d: couldn't find the corresponding ending '%c'", iline, icol, d);
			report(msg);
		}
		return NULL;
	}
	value = malloc(size + 1);
	if(value == NULL){
		if(report != NULL){
			report("Not enough memory");
		}
		return NULL;
	}
	j = *i;
	while(j < len){
		if(content[j] == '\\'){
			value[k ++] = escape(content[j + 1]);
			j += 2;
			*col += 2;
		}else if(content[j] == d){
			j ++;
			(*col) ++;
			break;
		}else if(content[j] == '\r'){
			value[k ++ ] = '\r';
			if(content[j] == '\n')
				value[k ++] = '\n';
			line_break_r(content, len, &j, line, col);
		}else if(content[j] == '\n'){
			value[k ++] = '\n';
			line_break_n(content, len, &j, line, col);
		}else{
			value[k ++] = content[j];
			j ++;
			(*col) ++;
		}
	}

	*i = j;
	value[k] = '\0';
	assert(size == k);
	return value;
}

static char *
string_apostrophe(const char content[], unsigned long len, 
				  unsigned long *i, int *line, int *col,
				  iniconfig_error_report_func_t report)
{
	return string_delimiter(content, len, i, line, col, report, '\'');
}

static char *
string_quote(const char content[], unsigned long len, 
				  unsigned long *i, int *line, int *col,
				  iniconfig_error_report_func_t report)
{
	return string_delimiter(content, len, i, line, col, report, '"');
}

static int
is_word_char(char c)
{
	return ((c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			|| c == '_');
}
static void
add_entry(iniconfig_section_t *sec,
		  iniconfig_entry_t *entry)
{
	iniconfig_list_append(&sec->entry_list, &entry->node);
	sec->n_ent ++;
}

static void
add_section(iniconfig_config_t *config,
			iniconfig_section_t *sec)
{
	iniconfig_list_append(&config->section_list, &sec->node);
	config->n_sec ++;
}

void
iniconfig_config_free(iniconfig_config_t *config)
{
	iniconfig_section_t *sec = NULL;
	list_node_t *next = NULL;

	if(config == NULL)
		return;

	sec = (iniconfig_section_t*) config->section_list.head;
	while(sec != NULL){
		iniconfig_entry_t *entry = (iniconfig_entry_t *)sec->entry_list.head;
		while(entry != NULL){
			next = entry->node.next;
			free(entry->name);
			free(entry->value);
			free(entry);
			entry = (iniconfig_entry_t *)next;
		}
		next = sec->node.next;
		free(sec->name);
		free(sec);
		sec = (iniconfig_section_t *)next;
	}
	free(config);
}


/* eat as many non-sense char as possible
 *
 * return 1 if the last char eaten was a line break,
 * 0 otherwise
 */
static
int
eat_nonsense(const char content[], unsigned long len, 
			 unsigned long *i, int *line, int *col)
{
	if(*i >= len)
		return 0;

	switch(content[*i]){
		case ';':
		case '#': /* comments till end of the line */
			comment(content, len, i, line, col);
			return 1;
		case ' ':
		case '\t':
			spaces(content, len, i, line, col);
			return 0;
		case '\r':
			line_break_r(content, len, i, line, col);
			return 1;
		case '\n':
			line_break_n(content, len, i, line, col);
			return 1;
		default:
			return 0;
	}

}

iniconfig_config_t *iniconfig_config_new(const char content[], unsigned long len,
										  iniconfig_error_report_func_t report)
{
	int line = 1, col = 1;
	state_t state = INITIAL;
	unsigned long i = 0;
	iniconfig_section_t *cur_sec = NULL;
	int line_break = 0;
	iniconfig_config_t *config = malloc(sizeof(iniconfig_config_t));
	if(config == NULL){
		if(report != NULL) 
			report("Not enough memory");

		return NULL;
	}
	memset(config, 0, sizeof(iniconfig_config_t));
	iniconfig_list_initialize(&config->section_list);

	while(i < len){
		line_break = eat_nonsense(content, len, &i, &line, &col);
		if(i >= len)
			break;

		switch(state){
			case INITIAL:
				switch(content[i]){
					case '[': /* section */
						state = SECTION;
						col ++;
						i ++;
						break;
					default:
						if(report != NULL){
							snprintf(msg, sizeof(msg), "ERROR, line %d, col %d: unexpected char '%c', expecting '['", line, col, content[i]);
							report(msg);
						}
						goto failed;
				}
			case SECTION:
				{
					unsigned long start = i;
					char *name = NULL;
					iniconfig_section_t *sec = NULL;
					debug("A new section starts at line %d, col %d\n", line, col);
					while(i < len && is_word_char(content[i])){
						i ++;
						col ++;
					}
					if(i >= len || content[i] != ']'){
						if(report != NULL){
							snprintf(msg, sizeof(msg), "ERROR, line %d, col %d: unexpected char '%c', expecting ']'", line, col, content[i]);
							report(msg);
						}
						goto failed;
					}
					if(i == start){
						if(report != NULL){
							snprintf(msg, sizeof(msg), "ERROR, line %d, col %d: empty section name", line, col);
							report(msg);
						}
						goto failed;
					}

					name = malloc(i - start + 1);
					if(name == NULL){
						if(report != NULL){
							report("Not enough memory");
						}
						free(cur_sec);
						goto failed;
					}
					memcpy(name, &content[start], i - start);
					name[i - start] = '\0';
					sec = iniconfig_find_section(config, name);
					if(sec != NULL){
						if(report != NULL){
							snprintf(msg, sizeof(msg), "WARN, line %d, col %d: duplicated section %s, merging the two sections", line, col, cur_sec->name);
							report(msg);
						}
						free(name);
						cur_sec = sec;
					}else{
						cur_sec = malloc(sizeof(iniconfig_section_t));
						if(cur_sec == NULL){
							if(report != NULL){
								report("Not enough memory");
							}
							free(name);
							goto failed;
						}
						memset(cur_sec, 0, sizeof(iniconfig_section_t));
						iniconfig_list_initialize(&cur_sec->entry_list);

						cur_sec->name = name;

						debug("Adding section \"%s\"\n", cur_sec->name);
						add_section(config, cur_sec);
					}

					debug("The section %s ends at line %d, col %d\n", cur_sec->name, line, col - 1);

					/* pass ']' */
					i ++;
					col ++;
					state = AFTER_SECTION;
				}
				break;

			case AFTER_SECTION:
				if(line_break){
					state = ENTRY;
				}else{
					if(report != NULL){
						snprintf(msg, sizeof(msg), "ERROR, line %d, col %d: unexpected char '%c'\n", line, col, content[i]);
						report(msg);
					}
				}
				break;

			case ENTRY:
				switch(content[i]){
					case '[':
						i ++;
						col ++;
						state = SECTION;
						break;
					default:
						{
							unsigned long start = i;
							unsigned long stop = i;
							char *name = NULL;
							char *value = NULL;
							iniconfig_entry_t *entry = NULL;

							debug("A new entry starts at line %d, col %d\n", line, col);
							while(i < len && is_word_char(content[i])) {
								i ++;
								col ++;
							}
							stop = i;
							if (stop == start){
								if(report != NULL){
									snprintf(msg, sizeof(msg), "ERROR, line %d, col %d: empty entry name", line, col);
									report(msg);
								}
								goto failed;
							}

							name = malloc(stop - start + 1);
							if(name == NULL){
								if(report != NULL){
									report("Not enough memory");
								}
								goto failed;
							}

							memcpy(name, &content[start], stop - start);
							name[stop - start] = '\0';

						   	debug("Entry %s ends at line %d, col %d\n", name, line, col);

							spaces(content, len, &i, &line, &col);

							if(i >= len || content[i] != '='){
								if(report != NULL){
									snprintf(msg, sizeof(msg), "ERROR, line %d, col %d: unexpected char '%c', expecting '='", line, col, content[i]);
									report(msg);
								}
								free(name);
								goto failed;
							}
							i ++;
							col ++;

							spaces(content, len, &i, &line, &col);

							if(i >= len){
								if(report != NULL){
									snprintf(msg, sizeof(msg), "ERROR, line %d, col %d: empty value", line, col);
									report(msg);
								}
								free(name);
								goto failed;
							}

							if(content[i] == '\''){
								i ++;
								value = string_apostrophe(content, len, &i, &line, &col, report);
							}else if(content[i] == '"'){
								i ++;
								value = string_quote(content, len, &i, &line, &col, report);
							}else{/* up to the first space char */
								start = i;
								while(i < len
									  && content[i] != ' '
									  && content[i] != '\t'
									  && content[i] != '\r'
									  && content[i] != '\n') {
									i ++;
									col ++;
								}
								if(i == start){
									if(report != NULL){
										snprintf(msg, sizeof(msg), "ERROR, line %d, col %d: empty value", line, col);
										report(msg);
									}
									free(name);
									goto failed;
								}
								value = malloc(i - start + 1);
								if(value == NULL){
									if(report != NULL){
										report("Not enough memory");
									}
									free(name);
									goto failed;
								}
								memcpy(value, &content[start], i - start);
								value[i - start] = '\0';

							}

							if(value == NULL){ /* error already reported */
								free(name);
								goto failed;
							}


							entry = malloc(sizeof(iniconfig_entry_t));
							if(entry == NULL){
								if(report != NULL){
									report("Not enough memory");
								}
								free(name);
								free(value);
								goto failed;
							}
							memset(entry, 0, sizeof(iniconfig_entry_t));

							entry->name = name;
							entry->value = value;
							
							if(iniconfig_find_entry(cur_sec, name) != NULL){
								if(report != NULL){
									snprintf(msg, sizeof(msg), "WARN, line %d, col %d: duplicated property %s, ignored", line, col, name);
									report(msg);
								}
								free(name);
								free(value);
								free(entry);
							}else{
								debug("Adding entry \"%s\" to sec \"%s\"\n", entry->name, cur_sec->name);
								add_entry(cur_sec, entry);
							}

							state = AFTER_ENTRY;
					}
				}
				break;
			case AFTER_ENTRY:
				if(line_break){
					state = ENTRY;
				}else{
					if(report != NULL){
						snprintf(msg, sizeof(msg), "ERROR, line %d, col %d: unexpected char '%c' at %lu, only comments/spaces are allowed after the value", line, col, content[i], i);
						report(msg);
					}
					/* this entry hasn't been added to the config, so free it manually */
					goto failed;
				}
				break;
			default:
				if(report != NULL){
					report("MUST not be here");
				}
		}
	}
	return config;
failed:
	iniconfig_config_free(config);
	return NULL;
}

iniconfig_section_t *
iniconfig_find_section(const iniconfig_config_t *config, const char * sec_name)
{
	iniconfig_section_t * sec = NULL;
	foreach(config->section_list, sec, iniconfig_section_t){
		if(!strcmp(sec->name, sec_name))
			return sec;
	}

	return NULL;
}

char *
iniconfig_find_entry(const iniconfig_section_t *sec, const char * ent_name)
{
	iniconfig_entry_t * ent = NULL;
	foreach(sec->entry_list, ent, iniconfig_entry_t){
		if(!strcmp(ent->name, ent_name))
			return ent->value;
	}

	return NULL;
}

void iniconfig_config_dump(const iniconfig_config_t *config)
{
	iniconfig_section_t *sec = NULL;
	iniconfig_entry_t *ent = NULL;

	if(config == NULL)
		return;
	foreach(config->section_list, sec, iniconfig_section_t){
		printf("[%s]\n", sec->name);
		foreach(sec->entry_list, ent, iniconfig_entry_t){
			printf("%s = %s\n", ent->name, ent->value);
		}
	}
}
