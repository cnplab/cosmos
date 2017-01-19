/*
 *          cosmos
 *
 *   file: clickos.c
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
 * Authors: Joao Martins <joao.martins@neclab.eu>
 *  	    Filipe Manco <filipe.manco@neclab.eu>
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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xenstore.h>
#include <unistd.h>
#include <sys/stat.h>
#include <poll.h>

#include "clickos.h"

int  domain_ctx_init(int flags);
void domain_ctx_free(void);
int domain_name_to_id(char *name);
int domain_create(struct clickos_domain *dom_info);
int domain_destroy(int domid, int force);
int domain_suspend(const int domid, const char *filename);

static struct xs_handle *xs;
static xs_transaction_t th;
static char *domain_root_path;
static struct xs_permissions default_domain_perms;
static const char clickos_config_fmt[] = "kernel = '%s'\n"
		"vcpus = '1'\n"
		"memory = 16\n"
		"name = '%s'\n"
		"on_start = 'pause'\n"
		"on_crash = 'destroy'\n"
		"extra = '-d  standard'\n\n";

#define XENBUS_ID_NET			"vif"
#define MAX_ENTRY_LENGTH		4096
#define MAX_PATH_LENGTH		 	256
#define MAX_CHUNK_LENGTH		992
#define MAX_ID_LENGTH 			32

int devid = -1;

static void xenstore_init(int domid)
{
	/* Get a connection to the daemon */
	xs = xs_daemon_open();

	if (!xs) {
		perror("error opening xenstore");
		exit(1);
	}

	/* Get the local domain path */
	domain_root_path = xs_get_domain_path(xs, domid);
}

#define xenstore_read(p)  xs_read(xs, XBT_NULL, p, NULL)
#define xenstore_write(p,v)  xs_write(xs, th, p, v, strlen(v))
#define xenstore_chmod(p, f) xs_set_permissions(xs, th, p, f, 1);

char* clickos_read_handler(int domid, char *elem, char *attr)
{
	char *ctrlpath = NULL, *elempath = NULL;
	char *rpath = NULL, *wpath = NULL;
	char *value;
	int err;
	unsigned int len = 0;
	struct pollfd fds[1];

	if (!xs) {
		xenstore_init(domid);
	}

	asprintf(&ctrlpath, "/local/domain/%d/data/clickos/0/control", domid);
	asprintf(&elempath, "/local/domain/%d/data/clickos/0/elements", domid);
	asprintf(&wpath, "%s/read/%s/%s", ctrlpath, elem, attr);
	asprintf(&rpath, "%s/%s/%s", elempath, elem, attr);

	th = xs_transaction_start(xs);
	xenstore_write(wpath, " ");
	xs_transaction_end(xs, th, 0);

	err = xs_watch(xs, rpath, "lock");
	if (!err) {
		printf("Error setting a watch\n");
		return NULL;
	}

	fds[0].fd = xs_fileno(xs);
	fds[0].events = (POLLIN);
	while (len <= 0) {
	   if (poll(fds, 1, 1000) <= 0) {
		   continue;
	   }

retry_wh:
	   th = xs_transaction_start(xs);
	   //value = xenstore_read(rpath);
	   value = xs_read(xs, XBT_NULL, rpath, &len);
	   //printf("read: len %d value %s\n", len, value);
	   if (!xs_transaction_end(xs, th, 0)) {
			if (errno == EAGAIN)
				goto retry_wh;
	   }
	   usleep(5000);
	}

	err = xs_unwatch(xs, rpath, "lock");
	th = xs_transaction_start(xs);
	xs_write(xs, th, rpath, "", 0);
	err = xs_transaction_end(xs, th, 0);
	return value;
}

char* clickos_write_handler(int domid, char *elem, char *attr, char *value)
{
	char *elempath = NULL;
	char *wpath = NULL;

	if (!xs) {
		xenstore_init(domid);
	}

	asprintf(&elempath, "/local/domain/%d/data/clickos/0/elements", domid);
	asprintf(&wpath, "%s/%s/%s", elempath, elem, attr);

	xenstore_write(wpath, value);
	return 0;
}

static unsigned get_file_size(const char * file_name)
{
	struct stat sb;
	if (stat(file_name, &sb) != 0) {
		fprintf (stderr, "'stat' failed for '%s': %s.\n",
				 file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return sb.st_size;
}

char* clickos_read_script(const char *script_path)
{
	int len, bytes_read;
	char *script;
	FILE *f;

	len = get_file_size(script_path);
	script = malloc(len + 1);
	f = fopen(script_path, "r");

	bytes_read = fread(script, sizeof(unsigned char), len, f);
	fclose(f);

	if (bytes_read != len) {
		fprintf(stderr, "(Only) Read %d of %d bytes from %s",
				bytes_read, len, script_path);
		free(script);
		errno = EINVAL;
		return NULL;
	}
	// make sure the string is null terminated
	script[len] = '\0';
	return script;
}

int clickos_start(int domid, const char *name, const char *script)
{
	const char *clickos_config_path_tail = "/config/";
	char clickos_script_chunk[1501];

	char *clickos_root_path = NULL;
	char *clickos_elem_path = NULL;
	char *clickos_ctl_path = NULL;
	char *clickos_config_name_path = NULL;
	char *clickos_config_path = NULL;
	char *clickos_status_path = NULL;
	char *clickos_router_path = NULL;
	int clickos_config_id = 0;
	default_domain_perms.id = domid;
	default_domain_perms.perms = XS_PERM_READ | XS_PERM_WRITE;

	if (!xs) {
		xenstore_init(domid);
	}

retry_clickos:
	// Transaction for ClickOS
	th = xs_transaction_start(xs);

	asprintf(&domain_root_path, "/local/domain/%d", domid);

	do {
		asprintf(&clickos_root_path, "%s/data/clickos/%d", domain_root_path, clickos_config_id);
		clickos_router_path = xenstore_read(clickos_root_path);
		if (clickos_router_path)
			clickos_config_id++;
	} while (clickos_router_path != NULL);

	asprintf(&clickos_elem_path, "%s/elements", clickos_root_path);
	asprintf(&clickos_ctl_path, "%s/control", clickos_root_path);
	asprintf(&clickos_config_name_path, "%s/config_name", clickos_root_path);
	asprintf(&clickos_status_path, "%s/status", clickos_root_path);

	xenstore_write(clickos_elem_path, "");
	xenstore_write(clickos_ctl_path, "");

	xenstore_chmod(clickos_elem_path, &default_domain_perms);
	xenstore_chmod(clickos_ctl_path, &default_domain_perms);

	xenstore_write(clickos_config_name_path, name);

	// we need one character for each chunk
	int config_path_len = strlen(clickos_root_path)
						  + strlen(clickos_config_path_tail) + MAX_ID_LENGTH + 1;
	clickos_config_path = malloc(config_path_len);
	int chunk = 0;
	int scriptSize = strlen(script);
	int remainingScriptSize = scriptSize;
	do {
		snprintf(clickos_config_path, config_path_len, "%s%s%d",
				 clickos_root_path, clickos_config_path_tail, chunk);
		int chunkSize = MAX_CHUNK_LENGTH;
		if (remainingScriptSize < MAX_CHUNK_LENGTH) {
			chunkSize = remainingScriptSize;
		}
		memcpy(clickos_script_chunk, script + (chunk * MAX_CHUNK_LENGTH), chunkSize);
		clickos_script_chunk[chunkSize] = '\0';
		xenstore_write(clickos_config_path, clickos_script_chunk);
		chunk++;
		remainingScriptSize -= chunkSize;
	} while (remainingScriptSize > 0);

	if (!xs_transaction_end(xs, th, 0)) {
		if (errno == EAGAIN)
			goto retry_clickos;
	}

retry_status:
	// Transaction for ClickOS state
	th = xs_transaction_start(xs);

	xenstore_write(clickos_status_path, "Running");

	if (!xs_transaction_end(xs, th, 0)) {
		if (errno == EAGAIN)
			goto retry_status;
	}

	return 0;
}

int clickos_stop(int domid, int configid)
{
	char *statuspath = NULL;

	if (!xs) {
		xenstore_init(domid);
	}

	asprintf(&statuspath, "/local/domain/%d/data/clickos/%d/status", domid, configid);

retry_stop:
	th = xs_transaction_start(xs);
	xenstore_write(statuspath, "Halted");
	if (!xs_transaction_end(xs, th, 0)) {
		if (errno == EAGAIN)
			goto retry_stop;
	}
	return 0;
}

int clickos_global_init(int flags)
{
	return domain_ctx_init(flags);
}

void clickos_global_free(void)
{
	domain_ctx_free();
}

int clickos_domid(char *domname)
{
	uint32_t domid;
	domid = domain_name_to_id(domname);
	return domid <= 0 ? -ENOENT : domid;
}

int clickos_create(const char *config_file)
{
	struct clickos_domain dom_info;
	memset(&dom_info, 0, sizeof(struct clickos_domain));
	dom_info.quiet = 1;
	dom_info.config_file = config_file;
	dom_info.click_file = NULL;
	dom_info.extra_config = "\0";
	dom_info.migrate_fd = -1;

	return domain_create(&dom_info);
}

int clickos_create1(const char *name, const char* kernel_path)
{
	struct clickos_domain dom_info;

	memset(&dom_info, 0, sizeof(struct clickos_domain));
	dom_info.quiet = 1;
	dom_info.config_file = NULL;
	dom_info.click_file = NULL;
	dom_info.extra_config = "\0";
	dom_info.migrate_fd = -1;

	asprintf(&dom_info.config_data, clickos_config_fmt, kernel_path, name);

	return domain_create(&dom_info);
}

#ifndef BINDING_SWIG
int clickos_create2(struct clickos_domain *dom_info)
{
	return domain_create(dom_info);
}
#endif

int clickos_create3(const char *config_file, const char *args)
{
	struct clickos_domain dom_info;

	memset(&dom_info, 0, sizeof(struct clickos_domain));
	dom_info.quiet = 1;
	dom_info.config_file = NULL;
	dom_info.click_file = NULL;
	dom_info.extra_config = "\0";
	dom_info.migrate_fd = -1;

	asprintf(&dom_info.config_data, "%s\nextra = '%s'\n", config_file, args);

	return domain_create(&dom_info);
}

int clickos_destroy(int domid, int force)
{
	return domain_destroy(domid, force);
}

int clickos_suspend(int domid, char* filename)
{
    return domain_suspend(domid, filename);
}

