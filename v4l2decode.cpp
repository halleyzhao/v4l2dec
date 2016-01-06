/*
 *  v4l2_encoder.cpp - a h264 encoder basing on v4l2 wrapper interface
 *
 *  Copyright (C) 2011-2014 Intel Corporation
 *    Author: Halley Zhao<halley.zhao@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include  <sys/mman.h>
#include <errno.h>
#include <string>
#include <vector>
#include "v4l2codec_device_ops.h"
#ifdef V4L2CODEC_HOST_SURFACE
#include <dlfcn/dlfcn.h>
#else
#include <dlfcn.h>
#endif
#include <fcntl.h>
#include "input/decodeinput.h"


const uint32_t k_maxInputBufferSize = 1024*1024;
const int k_inputPlaneCount = 1;
const int k_maxOutputPlaneCount = 3;
int outputPlaneCount = 2;
int videoWidth = 0;
int videoHeight = 0;

uint32_t inputQueueCapacity = 0;
uint32_t outputQueueCapacity = 0;
uint32_t k_extraOutputFrameCount = 2;
static std::vector<uint8_t*> inputFrames;

static bool isReadEOS=false;
static int32_t stagingBufferInDevice = 0;
static uint32_t renderFrameCount = 0;

// surface related
uint32_t g_surface_width = 1280;
uint32_t g_surface_height = 720;

#define CHECK_SURFACE_OPS_RET(ret, funcName) do {                       \
    if (ret) {                                                          \
        ERROR("%s failed: %s (%d)", funcName, strerror(errno), errno);  \
        return false;                                                   \
    } else {                                                            \
        INFO("%s done success", funcName);                              \
    }                                                                   \
} while(0)

#ifdef V4L2CODEC_ANDROID_SURFACE
#include <gui/SurfaceComposerClient.h>
#include <va/va_android.h>

sp<SurfaceComposerClient> mClient;
sp<SurfaceControl> mSurfaceCtl;
sp<Surface> mSurface;
sp<ANativeWindow> nativeWindow;

std::vector <ANativeWindowBuffer*> mWindBuff;
#define GET_ANATIVEWINDOW(w) w.get()
#define CAST_ANATIVEWINDOW(w)   (w)

int createNativeWindow(__u32 pixelformat)
{
    int ret = 0;
    mClient = new SurfaceComposerClient();
    mSurfaceCtl = mClient->createSurface(String8("testsurface"),
                    g_surface_width, g_surface_height, pixelformat, 0);

    // configure surface
    SurfaceComposerClient::openGlobalTransaction();
    mSurfaceCtl->setLayer(100000);
    mSurfaceCtl->setPosition(100, 100);
    mSurfaceCtl->setSize(g_surface_width, g_surface_height);
    SurfaceComposerClient::closeGlobalTransaction();

    mSurface = mSurfaceCtl->getSurface();
    nativeWindow = mSurface;

    ret = native_window_set_scaling_mode(nativeWindow.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    CHECK_SURFACE_OPS_RET(ret, "native_window_set_scaling_mode");

    return ret;
}
#elif defined(V4L2CODEC_HOST_SURFACE)
#include <WindowSurface.h>
#include "WindowSurfaceTestWindow.h"
#define WindowSurface Surface
bool g_animate_win = false;
bool g_use_sub_surface = false;

WaylandConnection* wl_connection = NULL;
TestWindow* test_window = NULL;
WindowSurface *nativeWindow = NULL;
std::vector <ANativeWindowBuffer*> mWindBuff;

#define GET_ANATIVEWINDOW(w)    (static_cast<ANativeWindow *>(w))
#define CAST_ANATIVEWINDOW(w)   (static_cast<ANativeWindow *>(w))

int initSubWindow(WindowSurface **nativeWindow);
int createNativeWindow(__u32 pixelformat)
{
    int ret = 0;
    if (g_use_sub_surface)
        initSubWindow(&nativeWindow);
    else {
        wl_connection = new WaylandConnection(NULL);
        test_window = new TestWindow(wl_connection);
        nativeWindow = new WindowSurface(wl_connection, test_window, g_surface_width, g_surface_width);
    }

    if (!nativeWindow) {
        ERROR("init sub surface fail");
        return -1;
    }

    return ret;
}
#endif

static struct V4l2CodecOps s_v4l2CodecOps;
static int32_t s_v4l2Fd = 0;

static bool loadV4l2CodecDevice(const char* libName )
{
    void * handle;

    memset(&s_v4l2CodecOps, 0, sizeof(s_v4l2CodecOps));
    s_v4l2Fd = 0;

#ifdef V4L2CODEC_ANDROID_SURFACE
    handle = dlopen(libName, RTLD_NOW | RTLD_GLOBAL);
#elif defined(V4L2CODEC_HOST_SURFACE)
    handle = hybris_dlopen(libName, RTLD_NOW | RTLD_GLOBAL);
#else
    handle = dlopen(libName, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
#endif

    if (!handle) {
      ERROR("Failed to load %s", libName);
      return false;
    }

    V4l2codecOperationInitFunc initFunc = NULL;
#ifdef V4L2CODEC_HOST_SURFACE
    initFunc = (V4l2codecOperationInitFunc)hybris_dlsym(handle, "v4l2codecOperationInit");
#else
    initFunc = (V4l2codecOperationInitFunc)dlsym(handle, "v4l2codecOperationInit");
#endif
    if (!initFunc) {
        ERROR("fail to dlsym v4l2codecOperationInit");
        return false;
    }

    INIT_V4L2CODEC_OPS_SIZE_VERSION(&s_v4l2CodecOps);
    if (!initFunc(&s_v4l2CodecOps)) {
        ERROR("fail to init v4l2 device operation func pointers");
        return false;
    }

    int isVersionMatch = 0;
    IS_V4L2CODEC_OPS_VERSION_MATCH(s_v4l2CodecOps.mVersion, isVersionMatch);
    if (!isVersionMatch) {
        ERROR("V4l2CodecOps interface version doesn't match");
        return false;
    }
    if(s_v4l2CodecOps.mSize != sizeof(V4l2CodecOps)) {
        ERROR("V4l2CodecOps interface data structure size doesn't match");
        return false;
    }

    return true;
}

bool feedOneInputFrame(DecodeInput * input, int fd, int index = -1 /* if index is not -1, simple enque it*/)
{

    struct v4l2_buffer buf;
    struct v4l2_plane planes[k_inputPlaneCount];
    int ioctlRet = -1;

    memset(&buf, 0, sizeof(buf));
    memset(&planes, 0, sizeof(planes));
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; // it indicates input buffer(raw frame) type
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = k_inputPlaneCount;

    if (index == -1) {
        ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_DQBUF, &buf);
        if (ioctlRet == -1)
            return true;
        stagingBufferInDevice --;
    } else {
        buf.index = index;
    }

    if (isReadEOS)
        return false;

    uint8_t *data = NULL;
    uint32_t size = 0;
    int64_t timeStamp = -1;
    uint32_t flags = 0;
    bool ret = input->getNextDecodeUnit(data, size, timeStamp, flags);
    if (ret) {
        ASSERT(size <= k_maxInputBufferSize);
        memcpy(inputFrames[buf.index], data, size);
        buf.m.planes[0].bytesused = size;
        buf.m.planes[0].m.mem_offset = 0;
        buf.timestamp.tv_sec = timeStamp;
        // buf.flags = ;
    } else {
        // send empty buffer for EOS
        buf.m.planes[0].bytesused = 0;
        isReadEOS = true;
    }

    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_QBUF, &buf);
    ASSERT(ioctlRet != -1);

    stagingBufferInDevice ++;
    return true;
}

static bool displayOneVideoFrame(uint32_t &index)
{
    int ret = 0;
    INFO("displayOneVideoFrame");

#if (defined(V4L2CODEC_ANDROID_SURFACE) || defined(V4L2CODEC_HOST_SURFACE))
    DEBUG("enque buffer index to WindowSurface: %d", index);
    ret = CAST_ANATIVEWINDOW(nativeWindow)->queueBuffer(GET_ANATIVEWINDOW(nativeWindow), mWindBuff[index], -1);
    CHECK_SURFACE_OPS_RET(ret, "queueBuffer");
    dbgToggleBufferIndex(index);
    #ifdef V4L2CODEC_HOST_SURFACE
    nativeWindow->finishSwap();
    #endif

    ANativeWindowBuffer* pbuf = NULL;
    ret = native_window_dequeue_buffer_and_wait(GET_ANATIVEWINDOW(nativeWindow), &pbuf);
    CHECK_SURFACE_OPS_RET(ret, "native_window_dequeue_buffer_and_wait");

    // FIXME, halley add it
    uint32_t i = 0;
    for (i=0; i<mWindBuff.size(); i++) {
        if (pbuf == mWindBuff[i]) {
            index = i;
            break;
        }
    }
    DEBUG("dequeue buffer index from WindowSurface: %d", index);
#endif

    return true;
}


bool takeOneOutputFrame(int fd, int index = -1/* if index is not -1, simple enque it*/)
{
    struct v4l2_buffer buf;
    struct v4l2_plane planes[k_maxOutputPlaneCount]; // YUV output, in fact, we use NV12 of 2 planes
    int ioctlRet = -1;
    bool ret = true;

    INFO("takeOneOutputFrame");
    memset(&buf, 0, sizeof(buf));
    memset(&planes, 0, sizeof(planes));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; //it indicates output buffer type
    buf.memory = V4L2_MEMORY_MMAP; // chromeos v4l2vea uses this mode only
    buf.m.planes = planes;
    buf.length = outputPlaneCount;

    if (index == -1) {
        ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_DQBUF, &buf);
        if (ioctlRet == -1) {
            WARNING("deque output buffer failed");
            return false;
        }

        renderFrameCount++;
        INFO("got output frame count: %d", renderFrameCount);
        ret = displayOneVideoFrame(buf.index);
        ASSERT(ret);
    } else {
        buf.index = index;
    }

    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_QBUF, &buf);
    ASSERT(ioctlRet != -1);
    INFO("enque buf index to v4l2codec: %d", buf.index);
    dbgToggleBufferIndex(buf.index);
    return true;
}

bool handleResolutionChange(int32_t fd)
{
    bool resolutionChanged = false;
    // check resolution change
    struct v4l2_event ev;
    memset(&ev, 0, sizeof(ev));

    INFO("handleResolutionChange");
    while (s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_DQEVENT, &ev) == 0) {
        if (ev.type == V4L2_EVENT_RESOLUTION_CHANGE) {
            resolutionChanged = true;
            break;
        }
    }

    if (!resolutionChanged)
        return false;

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_G_FMT, &format) == -1) {
        return false;
    }

    DEBUG("got video resolution");
    // resolution and pixelformat got here
    outputPlaneCount = format.fmt.pix_mp.num_planes;
    ASSERT(outputPlaneCount == 2);
    videoWidth = format.fmt.pix_mp.width;
    videoHeight = format.fmt.pix_mp.height;

    return true;
}

struct FormatEntry {
    uint32_t format;
    const char* mime;
};

static const FormatEntry FormatEntrys[] = {
    {V4L2_PIX_FMT_H264, MY_MIME_H264},
};

uint32_t v4l2PixelFormatFromMime(const char* mime)
{
    uint32_t format = 0;
    uint32_t count = sizeof(FormatEntrys)/sizeof(FormatEntrys[0]);
    uint32_t i = 0;

    for (i = 0; i < count; i++) {
        const FormatEntry* entry = FormatEntrys + i;
        if (!strcmp(mime, entry->mime)) {
            format = entry->format;
            break;
        }
    }
    return format;
}

void usage(void)
{
    ERROR("-i filename");
}
int main(int argc, char** argv)
{
    DecodeInput *input;
    int32_t fd = -1;
    uint32_t i = 0;
    int32_t ioctlRet = -1;
    std::string inputFileName;

    // FIXME, use libv4l2codec_hw.so instead
    if (!loadV4l2CodecDevice("libyami_v4l2.so")) {
        ERROR("fail to init v4l2codec device with __ENABLE_V4L2_OPS__");
        return -1;
    }

    char opt;
    while ((opt = getopt(argc, argv, "hi:?")) != -1)
    {
        switch (opt) {
        case 'i':
            DEBUG("optarg: %s", optarg);
            inputFileName = optarg;
            break;
        case 'h':
        case '?':
        default:
            usage();
            return false;
        }
    }
    DEBUG("inputFileName: %s", inputFileName.c_str());
    if (inputFileName.empty()) {
        fprintf(stderr, "no input media file specified");
        return -1;
    }

    INFO("input file: %s", inputFileName.c_str());

    // FIXME, set memoryType
    input = DecodeInput::create(inputFileName.c_str());
    if (input==NULL) {
        ERROR("fail to init input stream");
        return -1;
    }

    renderFrameCount = 0;
    // open device
    fd = s_v4l2CodecOps.mOpenFunc("decoder", 0);
    ASSERT(fd!=-1);

    // set output frame memory type
#if (defined(V4L2CODEC_ANDROID_SURFACE) || defined(V4L2CODEC_HOST_SURFACE))
    s_v4l2CodecOps.mSetParameterFunc(fd, "frame-memory-type", "android-native-buffer");
#else
    s_v4l2CodecOps.mSetParameterFunc(fd, "frame-memory-type", "drm-name");
#endif

    // query hw capability
    struct v4l2_capability caps;
    memset(&caps, 0, sizeof(caps));
    caps.capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_QUERYCAP, &caps);
    ASSERT(ioctlRet != -1);

    // set input/output data format
    uint32_t codecFormat = v4l2PixelFormatFromMime(input->getMimeType());
    if (!codecFormat) {
        ERROR("unsupported mime: %s", input->getMimeType());
        return -1;
    }

    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    format.fmt.pix_mp.pixelformat = codecFormat;
    format.fmt.pix_mp.num_planes = 1;
    format.fmt.pix_mp.plane_fmt[0].sizeimage = k_maxInputBufferSize;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_S_FMT, &format);
    ASSERT(ioctlRet != -1);

    // set preferred output format
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    uint8_t *data = NULL;
    uint32_t size = 0;
    if (input->getCodecData(data, size)) {
        //save codecdata, size+data, the type of format.fmt.raw_data is __u8[200]
        //we must make sure enough space (>=sizeof(uint32_t) + size) to store codecdata
        memcpy(format.fmt.raw_data, &size, sizeof(uint32_t));
        if(sizeof(format.fmt.raw_data) >= size + sizeof(uint32_t))
            memcpy(format.fmt.raw_data + sizeof(uint32_t), data, size);
        else {
            ERROR("No enough space to store codec data");
            return -1;
        }
        ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_S_FMT, &format);
        ASSERT(ioctlRet != -1);
    }
    // input port starts as early as possible to decide output frame format
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_STREAMON, &type);
    ASSERT(ioctlRet != -1);

    // setup input buffers
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    reqbufs.count = 2;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_REQBUFS, &reqbufs);
    ASSERT(ioctlRet != -1);
    ASSERT(reqbufs.count>0);
    inputQueueCapacity = reqbufs.count;
    inputFrames.resize(inputQueueCapacity);

    for (i=0; i<inputQueueCapacity; i++) {
        struct v4l2_plane planes[k_inputPlaneCount];
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(buffer));
        memset(planes, 0, sizeof(planes));
        buffer.index = i;
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.m.planes = planes;
        buffer.length = k_inputPlaneCount;
        ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_QUERYBUF, &buffer);
        ASSERT(ioctlRet != -1);

        // length and mem_offset should be filled by VIDIOC_QUERYBUF above
        void* address = s_v4l2CodecOps.mMmapFunc(NULL,
                                      buffer.m.planes[0].length,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd,
                                      buffer.m.planes[0].m.mem_offset);
        ASSERT(address);
        inputFrames[i] = static_cast<uint8_t*>(address);
        DEBUG("inputFrames[%d] = %p", i, inputFrames[i]);
    }

    // feed input frames first
    for (i=0; i<inputQueueCapacity; i++) {
        if (!feedOneInputFrame(input, fd, i)) {
            break;
        }
    }

    // query video resolution
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    while (1) {
        if (s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_G_FMT, &format) != 0) {
            if (errno != EINVAL) {
                // EINVAL means we haven't seen sufficient stream to decode the format.
                INFO("ioctl() failed: VIDIOC_G_FMT, haven't get video resolution during start yet, waiting");
            }
        } else {
            break;
        }
        usleep(50);
    }
    outputPlaneCount = format.fmt.pix_mp.num_planes;
    ASSERT(outputPlaneCount == 2);
    videoWidth = format.fmt.pix_mp.width;
    videoHeight = format.fmt.pix_mp.height;
    ASSERT(videoWidth && videoHeight);

    __u32 pixelformat = format.fmt.pix_mp.pixelformat;
    DEBUG("got video size %dx%d with pixelformat: %d", videoWidth, videoHeight, pixelformat);

#if (defined(V4L2CODEC_ANDROID_SURFACE) || defined(V4L2CODEC_HOST_SURFACE))
    int ret = 0;
    ret = createNativeWindow(pixelformat);
    CHECK_SURFACE_OPS_RET(ret, "createNativeWindow");

    // configure native window
    ASSERT(videoWidth>0 && videoHeight>0);
    ret = native_window_set_buffers_dimensions(GET_ANATIVEWINDOW(nativeWindow), videoWidth, videoHeight);
    CHECK_SURFACE_OPS_RET(ret, "native_window_set_buffers_dimensions");

    // FIXME, the mFormat is got from v4l2codec, and set to WindowSurface;
    // from v4l2/videodev2.h: it is fourcc/uint32_t, but android::surface uses enum from OMX definition
    INFO("native window set buffers format %d", pixelformat);
    ret = native_window_set_buffers_format(GET_ANATIVEWINDOW(nativeWindow), pixelformat);
    CHECK_SURFACE_OPS_RET(ret, "native_window_set_buffers_format");

    // FIXME, getGraphicBufferUsage() from v4l2codec
    ret = native_window_set_usage(GET_ANATIVEWINDOW(nativeWindow), GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);
    CHECK_SURFACE_OPS_RET(ret, "native_window_set_usage");

    android_native_rect_t crop;
    crop.top = crop.left = 0;
    crop.right = videoWidth;
    crop.bottom = videoHeight;
    ret = native_window_set_crop(GET_ANATIVEWINDOW(nativeWindow), &crop);
    CHECK_SURFACE_OPS_RET(ret, "native_window_set_crop");

    int minUndequeuedBuffs = 0;
    ret = CAST_ANATIVEWINDOW(nativeWindow)->query(GET_ANATIVEWINDOW(nativeWindow), NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBuffs);
    CHECK_SURFACE_OPS_RET(ret, "nativeWindow->query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS)");
    DEBUG("minUndequeuedBuffs: %d", minUndequeuedBuffs);
#endif

    // setup output buffers
    // Number of output buffers we need.
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_G_CTRL, &ctrl);
    DEBUG("buffer count required from v4l2code; %d", ctrl.value);
    uint32_t minOutputFrameCount = ctrl.value + k_extraOutputFrameCount;
#if defined(V4L2CODEC_ANDROID_SURFACE) || defined(V4L2CODEC_HOST_SURFACE)
    minOutputFrameCount += minUndequeuedBuffs;
#endif

    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    reqbufs.count = minOutputFrameCount;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_REQBUFS, &reqbufs);
    ASSERT(ioctlRet != -1);
    ASSERT(reqbufs.count>0);
    outputQueueCapacity = reqbufs.count;
    DEBUG("outputQueueCapacity(buffer count): %d", outputQueueCapacity);

    dbgSetBufferCount(outputQueueCapacity);
#if defined(V4L2CODEC_ANDROID_SURFACE) || defined(V4L2CODEC_HOST_SURFACE)
    struct v4l2_buffer buffer;

    ret = native_window_set_buffer_count(GET_ANATIVEWINDOW(nativeWindow), outputQueueCapacity);
    CHECK_SURFACE_OPS_RET(ret, "native_window_set_buffer_count");

    //queue buffs
    for (i = 0; i < outputQueueCapacity; i++) {
        ANativeWindowBuffer* pbuf = NULL;
        memset(&buffer, 0, sizeof(buffer));

        ret = native_window_dequeue_buffer_and_wait(GET_ANATIVEWINDOW(nativeWindow), &pbuf);
        CHECK_SURFACE_OPS_RET(ret, "native_window_dequeue_buffer_and_wait");

        buffer.m.userptr = (unsigned long)pbuf;
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buffer.index = i;

        ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_QBUF, &buffer);
        ASSERT(ioctlRet != -1);
        mWindBuff.push_back(pbuf);
        dbgToggleBufferIndex(i);
    }

    for (i = 0; (int32_t)i < minUndequeuedBuffs; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

        ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_DQBUF, &buffer);
        ASSERT(ioctlRet != -1);

        INFO("cancelBuffer, buffer.index: %d", buffer.index);
        ret = CAST_ANATIVEWINDOW(nativeWindow)->cancelBuffer(GET_ANATIVEWINDOW(nativeWindow), mWindBuff[buffer.index], -1);
        CHECK_SURFACE_OPS_RET(ret, "cancelBuffer");
        dbgToggleBufferIndex(buffer.index);
    }
#else
    INFO("else case");
    // feed output frames first
    for (i=0; i<outputQueueCapacity; i++) {
        if (!takeOneOutputFrame(fd, i)) {
            ASSERT(0);
        }
    }
#endif

    // output port starts as late as possible to adopt user provide output buffer
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_STREAMON, &type);
    ASSERT(ioctlRet != -1);

    bool event_pending=true; // try to get video resolution.
    uint32_t dqCountAfterEOS = 0;
    do {
        if (event_pending) {
            handleResolutionChange(fd);
        }

        takeOneOutputFrame(fd);
        if (!feedOneInputFrame(input, fd)) {
            if (stagingBufferInDevice == 0)
                break;
            dqCountAfterEOS++;
        }
        if (dqCountAfterEOS == inputQueueCapacity)  // input drain
            break;
    } while (s_v4l2CodecOps.mPollFunc(fd, true, &event_pending) == 0);

    // drain output buffer
    int retry = 3;
    while (takeOneOutputFrame(fd) || (--retry)>0) { // output drain
        usleep(10000);
    }

    // s_v4l2CodecOps.mMunmapFunc(void* addr, size_t length)

    // release queued input/output buffer
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    reqbufs.count = 0;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_REQBUFS, &reqbufs);
    ASSERT(ioctlRet != -1);

    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    reqbufs.count = 0;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_REQBUFS, &reqbufs);
    ASSERT(ioctlRet != -1);

    // stop input port
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_STREAMOFF, &type);
    ASSERT(ioctlRet != -1);

    // stop output port
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctlRet = s_v4l2CodecOps.mIoctlFunc(fd, VIDIOC_STREAMOFF, &type);
    ASSERT(ioctlRet != -1);

#ifdef V4L2CODEC_ANDROID_SURFACE
    //TODO, some resources need to destroy?
#elif defined(V4L2CODEC_HOST_SURFACE)

#endif

    // close device
    ioctlRet = s_v4l2CodecOps.mCloseFunc(fd);
    ASSERT(ioctlRet != -1);

    if(input)
        delete input;

#ifdef V4L2CODEC_ANDROID_SURFACE
    INFO("decode with android surface done");
#elif defined(V4L2CODEC_HOST_SURFACE)
    INFO("decode with host surface done");
#else
    INFO("decode done");
#endif

    return 0;
}

