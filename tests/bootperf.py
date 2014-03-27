import cosmos
import time
import os, subprocess, commands
import sys

def isvmalive(name):
    cmd = "xl list | grep \"%s\" | grep \"sc\"" % name
    domid = cosmos.domid(name)    
    ret, output = commands.getstatusoutput(cmd)
    time.sleep(1.)
    print output
    if len(output) > 0:
        return domid
    return 0

#click_script = open("/root/cosmos/etc/click/elements/idle.click").read();
click_script = open("/root/cosmos/etc/click/pingresponder.click").read();
domids = []
creation_time = []
attach_time = []
start_time = []
flags = 0;

# Initializes a context for cosmos with verbose enabled.
# <flags> consists of a bitmask which
# 0x01 = enable verbose
# 0x10 = enable autoattach in clickos_start
cosmos.ctx(flags)

for i in range(int(sys.argv[1])):
    name = "test%d" % i
    # Create the domain
    #domid = cosmos.create(xen_config);
    
    # Create the domain with <name> and <kernel>
    #  plus default options
    start = time.time()
    domid = cosmos.create_default(name, "/root/click/clickos/build/click_os_x86_64")
    stop = time.time()
    creation_time.append(stop - start);
    
    # Network attach for vm with <domid>
    start = time.time()
    cosmos.network_attach(domid, "11:22:33:44:55:66", 0, "")
    stop = time.time()
    attach_time.append(stop - start);
    domids.append(domid)
    #print "VALE Port: %s" % cosmos.vport(domid, 0)
    
for i in range(int(sys.argv[1])):
    domid = domids[i]
    print "Click : %s" % click_script
    start = time.time()
    # Start the domain
    cosmos.start(domid, "script", click_script);
    stop = time.time()
    start_time.append(stop - start);
    print "Alive? "
    print "\033[32mYes\033[0m" if not isvmalive(name) else "\033[31mNo\033[0m";

raw_input()

for i in range(int(sys.argv[1])):
    id = domids[i]
    # Destroy the domain
    if cosmos.destroy(id) < 0:
        print "Domain invalid"
    
    print "Creation time: %f" % creation_time[i]
    print "Attach time: %f" % attach_time[i]
    print "Start time: %f" % start_time[i]


# Frees the context
cosmos.free()
