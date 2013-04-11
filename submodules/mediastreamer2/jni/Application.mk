APP_PROJECT_PATH := $(call my-dir)/../
$(info APP_PRJ_PATH=$(APP_PROJECT_PATH))
APP_MODULES      :=libspeex libgsm libortp libavutil libavcore libavcodec libswscale libvpx libmediastreamer2

APP_STL := stlport_static

# define me-root-dir to media engine base dir
BUILD_MS2 := 1
BUILD_SRTP := 0
me-root-dir:=$(APP_PROJECT_PATH)/../../../

$(info me-root-dir=$(me-root-dir))
APP_BUILD_SCRIPT:=$(me-root-dir)/jni/Android.mk
APP_PLATFORM := android-8
APP_ABI := armeabi-v7a x86
APP_CFLAGS:=-DDISABLE_NEON
