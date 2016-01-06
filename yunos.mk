LOCAL_PATH:=$(call my-dir)

#### v4l2dec
include $(CLEAR_VARS)
## FIXME, remove it
include $(YUNOS_ROOT)/framework/services/multimedia/src/cow/build/cow_common.mk
## FIXME, move WindowSurfaceTestWindow.cc/simple-subwindow.cpp to a common folder
LOCAL_SRC_FILES := v4l2decode.cpp                               \
                   ./util.cpp                                   \
                   ./input/decodeinput.cpp                      \
                   ./input/decodeinputavformat.cpp              \
                   $(YUNOS_ROOT)/framework/services/multimedia/src/ext/WindowSurface/WindowSurfaceTestWindow.cc   \
                   $(YUNOS_ROOT)/framework/services/multimedia/src/ext/WindowSurface/simple-subwindow.cpp

LOCAL_C_INCLUDES += \
    $(WINDOWSURFACE_INCLUDE) \
    $(HYBRIS_EGLPLATFORMCOMMON) \
    $(MM_MEDIACODEC_INCLUDE) \
    $(ANDROID_INCLUDE) \
    $(LIBAV_INCLUDE) \
    $(YUNOS_ROOT)/hal/hardware/libhybris/hybris/include/hybris \
    $(YUNOS_ROOT)/hal/hardware/libhybris/hybris/include \
    $(YUNOS_ROOT)/framework/libs/gui/window/lib/pageui/includes \
    $(YUNOS_ROOT)/framework/services/multimedia/src/ext/WindowSurface

LOCAL_LDFLAGS += -L$(XMAKE_BUILD_OUT)/target/rootfs/usr/lib/compat

LOCAL_CPPFLAGS += -DANDROID -DMP_HYBRIS_EGLPLATFORM="\"wayland\"" -fpermissive -DANDROID_HAL
LOCAL_CPPFLAGS += -DV4L2CODEC_HOST_SURFACE -D__ENABLE_AVFORMAT__
LOCAL_CPPFLAGS += -Wno-deprecated-declarations -Wno-invalid-offsetof

LOCAL_LDFLAGS += -lpthread -lpagewindow2 -lwayland-cursor -lwayland-egl -lwayland-server -lwayland-client \
                 -lhybris-common -ldl -lgui -lsurface -lstdc++ -lbase -llog
LOCAL_LDFLAGS += -lavformat -lavcodec -lavutil

LOCAL_REQUIRED_MODULES += libpthread-stubs wayland android-core containerd libhybris surface pageui

LOCAL_MODULE := v4l2dec

include $(BUILD_EXECUTABLE)

