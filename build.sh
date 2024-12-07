#!/bin/bash

set -e

`xcrun --sdk iphoneos -f clang` -dynamiclib -x objective-c -arch arm64 -isysroot `xcrun --sdk iphoneos --show-sdk-path` -framework Foundation -framework UIKit -miphoneos-version-min=13.0 -o LuckySpeeder.dylib LuckySpeeder.m -O3 -flto
