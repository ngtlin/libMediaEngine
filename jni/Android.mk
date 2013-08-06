# script expect me-root-dir variable to be set by parent !

#default values
ifeq ($(BUILD_AMRNB),)
BUILD_AMRNB=light
endif
ifeq ($(BUILD_AMRWB),)
BUILD_AMRWB=0
endif
ifeq ($(BUILD_G729),)
BUILD_G729=0
endif

ifeq ($(TARGET_ARCH_ABI),armeabi)
$(error "STOP video not supported on target armeabi devices!")
endif


##ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
ifeq ($(BUILD_GPLV3_ZRTP), 1)
	BUILD_SRTP=1
ZRTP_C_INCLUDE= \
	$(me-root-dir)/submodules/externals/libzrtpcpp/src
endif

ifeq ($(BUILD_SRTP), 1)
SRTP_C_INCLUDE= \
	$(me-root-dir)/submodules/externals/srtp \
	$(me-root-dir)/submodules/externals/srtp/include \
	$(me-root-dir)/submodules/externals/srtp/crypto/include
endif
#endif

# Speex
include $(me-root-dir)/submodules/externals/build/speex/Android.mk

# Gsm
include $(me-root-dir)/submodules/externals/build/gsm/Android.mk

# Openssl
include $(me-root-dir)/submodules/externals/openssl/Android.mk

include $(me-root-dir)/submodules/oRTP/build/android/Android.mk

include $(me-root-dir)/submodules/mediastreamer2/build/android/Android.mk
include $(me-root-dir)/submodules/mediastreamer2/tools/Android.mk

ifeq ($(BUILD_SILK), 1)
ifeq (,$(DUMP_VAR))
$(info Build proprietary SILK plugin for mediastreamer2)
endif
include $(me-root-dir)/submodules/mssilk/Android.mk
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
include $(me-root-dir)/submodules/msilbc/Android.mk

ifeq ($(BUILD_X264), 1)
ifeq (,$(DUMP_VAR))
$(info Build X264 plugin for mediastreamer2)
endif
include $(me-root-dir)/submodules/msx264/Android.mk
ifeq ($(wildcard $(me-root-dir)/submodules/externals/prebuilts/x264.mk),)
$(info build X264)
include $(me-root-dir)/submodules/externals/build/x264/Android.mk
else
$(info use prebuilt X264)
include $(me-root-dir)/submodules/externals/prebuilts/x264.mk
endif
endif

ifeq ($(wildcard $(me-root-dir)/submodules/externals/prebuilts/ffmpeg.mk),)
$(info build ffmepg)
include $(me-root-dir)/submodules/externals/build/ffmpeg/Android.mk
include $(me-root-dir)/submodules/externals/build/ffmpeg-no-neon/Android.mk
else
$(info use prebuilt X264)
include $(me-root-dir)/submodules/externals/prebuilts/ffmpeg.mk
endif

include $(me-root-dir)/submodules/externals/build/libvpx/Android.mk
endif #armeabi-v7a


ifeq ($(BUILD_GPLV3_ZRTP), 1)
ifeq (,$(DUMP_VAR))
$(info Build ZRTP support - makes application GPLv3)
endif
ifeq ($(wildcard $(me-root-dir)/submodules/externals/prebuilts/zrtpcpp.mk),)
include $(me-root-dir)/submodules/externals/build/libzrtpcpp/Android.mk
else
include $(me-root-dir)/submodules/externals/prebuilts/zrtpcpp.mk
endif
endif

ifeq ($(BUILD_SRTP), 1)
$(info Build SRTP support)
include $(me-root-dir)/submodules/externals/build/srtp/Android.mk
endif

#ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
include $(me-root-dir)/build/android/Android.mk
#endif
include $(me-root-dir)/build/android/Android-no-neon.mk

_BUILD_AMR=0
ifneq ($(BUILD_AMRNB), 0)
_BUILD_AMR=1
endif

ifneq ($(BUILD_AMRWB), 0)
_BUILD_AMR=1
endif

ifneq ($(_BUILD_AMR), 0)
$(info build AMR)
include $(me-root-dir)/submodules/externals/build/opencore-amr/Android.mk
include $(me-root-dir)/submodules/msamr/Android.mk
endif

ifneq ($(BUILD_AMRWB), 0)
$(info build AMRWB)
include $(me-root-dir)/submodules/externals/build/vo-amrwbenc/Android.mk
endif

ifneq ($(BUILD_G729), 0)
$(info build G729)
include $(me-root-dir)/submodules/bcg729/Android.mk
include $(me-root-dir)/submodules/bcg729/msbcg729/Android.mk
endif

ifneq ($(BUILD_WEBRTC_AECM), 0)
$(info build WebRTC_AECM)
ifneq ($(TARGET_ARCH), x86)
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
WEBRTC_BUILD_NEON_LIBS=true
endif
include $(me-root-dir)/submodules/externals/build/webrtc/system_wrappers/Android.mk
include $(me-root-dir)/submodules/externals/build/webrtc/common_audio/signal_processing/Android.mk
include $(me-root-dir)/submodules/externals/build/webrtc/modules/audio_processing/utility/Android.mk
include $(me-root-dir)/submodules/externals/build/webrtc/modules/audio_processing/aecm/Android.mk
endif
endif
