/*
 *          Xen control light
 *
 *   file: xcl_net.c
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
#include "xcl.h"
#include "xcl_util.h"

static inline int  xs_write_config(char *path, char *key, char *value)
{
	unsigned int c, timeout = (1<<16)-1;
	char *s = value, *config;
	do {
		if (*s == ':')
			c++;
	} while (*(s++));


	if (c == 3) {
		asprintf(&config, "%s:%u:%u", value, timeout, timeout);
	} else if (c == 5) {
		config = value;
	} else {
		fprintf(stderr, "invalid config\n");
		return -EINVAL;
	}

	__xswritek(path, key, config);
	return 0;
}

static void xennet_create_fe(int domid, char *macaddr, int backend_id,
				char *type)
{
	void* nr;
	char *basename_path = NULL;
	char *fe_path = NULL;
	char *be_path = NULL;
	char *fe_state_path = NULL;
	char *backend_id_str = NULL;
	int devid = -1;
	char *devid_str = NULL;
	char *addr = (!macaddr ? "00:00:00:00:00:00" : macaddr);
	struct xs_permissions frontend_perms[2];

	frontend_perms[0].id = domid;
	frontend_perms[0].perms = XS_PERM_NONE;
	frontend_perms[1].id = backend_id;
	frontend_perms[1].perms = XS_PERM_READ;

	asprintf(&basename_path, "/local/domain/%d/device/%s", domid, type);

	do {
		++devid;
		asprintf(&fe_path, "%s/%d", basename_path, devid);
		asprintf(&be_path, "/local/domain/%d/backend/%s/%d/%d",
				backend_id, type, domid, devid);
		asprintf(&fe_state_path, "%s/state", fe_path);

		nr = __xsread(be_path);
	}
	while (nr != 0);

	asprintf(&devid_str, "%d", devid);
retry_frontend:
	// Transaction for the vale ring attach
	t = xs_transaction_start(ctx->xsh);

	xs_mkdir(ctx->xsh, t, fe_path);
	xs_set_permissions(ctx->xsh, t, fe_path, frontend_perms, 2);

	__xswritek(fe_path, "backend", be_path);

	asprintf(&backend_id_str, "%d", backend_id);
	__xswritek(fe_path, "backend-id", backend_id_str);
	__xswritek(fe_path, "state", "1");
	__xswritek(fe_path, "handle", devid_str);
	__xswritek(fe_path, "mac", addr);

	if (!xs_transaction_end(ctx->xsh, t, 0)) {
		if (errno == EAGAIN)
			goto retry_frontend;
	}
}

static void xennet_create_bk(int domid, char *macaddr, int backend_id,
				struct xcl_device_nic *vif)
{
	char *bridge = vif->bridge;
	char *config = vif->config;
	char *basename_path = NULL,
	     *be_path = NULL, *be_state_path = NULL,
	     *fe_path = NULL, *fe_id_str = NULL,
	     *devid_str = NULL;
	int devid = 0;
	char *addr = (macaddr == NULL ? "00:00:00:00:00:00" : macaddr);
	struct xs_permissions backend_perms[2];

	backend_perms[0].id = backend_id;
	backend_perms[0].perms = XS_PERM_NONE;
	backend_perms[1].id = domid;
	backend_perms[1].perms = XS_PERM_READ;

	asprintf(&basename_path, "/local/domain/%d/backend/%s/%d",
			backend_id, vif->type, domid);
	asprintf(&be_path, "%s/%d", basename_path, devid);
	asprintf(&be_state_path, "%s/state", be_path);
	asprintf(&fe_path, "/local/domain/%d/device/%s/%d", domid,
			vif->type, devid);
	asprintf(&fe_id_str, "%d", domid);
	asprintf(&devid_str, "%d", devid);

retry_backend:
	// Transaction for the vale ring attach
	t = xs_transaction_start(ctx->xsh);
	__xswrite(basename_path, "");
	xs_set_permissions(ctx->xsh, t, basename_path, backend_perms, 2);

	__xswritek(be_path, "frontend", fe_path);
	__xswritek(be_path, "frontend-id", fe_id_str);
	__xswritek(be_path, "online", "1");

	if (config) {
		xs_write_config(be_path, "config", config);
	}

	if (bridge) {
		__xswritek(be_path, "bridge", bridge);
	}

	if (!strcmp(vif->type,"vif")) {
		__xswritek(be_path, "script", vif->script);
		__xswritek(be_path, "type", vif->type);
		__xswritek(be_path, "hotplug-status", "");
	}

	__xswritek(be_path, "handle", devid_str);
	__xswritek(be_path, "mac", addr);

	// Backend reacts here
	__xswrite(be_state_path, "1");

	if (!xs_transaction_end(ctx->xsh, t, 0)) {
		if (errno == EAGAIN)
			goto retry_backend;
	}
}

static void do_hotplug(const char *arg0, char *const args[], char *const env[])
{
	int i;
        for (i = 0; env[i] != NULL && env[i+1] != NULL; i += 2) {
            if (setenv(env[i], env[i+1], 1) < 0) {
                LOGE("setting env vars (%s = %s)",
                                    env[i], env[i+1]);
                goto out;
            }
        }

	xcl_ctx_dispose();
	execvp(arg0, args);
out:
	LOGE("cannot call hotplug script %s: %s\n", arg0, strerror(errno));
	exit(-1);
}

#define WATCHDOG_WAIT_TIME 	5000
static void xennet_watchdog(char *path, const char *expected)
{
	int watchdog = 0;
	while (watchdog < 100) {
		char *buf = __xsread(path);
		if (buf == NULL) {
			if (errno == ENOENT)
				LOGE("watchdog error: %s", strerror(errno));
		}

		if (!strcmp(buf, expected)) {
			NLOG("hotplug connected");
			break;
		} else {
			usleep(WATCHDOG_WAIT_TIME);
			watchdog++;
		}
	}
}

static void xennet_hotplug(int domid, char *script, char *type, int online)
{
	char *dev_path, *state_path, *vif_name;
	const char *state_connected = online ? "connected" : "6";
	char **args = malloc(4*sizeof(char *));
	char **env = malloc(15*sizeof(char *));
	pid_t pid;
	int nr = 0, argnr = 0;

	asprintf(&dev_path, "backend/%s/%u/%d", type, domid, 0);
	asprintf(&state_path, online ? "%s/hotplug-status" : "%s/state", dev_path);
	asprintf(&vif_name, "vif%u.%u", domid, 0);

	args[argnr++] = script;
	args[argnr++] = online ? "online" : "offline";
	args[argnr++] = "type_if=vif";
	args[argnr++] = NULL;

	env[nr++] = "script";
	env[nr++] = script;
	env[nr++] = "XENBUS_TYPE";
	env[nr++] = type;
	env[nr++] = "XENBUS_PATH";
	env[nr++] = dev_path;
	env[nr++] = "XENBUS_BASE_PATH";
	env[nr++] = "backend";
	env[nr++] = "netdev";
	env[nr++] = "";
	env[nr++] = "vif";
	env[nr++] = vif_name;
	env[nr++] = NULL;

	pid = fork();

	if (pid == -1) {
		LOGE("cannot fork: %s", strerror(errno));
		exit(0);
	}

	if (!pid) { /* child */
		do_hotplug(args[0], args, env);
	}

	xennet_watchdog(state_path, state_connected);
}

void xcl_net_attach(int domid, int nr_nics, struct xcl_device_nic *nics)
{
	int i;

	for (i = 0; i < nr_nics; ++i) {
		struct xcl_device_nic *nic = &nics[i];

		xennet_create_fe(domid, nic->mac, nic->backend_domid,
			nic->type);
		xennet_create_bk(domid, nic->mac, nic->backend_domid,
			nic);
		xennet_hotplug(domid, nic->script, nic->type, 1);
	}
}

void xcl_net_detach(int domid)
{
	char *dom_path, *dom_vm_path, *vm_path;

	dom_path = xs_get_domain_path(ctx->xsh, domid);
	asprintf(&dom_vm_path, "%s/vm", dom_path);
	vm_path = __xsread(dom_vm_path);

retry_detach:
	t = xs_transaction_start(ctx->xsh);
	xs_rm(ctx->xsh, t, dom_path);
	xs_rm(ctx->xsh, t, vm_path);

	if (!xs_transaction_end(ctx->xsh, t, 0)) {
		if (errno == EAGAIN) {
			goto retry_detach;
		}
	}
}
