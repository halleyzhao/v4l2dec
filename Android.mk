LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        v4l2decode.cpp      \
        util.cpp            \
        input/decodeinput.cpp

LOCAL_C_INCLUDES:= \
        $(LOCAL_PATH)/../avformat/ \
        $(LOCAL_PATH)/../../../include/multimedia/ \
        external/libcxx/include \
        $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES := \
        libdl    \
        libutils \
        liblog \
        libc++ \
        libgui

LOCAL_CFLAGS := \
         -DV4L2CODEC_ANDROID_SURFACE    \
         -O2

LOCAL_MODULE := v4l2dec
include $(BUILD_EXECUTABLE)
