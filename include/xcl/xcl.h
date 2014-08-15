/*
 *          Xen control light
 *
 *   file: xcl.h
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
#ifndef LIBXCL_H
#define LIBXCL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <uuid/uuid.h>
#include <xenstore.h>
#include <xenctrl.h>
#include <xenguest.h>
#ifdef LIBXCL
#include <xc_dom.h>
#endif

#define NLOG(fmt, ...)
#define LOG(fmt, ...) \
	fprintf(stderr, "[%s:%d]: " fmt "\n", \
		__FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGE(fmt, ...) \
	fprintf(stderr, "[%s:%d] error: " fmt "\n", \
		__FUNCTION__, __LINE__, ##__VA_ARGS__)

struct xcl_domstate {
	uint32_t store_port;
	uint32_t store_domid;
	unsigned long store_mfn;
	uint32_t console_port;
	uint32_t console_domid;
	unsigned long console_mfn;
	char *kernel_path;
	const char *cmdline;
#define MAX_DEVICE_NIC 		16

	int nr_nics;
	struct xcl_device_nic {
		u_int backend_domid;
		char *mac;
		char *bridge;
		char *ifname;
		char *script;
		char *type;
	} *nics;
};

struct xcl_dominfo {
	/* main info */
	uint32_t domid;
	char *name;
	uint8_t uuid[16];
	char *uuid_string;
	uint32_t ssidref;
	bool pvh;
	int max_vcpus;
	uint32_t pcpu;
	uint64_t max_memkb;
	uint64_t target_memkb;
	uint64_t video_memkb;
	uint64_t shadow_memkb;
	uint32_t rtc_timeoffset;

	/* guest */
	char * kernel;
	uint64_t slack_memkb;
	char * cmdline;
	const char * features;

	/* dom state*/
	struct xcl_domstate *state;
};

struct xcl_ctx {
	struct xs_handle *xsh;
	xc_interface *xch;
};

void xcl_ctx_init(void);
void xcl_ctx_dispose(void);
int  xcl_dom_id(char *name);

void xcl_dom_create(struct xcl_dominfo *info);
void xcl_dom_shutdown(int domid);
void xcl_dom_destroy(int domid);

int  xcl_net_nrdevs(int domid);
void xcl_net_attach(int domid, int nr_nics, struct xcl_device_nic *nics);
void xcl_net_detach(int domid, int devid);

#endif
