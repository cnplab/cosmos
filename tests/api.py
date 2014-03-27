import cosmos
import time
import os
import subprocess, commands

def isvmalive(name):
    cmd = "xl list | grep \"%s\" | grep \"sc\"" % name
    domid = cosmos.domid(name)
    ret, output = commands.getstatusoutput(cmd)
    print output
    if len(output) > 0:
        return domid
    return 0

xen_config = "./etc/xen/test.cfg";
#click_script = open("./etc/click/pingresponder.click").read();
click_script = open("./etc/click/elements/idle.click").read();
#click_script = open("./etc/click/elements/fromnetfront.click").read();

flags = 0;

# Initializes a context for cosmos with verbose enabled.
# <flags> consists of a bitmask which
# 0x01 = enable verbose
# 0x10 = enable autoattach in clickos_start
cosmos.ctx(flags)

start = time.time()

# Create the domain
#domid = cosmos.create(xen_config);

# Create the domain with <name> and <kernel>
#  plus default options
domid = cosmos.create_default("test", "/root/click/clickos/build/click_os_x86_64")

stop = time.time()
creation_time = stop - start;

# Network attach for vm with <domid>
#cosmos.network_attach(domid, "00:11:22:33:44:66", 0, "ixgbe.52.1")
start = time.time()
cosmos.network_attach(domid, "11:22:33:44:55:66", 0, "")
stop = time.time()
attach_time = stop - start;

#print "VALE Port: %s" % cosmos.vport(domid, 0)

start = time.time()
# Start the domain
cosmos.start(domid, "script", click_script);
stop = time.time()
start_time = stop - start;

time.sleep(0.5);
#time.sleep(7);
print "Alive? "
print "\033[32mYes\033[0m" if not isvmalive("test") else "\033[31mNo\033[0m";

# Destroy the domain
if cosmos.destroy(domid) < 0:
    print "Domain invalid"

print "Creation time: %f" % creation_time
print "Attach time: %f" % attach_time
print "Start time: %f" % start_time

# Frees the context
cosmos.free()
