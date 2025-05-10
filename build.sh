#!/bin/bash

set -e

if [[ $# -eq 0 ]]; then
    echo "Usage: $0 [arm64-apple-ios|arm64-apple-ios-macabi|arm64-apple-xros|arm64-apple-tvos]"
    exit 1
fi

OUTPUT_DIR="out"

for target in "$@"; do
    case $target in
    arm64-apple-ios)
        echo "Building for $target..."
        clang=$(xcrun --sdk iphoneos -f clang)
        sdk_dir=$(xcrun --sdk iphoneos --show-sdk-path)
        out_dir=$OUTPUT_DIR/$target
        mkdir -p $out_dir

        $clang -dynamiclib \
            -target arm64-apple-ios13.1 \
            -isysroot $sdk_dir \
            -framework Foundation \
            -framework UIKit \
            -framework SpriteKit \
            -o $out_dir/LuckySpeeder.dylib fishhook.c LuckySpeeder.c LuckySpeeder.m LuckySpeederView.m Main.m \
            -Ofast \
            -flto
        strip -x $out_dir/LuckySpeeder.dylib
        ;;
    arm64-apple-ios-macabi)
        echo "Building for $target..."
        clang=$(xcrun --sdk macosx -f clang)
        sdk_dir=$(xcrun --sdk macosx --show-sdk-path)
        out_dir=$OUTPUT_DIR/$target
        mkdir -p $out_dir

        $clang -dynamiclib \
            -target arm64-apple-ios13.1-macabi \
            -isysroot $sdk_dir \
            -iframework $sdk_dir/System/iOSSupport/System/Library/Frameworks \
            -framework Foundation \
            -framework UIKit \
            -framework SpriteKit \
            -o $out_dir/LuckySpeeder.dylib fishhook.c LuckySpeeder.c LuckySpeeder.m LuckySpeederView.m Main.m \
            -Ofast \
            -flto
        strip -x $out_dir/LuckySpeeder.dylib
        ;;
    arm64-apple-xros)
        echo "Building for $target..."
        clang=$(xcrun --sdk xros -f clang)
        sdk_dir=$(xcrun --sdk xros --show-sdk-path)
        out_dir=$OUTPUT_DIR/$target
        mkdir -p $out_dir

        $clang -dynamiclib \
            -target arm64-apple-xros1.0 \
            -isysroot $sdk_dir \
            -framework Foundation \
            -framework UIKit \
            -framework SpriteKit \
            -o $out_dir/LuckySpeeder.dylib fishhook.c LuckySpeeder.c LuckySpeeder.m LuckySpeederView.m Main.m \
            -Ofast \
            -flto
        strip -x $out_dir/LuckySpeeder.dylib
        ;;
    arm64-apple-tvos)
        echo "Building for $target..."
        clang=$(xcrun --sdk appletvos -f clang)
        sdk_dir=$(xcrun --sdk appletvos --show-sdk-path)
        out_dir=$OUTPUT_DIR/$target
        mkdir -p $out_dir

        $clang -dynamiclib \
            -target arm64-apple-tvos13.2 \
            -isysroot $sdk_dir \
            -framework Foundation \
            -framework UIKit \
            -framework SpriteKit \
            -o $out_dir/LuckySpeeder.dylib fishhook.c LuckySpeeder.c LuckySpeeder.m LuckySpeederView.m Main.m \
            -Ofast \
            -flto
        strip -x $out_dir/LuckySpeeder.dylib
        ;;
    *)
        echo "Invalid target: $target"
        echo "Usage: $0 [arm64-apple-ios|arm64-apple-ios-macabi|arm64-apple-xros|arm64-apple-tvos]"
        exit 1
        ;;
    esac
done

echo "Build completed!"
