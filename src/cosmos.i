%module cosmos

%rename(ctx)	clickos_global_init;
%rename(free)   clickos_global_free;
%rename(domid)  clickos_domid;
%rename(create) clickos_create;
%rename(create_default) clickos_create1;
%rename(destroy)		clickos_destroy;
%rename(network_attach) clickos_network_attach;
%rename(script) clickos_read_script;
%rename(start)  clickos_start;
%rename(stop)   clickos_stop;
%rename(readh)  clickos_read_handler;
%rename(writeh) clickos_write_handler;

%ignore clickos_network_attach1;

%inline %{
#ifdef __cplusplus
extern "C" {
#endif

extern int   clickos_global_init(int verbose);
extern void  clickos_global_free(void);
extern int   clickos_domid(char *domname);
extern int   clickos_create(const char *config_file);
extern int   clickos_create1(const char *name, const char* kernel_path);
extern int   clickos_stop(int domid, int configid);
extern int   clickos_destroy(int domid, int force);
extern int   clickos_suspend(int domid, char* filename);
extern int   clickos_network_attach(int domid, char *macaddr, int backend_id, char *bridge);
extern int   clickos_start(int domid, const char *name, const char *script);
extern char* clickos_read_script(const char *script_path);
extern char* clickos_read_handler(int domid, char *element, char *attr);
extern char* clickos_write_handler(int domid, char *element, char *attr, char *value);

#ifdef __cplusplus
}
#endif

#ifdef BUILDING_NODE_EXTENSION
#include <node.h>
#endif
%}

#include "clickos.h"
