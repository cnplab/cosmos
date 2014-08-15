/*
 *          cosmos
 *
 *   file: domain_xcl.c
 *
 *          NEC Europe Ltd. PROPRIETARY INFORMATION
 *
 * This software is supplied under the terms of a license agreement
 * or nondisclosure agreement with NEC Europe Ltd. and may not be
 * copied or disclosed except in accordance with the terms of that
 * agreement. The software and its source code contain valuable trade
 * secrets and confidential information which have to be maintained in
 * confidence.
 * Any unauthorized publication, transfer to third parties or duplication
 * of the object or source code - either totally or in part – is
 * prohibited.
 *
 *      Copyright (c) 2014 NEC Europe Ltd. All Rights Reserved.
 *
 * Authors: Joao Martins <Joao.Martins@neclab.eu>
 *
 * NEC Europe Ltd. DISCLAIMS ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE AND THE WARRANTY AGAINST LATENT
 * DEFECTS, WITH RESPECT TO THE PROGRAM AND THE ACCOMPANYING
 * DOCUMENTATION.
 *
 * No Liability For Consequential Damages IN NO EVENT SHALL NEC Europe
 * Ltd., NEC Corporation OR ANY OF ITS SUBSIDIARIES BE LIABLE FOR ANY
 * DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS
 * OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF INFORMATION, OR
 * OTHER PECUNIARY LOSS AND INDIRECT, CONSEQUENTIAL, INCIDENTAL,
 * ECONOMIC OR PUNITIVE DAMAGES) ARISING OUT OF THE USE OF OR INABILITY
 * TO USE THIS PROGRAM, EVEN IF NEC Europe Ltd. HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 *     THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */
#include <stdlib.h>
#include <ctype.h>
#include <xcl/xcl.h>

#include "clickos.h"

static char* lstrip(const char* s)
{
	while (*s && isspace((unsigned char)(*s)))
		s++;
	return (char*)s;
}

static char* rstrip(char *s)
{
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)(*--end)))
		*end = '\0';

	return (char *) s;
}

#define strip(s)	lstrip(rstrip(s))

static char* find_attr(const char *s)
{
    char *attr = strchr(s, '=');
    return attr ? attr : (char *) s;
}

static int parse_config_item(struct xcl_dominfo **_info, const char *name, 
		char *value, int devid)
{
	struct xcl_dominfo *info = *_info;
	struct xcl_device_nic *nic = &info->state->nics[devid];
	char *p, *p2;

	memset(nic, 0, sizeof(struct xcl_device_nic));

	if (!strcmp(name, "vif")) {
		p = strtok(value, ",");
		if (!p) {
			return -EINVAL;
		}
		do {
			while (*p == ' ')
				p++;
			if ((p2 = strchr(p, '=')) == NULL)
				break;

			*p2 = '\0';
			if (!strcmp(p, "mac")) {
				nic->mac = strndup(p2 + 1, 17);
			} else if (!strcmp(p, "bridge")) {
				nic->bridge = strdup(p2 + 1);
			} else if (!strcmp(p, "script")) {
				nic->script = strdup(p2 + 1);
			} else if (!strcmp(p, "backend")) {
				nic->backend_domid = 0;
			}

		} while ((p = strtok(NULL, ",")) != NULL);

		if (!nic->script)
			nic->script = "/etc/xen/scripts/vif-bridge";

		if (!nic->type)
			nic->type = "vif";
	}
	return 0;
}

#define is_sep(str, i) \
	(str[i] == ',') && \
	(str[i-1] == '\'') && \
	(str[i+1] == '\'')

#define XCL_MAX_DEVICES		4
	
static void parse_config_list(struct xcl_dominfo **_info, const char *name, 
		char *val)
{
	struct xcl_dominfo *info = *_info;
	int i;
	int is_item = 0, item_parsed = 0;
	int nr_items = 0;
	char *token, *token_end, *buf;
	char *end = strchr(++val, ']');
	*end = '\0';
	
	if (!strcmp(name, "vif"))
		info->state->nics = malloc(XCL_MAX_DEVICES * 
				sizeof(struct xcl_device_nic*));
	
	for (i=0 ; val[i] != 0 ;++i) {
		if (is_sep(val, i)) {
			continue;
		}
		
		if (val[i] == '\'') {
			if (is_item) {
				nr_items++;
			}
			is_item = !is_item;
			item_parsed = 0;
			continue;
		}
	
		if (is_item && !item_parsed) {
			item_parsed = 1;
			token = strdup(&val[i]);
			token_end = strchr(token, '\'');
			*token_end = '\0';
			buf = malloc(strlen(token));
			strncpy(buf, token, token_end - token);
			i += strlen(buf) - 2;
			parse_config_item(_info, name, buf, nr_items);
		}
	}
	
	info->state->nr_nics = nr_items;
}

static void parse_config_value(struct xcl_dominfo **dinfo, const char **click_file,
		const char *name, char *value)
{
	struct xcl_dominfo *info = *dinfo;
	int len = strlen(value);
	char *val = value;

	if (val[0] == '\'') {
		val++;
		val[len-2] = '\0';
		len--;
	}	

	if (!strcmp(name, "pvh"))
		info->pvh = atoi(val) != 0 ? true : false;

	if (!strcmp(name, "cpus"))
		info->pcpu = atoi(val);
	
	if (!strcmp(name, "vcpus")) 
		info->max_vcpus = atoi(val);
	
	if (!strcmp(name, "name")) {
		info->name = malloc(sizeof(char) * len);
		info->name[len] = '\0';
		strncpy(info->name, val, len);
	}
	
	if (!strcmp(name, "memory")) {
		info->target_memkb = atoi(val) * 1024;
		info->max_memkb = info->target_memkb * 2;
	}

	if (!strcmp(name, "kernel")) {
		info->state->kernel_path = malloc(sizeof(char) * len);
		info->state->kernel_path[len] = '\0';
		strncpy(info->state->kernel_path, val, len);
	}
	
	if (!strcmp(name, "click")) {
		char *click_cfg;
		click_cfg = malloc(sizeof(char) * len);
		click_cfg[len] = '\0';
		strncpy(click_cfg, val, len);
		*click_file = click_cfg;
	}
}

#define MAX_LINE_LEN	 256
static void parse_config(const char *config, struct xcl_dominfo *info, 
		const char **click_file)
{
	FILE *file;
	char *line = (char*) malloc(MAX_LINE_LEN);
	char *buf, *cont;
	
	file = fopen(config, "r");
	if (!file) {
		errno = ENOENT;
		return;
	}
	
	while (fgets(line, MAX_LINE_LEN, file) != NULL) {
        	buf = lstrip(rstrip(line));
        	if (*buf == '#') {
			continue;
		}
		
		cont = find_attr(buf);
		if (*cont == '=') {
			char *name, *value;
			*cont = '\0';
			name = rstrip(buf);
			value = lstrip(cont + 1);	
			if (value[0] == '[')
				parse_config_list(&info, name, value);
			else
				parse_config_value(&info, click_file, name, value);
		}

	}
	fclose(file);
}

int domain_name_to_id(char *name)
{
	return xcl_dom_id(name);
}

int domain_ctx_init(int flags)
{
	xcl_ctx_init();
	return 0;
}

void domain_ctx_free(void)
{
	xcl_ctx_dispose();
}

int domain_create(struct clickos_domain *dom_info)
{
	struct xcl_dominfo info;
	struct xcl_domstate state;
	
	memset(&info, 0, sizeof(struct xcl_dominfo));
	memset(&state, 0, sizeof(struct xcl_domstate));
	
	info.state = &state;
	parse_config(dom_info->config_file, &info, &dom_info->click_file);

	xcl_dom_create(&info);

	if (dom_info->click_file) {
		clickos_start(info.domid, dom_info->click_file, 
			clickos_read_script(dom_info->click_file));
	}

	return 0;
}

int domain_suspend(const int domid, const char *filename)
{
	return -ENOTSUP;
}

int domain_destroy(int domid, int force)
{
	if (domid <= 0) {
		fprintf(stderr, "domain_xcl: please supply a domain id\n");
		return -EINVAL;
	} 
	
	if (force)
		xcl_dom_destroy(domid);
	else
		xcl_dom_shutdown(domid);
	return 0;
}
