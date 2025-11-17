#!/bin/bash

set -euo pipefail

mig -server mach_excServer.c \
    -sheader mach_excServer.h \
    -user /dev/null \
    -header /dev/null \
    $(xcrun --sdk macosx --show-sdk-path)/usr/include/mach/mach_exc.defs
