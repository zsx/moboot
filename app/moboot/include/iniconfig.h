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

#ifndef INI_CONFIG_H_
#define INI_CONFIG_H_

#include "iniconfig_list.h"

typedef struct{
	list_node_t node; /* list_node_t must be the first field */
	char 		*name;
	char 		*value;
} iniconfig_entry_t;

typedef struct{
	list_node_t node;
	list_t 		entry_list;
	unsigned 	n_ent; /* how many entries */
	char 		*name;
} iniconfig_section_t;

typedef struct {
	list_t		section_list;
	unsigned 	n_sec; /* how many sections */
}iniconfig_config_t;

typedef void (*iniconfig_error_report_func_t)(const char *msg);

/* parse the @content as .ini file
 *
 * returns a malloc'ed iniconfig_config_t, it must be free'ed by calling iniconfig_config_free;
 * any error will cause it return NULL and the error be reported by @report if it's not NULL
 */
iniconfig_config_t *iniconfig_config_new(const char content[], unsigned long len,
										  iniconfig_error_report_func_t report);

/* returns the section of the @section if found or NULL */
iniconfig_section_t *iniconfig_find_section(const iniconfig_config_t *config, const char *section);

/* returns the value of the @entry if found or NULL */
char *iniconfig_find_entry(const iniconfig_section_t *section, const char *entry);

void iniconfig_config_free(iniconfig_config_t *config);

void iniconfig_config_dump(const iniconfig_config_t *config);
#endif //INI_CONFIG_H_
