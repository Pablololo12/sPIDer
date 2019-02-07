CROSS_COMPILE=arm-linux-musleabi- make || exit
mv user_governor bin/armeabi
CROSS_COMPILE=aarch64-linux-musl- make || exit
mv user_governor bin/arm64
