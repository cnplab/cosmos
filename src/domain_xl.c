/*
 * Domain creation based on xl toolstack
 * Copyright (c) 2014 NEC Europe Ltd., NEC Corporation
 * Author Joao Martins <joao.martins@neclab.eu>
 * Author Filipe Manco <filipe.manco@neclab.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 only. with the special
 * exception on linking described in file LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */
#ifndef CLICKOS_DOMAIN_H
#define CLICKOS_DOMAIN_H

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/utsname.h> /* for utsname in xl info */
#include <xentoollog.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>

#include <libxl.h>
#include <libxl_utils.h>
#include <libxlutil.h>

#include "clickos.h"

#define CHK_ERRNO( call ) ({											\
		int chk_errno = (call);										 \
		if (chk_errno < 0) {												\
			fprintf(stderr,"xl: fatal error: %s:%d: %s: %s\n",		  \
					__FILE__,__LINE__, strerror(chk_errno), #call);	 \
			exit(-ERROR_FAIL);										  \
		}															   \
	})

#define MUST( call ) ({												 \
		int must_rc = (call);										   \
		if (must_rc < 0) {												  \
			fprintf(stderr,"xl: fatal error: %s:%d, rc=%d: %s\n",	   \
					__FILE__,__LINE__, must_rc, #call);				 \
			exit(-must_rc);											 \
		}															   \
	})

xentoollog_logger_stdiostream *logger;
xentoollog_level minmsglevel = XTL_PROGRESS;
/* every libxl action in xl uses this same libxl context */
libxl_ctx *ctx;
char *lockfile;

int autoballoon = 0;
int run_hotplug_scripts = 1;
int logfile = 2;
char *lockfile;
char *default_vifscript = NULL;
char *default_bridge = "xenbr0";
char *blkdev_start;

/* when we operate on a domain, it is this one: */
/* when we operate on a domain, it is this one: */
#define INVALID_DOMID ~0
static uint32_t domid = INVALID_DOMID;
static const char *common_domname;
static int fd_lock = -1;

/* Stash for specific vcpu to pcpu mappping */
static int *vcpu_to_pcpu;

static const char savefileheader_magic[32]=
	"Xen saved domain, xl format\n \0 \r";

static const char migrate_receiver_banner[]=
	"xl migration receiver ready, send binary domain data.\n";
static const char migrate_receiver_ready[]=
	"domain received, ready to unpause";
static const char migrate_permission_to_go[]=
	"domain is yours, you are cleared to unpause";
static const char migrate_report[]=
	"my copy unpause results are as follows";
  /* followed by one byte:
   *	 0: everything went well, domain is running
   *			next thing is we all exit
   * non-0: things went badly
   *			next thing should be a migrate_permission_to_go
   *			from target to source
   */

struct save_file_header {
	char magic[32]; /* savefileheader_magic */
	/* All uint32_ts are in domain's byte order. */
	uint32_t byteorder; /* SAVEFILE_BYTEORDER_VALUE */
	uint32_t mandatory_flags; /* unknown flags => reject restore */
	uint32_t optional_flags; /* unknown flags => reject restore */
	uint32_t optional_data_len; /* skip, or skip tail, if not understood */
};

typedef struct {
	char *click_file;
} clickos_extra_config;

static const char *action_on_shutdown_names[] = {
	[LIBXL_ACTION_ON_SHUTDOWN_DESTROY] = "destroy",

	[LIBXL_ACTION_ON_SHUTDOWN_RESTART] = "restart",
	[LIBXL_ACTION_ON_SHUTDOWN_RESTART_RENAME] = "rename-restart",

	[LIBXL_ACTION_ON_SHUTDOWN_PRESERVE] = "preserve",

	[LIBXL_ACTION_ON_SHUTDOWN_COREDUMP_DESTROY] = "coredump-destroy",
	[LIBXL_ACTION_ON_SHUTDOWN_COREDUMP_RESTART] = "coredump-restart",
};

/* Optional data, in order:
 *   4 bytes uint32_t  config file size
 *   n bytes		   config file in Unix text file format
 */

#define SAVEFILE_BYTEORDER_VALUE ((uint32_t)0x01020304UL)

static int qualifier_to_id(const char *p, uint32_t *id_r)
{
	int i, alldigit;

	alldigit = 1;
	for (i = 0; p[i]; i++) {
		if (!isdigit((uint8_t)p[i])) {
			alldigit = 0;
			break;
		}
	}

	if (i > 0 && alldigit) {
		*id_r = strtoul(p, NULL, 10);
		return 0;
	} else {
		/* check here if it's a uuid and do proper conversion */
	}
	return 1;
}

static int cpupool_qualifier_to_cpupoolid(const char *p, uint32_t *poolid_r,
									 int *was_name_r)
{
	int was_name;

	was_name = qualifier_to_id(p, poolid_r);
	if (was_name_r) *was_name_r = was_name;
	return was_name ? libxl_name_to_cpupoolid(ctx, p, poolid_r) : 0;
}

static int acquire_lock(void)
{
	int rc;
	struct flock fl;

	/* lock already acquired */
	if (fd_lock >= 0)
		return ERROR_INVAL;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fd_lock = open(lockfile, O_WRONLY|O_CREAT, S_IWUSR);
	if (fd_lock < 0) {
		fprintf(stderr, "cannot open the lockfile %s errno=%d\n", lockfile, errno);
		return ERROR_FAIL;
	}
	if (fcntl(fd_lock, F_SETFD, FD_CLOEXEC) < 0) {
		close(fd_lock);
		fprintf(stderr, "cannot set cloexec to lockfile %s errno=%d\n", lockfile, errno);
		return ERROR_FAIL;
	}
get_lock:
	rc = fcntl(fd_lock, F_SETLKW, &fl);
	if (rc < 0 && errno == EINTR)
		goto get_lock;
	if (rc < 0) {
		fprintf(stderr, "cannot acquire lock %s errno=%d\n", lockfile, errno);
		rc = ERROR_FAIL;
	} else
		rc = 0;
	return rc;
}

static int release_lock(void)
{
	int rc;
	struct flock fl;

	/* lock not acquired */
	if (fd_lock < 0)
		return ERROR_INVAL;

release_lock:
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	rc = fcntl(fd_lock, F_SETLKW, &fl);
	if (rc < 0 && errno == EINTR)
		goto release_lock;
	if (rc < 0) {
		fprintf(stderr, "cannot release lock %s, errno=%d\n", lockfile, errno);
		rc = ERROR_FAIL;
	} else
		rc = 0;
	close(fd_lock);
	fd_lock = -1;

	return rc;
}

static void *xmalloc(size_t sz) {
	void *r;
	r = malloc(sz);
	if (!r) { fprintf(stderr,"xl: Unable to malloc %lu bytes.\n",
					  (unsigned long)sz); exit(-ERROR_FAIL); }
	return r;
}

static int parse_action_on_shutdown(const char *buf, libxl_action_on_shutdown *a)
{
	int i;
	const char *n;

	for (i = 0; i < sizeof(action_on_shutdown_names) / sizeof(action_on_shutdown_names[0]); i++) {
		n = action_on_shutdown_names[i];

		if (!n) continue;

		if (strcmp(buf, n) == 0) {
			*a = i;
			return 1;
		}
	}
	return 0;
}

static void parse_disk_config_multistring(XLU_Config **config,
										  int nspecs, const char *const *specs,
										  libxl_device_disk *disk)
{
	int e;

	libxl_device_disk_init(disk);

	if (!*config) {
		*config = xlu_cfg_init(stderr, "command line");
		if (!*config) { perror("xlu_cfg_init"); exit(-1); }
	}

	e = xlu_disk_parse(*config, nspecs, specs, disk);
	if (e == EINVAL) exit(-1);
	if (e) {
		fprintf(stderr,"xlu_disk_parse failed: %s\n",strerror(errno));
		exit(-1);
	}
}

static void parse_disk_config(XLU_Config **config, const char *spec,
							  libxl_device_disk *disk)
{
	parse_disk_config_multistring(config, 1, &spec, disk);
}

static void parse_vif_rate(XLU_Config **config, const char *rate,
						   libxl_device_nic *nic)
{
	int e;

	e = xlu_vif_parse_rate(*config, rate, nic);
	if (e == EINVAL || e == EOVERFLOW) exit(-1);
	if (e) {
		fprintf(stderr,"xlu_vif_parse_rate failed: %s\n",strerror(errno));
		exit(-1);
	}
}

static void split_string_into_string_list(const char *str,
										  const char *delim,
										  libxl_string_list *psl)
{
	char *s, *saveptr;
	const char *p;
	libxl_string_list sl;

	int i = 0, nr = 0;

	s = strdup(str);
	if (s == NULL) {
		fprintf(stderr, "unable to allocate memory to parse bootloader args\n");
		exit(-1);
	}

	/* Count number of entries */
	p = strtok_r(s, delim, &saveptr);
	do {
		nr++;
	} while ((p = strtok_r(NULL, delim, &saveptr)));

	free(s);

	s = strdup(str);

	sl = malloc((nr+1) * sizeof (char *));
	if (sl == NULL) {
		fprintf(stderr, "unable to allocate memory for bootloader args\n");
		exit(-1);
	}

	p = strtok_r(s, delim, &saveptr);
	do {
		assert(i < nr);
		sl[i] = strdup(p);
		i++;
	} while ((p = strtok_r(NULL, delim, &saveptr)));
	sl[i] = NULL;

	*psl = sl;

	free(s);
}

static int vcpupin_parse(char *cpu, libxl_bitmap *cpumap)
{
	libxl_bitmap exclude_cpumap;
	uint32_t cpuida, cpuidb;
	char *endptr, *toka, *tokb, *saveptr = NULL;
	int i, rc = 0, rmcpu;

	if (!strcmp(cpu, "all")) {
		libxl_bitmap_set_any(cpumap);
		return 0;
	}

	if (libxl_cpu_bitmap_alloc(ctx, &exclude_cpumap, 0)) {
		fprintf(stderr, "Error: Failed to allocate cpumap.\n");
		return ENOMEM;
	}

	for (toka = strtok_r(cpu, ",", &saveptr); toka;
		 toka = strtok_r(NULL, ",", &saveptr)) {
		rmcpu = 0;
		if (*toka == '^') {
			/* This (These) Cpu(s) will be removed from the map */
			toka++;
			rmcpu = 1;
		}
		/* Extract a valid (range of) cpu(s) */
		cpuida = cpuidb = strtoul(toka, &endptr, 10);
		if (endptr == toka) {
			fprintf(stderr, "Error: Invalid argument.\n");
			rc = EINVAL;
			goto vcpp_out;
		}
		if (*endptr == '-') {
			tokb = endptr + 1;
			cpuidb = strtoul(tokb, &endptr, 10);
			if (endptr == tokb || cpuida > cpuidb) {
				fprintf(stderr, "Error: Invalid argument.\n");
				rc = EINVAL;
				goto vcpp_out;
			}
		}
		while (cpuida <= cpuidb) {
			rmcpu == 0 ? libxl_bitmap_set(cpumap, cpuida) :
						 libxl_bitmap_set(&exclude_cpumap, cpuida);
			cpuida++;
		}
	}

	/* Clear all the cpus from the removal list */
	libxl_for_each_set_bit(i, exclude_cpumap) {
		libxl_bitmap_reset(cpumap, i);
	}

vcpp_out:
	libxl_bitmap_dispose(&exclude_cpumap);

	return rc;
}
static void parse_config_data(const char *config_source,
							  const char *config_data,
							  int config_len,
							  libxl_domain_config *d_config,
							  struct clickos_domain *dom_info,
							  clickos_extra_config *e_config)

{
	const char *buf;
	long l;
	XLU_Config *config;
	XLU_ConfigList *cpus, *vbds, *nics, *pcis, *cvfbs, *cpuids;
	XLU_ConfigList *ioports, *irqs;
	int num_ioports, num_irqs;
	int pci_power_mgmt = 0;
	int pci_msitranslate = 0;
	int pci_permissive = 0;
	int i, e;

	libxl_domain_create_info *c_info = &d_config->c_info;
	libxl_domain_build_info *b_info = &d_config->b_info;

	config= xlu_cfg_init(stderr, config_source);
	if (!config) {
		fprintf(stderr, "Failed to allocate for configuration\n");
		exit(1);
	}

	e= xlu_cfg_readdata(config, config_data, config_len);
	if (e) {
		fprintf(stderr, "Failed to parse config: %s\n", strerror(e));
		exit(1);
	}
	if (!xlu_cfg_get_string (config, "seclabel", &buf, 0)) {
		e = libxl_flask_context_to_sid(ctx, (char *)buf, strlen(buf),
									&c_info->ssidref);
		if (e) {
			if (errno == ENOSYS) {
				fprintf(stderr, "XSM Disabled: seclabel not supported\n");
			} else {
				fprintf(stderr, "Invalid seclabel: %s\n", buf);
				exit(1);
			}
		}
	}

	libxl_defbool_set(&c_info->run_hotplug_scripts, run_hotplug_scripts);
	c_info->type = LIBXL_DOMAIN_TYPE_PV;
	if (!xlu_cfg_get_string (config, "builder", &buf, 0) &&
		!strncmp(buf, "hvm", strlen(buf)))
		c_info->type = LIBXL_DOMAIN_TYPE_HVM;

	xlu_cfg_get_defbool(config, "hap", &c_info->hap, 0);

	if (xlu_cfg_replace_string (config, "name", &c_info->name, 0)) {
		fprintf(stderr, "Domain name must be specified.\n");
		exit(1);
	}

	if (!xlu_cfg_get_string (config, "uuid", &buf, 0) ) {
		if ( libxl_uuid_from_string(&c_info->uuid, buf) ) {
			fprintf(stderr, "Failed to parse UUID: %s\n", buf);
			exit(1);
		}
	}else{
		libxl_uuid_generate(&c_info->uuid);
	}

	xlu_cfg_get_defbool(config, "oos", &c_info->oos, 0);

	if (!xlu_cfg_get_string (config, "pool", &buf, 0)) {
		c_info->poolid = -1;
		cpupool_qualifier_to_cpupoolid(buf, &c_info->poolid, NULL);
	}
	if (!libxl_cpupoolid_to_name(ctx, c_info->poolid)) {
		fprintf(stderr, "Illegal pool specified\n");
		exit(1);
	}

	libxl_domain_build_info_init_type(b_info, c_info->type);
	if (blkdev_start)
		b_info->blkdev_start = strdup(blkdev_start);

	/* the following is the actual config parsing with overriding
	 * values in the structures */
	if (!xlu_cfg_get_long (config, "cpu_weight", &l, 0))
		b_info->sched_params.weight = l;
	if (!xlu_cfg_get_long (config, "cap", &l, 0))
		b_info->sched_params.cap = l;
	if (!xlu_cfg_get_long (config, "period", &l, 0))
		b_info->sched_params.period = l;
	if (!xlu_cfg_get_long (config, "slice", &l, 0))
		b_info->sched_params.slice = l;
	if (!xlu_cfg_get_long (config, "latency", &l, 0))
		b_info->sched_params.latency = l;
	if (!xlu_cfg_get_long (config, "extratime", &l, 0))
		b_info->sched_params.extratime = l;

	if (!xlu_cfg_get_long (config, "vcpus", &l, 0)) {
		b_info->max_vcpus = l;

		if (libxl_cpu_bitmap_alloc(ctx, &b_info->avail_vcpus, l)) {
			fprintf(stderr, "Unable to allocate cpumap\n");
			exit(1);
		}
		libxl_bitmap_set_none(&b_info->avail_vcpus);
		while (l-- > 0)
			libxl_bitmap_set((&b_info->avail_vcpus), l);
	}

	if (!xlu_cfg_get_long (config, "maxvcpus", &l, 0))
		b_info->max_vcpus = l;

	if (!xlu_cfg_get_list (config, "cpus", &cpus, 0, 1)) {
		int i, n_cpus = 0;

		if (libxl_cpu_bitmap_alloc(ctx, &b_info->cpumap, 0)) {
			fprintf(stderr, "Unable to allocate cpumap\n");
			exit(1);
		}

		/* Prepare the array for single vcpu to pcpu mappings */
		vcpu_to_pcpu = xmalloc(sizeof(int) * b_info->max_vcpus);
		memset(vcpu_to_pcpu, -1, sizeof(int) * b_info->max_vcpus);

		/*
		 * Idea here is to let libxl think all the domain's vcpus
		 * have cpu affinity with all the pcpus on the list.
		 * It is then us, here in xl, that matches each single vcpu
		 * to its pcpu (and that's why we need to stash such info in
		 * the vcpu_to_pcpu array now) after the domain has been created.
		 * Doing it like this saves the burden of passing to libxl
		 * some big array hosting the single mappings. Also, using
		 * the cpumap derived from the list ensures memory is being
		 * allocated on the proper nodes anyway.
		 */
		libxl_bitmap_set_none(&b_info->cpumap);
		while ((buf = xlu_cfg_get_listitem(cpus, n_cpus)) != NULL) {
			i = atoi(buf);
			if (!libxl_bitmap_cpu_valid(&b_info->cpumap, i)) {
				fprintf(stderr, "cpu %d illegal\n", i);
				exit(1);
			}
			libxl_bitmap_set(&b_info->cpumap, i);
			if (n_cpus < b_info->max_vcpus)
				vcpu_to_pcpu[n_cpus] = i;
			n_cpus++;
		}

		/* We have a cpumap, disable automatic placement */
		libxl_defbool_set(&b_info->numa_placement, false);
	}
	else if (!xlu_cfg_get_string (config, "cpus", &buf, 0)) {
		char *buf2 = strdup(buf);

		if (libxl_cpu_bitmap_alloc(ctx, &b_info->cpumap, 0)) {
			fprintf(stderr, "Unable to allocate cpumap\n");
			exit(1);
		}

		libxl_bitmap_set_none(&b_info->cpumap);
		if (vcpupin_parse(buf2, &b_info->cpumap))
			exit(1);
		free(buf2);

		libxl_defbool_set(&b_info->numa_placement, false);
	}

	if (!xlu_cfg_get_long (config, "memory", &l, 0)) {
		b_info->max_memkb = l * 1024;
		b_info->target_memkb = b_info->max_memkb;
	}

	if (!xlu_cfg_get_long (config, "maxmem", &l, 0))
		b_info->max_memkb = l * 1024;

	if (xlu_cfg_get_string (config, "on_poweroff", &buf, 0))
		buf = "destroy";

	if (!parse_action_on_shutdown(buf, &d_config->on_poweroff)) {
		fprintf(stderr, "Unknown on_poweroff action \"%s\" specified\n", buf);
		exit(1);
	}

	if (xlu_cfg_get_string (config, "on_reboot", &buf, 0))
		buf = "restart";
	if (!parse_action_on_shutdown(buf, &d_config->on_reboot)) {
		fprintf(stderr, "Unknown on_reboot action \"%s\" specified\n", buf);
		exit(1);
	}

	if (xlu_cfg_get_string (config, "on_watchdog", &buf, 0))
		buf = "destroy";
	if (!parse_action_on_shutdown(buf, &d_config->on_watchdog)) {
		fprintf(stderr, "Unknown on_watchdog action \"%s\" specified\n", buf);
		exit(1);
	}

	if (xlu_cfg_get_string (config, "on_crash", &buf, 0))
		buf = "destroy";
	if (!parse_action_on_shutdown(buf, &d_config->on_crash)) {
		fprintf(stderr, "Unknown on_crash action \"%s\" specified\n", buf);
		exit(1);
	}

	/* libxl_get_required_shadow_memory() must be called after final values
	 * (default or specified) for vcpus and memory are set, because the
	 * calculation depends on those values. */
	b_info->shadow_memkb = !xlu_cfg_get_long(config, "shadow_memory", &l, 0)
		? l * 1024
		: libxl_get_required_shadow_memory(b_info->max_memkb,
										   b_info->max_vcpus);

	xlu_cfg_get_defbool(config, "nomigrate", &b_info->disable_migrate, 0);

	if (!xlu_cfg_get_long(config, "tsc_mode", &l, 1)) {
		const char *s = libxl_tsc_mode_to_string(l);
		fprintf(stderr, "WARNING: specifying \"tsc_mode\" as an integer is deprecated. "
				"Please use the named parameter variant. %s%s%s\n",
				s ? "e.g. tsc_mode=\"" : "",
				s ? s : "",
				s ? "\"" : "");

		if (l < LIBXL_TSC_MODE_DEFAULT ||
			l > LIBXL_TSC_MODE_NATIVE_PARAVIRT) {
			fprintf(stderr, "ERROR: invalid value %ld for \"tsc_mode\"\n", l);
			exit (1);
		}
		b_info->tsc_mode = l;
	} else if (!xlu_cfg_get_string(config, "tsc_mode", &buf, 0)) {
		fprintf(stderr, "got a tsc mode string: \"%s\"\n", buf);
		if (libxl_tsc_mode_from_string(buf, &b_info->tsc_mode)) {
			fprintf(stderr, "ERROR: invalid value \"%s\" for \"tsc_mode\"\n",
					buf);
			exit (1);
		}
	}

	if (!xlu_cfg_get_long(config, "rtc_timeoffset", &l, 0))
		b_info->rtc_timeoffset = l;

	if (dom_info && !xlu_cfg_get_long(config, "vncviewer", &l, 0)) {
		/* Command line arguments must take precedence over what's
		 * specified in the configuration file. */
		if (!dom_info->vnc)
			dom_info->vnc = l;
	}

	xlu_cfg_get_defbool(config, "localtime", &b_info->localtime, 0);

	if (!xlu_cfg_get_long (config, "videoram", &l, 0))
		b_info->video_memkb = l * 1024;

	switch(b_info->type) {
	case LIBXL_DOMAIN_TYPE_HVM:
		if (!xlu_cfg_get_string (config, "kernel", &buf, 0))
			fprintf(stderr, "WARNING: ignoring \"kernel\" directive for HVM guest. "
					"Use \"firmware_override\" instead if you really want a non-default firmware\n");

		xlu_cfg_replace_string (config, "firmware_override",
								&b_info->u.hvm.firmware, 0);
		if (!xlu_cfg_get_string(config, "bios", &buf, 0) &&
			libxl_bios_type_from_string(buf, &b_info->u.hvm.bios)) {
				fprintf(stderr, "ERROR: invalid value \"%s\" for \"bios\"\n",
					buf);
				exit (1);
		}

		xlu_cfg_get_defbool(config, "pae", &b_info->u.hvm.pae, 0);
		xlu_cfg_get_defbool(config, "apic", &b_info->u.hvm.apic, 0);
		xlu_cfg_get_defbool(config, "acpi", &b_info->u.hvm.acpi, 0);
		xlu_cfg_get_defbool(config, "acpi_s3", &b_info->u.hvm.acpi_s3, 0);
		xlu_cfg_get_defbool(config, "acpi_s4", &b_info->u.hvm.acpi_s4, 0);
		xlu_cfg_get_defbool(config, "nx", &b_info->u.hvm.nx, 0);
		xlu_cfg_get_defbool(config, "viridian", &b_info->u.hvm.viridian, 0);
		xlu_cfg_get_defbool(config, "hpet", &b_info->u.hvm.hpet, 0);
		xlu_cfg_get_defbool(config, "vpt_align", &b_info->u.hvm.vpt_align, 0);

		if (!xlu_cfg_get_long(config, "timer_mode", &l, 1)) {
			const char *s = libxl_timer_mode_to_string(l);
			fprintf(stderr, "WARNING: specifying \"timer_mode\" as an integer is deprecated. "
					"Please use the named parameter variant. %s%s%s\n",
					s ? "e.g. timer_mode=\"" : "",
					s ? s : "",
					s ? "\"" : "");

			if (l < LIBXL_TIMER_MODE_DELAY_FOR_MISSED_TICKS ||
				l > LIBXL_TIMER_MODE_ONE_MISSED_TICK_PENDING) {
				fprintf(stderr, "ERROR: invalid value %ld for \"timer_mode\"\n", l);
				exit (1);
			}
			b_info->u.hvm.timer_mode = l;
		} else if (!xlu_cfg_get_string(config, "timer_mode", &buf, 0)) {
			if (libxl_timer_mode_from_string(buf, &b_info->u.hvm.timer_mode)) {
				fprintf(stderr, "ERROR: invalid value \"%s\" for \"timer_mode\"\n",
						buf);
				exit (1);
			}
		}

		xlu_cfg_get_defbool(config, "nestedhvm", &b_info->u.hvm.nested_hvm, 0);
		break;
	case LIBXL_DOMAIN_TYPE_PV:
	{
		char *cmdline = NULL;
		const char *root = NULL, *extra = "";

		xlu_cfg_replace_string (config, "kernel", &b_info->u.pv.kernel, 0);

		xlu_cfg_get_string (config, "root", &root, 0);
		xlu_cfg_get_string (config, "extra", &extra, 0);

		if (root) {
			if (asprintf(&cmdline, "root=%s %s", root, extra) == -1)
				cmdline = NULL;
		} else {
			cmdline = strdup(extra);
		}

		if ((root || extra) && !cmdline) {
			fprintf(stderr, "Failed to allocate memory for cmdline\n");
			exit(1);
		}

		xlu_cfg_replace_string (config, "bootloader", &b_info->u.pv.bootloader, 0);
		switch (xlu_cfg_get_list_as_string_list(config, "bootloader_args",
									  &b_info->u.pv.bootloader_args, 1))
		{

		case 0: break; /* Success */
		case ESRCH: break; /* Option not present */
		case EINVAL:
			if (!xlu_cfg_get_string(config, "bootloader_args", &buf, 0)) {

				fprintf(stderr, "WARNING: Specifying \"bootloader_args\""
						" as a string is deprecated. "
						"Please use a list of arguments.\n");
				split_string_into_string_list(buf, " \t\n",
											  &b_info->u.pv.bootloader_args);
			}
			break;
		default:
			fprintf(stderr,"xl: Unable to parse bootloader_args.\n");
			exit(-ERROR_FAIL);
		}

		if (!b_info->u.pv.bootloader && !b_info->u.pv.kernel) {
			fprintf(stderr, "Neither kernel nor bootloader specified\n");
			exit(1);
		}

		b_info->u.pv.cmdline = cmdline;
		xlu_cfg_replace_string (config, "ramdisk", &b_info->u.pv.ramdisk, 0);
		break;
	}
	default:
		abort();
	}

	if (!xlu_cfg_get_list(config, "ioports", &ioports, &num_ioports, 0)) {
		b_info->num_ioports = num_ioports;
		b_info->ioports = calloc(num_ioports, sizeof(*b_info->ioports));
		if (b_info->ioports == NULL) {
			fprintf(stderr, "unable to allocate memory for ioports\n");
			exit(-1);
		}

		for (i = 0; i < num_ioports; i++) {
			const char *buf2;
			char *ep;
			uint32_t start, end;
			unsigned long ul;

			buf = xlu_cfg_get_listitem (ioports, i);
			if (!buf) {
				fprintf(stderr,
						"xl: Unable to get element #%d in ioport list\n", i);
				exit(1);
			}
			ul = strtoul(buf, &ep, 16);
			if (ep == buf) {
				fprintf(stderr, "xl: Invalid argument parsing ioport: %s\n",
						buf);
				exit(1);
			}
			if (ul >= UINT32_MAX) {
				fprintf(stderr, "xl: ioport %lx too big\n", ul);
				exit(1);
			}
			start = end = ul;

			if (*ep == '-') {
				buf2 = ep + 1;
				ul = strtoul(buf2, &ep, 16);
				if (ep == buf2 || *ep != '\0' || start > end) {
					fprintf(stderr,
							"xl: Invalid argument parsing ioport: %s\n", buf);
					exit(1);
				}
				if (ul >= UINT32_MAX) {
					fprintf(stderr, "xl: ioport %lx too big\n", ul);
					exit(1);
				}
				end = ul;
			} else if ( *ep != '\0' )
				fprintf(stderr,
						"xl: Invalid argument parsing ioport: %s\n", buf);
			b_info->ioports[i].first = start;
			b_info->ioports[i].number = end - start + 1;
		}
	}

	if (!xlu_cfg_get_list(config, "irqs", &irqs, &num_irqs, 0)) {
		b_info->num_irqs = num_irqs;
		b_info->irqs = calloc(num_irqs, sizeof(*b_info->irqs));
		if (b_info->irqs == NULL) {
			fprintf(stderr, "unable to allocate memory for ioports\n");
			exit(-1);
		}
		for (i = 0; i < num_irqs; i++) {
			char *ep;
			unsigned long ul;
			buf = xlu_cfg_get_listitem (irqs, i);
			if (!buf) {
				fprintf(stderr,
						"xl: Unable to get element %d in irq list\n", i);
				exit(1);
			}
			ul = strtoul(buf, &ep, 10);
			if (ep == buf) {
				fprintf(stderr,
						"xl: Invalid argument parsing irq: %s\n", buf);
				exit(1);
			}
			if (ul >= UINT32_MAX) {
				fprintf(stderr, "xl: irq %lx too big\n", ul);
				exit(1);
			}
			b_info->irqs[i] = ul;
		}
	}

	if (!xlu_cfg_get_list (config, "disk", &vbds, 0, 0)) {
		d_config->num_disks = 0;
		d_config->disks = NULL;
		while ((buf = xlu_cfg_get_listitem (vbds, d_config->num_disks)) != NULL) {
			libxl_device_disk *disk;
			char *buf2 = strdup(buf);

			d_config->disks = (libxl_device_disk *) realloc(d_config->disks, sizeof (libxl_device_disk) * (d_config->num_disks + 1));
			disk = d_config->disks + d_config->num_disks;
			parse_disk_config(&config, buf2, disk);

			free(buf2);
			d_config->num_disks++;
		}
	}

	if (!xlu_cfg_get_list (config, "vif", &nics, 0, 0)) {
		d_config->num_nics = 0;
		d_config->nics = NULL;
		while ((buf = xlu_cfg_get_listitem (nics, d_config->num_nics)) != NULL) {
			libxl_device_nic *nic;
			char *buf2 = strdup(buf);
			char *p, *p2;

			d_config->nics = (libxl_device_nic *) realloc(d_config->nics, sizeof (libxl_device_nic) * (d_config->num_nics+1));
			nic = d_config->nics + d_config->num_nics;
			libxl_device_nic_init(nic);
			nic->devid = d_config->num_nics;

			if (default_vifscript) {
				free(nic->script);
				nic->script = strdup(default_vifscript);
			}

			if (default_bridge) {
				free(nic->bridge);
				nic->bridge = strdup(default_bridge);
			}

			p = strtok(buf2, ",");
			if (!p)
				goto skip;
			do {
				while (*p == ' ')
					p++;
				if ((p2 = strchr(p, '=')) == NULL)
					break;
				*p2 = '\0';
				if (!strcmp(p, "model")) {
					free(nic->model);
					nic->model = strdup(p2 + 1);
				} else if (!strcmp(p, "mac")) {
					char *p3 = p2 + 1;
					*(p3 + 2) = '\0';
					nic->mac[0] = strtol(p3, NULL, 16);
					p3 = p3 + 3;
					*(p3 + 2) = '\0';
					nic->mac[1] = strtol(p3, NULL, 16);
					p3 = p3 + 3;
					*(p3 + 2) = '\0';
					nic->mac[2] = strtol(p3, NULL, 16);
					p3 = p3 + 3;
					*(p3 + 2) = '\0';
					nic->mac[3] = strtol(p3, NULL, 16);
					p3 = p3 + 3;
					*(p3 + 2) = '\0';
					nic->mac[4] = strtol(p3, NULL, 16);
					p3 = p3 + 3;
					*(p3 + 2) = '\0';
					nic->mac[5] = strtol(p3, NULL, 16);
				} else if (!strcmp(p, "bridge")) {
					free(nic->bridge);
					nic->bridge = strdup(p2 + 1);
				} else if (!strcmp(p, "type")) {
					if (!strcmp(p2 + 1, "ioemu"))
						nic->nictype = LIBXL_NIC_TYPE_VIF_IOEMU;
					else
						nic->nictype = LIBXL_NIC_TYPE_VIF;
				} else if (!strcmp(p, "ip")) {
					free(nic->ip);
					nic->ip = strdup(p2 + 1);
				} else if (!strcmp(p, "script")) {
					free(nic->script);
					nic->script = strdup(p2 + 1);
				} else if (!strcmp(p, "vifname")) {
					free(nic->ifname);
					nic->ifname = strdup(p2 + 1);
				} else if (!strcmp(p, "backend")) {
					if(libxl_name_to_domid(ctx, (p2 + 1), &(nic->backend_domid))) {
						fprintf(stderr, "Specified backend domain does not exist, defaulting to Dom0\n");
						nic->backend_domid = 0;
					}
				} else if (!strcmp(p, "rate")) {
					parse_vif_rate(&config, (p2 + 1), nic);
				} else if (!strcmp(p, "accel")) {
					fprintf(stderr, "the accel parameter for vifs is currently not supported\n");
				}
			} while ((p = strtok(NULL, ",")) != NULL);
skip:
			free(buf2);
			d_config->num_nics++;
		}
	}


	if (xlu_cfg_replace_string (config, "click", &e_config->click_file, 0)) {
		e_config->click_file = NULL;
	}

	if (!xlu_cfg_get_list(config, "vif2", NULL, 0, 0)) {
		fprintf(stderr, "WARNING: vif2: netchannel2 is deprecated and not supported by xl\n");
	}

	if (!xlu_cfg_get_list (config, "vfb", &cvfbs, 0, 0)) {
		d_config->num_vfbs = 0;
		d_config->num_vkbs = 0;
		d_config->vfbs = NULL;
		d_config->vkbs = NULL;
		while ((buf = xlu_cfg_get_listitem (cvfbs, d_config->num_vfbs)) != NULL) {
			libxl_device_vfb *vfb;
			libxl_device_vkb *vkb;

			char *buf2 = strdup(buf);
			char *p, *p2;

			d_config->vfbs = (libxl_device_vfb *) realloc(d_config->vfbs, sizeof(libxl_device_vfb) * (d_config->num_vfbs + 1));
			vfb = d_config->vfbs + d_config->num_vfbs;
			libxl_device_vfb_init(vfb);
			vfb->devid = d_config->num_vfbs;

			d_config->vkbs = (libxl_device_vkb *) realloc(d_config->vkbs, sizeof(libxl_device_vkb) * (d_config->num_vkbs + 1));
			vkb = d_config->vkbs + d_config->num_vkbs;
			libxl_device_vkb_init(vkb);
			vkb->devid = d_config->num_vkbs;

			p = strtok(buf2, ",");
			if (!p)
				goto skip_vfb;
			do {
				while (*p == ' ')
					p++;
				if ((p2 = strchr(p, '=')) == NULL)
					break;
				*p2 = '\0';
				if (!strcmp(p, "vnc")) {
					libxl_defbool_set(&vfb->vnc.enable, atoi(p2 + 1));
				} else if (!strcmp(p, "vnclisten")) {
					free(vfb->vnc.listen);
					vfb->vnc.listen = strdup(p2 + 1);
				} else if (!strcmp(p, "vncpasswd")) {
					free(vfb->vnc.passwd);
					vfb->vnc.passwd = strdup(p2 + 1);
				} else if (!strcmp(p, "vncdisplay")) {
					vfb->vnc.display = atoi(p2 + 1);
				} else if (!strcmp(p, "vncunused")) {
					libxl_defbool_set(&vfb->vnc.findunused, atoi(p2 + 1));
				} else if (!strcmp(p, "keymap")) {
					free(vfb->keymap);
					vfb->keymap = strdup(p2 + 1);
				} else if (!strcmp(p, "sdl")) {
					libxl_defbool_set(&vfb->sdl.enable, atoi(p2 + 1));
				} else if (!strcmp(p, "opengl")) {
					libxl_defbool_set(&vfb->sdl.opengl, atoi(p2 + 1));
				} else if (!strcmp(p, "display")) {
					free(vfb->sdl.display);
					vfb->sdl.display = strdup(p2 + 1);
				} else if (!strcmp(p, "xauthority")) {
					free(vfb->sdl.xauthority);
					vfb->sdl.xauthority = strdup(p2 + 1);
				}
			} while ((p = strtok(NULL, ",")) != NULL);
skip_vfb:
			free(buf2);
			d_config->num_vfbs++;
			d_config->num_vkbs++;
		}
	}

	if (!xlu_cfg_get_long (config, "pci_msitranslate", &l, 0))
		pci_msitranslate = l;

	if (!xlu_cfg_get_long (config, "pci_power_mgmt", &l, 0))
		pci_power_mgmt = l;

	if (!xlu_cfg_get_long (config, "pci_permissive", &l, 0))
		pci_permissive = l;

	/* To be reworked (automatically enabled) once the auto ballooning
	 * after guest starts is done (with PCI devices passed in). */
	if (c_info->type == LIBXL_DOMAIN_TYPE_PV) {
		xlu_cfg_get_defbool(config, "e820_host", &b_info->u.pv.e820_host, 0);
	}

	if (!xlu_cfg_get_list (config, "pci", &pcis, 0, 0)) {
		int i;
		d_config->num_pcidevs = 0;
		d_config->pcidevs = NULL;
		for(i = 0; (buf = xlu_cfg_get_listitem (pcis, i)) != NULL; i++) {
			libxl_device_pci *pcidev;

			d_config->pcidevs = (libxl_device_pci *) realloc(d_config->pcidevs, sizeof (libxl_device_pci) * (d_config->num_pcidevs + 1));
			pcidev = d_config->pcidevs + d_config->num_pcidevs;
			libxl_device_pci_init(pcidev);

			pcidev->msitranslate = pci_msitranslate;
			pcidev->power_mgmt = pci_power_mgmt;
			pcidev->permissive = pci_permissive;
			if (!xlu_pci_parse_bdf(config, pcidev, buf))
				d_config->num_pcidevs++;
		}
		if (d_config->num_pcidevs && c_info->type == LIBXL_DOMAIN_TYPE_PV)
			libxl_defbool_set(&b_info->u.pv.e820_host, true);
	}

	switch (xlu_cfg_get_list(config, "cpuid", &cpuids, 0, 1)) {
	case 0:
		{
			int i;
			const char *errstr;

			for (i = 0; (buf = xlu_cfg_get_listitem(cpuids, i)) != NULL; i++) {
				e = libxl_cpuid_parse_config_xend(&b_info->cpuid, buf);
				switch (e) {
				case 0: continue;
				case 1:
					errstr = "illegal leaf number";
					break;
				case 2:
					errstr = "illegal subleaf number";
					break;
				case 3:
					errstr = "missing colon";
					break;
				case 4:
					errstr = "invalid register name (must be e[abcd]x)";
					break;
				case 5:
					errstr = "policy string must be exactly 32 characters long";
					break;
				default:
					errstr = "unknown error";
					break;
				}
				fprintf(stderr, "while parsing CPUID line: \"%s\":\n", buf);
				fprintf(stderr, "  error #%i: %s\n", e, errstr);
			}
		}
		break;
	case EINVAL:	/* config option is not a list, parse as a string */
		if (!xlu_cfg_get_string(config, "cpuid", &buf, 0)) {
			char *buf2, *p, *strtok_ptr = NULL;
			const char *errstr;

			buf2 = strdup(buf);
			p = strtok_r(buf2, ",", &strtok_ptr);
			if (p == NULL) {
				free(buf2);
				break;
			}
			if (strcmp(p, "host")) {
				fprintf(stderr, "while parsing CPUID string: \"%s\":\n", buf);
				fprintf(stderr, "  error: first word must be \"host\"\n");
				free(buf2);
				break;
			}
			for (p = strtok_r(NULL, ",", &strtok_ptr); p != NULL;
				 p = strtok_r(NULL, ",", &strtok_ptr)) {
				e = libxl_cpuid_parse_config(&b_info->cpuid, p);
				switch (e) {
				case 0: continue;
				case 1:
					errstr = "missing \"=\" in key=value";
					break;
				case 2:
					errstr = "unknown CPUID flag name";
					break;
				case 3:
					errstr = "illegal CPUID value (must be: [0|1|x|k|s])";
					break;
				default:
					errstr = "unknown error";
					break;
				}
				fprintf(stderr, "while parsing CPUID flag: \"%s\":\n", p);
				fprintf(stderr, "  error #%i: %s\n", e, errstr);
			}
			free(buf2);
		}
		break;
	default:
		break;
	}

	/* parse device model arguments, this works for pv, hvm and stubdom */
	if (!xlu_cfg_get_string (config, "device_model", &buf, 0)) {
		fprintf(stderr,
				"WARNING: ignoring device_model directive.\n"
				"WARNING: Use \"device_model_override\" instead if you"
				" really want a non-default device_model\n");
		if (strstr(buf, "stubdom-dm")) {
			if (c_info->type == LIBXL_DOMAIN_TYPE_HVM)
				fprintf(stderr, "WARNING: Or use"
						" \"device_model_stubdomain_override\" if you "
						" want to enable stubdomains\n");
			else
				fprintf(stderr, "WARNING: ignoring"
						" \"device_model_stubdomain_override\" directive"
						" for pv guest\n");
		}
	}


	xlu_cfg_replace_string (config, "device_model_override",
							&b_info->device_model, 0);
	if (!xlu_cfg_get_string (config, "device_model_version", &buf, 0)) {
		if (!strcmp(buf, "qemu-xen-traditional")) {
			b_info->device_model_version
				= LIBXL_DEVICE_MODEL_VERSION_QEMU_XEN_TRADITIONAL;
		} else if (!strcmp(buf, "qemu-xen")) {
			b_info->device_model_version
				= LIBXL_DEVICE_MODEL_VERSION_QEMU_XEN;
		} else {
			fprintf(stderr,
					"Unknown device_model_version \"%s\" specified\n", buf);
			exit(1);
		}
	} else if (b_info->device_model)
		fprintf(stderr, "WARNING: device model override given without specific DM version\n");
	xlu_cfg_get_defbool (config, "device_model_stubdomain_override",
						 &b_info->device_model_stubdomain, 0);

	if (!xlu_cfg_get_string (config, "device_model_stubdomain_seclabel",
							 &buf, 0)) {
		e = libxl_flask_context_to_sid(ctx, (char *)buf, strlen(buf),
									&b_info->device_model_ssidref);
		if (e) {
			if (errno == ENOSYS) {
				fprintf(stderr, "XSM Disabled:"
						" device_model_stubdomain_seclabel not supported\n");
			} else {
				fprintf(stderr, "Invalid device_model_stubdomain_seclabel:"
						" %s\n", buf);
				exit(1);
			}
		}
	}
#define parse_extra_args(type)											\
	e = xlu_cfg_get_list_as_string_list(config, "device_model_args"#type, \
									&b_info->extra##type, 0);			\
	if (e && e != ESRCH) {												\
		fprintf(stderr,"xl: Unable to parse device_model_args"#type".\n");\
		exit(-ERROR_FAIL);												\
	}

	/* parse extra args for qemu, common to both pv, hvm */
	parse_extra_args();

	/* parse extra args dedicated to pv */
	parse_extra_args(_pv);

	/* parse extra args dedicated to hvm */
	parse_extra_args(_hvm);

#undef parse_extra_args

	if (c_info->type == LIBXL_DOMAIN_TYPE_HVM) {
		if (!xlu_cfg_get_long(config, "stdvga", &l, 0))
			b_info->u.hvm.vga.kind = l ? LIBXL_VGA_INTERFACE_TYPE_STD :
										 LIBXL_VGA_INTERFACE_TYPE_CIRRUS;

		xlu_cfg_get_defbool(config, "vnc", &b_info->u.hvm.vnc.enable, 0);
		xlu_cfg_replace_string (config, "vnclisten",
								&b_info->u.hvm.vnc.listen, 0);
		xlu_cfg_replace_string (config, "vncpasswd",
								&b_info->u.hvm.vnc.passwd, 0);
		if (!xlu_cfg_get_long (config, "vncdisplay", &l, 0))
			b_info->u.hvm.vnc.display = l;
		xlu_cfg_get_defbool(config, "vncunused",
							&b_info->u.hvm.vnc.findunused, 0);
		xlu_cfg_replace_string (config, "keymap", &b_info->u.hvm.keymap, 0);
		xlu_cfg_get_defbool(config, "sdl", &b_info->u.hvm.sdl.enable, 0);
		xlu_cfg_get_defbool(config, "opengl", &b_info->u.hvm.sdl.opengl, 0);
		xlu_cfg_get_defbool (config, "spice", &b_info->u.hvm.spice.enable, 0);
		if (!xlu_cfg_get_long (config, "spiceport", &l, 0))
			b_info->u.hvm.spice.port = l;
		if (!xlu_cfg_get_long (config, "spicetls_port", &l, 0))
			b_info->u.hvm.spice.tls_port = l;
		xlu_cfg_replace_string (config, "spicehost",
								&b_info->u.hvm.spice.host, 0);
		xlu_cfg_get_defbool(config, "spicedisable_ticketing",
							&b_info->u.hvm.spice.disable_ticketing, 0);
		xlu_cfg_replace_string (config, "spicepasswd",
								&b_info->u.hvm.spice.passwd, 0);
		xlu_cfg_get_defbool(config, "spiceagent_mouse",
							&b_info->u.hvm.spice.agent_mouse, 0);
		xlu_cfg_get_defbool(config, "nographic", &b_info->u.hvm.nographic, 0);
		xlu_cfg_get_defbool(config, "gfx_passthru",
							&b_info->u.hvm.gfx_passthru, 0);
		xlu_cfg_replace_string (config, "serial", &b_info->u.hvm.serial, 0);
		xlu_cfg_replace_string (config, "boot", &b_info->u.hvm.boot, 0);
		xlu_cfg_get_defbool(config, "usb", &b_info->u.hvm.usb, 0);
		xlu_cfg_replace_string (config, "usbdevice",
								&b_info->u.hvm.usbdevice, 0);
		xlu_cfg_replace_string (config, "soundhw", &b_info->u.hvm.soundhw, 0);
		xlu_cfg_get_defbool(config, "xen_platform_pci",
							&b_info->u.hvm.xen_platform_pci, 0);
	}

	xlu_cfg_destroy(config);
}

static int freemem(libxl_domain_build_info *b_info)
{
	int rc, retries = 3;
	uint32_t need_memkb, free_memkb;

	if (!autoballoon)
		return 0;

	rc = libxl_domain_need_memory(ctx, b_info, &need_memkb);
	if (rc < 0)
		return rc;

	do {
		rc = libxl_get_free_memory(ctx, &free_memkb);
		if (rc < 0)
			return rc;

		if (free_memkb >= need_memkb)
			return 0;

		rc = libxl_set_memory_target(ctx, 0, free_memkb - need_memkb, 1, 0);
		if (rc < 0)
			return rc;

		rc = libxl_wait_for_free_memory(ctx, domid, need_memkb, 10);
		if (!rc)
			return 0;
		else if (rc != ERROR_NOMEM)
			return rc;

		/* the memory target has been reached but the free memory is still
		 * not enough: loop over again */
		rc = libxl_wait_for_memory_target(ctx, 0, 1);
		if (rc < 0)
			return rc;

		retries--;
	} while (retries > 0);

	return ERROR_NOMEM;
}

static int domain_wait_event(libxl_event **event_r)
{
	int ret;
	for (;;) {
		ret = libxl_event_wait(ctx, event_r, LIBXL_EVENTMASK_ALL, 0,0);
		if (ret) {
			fprintf(stderr, "Domain %d, failed to get event, quitting (rc=%d)", domid, ret);
			return ret;
		}
		if ((*event_r)->domid != domid) {
			char *evstr = libxl_event_to_json(ctx, *event_r);
			fprintf(stderr, "Ignoring unexpected event for domain %d (expected %d): event=%s",
				(*event_r)->domid, domid, evstr);
			free(evstr);
			libxl_event_free(ctx, *event_r);
			continue;
		}
		return ret;
	}
}
#endif

static void write_domain_config(int fd,
								const char *source,
								const uint8_t *config_data,
								int config_len)
{
	struct save_file_header hdr;
	uint8_t *optdata_begin;
	union { uint32_t u32; char b[4]; } u32buf;

	memset(&hdr, 0, sizeof(hdr));
	memcpy(hdr.magic, savefileheader_magic, sizeof(hdr.magic));
	hdr.byteorder = SAVEFILE_BYTEORDER_VALUE;

	optdata_begin = 0;

#define ADD_OPTDATA(ptr, len) ({                                        \
	if ((len)) {                                                        \
		hdr.optional_data_len += (len);                                 \
		optdata_begin = realloc(optdata_begin, hdr.optional_data_len);  \
		memcpy(optdata_begin + hdr.optional_data_len - (len),           \
			(ptr), (len));                                              \
	}                                                                   \
})

	u32buf.u32 = config_len;
	ADD_OPTDATA(u32buf.b, 4);
	ADD_OPTDATA(config_data, config_len);

	/* that's the optional data */

	CHK_ERRNO( libxl_write_exactly(ctx, fd,
				&hdr, sizeof(hdr), source, "header") );
	CHK_ERRNO( libxl_write_exactly(ctx, fd,
				optdata_begin, hdr.optional_data_len, source, "header") );

	fprintf(stderr, "Saving to %s new xl format (info"
			" 0x%"PRIx32"/0x%"PRIx32"/%"PRIu32")\n",
			source, hdr.mandatory_flags, hdr.optional_flags,
			hdr.optional_data_len);
}

int domain_name_to_id(char *domname)
{
	uint32_t domid;

	if(libxl_name_to_domid(ctx, domname, &domid)) {
		return -EINVAL;
	}

	return domid;
}

int domain_ctx_init(int flags)
{
	int ret;
	int verbose = flags & 0x01;
	if (verbose) {
		minmsglevel--;
		printf("Setting loglevel to %d\n", minmsglevel);
	}

	logger = xtl_createlogger_stdiostream(stderr, minmsglevel,  0);
	if (!logger)
		exit(1);

	if (libxl_ctx_alloc(&ctx, LIBXL_VERSION, 0, (xentoollog_logger*)logger)) {
		fprintf(stderr, "cannot init xl context\n");
		exit(1);
	}

	ret = asprintf(&lockfile, "/var/lock/xl");
	if (ret < 0) {
		fprintf(stderr, "asprintf memory allocation failed\n");
		exit(1);
	}

	return ret;
}

void domain_ctx_free(void)
{
	libxl_ctx_free(ctx);
	xtl_logger_destroy((xentoollog_logger*) logger);
}

int domain_create(struct clickos_domain *dom_info)
{
	libxl_domain_config d_config;
	clickos_extra_config e_config;

	const char *config_file = dom_info->config_file;
	const char *extra_config = dom_info->extra_config;
	const char *restore_file = dom_info->restore_file;
	int migrate_fd = dom_info->migrate_fd;

	int i, ret, rc;
	void *config_data = (char*) dom_info->config_data;
	int config_len = 0;
	int restore_fd = -1;
	int status = 0;
	const libxl_asyncprogress_how *autoconnect_console_how;
	pid_t child_console_pid = -1;
	struct save_file_header hdr;
	char *click_script;

	libxl_domain_config_init(&d_config);

	if (restore_file) {
		uint8_t *optdata_begin = 0;
		const uint8_t *optdata_here = 0;
		union { uint32_t u32; char b[4]; } u32buf;
		uint32_t badflags;

		restore_fd = migrate_fd >= 0 ? migrate_fd :
			open(restore_file, O_RDONLY);

		CHK_ERRNO( libxl_read_exactly(ctx, restore_fd, &hdr,
				   sizeof(hdr), restore_file, "header") );
		if (memcmp(hdr.magic, savefileheader_magic, sizeof(hdr.magic))) {
			fprintf(stderr, "File has wrong magic number -"
					" corrupt or for a different tool?\n");
			return ERROR_INVAL;
		}
		if (hdr.byteorder != SAVEFILE_BYTEORDER_VALUE) {
			fprintf(stderr, "File has wrong byte order\n");
			return ERROR_INVAL;
		}
		fprintf(stderr, "Loading new save file %s"
				" (new xl fmt info"
				" 0x%"PRIx32"/0x%"PRIx32"/%"PRIu32")\n",
				restore_file, hdr.mandatory_flags, hdr.optional_flags,
				hdr.optional_data_len);

		badflags = hdr.mandatory_flags & ~( 0 /* none understood yet */ );
		if (badflags) {
			fprintf(stderr, "Savefile has mandatory flag(s) 0x%"PRIx32" "
					"which are not supported; need newer xl\n",
					badflags);
			return ERROR_INVAL;
		}
		if (hdr.optional_data_len) {
			optdata_begin = xmalloc(hdr.optional_data_len);
			CHK_ERRNO( libxl_read_exactly(ctx, restore_fd, optdata_begin,
				   hdr.optional_data_len, restore_file, "optdata") );
		}

#define OPTDATA_LEFT  (hdr.optional_data_len - (optdata_here - optdata_begin))
#define WITH_OPTDATA(amt, body)								 \
			if (OPTDATA_LEFT < (amt)) {						 \
				fprintf(stderr, "Savefile truncated.\n");	   \
				return ERROR_INVAL;							 \
			} else {											\
				body;										   \
				optdata_here += (amt);						  \
			}

		optdata_here = optdata_begin;

		if (OPTDATA_LEFT) {
			fprintf(stderr, " Savefile contains xl domain config\n");
			WITH_OPTDATA(4, {
				memcpy(u32buf.b, optdata_here, 4);
				config_len = u32buf.u32;
			});
			WITH_OPTDATA(config_len, {
				config_data = xmalloc(config_len);
				memcpy(config_data, optdata_here, config_len);
			});
		}

	}

	if (config_file) {
		free(config_data);  config_data = 0;
		ret = libxl_read_file_contents(ctx, config_file,
									   &config_data, &config_len);
		if (ret) { fprintf(stderr, "Failed to read config file: %s: %s\n",
						   config_file, strerror(errno)); return ERROR_FAIL; }
		if (!restore_file && extra_config && strlen(extra_config)) {
			if (config_len > INT_MAX - (strlen(extra_config) + 2 + 1)) {
				fprintf(stderr, "Failed to attach extra configration\n");
				return ERROR_FAIL;
			}
			/* allocate space for the extra config plus two EOLs plus \0 */
			config_data = realloc(config_data, config_len
				+ strlen(extra_config) + 2 + 1);
			if (!config_data) {
				fprintf(stderr, "Failed to realloc config_data\n");
				return ERROR_FAIL;
			}
			config_len += sprintf(config_data + config_len, "\n%s\n",
				extra_config);
		}
	} else {
		if (!config_data) {
			fprintf(stderr, "Config file not specified and"
					" none in save file\n");
			return ERROR_INVAL;
		}
		config_file = "<saved>";
		config_len = strlen(config_data);
	}

	//fprintf(stderr, "%s\n", config_data);

	if (!dom_info->quiet)
		printf("Parsing config file %s\n", config_file);

	e_config.click_file = NULL;
	click_script = NULL;

	parse_config_data(config_file, config_data, config_len, &d_config, dom_info, &e_config);

	if (migrate_fd >= 0) {
		if (d_config.c_info.name) {
			/* when we receive a domain we get its name from the config
			 * file; and we receive it to a temporary name */
			assert(!common_domname);

			common_domname = d_config.c_info.name;
			d_config.c_info.name = 0; /* steals allocation from config */

			if (asprintf(&d_config.c_info.name,
						 "%s--incoming", common_domname) < 0) {
				fprintf(stderr, "Failed to allocate memory in asprintf\n");
				exit(1);
			}
			*dom_info->migration_domname_r = strdup(d_config.c_info.name);
		}
	}

	ret = 0;

	domid = -1;

	rc = acquire_lock();
	if (rc < 0)
		goto error_out;

	ret = freemem(&d_config.b_info);
	if (ret < 0) {
		fprintf(stderr, "failed to free memory for the domain\n");
		ret = ERROR_FAIL;
		goto error_out;
	}

	autoconnect_console_how = 0;

	if ( restore_file ) {
#if XEN_VERSION >= 40400
		libxl_domain_restore_params params;
		params.checkpointed_stream = 0;
#endif
		ret = libxl_domain_create_restore(ctx, &d_config,
											&domid, restore_fd,
#if XEN_VERSION >= 40400
											&params,
#endif
											0, autoconnect_console_how);
	}else{
		ret = libxl_domain_create_new(ctx, &d_config, &domid,
										0, autoconnect_console_how);
	}

	if ( ret ) {
		perror("error creating domain");
		goto error_out;
	}

	/* If single vcpu to pcpu mapping was requested, honour it */
	if (vcpu_to_pcpu) {
		libxl_bitmap vcpu_cpumap;

		ret = libxl_cpu_bitmap_alloc(ctx, &vcpu_cpumap, 0);
		if (ret)
			goto error_out;
		for (i = 0; i < d_config.b_info.max_vcpus; i++) {

			if (vcpu_to_pcpu[i] != -1) {
				libxl_bitmap_set_none(&vcpu_cpumap);
				libxl_bitmap_set(&vcpu_cpumap, vcpu_to_pcpu[i]);
			} else {
				libxl_bitmap_set_any(&vcpu_cpumap);
			}
			if (libxl_set_vcpuaffinity(ctx, domid, i, &vcpu_cpumap)) {
				fprintf(stderr, "setting affinity failed on vcpu `%d'.\n", i);
				libxl_bitmap_dispose(&vcpu_cpumap);
				free(vcpu_to_pcpu);
				ret = ERROR_FAIL;
				goto error_out;
			}
		}
		libxl_bitmap_dispose(&vcpu_cpumap);
		free(vcpu_to_pcpu); vcpu_to_pcpu = NULL;
	}


	ret = libxl_userdata_store(ctx, domid, "xl",
									config_data, config_len);
	if (ret) {
		perror("cannot save config file");
		ret = ERROR_FAIL;
		goto error_out;
	}

	release_lock();

	if (!dom_info->paused)
		libxl_domain_unpause(ctx, domid);

	if (e_config.click_file != NULL) {
		click_script = clickos_read_script(e_config.click_file);
	}

	if (click_script) {
		clickos_start(domid, d_config.c_info.name, click_script);
	}

	ret = domid; /* caller gets success in parent */

   	goto out;

error_out:
	release_lock();
	if (libxl_domid_valid_guest(domid))
		libxl_domain_destroy(ctx, domid, 0);

	if (logfile != 2)
			close(logfile);
out:
	libxl_domain_config_dispose(&d_config);

	free(config_data);

waitpid_out:
	if (child_console_pid > 0 &&
			waitpid(child_console_pid, &status, 0) < 0 && errno == EINTR)
		goto waitpid_out;

	return ret;

}

int domain_destroy(int _domid, int force)
{
	int rc;
	libxl_event *event;
	libxl_evgen_domain_death *deathw;

	domid = _domid;

	if (!libxl_domid_valid_guest(domid))
		return -EINVAL;

	if (force) {
		rc = libxl_domain_destroy(ctx, domid, 0);
		return rc;
	}

	rc = libxl_domain_shutdown(ctx, domid);
	if (rc)
		fprintf(stderr, "shutdown failed (rc=%d)\n", rc);

	rc = libxl_evenable_domain_death(ctx, domid, 0, &deathw);
	if (rc) {
		fprintf(stderr,"wait for death failed (evgen, rc=%d)\n",rc);
		exit(-1);
	}

	for (;;) {
		rc = domain_wait_event(&event);
		if (rc) exit(-1);

		switch (event->type) {

		case LIBXL_EVENT_TYPE_DOMAIN_DEATH:
			goto done;

		case LIBXL_EVENT_TYPE_DOMAIN_SHUTDOWN:
			rc = libxl_domain_destroy(ctx, domid, 0);
			break;

		default:
			fprintf(stderr, "Unexpected event type %d", event->type);
			break;
		}
		libxl_event_free(ctx, event);
	}
done:
	libxl_event_free(ctx, event);
	libxl_evdisable_domain_death(ctx, deathw);

	return rc;
}

int domain_suspend(const int domid, const char *filename)
{
	int rc;
	int fd;
	uint8_t *config_data;
	int config_len;

	rc = libxl_userdata_retrieve(ctx, domid, "xl", &config_data, &config_len);

	if (rc) {
		fputs("Unable to get config file\n",stderr);
		exit(2);
	}

	if (!config_len) {
		fputs(" Savefile will not contain xl domain config\n", stderr);
	}

	fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		fprintf(stderr, "Failed to open temp file %s for writing\n", filename);
		exit(2);
	}

	write_domain_config(fd, filename, config_data, config_len);

	MUST(libxl_domain_suspend(ctx, domid, fd, 0, NULL));
	close(fd);

	libxl_domain_destroy(ctx, domid, 0);

	return 0;
}
