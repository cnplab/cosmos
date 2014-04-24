/*
 *          cosmos
 *
 *   file: main.c
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
 * 	    Filipe Manco <filipe.manco@neclab.eu>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>

#ifdef HAVE_XL
# include <libxl.h>
# include <libxl_utils.h>
# include <libxlutil.h>
#endif

#include "clickos.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#ifdef HAVE_XL
extern libxl_ctx *ctx;
#endif

struct cmd_struct {
    const char *cmd;
    int (*fn)(int, char **);
};

const char cosmos_usage_string[] =
#ifdef HAVE_XL
    "cosmos [-v] [create|load|destroy|start|stop|suspend|resume|write|read] [<args>]";
#else
    "cosmos [-v] [create|destroy|start|stop|suspend|resume|write|read] [<args>]";
#endif

const char cosmos_more_info_string[] =
    "See 'cosmos help <command>' for more information on a specific command.";

int cmd_help(int argc, char **argv);
int cmd_create(int argc, char **argv);
#ifdef HAVE_XL
int cmd_load(int argc, char **argv);
#endif
int cmd_start(int argc, char **argv);
int cmd_stop(int argc, char **argv);
int cmd_destroy(int argc, char **argv);
int cmd_suspend(int argc, char **argv);
int cmd_resume(int argc, char **argv);
int cmd_read_h(int argc, char **argv);
int cmd_write_h(int argc, char **argv);

static struct cmd_struct commands[] = {
    { "create", cmd_create },
#ifdef HAVE_XL
    { "load", cmd_load },
#endif
    { "destroy", cmd_destroy },
    { "suspend", cmd_suspend },
    { "resume", cmd_resume },
    { "start", cmd_start },
    { "stop", cmd_stop },
    { "read", cmd_read_h },
    { "write", cmd_write_h },
    { "help", cmd_help },
};

static int report_timing;
static struct timeval tic, toc;

static inline void timing_record(struct timeval *t)
{
    if (report_timing) {
        gettimeofday(t, NULL);
    }
}

static inline void timing_report(struct timeval *tic, struct timeval *toc, char *msg_fmt)
{
    if (report_timing) {
        timersub(toc, tic, toc);
        double diff = toc->tv_sec + 1e-6 * toc->tv_usec;

        timerclear(tic);
        timerclear(toc);
        printf(msg_fmt, diff);
    }
}

#define TRACE(msg, fn)              \
do {                                \
    timing_record(&tic);            \
    fn                              \
    timing_record(&toc);            \
    timing_report(&tic, &toc, msg); \
} while(0)


int cmd_help(int argc, char **argv)
{
    return 0;
}

int cmd_create(int argc, char **argv)
{
    const char *filename = NULL;
    char extra_config[1024];
    struct clickos_domain dom_info;
    int paused = 0, quiet = 0;
    int opt, domid;
    int option_index = 0;
    static struct option long_options[] = {
        {"quiet", 0, 0, 'q'},
        {"defconfig", 1, 0, 'f'},
        {0, 0, 0, 0}
    };

    if (argv[1] && argv[1][0] != '-' && !strchr(argv[1], '=')) {
        filename = argv[1];
        argc--;
        argv++;
    }

    while (1) {
        opt = getopt_long(argc, argv, "qf:p", long_options, &option_index);
        if (opt == -1)
            break;

        switch (opt) {
        case 'f':
            filename = optarg;
            break;
        case 'p':
            paused = 1;
            break;
        case 'q':
            quiet = 1;
            break;
        default:
            fprintf(stderr, "option `%c' not supported.\n", optopt);
            break;
        }
    }

    extra_config[0] = '\0';
    filename = argv[optind - 1];

    memset(&dom_info, 0, sizeof(struct clickos_domain));
    dom_info.quiet = quiet;
    dom_info.paused = paused;
    dom_info.config_file = filename;
    dom_info.config_data = 0;
    dom_info.click_file = NULL;
    dom_info.extra_config = extra_config;
    dom_info.migrate_fd = -1;

    TRACE("Created domain in %f s\n", domid = clickos_create2(&dom_info););

    if (domid < 0)
        return 1;

    return 0;
}

int cmd_destroy(int argc, char **argv)
{
    int domid, force = 0;
    int opt, option_index = 0;
    static struct option long_options[] = {
        {"force", 0, 0, 'f'},
        {0, 0, 0, 0}
    };
    
	while (1) {
        opt = getopt_long(argc, argv, "f", long_options, &option_index);
        if (opt == -1)
            break;

        switch (opt) {
        case 'f':
            fprintf(stderr, "option -f enabled.\n");
            force = 1;
            break;
        default:
            fprintf(stderr, "option `%c' not supported.\n", optopt);
            break;
        }
    }

    if (!(domid = strtol(argv[optind], NULL, 10)))
        domid = clickos_domid(argv[optind]);

    TRACE("Destroyed domain in %f s\n", clickos_destroy(domid, force););

    return 0;
}

#ifdef HAVE_XL
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

int cmd_load(int argc, char **argv)
{
    int domid, pid;
    int len, i;
    char *buf, args[1024];
    const char *wspace=" ";
    FILE *fp;

    if (argc < 2) {
        fprintf(stderr, "Must specify a valid XEN configuration.\n");
        return -EINVAL;
    }

    argc = argc - 1;
    args[0] = '\0';

    for (i = 2; i <= argc; ++i) {
        strcat(args, argv[i]);
        strcat(args, wspace);
    }

    len = get_file_size(argv[1]);
    buf = malloc(len + 1);

    fp = fopen(argv[1], "r");
    fread(buf, sizeof(unsigned char), len, fp);
    fclose(fp);

    domid = clickos_create3(buf, args);
    fprintf(stderr, "Created Domain with ID %d\n", domid);

    pid = fork();
    if (pid) {
        while ((pid = waitpid(-1, NULL, 0)) > 0) {
            if (errno == ECHILD) {
                break;
            }
        }
        libxl_domain_destroy(ctx, domid, 0);
    }
    else
    {
        clickos_global_init(4);
        libxl_primary_console_exec(ctx, domid);
    }
    return 0;
}
#endif

int cmd_suspend(int argc, char **argv)
{
    int domid;

    if (argc != 3) {
        fprintf(stderr, "Usage: cosmos suspend <domain> <file>\n");
        return -EINVAL;
    }

    if (!(domid = strtol(argv[1], NULL, 10)))
        domid = clickos_domid(argv[1]);

    if (domid == -EINVAL) {
        fprintf(stderr, "Invalid domain identifier: %s\n", argv[2]);
        return -EINVAL;
    }

    TRACE("Suspended domain in %f s\n", clickos_suspend(domid, argv[2]););

    return 0;
}

int cmd_resume(int argc, char **argv)
{
    char *filename = NULL;
    char extra_config[1024];
    struct clickos_domain dom_info;
    int paused = 0, quiet = 0;
    int opt, domid;
    int option_index = 0;
    static struct option long_options[] = {
        {"quiet", 0, 0, 'q'},
        {0, 0, 0, 0}
    };

    if (argv[1] && argv[1][0] != '-' && !strchr(argv[1], '=')) {
        filename = argv[1];
        argc--;
        argv++;
    }

    while (1) {
        opt = getopt_long(argc, argv, "qf:p", long_options, &option_index);
        if (opt == -1)
            break;

        switch (opt) {
        case 'f':
            filename = optarg;
            break;
        case 'p':
            paused = 1;
            break;
        case 'q':
            quiet = 1;
            break;
        default:
            fprintf(stderr, "option `%c' not supported.\n", optopt);
            break;
        }
    }

    extra_config[0] = '\0';
    filename = argv[optind - 1];

    memset(&dom_info, 0, sizeof(struct clickos_domain));
    dom_info.quiet = quiet;
    dom_info.paused = paused;
    dom_info.config_file = NULL;
    dom_info.config_data = 0;
    dom_info.click_file = NULL;
    dom_info.extra_config = extra_config;
    dom_info.migrate_fd = -1;
    dom_info.restore_file = filename;

    TRACE("Resumed domain in %f seconds\n", domid = clickos_create2(&dom_info););

    if (domid < 0)
        return 1;

    return 0;
}

#define parse_h_args(id,elem,attr) \
    if (!(id = strtol(argv[1], NULL, 10))) \
        id = clickos_domid(argv[1]); \
    elem = argv[2]; \
    attr = argv[3]; \


int cmd_read_h(int argc, char **argv)
{
    int domid;
    char *elem, *attr, *value;
    
    parse_h_args(domid, elem, attr);

    value = clickos_read_handler(domid, elem, attr);
    printf("read(%s/%s) => %s\n", elem, attr, value);
    
    return 0;
}

int cmd_write_h(int argc, char **argv)
{
    int domid;
    char *elem, *attr, *value;

    parse_h_args(domid, elem, attr);
    value = argv[4];

    clickos_write_handler(domid, elem, attr, value);
    printf("write(%s/%s, %s)\n", elem, attr, value);

    return 0;
}

int cmd_start(int argc, char **argv)
{    
    char *script, *script_path;
    int opt, domid;
    int option_index = 0;
    static struct option long_options[] = {
        {"autoattach", 0, 0, 'a'},
        {0, 0, 0, 0}
    };

    if (argv[1] && argv[1][0] != '-') {
        argc--;
        argv++;
    }

    while (1) {
        opt = getopt_long(argc, argv, "ax", long_options, &option_index);
        if (opt == -1)
            break;

        switch (opt) {
        default:
            fprintf(stderr, "option `%c' not supported.\n", optopt);
            break;
        }
    }

    if ((argc - optind + 1) != 2) {
        fprintf(stderr, "wrong number of arguments (expected 2, got %d).\n",
                ((argc - optind) <= 0 ? 0 : (argc - optind)));
            return -EINVAL;
    }
    
    domid = clickos_domid(argv[optind - 1]);
    script_path = argv[optind];

    fprintf(stderr, "Domain ID for %s: %d\n", argv[optind - 1], domid);
    fprintf(stderr, "Location of click script: %s\n", script_path);

    script = clickos_read_script(script_path);
    clickos_start(domid, "script", script);
    return 0;
}

int cmd_stop(int argc, char **argv)
{
    int domid, configid;

    if (!(domid = strtol(argv[1], NULL, 10)))
        domid = clickos_domid(argv[1]);
    
    configid = atoi(argv[2]);

    clickos_stop(domid, configid);

    return 0;
}

int main(int argc, char **argv)
{
    int i;
    int opt = 0, verbose=0;
    int ret;

    report_timing = 0;

    while ((opt = getopt(argc, argv, "+v+xt")) >= 0) {
        switch (opt) {
        case 'v':
            verbose = 1;
            break;
        case 't':
            report_timing = 1;
            break;
        default:
            fprintf(stderr, "unknown global option\n");
            exit(2);
        }
    }

    opterr = 0;

    /* Reset options for per-command use of getopt. */
    argv += optind;
    argc -= optind;
    optind = 1;

    if (argc == 0) {
        printf("usage: %s\n\n", cosmos_usage_string);
        printf("\n%s\n", cosmos_more_info_string);
        exit(1);
    }

    clickos_global_init(verbose);

    for (i = 0; i < ARRAY_SIZE(commands); i++) {
        struct cmd_struct *p = commands + i;
        if (strcmp(p->cmd, *argv))
            continue;

        ret = p->fn(argc, argv);
    }

    clickos_global_free();

    return ret;
}
