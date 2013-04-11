LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libmsbcg729


LOCAL_SRC_FILES = bcg729_enc.c bcg729_dec.c 


LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../oRTP/include \
	$(LOCAL_PATH)/../../mediastreamer2/include \
	$(LOCAL_PATH)/../include
LOCAL_SHARED_LIBRARIES = libbcg729
include $(BUILD_STATIC_LIBRARY)


