LOCAL_SRC_FILES := \
         MediaEngine.cpp

LOCAL_CFLAGS += \
	-D_BYTE_ORDER=_LITTLE_ENDIAN \
	-DORTP_INET6 \
	-DINET6 

LOCAL_CFLAGS += -DIN_LINPHONE

ifeq ($(BUILD_VIDEO),1)
LOCAL_CFLAGS += -DVIDEO_ENABLED
ifeq ($(BUILD_X264),1)
LOCAL_CFLAGS += -DHAVE_X264
endif
endif

ifeq ($(USE_JAVAH),1)
LOCAL_CFLAGS += -DUSE_JAVAH
endif

$(info LOCAL_PATH=$(LOCAL_PATH))
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../submodules/oRTP/include \
	$(LOCAL_PATH)/../submodules/mediastreamer2/include \
	$(LOCAL_PATH)/../gen

$(info LOCAL_C_INCLUDES=$(LOCAL_C_INCLUDES))

LOCAL_LDLIBS += -llog -ldl


LOCAL_STATIC_LIBRARIES := \
	cpufeatures \
	libmediastreamer2 \
	libortp \
	libgsm

ifeq ($(BUILD_TUNNEL),1)
LOCAL_CFLAGS +=-DTUNNEL_ENABLED
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../submodules/tunnel/include $(LOCAL_PATH)/../submodules/tunnel/src
LOCAL_SRC_FILES +=  TunnelManager.cc
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_SHARED_LIBRARIES += libtunnelclient
else
LOCAL_STATIC_LIBRARIES += libtunnelclient
endif
endif


_BUILD_AMR=0
ifneq ($(BUILD_AMRNB), 0)
_BUILD_AMR=1
endif

ifneq ($(BUILD_AMRWB), 0)
_BUILD_AMR=1
endif

ifneq ($(_BUILD_AMR), 0)
LOCAL_CFLAGS += -DHAVE_AMR
LOCAL_STATIC_LIBRARIES += \
        libmsamr \
        libopencoreamr
endif

ifneq ($(BUILD_AMRWB), 0)
LOCAL_STATIC_LIBRARIES += \
	libvoamrwbenc
endif


ifeq ($(BUILD_SILK),1)
LOCAL_CFLAGS += -DHAVE_SILK
LOCAL_STATIC_LIBRARIES += libmssilk
endif

ifeq ($(BUILD_G729),1)
LOCAL_CFLAGS += -DHAVE_G729
LOCAL_SHARED_LIBRARIES += libbcg729
LOCAL_STATIC_LIBRARIES += libmsbcg729
endif

ifeq ($(BUILD_VIDEO),1)
LOCAL_LDLIBS    += -lGLESv2
LOCAL_STATIC_LIBRARIES += libvpx
ifeq ($(BUILD_X264),1)
LOCAL_STATIC_LIBRARIES += \
	libmsx264 \
	libx264
endif
endif

LOCAL_STATIC_LIBRARIES += libspeex 

ifeq ($(BUILD_SRTP), 1)
$(error SRTP building included!!!)
	LOCAL_C_INCLUDES += $(SRTP_C_INCLUDE)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS += -DHAVE_ILBC=1
LOCAL_STATIC_LIBRARIES += libmsilbc
endif

LOCAL_C_INCLUDES += $(ME_EXTENDED_C_INCLUDES) 
LOCAL_WHOLE_STATIC_LIBRARIES += $(ME_EXTENDED_STATIC_LIBS)
LOCAL_SRC_FILES  += $(ME_EXTENDED_SRC_FILES)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_SHARED_LIBRARIES += liblinssl liblincrypto
	ifeq ($(BUILD_GPLV3_ZRTP),1)
	LOCAL_SHARED_LIBRARIES += libzrtpcpp
	endif

	ifeq ($(BUILD_SRTP),1)
	LOCAL_SHARED_LIBRARIES += libsrtp
	endif
else
	LOCAL_LDLIBS += -lz
	#LOCAL_STATIC_LIBRARIES += libz libdl
	LOCAL_STATIC_LIBRARIES += \
		libssl-static libcrypto-static
	ifeq ($(BUILD_GPLV3_ZRTP),1)
		LOCAL_STATIC_LIBRARIES += libzrtpcpp-static
	endif

	ifeq ($(BUILD_SRTP),1)
		LOCAL_STATIC_LIBRARIES += libsrtp-static
	endif
endif

