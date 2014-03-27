/*
 *          cosmos
 *
 *   file: clickos.h
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
 *	    Filipe Manco <filipe.manco@neclab.eu>
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
#ifndef CLICKOS_H
#define CLICKOS_H

#define MAX_CLICK_SCRIPTS 32

#ifndef BINDING_SWIG
struct click_script {
	char *name;
	char *path;
};

struct clickos_domain {
    int quiet;
    int paused;
    int vnc;
    const char *config_file;
    char *config_data;
    const char *click_file;
    char *extra_config; /* extra config string */
    char *restore_file;
    int migrate_fd; /* -1 means none */
    char **migration_domname_r; /* from malloc */
};

int clickos_create2(struct clickos_domain *dom_info);
int clickos_create3(const char *config_file, const char *args);
#endif /* BINDING_SWIG */

int  clickos_global_init(int verbose);
void clickos_global_free(void);

int   clickos_domid(char *domname);

int clickos_create(const char *config_file);
int clickos_create1(const char *name, const char* kernel_path);

int clickos_destroy(int domid, int force);

int clickos_suspend(int domid, char* filename);

int clickos_start(int domid, const char *name, const char *script);
int clickos_stop(int domid, int configid);

char* clickos_read_script(const char *script_path);
char* clickos_read_handler(int domid, char *element, char *attr);
char* clickos_write_handler(int domid, char *element, char *attr, char *value);

#endif
