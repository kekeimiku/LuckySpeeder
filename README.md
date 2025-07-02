# Lucky Speeder

> Support (Jailbreak/Jailed)  
> iOS 13.1+ | iPadOS 13.1+ | Mac Catalyst 13.1+ | visionOS 1.0+ | tvOS 13.2+  

## What's this

Hacking Applications: A Universal Game Speed Controller

Click Heart, Spade, Club, Diamond, Star to switch modes.

Click Forward, Backward to adjust the speed.

Click the number to customize the speed.

Click Play, Pause to start or stop the speed change.

If that doesn't work, try another mode.

NOTE: Not all programs will work - you may need some luck.

## Demo Video

<https://github.com/user-attachments/assets/7937883f-74ab-450e-8a96-cf7ce4b8da43>

## How to use

Inject [LuckySpeeder.dylib](https://github.com/kekeimiku/LuckySpeeder/releases) into your IPA file.

Google Search: [How to inject dylib into ipa](https://www.google.com/search?q=How+to+inject+dylib+into+ipa)

PS: If you can use [TrollStore](https://github.com/opa334/TrollStore), [TrollFools](https://github.com/Lessica/TrollFools) is a great choice.

## Tested Games

[WarmSnow](https://apps.apple.com/us/app/warm-snow/id6447508479)

[Hearthstone](https://apps.apple.com/us/app/hearthstone/id625257520)

[Brotato](https://apps.apple.com/us/app/brotato/id6445884925)

[Subway Surfers](https://apps.apple.com/us/app/subway-surfers/id512939461)

[Laya's Horizon](https://apps.apple.com/us/app/layas-horizon/id1615116545)

[Kingdom Rush Tower Defense](https://apps.apple.com/us/app/kingdom-rush-tower-defense-td/id516378985)

[Tap Titans 2 - Hero Legends](https://apps.apple.com/us/app/tap-titans-2-hero-legends/id1120294802)

And more...

## Platform Support

Since v0.0.6, I can no longer test on versions lower than iOS 15.

VisionOS and tvOS are currently experimental.

## Build

### macOS

```bash
bash build.sh arm64-apple-ios
```

### Linux

```bash
wget https://github.com/theos/sdks/releases/download/master-146e41f/iPhoneOS16.5.sdk.tar.xz

tar -xf iPhoneOS16.5.sdk.tar.xz

clang -shared \
    -target arm64-apple-ios13.1 \
    -isysroot iPhoneOS16.5.sdk \
    -fobjc-arc \
    -O3 \
    -flto \
    -fvisibility=hidden \
    -fuse-ld=lld \
    fishhook.c LuckySpeeder.c LuckySpeeder.m LuckySpeederView.m Main.m \
    -framework Foundation \
    -framework UIKit \
    -framework SpriteKit \
    -o LuckySpeeder.dylib

llvm-strip -x LuckySpeeder.dylib
```

## Disclaimer

Use this program at your own risk.
