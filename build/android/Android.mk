LOCAL_PATH:= $(call my-dir)/../../src


include $(CLEAR_VARS)

include $(me-root-dir)/build/android/common.mk

ifeq ($(BUILD_VIDEO),1)
LOCAL_SHARED_LIBRARIES += \
	libavcodec \
	libswscale \
	libavcore \
	libavutil
endif

LOCAL_MODULE := libmediaengine

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/cpufeatures)
