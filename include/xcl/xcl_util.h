/*
 *          Xen control light
 *
 *   file: xcl_util.h
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
#ifndef LIBXCL_UTIL_H
#define LIBXCL_UTIL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "xcl.h"

extern struct xcl_ctx *ctx;
extern xs_transaction_t t;

#define UUID_FMT "%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
#define UUID_FMTLEN ((2*16)+4)
#define UUID_BYTES(uuid) uuid[0], uuid[1], uuid[2], uuid[3], \
                                uuid[4], uuid[5], uuid[6], uuid[7], \
                                uuid[8], uuid[9], uuid[10], uuid[11], \
                                uuid[12], uuid[13], uuid[14], uuid[15]

#define TO_UUID_BYTES(arg) UUID_BYTES(((uint8_t *) arg))

static inline char* uuid_to_str(uint8_t uuid[])
{
	char *uuidstr = NULL;
	asprintf(&uuidstr, UUID_FMT, TO_UUID_BYTES(uuid));
	return uuidstr;
}

#define __xsread(p)  xs_read(ctx->xsh, XBT_NULL, p, NULL)
#define __xswrite(p,v)  xs_write(ctx->xsh, t, p, v, strlen(v))
#define __xschmod(p, f) xs_set_permissions(ctx->xsh, t, p, f, 1);

static inline char* __xsreadk(char *path, char *key)
{
	char *xspath = NULL;
	asprintf(&xspath, "%s/%s", path, key);
	return __xsread(xspath);
}

static inline void __xswritek(char *path, char *key, char *value)
{
	char *xspath = NULL;
	asprintf(&xspath, "%s/%s", path, key);
	__xswrite(xspath, value);
}

#endif
