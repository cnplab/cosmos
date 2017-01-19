/*
 *          Xen control light
 *
 *   file: xcl_dom.c
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
#include <xcl/xcl.h>
#include <xcl_util.h>

#include <xc_dom.h>

#define XCL_MAXMEM	1024
static xentoollog_level minmsglevel = XTL_PROGRESS;
static xentoollog_logger_stdiostream *lg;
struct xcl_ctx *ctx;
xs_transaction_t t;

static void __xcl_dom_pin(struct xcl_dominfo *info)
{
	int max_cpus, l;
	xc_cpumap_t map;

	max_cpus = xc_get_max_cpus(ctx->xch);

	/* Set 'any cpu' on invalid option */
	if (info->pcpu > max_cpus)
		info->pcpu = 0;

	l = (max_cpus + 7)/8;

	map = malloc(l * sizeof(*map));
	memset(map, 0, l*sizeof(*map));

	map[info->pcpu / 8] |= 1 << (info->pcpu % 8);

	xc_vcpu_setaffinity(ctx->xch, info->domid, 0, map);

	free(map);
}

void xcl_ctx_init()
{
	xentoollog_logger *l;
	if (!lg)
		lg = xtl_createlogger_stdiostream(stderr, minmsglevel,  0);

	l = (xentoollog_logger*) lg;
	ctx = malloc(sizeof(struct xcl_ctx));
	memset(ctx, 0, sizeof(struct xcl_ctx));

	ctx->xch = xc_interface_open(l,l,0);
	if (!ctx->xch)
		LOGE("cannot open libxc handle");

	ctx->xsh = xs_daemon_open();
	if (!ctx->xsh)
			ctx->xsh = xs_domain_open();
	if (!ctx->xsh) {
		LOGE("cannot connect to xenstore");
		exit(1);
	}
}

void xcl_ctx_dispose()
{
	if (ctx->xch)
		xc_interface_close(ctx->xch);

	if (ctx->xsh)
		xs_daemon_close(ctx->xsh);

	if (lg) {
		xtl_logger_destroy((xentoollog_logger*)lg);
		lg = NULL;
	}

}

void xcl_pre(struct xcl_dominfo *info, struct xcl_domstate *state)
{
	char *dom_path, *vm_path;
	char *domid_val;
	struct xs_permissions roperm[2];
	struct xs_permissions rwperm[1];

	roperm[0].id = 0;
	roperm[0].perms = XS_PERM_NONE;
	roperm[1].id = info->domid;
	roperm[1].perms = XS_PERM_READ;

	rwperm[0].id = info->domid;
	rwperm[0].perms = XS_PERM_NONE;

retry_transaction:
	t = xs_transaction_start(ctx->xsh);

	dom_path = xs_get_domain_path(ctx->xsh, info->domid);

	{
	char *control_path, *shutdown_path, *clickos_path, *clickos_shutdown_path;
	asprintf(&clickos_path, "%s/data/clickos", dom_path);
	asprintf(&control_path, "%s/control", dom_path);
	asprintf(&shutdown_path, "%s/control/shutdown", dom_path);

	xs_mkdir(ctx->xsh, t, shutdown_path);
	xs_set_permissions(ctx->xsh, t,  shutdown_path, rwperm, 1);
	xs_set_permissions(ctx->xsh, t, control_path, roperm, 2);

	asprintf(&clickos_shutdown_path, "%s/data/clickos/shutdown", dom_path);
	xs_mkdir(ctx->xsh, t, clickos_shutdown_path);
	xs_set_permissions(ctx->xsh, t,  clickos_shutdown_path, rwperm, 1);
	xs_set_permissions(ctx->xsh, t,  clickos_path, rwperm, 1);
	}


	asprintf(&vm_path, "/vm/%s", (char *) info->uuid_string);
	__xswritek(dom_path, "vm", vm_path);

		__xswritek(vm_path, "uuid", info->uuid_string);
	__xswritek(vm_path, "name", info->name);
	xs_set_permissions(ctx->xsh, t, vm_path, roperm, 2);

	__xswritek(dom_path, "name", info->name);
	xs_set_permissions(ctx->xsh, t, dom_path, roperm, 2);

	asprintf(&domid_val, "%u", info->domid);
	__xswritek(dom_path, "domid", domid_val);

	if (!xs_transaction_end(ctx->xsh, t, 0)) {
		if (errno == EAGAIN) {
			goto retry_transaction;
		}
	}
}

void xcl_post(struct xcl_dominfo *info, struct xcl_domstate *state)
{
	struct xs_permissions rwperm[1];
	char *dom_path, *console_path;
	char *port_val, *mfn_val;

	rwperm[0].id = info->domid;
	rwperm[0].perms = XS_PERM_NONE;

	dom_path = xs_get_domain_path(ctx->xsh, info->domid);

retry_console:
	t = xs_transaction_start(ctx->xsh);

	asprintf(&console_path, "%s/console", dom_path);
	xs_mkdir(ctx->xsh, t, console_path);
	xs_set_permissions(ctx->xsh, t, console_path, rwperm, 1);

	asprintf(&port_val, "%u", state->console_port);
	__xswritek(console_path, "port", port_val);
	asprintf(&mfn_val, "%lu", state->console_mfn);
	__xswritek(console_path, "ring-ref", mfn_val);

	if (!xs_transaction_end(ctx->xsh, t, 0)) {
		if (errno == EAGAIN) {
			goto retry_console;
		}
	}

	if (state->nr_nics) {
		xcl_net_attach(info->domid, state->nr_nics, state->nics);
	}
}

#define TSC_MODE_NATIVE		 2
#define TSC_MODE_NATIVE_PARAVIRT 3

void xcl_dom_create(struct xcl_dominfo *info)
{
	struct xcl_domstate *state = info->state;
	struct xc_dom_image *dom;
	int ret;
	int flags = 0, tsc_mode = TSC_MODE_NATIVE_PARAVIRT;
	uint32_t domid = -1;

	xen_domain_handle_t handle;

	uuid_generate(info->uuid);
	info->uuid_string = uuid_to_str(info->uuid);

	if (info->pvh) {
#if XEN_VERSION >= 40400
		flags |= XEN_DOMCTL_CDF_pvh_guest;
		flags |= XEN_DOMCTL_CDF_hap;
		tsc_mode = TSC_MODE_NATIVE;
		info->slack_memkb = 0;
#else
		LOG("PVH mode supported only for Xen >= 4.4.0");
		info->pvh = 0;
#endif
	}


	ret = xc_domain_create(ctx->xch, 0, handle, flags, &domid);
	info->domid = domid;

	ret = xc_cpupool_movedomain(ctx->xch, 0, domid);
	xc_domain_max_vcpus(ctx->xch, domid, info->max_vcpus);
	__xcl_dom_pin(info);

	/* pre */
	xcl_pre(info, state);

	xc_domain_setmaxmem(ctx->xch, domid, info->target_memkb + XCL_MAXMEM);
	state->store_port = xc_evtchn_alloc_unbound(ctx->xch, domid, state->store_domid);
	state->console_port = xc_evtchn_alloc_unbound(ctx->xch, domid, state->console_domid);
	xc_domain_set_memmap_limit(ctx->xch, domid, (info->max_memkb + info->slack_memkb));
	xc_domain_set_tsc_info(ctx->xch, domid, tsc_mode, 0, 0, 0);
	if (info->pvh) {
		info->features = "writable_descriptor_tables|"
				   "auto_translated_physmap|"
				   "supervisor_mode_kernel|"
				   "hvm_callback_vector";
	}

	/* build */
	xc_dom_loginit(ctx->xch);

	dom = (struct xc_dom_image*) xc_dom_allocate(ctx->xch, state->cmdline, info->features);
	dom->flags = flags;
	dom->console_evtchn = state->console_port;
	dom->console_domid = state->console_domid;
	dom->xenstore_evtchn = state->store_port;
	dom->xenstore_domid = state->store_domid;
#if XEN_VERSION >= 40400
	dom->pvh_enabled = info->pvh;
#endif

	/* We can also map the image size - could be useful to
	 * avoid always reading from a filei (e.g using xxd)
	 */
	//ret = xc_dom_kernel_mem(dom, state->pv_kernel.data, state->pv_kernel.size);
	ret = xc_dom_kernel_file(dom, state->kernel_path);
	ret = xc_dom_boot_xen_init(dom, ctx->xch, domid);
	ret = xc_dom_parse_image(dom);
	ret = xc_dom_mem_init(dom, info->target_memkb / 1024);
	ret = xc_dom_boot_mem_init(dom);
	ret = xc_dom_build_image(dom);
	ret = xc_dom_boot_image(dom);
	ret = xc_dom_gnttab_init(dom);

	if (info->pvh) {
		state->console_mfn = dom->console_pfn;
		state->store_mfn = dom->xenstore_pfn;
	} else {
		state->console_mfn = xc_dom_p2m_host(dom, dom->console_pfn);
		state->store_mfn = xc_dom_p2m_host(dom, dom->xenstore_pfn);
	}

	xc_dom_release(dom);

	/* post */
	xcl_post(info, state);
	xs_introduce_domain(ctx->xsh, domid, state->store_mfn, state->store_port);

	ret = xc_domain_unpause(ctx->xch, domid);
	(void) (ret);
}

#define INFOLIST_MAX_DOMAINS 1024
int xcl_dom_id(char *name)
{
	int sz = INFOLIST_MAX_DOMAINS;
		xc_domaininfo_t info[sz];
		char path[strlen("/local/domain") + 20];
	void *s;
	int i, rc;
	rc = xc_domain_getinfolist(ctx->xch, 1, sz, info);

	rc = -ENOENT;
	for(i=0; i<sz; ++i)
	{
		if (info[i].domain <= 0)
			continue;

		snprintf(path, sizeof(path), "/local/domain/%d/name", info[i].domain);
		s = __xsread(path);
		if (!strcmp(s, name)) {
			rc = info[i].domain;
			break;
		}
	}

	return rc;
}

#define SHUTDOWN_poweroff	0
void xcl_dom_shutdown(int domid)
{
	char *dom_path;
	char *shutdown_path;
		xc_domaininfo_t info;
	bool shutdown = false;
	int retries = 100;

retry_shutdown:
	t = xs_transaction_start(ctx->xsh);
	dom_path = xs_get_domain_path(ctx->xsh, domid);
	asprintf(&shutdown_path, "%s/control", dom_path);
	__xswritek(shutdown_path, "shutdown", "poweroff");
	if (!xs_transaction_end(ctx->xsh, t, 0)) {
		if (errno == EAGAIN)
			goto retry_shutdown;
	}

	do {
		xc_domain_getinfolist(ctx->xch, domid, 1, &info);
		shutdown = !!(info.flags & XEN_DOMINF_dying) ||
			!!(info.flags & XEN_DOMINF_shutdown);
		usleep(5E3);
		--retries;
	} while (!shutdown && retries);

	if (shutdown)
		xcl_dom_destroy(domid);
}

void xcl_dom_destroy(int domid)
{
	int rc, nr_devs;
	char *dom_path, *dom_vm_path, *vm_path;

	if (xc_domain_pause(ctx->xch, domid) < 0)
		LOG("error unpausing domain %d", domid);

	nr_devs = xcl_net_nrdevs(domid);
	for(int i = 0; i < nr_devs; ++i) {
		xcl_net_detach(domid, i);
	}

	if (xc_domain_destroy(ctx->xch, domid) < 0)
		LOG("error destroying domain %d", domid);

	dom_path = xs_get_domain_path(ctx->xsh, domid);
	asprintf(&dom_vm_path, "%s/vm", dom_path);
	vm_path = __xsread(dom_vm_path);

	do {
		t = xs_transaction_start(ctx->xsh);
		xs_rm(ctx->xsh, t, dom_path);
		xs_rm(ctx->xsh, t, vm_path);
		rc = xs_transaction_end(ctx->xsh, t, 0);
	} while (!rc && errno == EAGAIN);

	xs_release_domain(ctx->xsh, domid);
}
