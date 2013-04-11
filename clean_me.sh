#!/bin/bash

source ./env.sh

NDK_BUILD=`which ndk-build`

if [ -z "$NDK_BUILD" ]
then
    echo "NDK not installed, please install Android NDK!"
    exit
fi

export TOPDIR=`pwd`

$NDK_BUILD clean
