TARGET = benchmark
NDK =  /Users/pablo/Library/Android/sdk/ndk-bundle
ANDROID_SYSROOT = /Users/pablo/Library/Android/sdk/ndk-bundle/platforms/android-23

FLAGS = -O3 -fPIE -pie \
		-isystem $(NDK)/sysroot/usr/include/ \
		-isystem $(NDK)/sources/cxx-stl/gnu-libstdc++/4.9/include

FLAGSARM = -mfloat-abi=softfp -mfpu=neon \
		-isystem $(NDK)/sysroot/usr/include/arm-linux-androideabi \
		-isystem $(NDK)/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi/include \
		--sysroot=$(ANDROID_SYSROOT)/arch-arm
		
FLAGSARM64 = -isystem $(NDK)/sysroot/usr/include/aarch64-linux-android \
			-isystem $(NDK)/sources/cxx-stl/gnu-libstdc++/4.9/libs/arm64-v8a/include \
			--sysroot=$(ANDROID_SYSROOT)/arch-arm64

CCARM64 = $(NDK)/toolchains/aarch64-linux-android-4.9/prebuilt/darwin-x86_64/bin/aarch64-linux-android-g++
CCARM = $(NDK)/toolchains/arm-linux-androideabi-4.9/prebuilt/darwin-x86_64/bin/arm-linux-androideabi-g++

LIBS=-lm "$(NDK)/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi/libgnustl_static.a"
LIBS64=-lm "$(NDK)/sources/cxx-stl/gnu-libstdc++/4.9/libs/arm64-v8a/libgnustl_static.a"

.PHONY: all clean
 
all:
	$(CCARM) $(FLAGS) $(FLAGSARM) $(REPORT_FLAG) $(TARGET).cpp $(LIBS) -o bin/armeabi/$(TARGET)
	$(CCARM64) $(FLAGS) $(FLAGSARM64) $(REPORT_FLAG) $(TARGET).cpp $(LIBS64) -o bin/arm64/$(TARGET)

clean:
	rm -f $(TARGET)
