#!/bin/bash

if [ ! -z "$LSF_BIN_PATH" ]; then
    binpath=${LSF_BIN_PATH}/
else
    binpath=/usr/local/lsf/bin/
fi

requested=`echo $1 | sed 's/^.*\///'`
${binpath}bkill $requested