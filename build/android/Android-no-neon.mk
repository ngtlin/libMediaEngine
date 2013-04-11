LOCAL_PATH:= $(call my-dir)/../../src


include $(CLEAR_VARS)

include $(me-root-dir)/build/android/common.mk

ifeq ($(BUILD_VIDEO),1)
LOCAL_SHARED_LIBRARIES += \
	libavcodecnoneon \
	libswscale \
	libavcore \
	libavutil
endif

LOCAL_MODULE := libmediaenginenoneon
ifeq ($(TARGET_ARCH_ABI),armeabi)
LOCAL_MODULE_FILENAME := libmediaenginearmv5
endif

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/cpufeatures)


