#!/bin/bash
#
# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.  Oracle designates this
# particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#

# This script copies a generated standalone toolchain from an android
# developer workspace and adds a devkit.info file as needed 
# for building OpenJDK and OracleJDK.

# e.g.  
# createAndroidDevkit.sh x86 \
#   /java/embedded/buildtools/android/generated-toolchains/android-x86-toolchain

USAGE="$0 <arch> <generated-toolchain-dir>"

if [ "$1" = "" ] || [ "$2" = "" ]; then
    echo $USAGE
    exit 1
fi

ARCH="$1"
ANDROID_TOOLCHAIN=$2
TARGET_TOOLCHAIN=`basename ${ANDROID_TOOLCHAIN}`

SCRIPT_DIR="$(cd "$(dirname $0)" > /dev/null && pwd)"
mkdir -p ${SCRIPT_DIR}/../../build/devkit
BUILD_DIR="$(cd ${SCRIPT_DIR}/../../build/devkit > /dev/null && pwd)"


DEVKIT_ROOT="${BUILD_DIR}/android/${TARGET_TOOLCHAIN}"
DEVKIT_BUNDLE="${DEVKIT_ROOT}.${ARCH}.tar.gz"

echo "Creating android devkit in ${BUILD_DIR}/android/${TARGET_TOOLCHAIN}"

################################################################################
# Copy files to root
mkdir -p ${DEVKIT_ROOT}
echo "Copying android generated standalone toolchain ..."
cp -r ${ANDROID_TOOLCHAIN}  ${BUILD_DIR}/android/

################################################################################
# Generate devkit.info

echo-info() {
    echo "$1" >> ${DEVKIT_ROOT}/devkit.info
}

echo "Generating devkit.info..."
rm -f ${DEVKIT_ROOT}/devkit.info
echo-info "# This file describes to configure how to interpret the contents of this devkit"
echo-info "# The parameters used to create this devkit were:"
echo-info "# $*"
echo-info "DEVKIT_NAME=\"Android ${ARCH} (devkit)\""

case ${ARCH} in 
 "x86" )
    echo-info "DEVKIT_TOOLCHAIN_PATH=\"${BUILD_DIR}/android/${TARGET_TOOLCHAIN}/i686-linux-android/bin" ;;
 "x86_64" )
    echo-info "DEVKIT_TOOLCHAIN_PATH=\"${BUILD_DIR}/android/${TARGET_TOOLCHAIN}/x86_64-linux-android/bin" ;;
 "arm" )
    echo-info "DEVKIT_TOOLCHAIN_PATH=\"${BUILD_DIR}/android/${TARGET_TOOLCHAIN}/arm-linux-androideabi/bin" ;;
 "aarch64" )
    echo-info "DEVKIT_TOOLCHAIN_PATH=\"${BUILD_DIR}/android/${TARGET_TOOLCHAIN}/aarch64-linux-android/bin" ;;
esac

echo-info "DEVKIT_SYSROOT=\"${DEVKIT_ROOT}/sysroot"

################################################################################
# Copy this script

echo "Copying this script..."
cp $0 ${DEVKIT_ROOT}/

################################################################################
# Create bundle

echo "Creating bundle..."
(cd ${BUILD_DIR}/android && tar c  ${TARGET_TOOLCHAIN} | gzip - > "${DEVKIT_BUNDLE}")
