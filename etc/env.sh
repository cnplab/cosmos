#!/bin/bash

CLICKOS_TOOLS=`readlink -f .`
CLICKOS_IMAGE=click-os-x86_64 

export PATH=$PATH:$CLICKOS_TOOLS/bin
export PYTHONPATH=$PYTHONPATH:$CLICKOS_TOOLS/build/python

cp /root/workspace/clickos-stable/trunk/click/clickos/build/bin/$CLICKOS_IMAGE /tmp/$CLICKOS_IMAGE-vif
cp /root/workspace/clickos/trunk/click/clickos/build/bin/$CLICKOS_IMAGE /tmp/$CLICKOS_IMAGE-vale
