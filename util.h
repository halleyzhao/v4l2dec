#include <stdio.h>
#include <stdint.h>
#include <list>
#include <string>
#include <algorithm>

#include <sys/syscall.h>
#include <unistd.h>
#define GETTID()    syscall(__NR_gettid)

#define DISALLOW_COPY_AND_ASSIGN(_class) \
private: \
    _class(const _class &); \
    _class & operator=(const _class &);
#define SHORT_FILE_NAME     (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/')+1 : __FILE__)
#define ERROR(format, ...)  fprintf(stderr, "v4l2dec %ld (%s, %s, %d): " format"\n", (long int)GETTID(), SHORT_FILE_NAME, __func__, __LINE__, ##__VA_ARGS__)
#define WARNING(format, ...)  fprintf(stderr, "v4l2dec %ld (%s, %s, %d): " format"\n", (long int)GETTID(), SHORT_FILE_NAME, __func__, __LINE__, ##__VA_ARGS__)
#define INFO(format, ...)  fprintf(stderr, "v4l2dec %ld (%s, %s, %d): " format"\n", (long int)GETTID(), SHORT_FILE_NAME, __func__, __LINE__, ##__VA_ARGS__)
#define DEBUG(format, ...)  fprintf(stderr, "v4l2dec %ld (%s, %s, %d): " format"\n", (long int)GETTID(), SHORT_FILE_NAME, __func__, __LINE__, ##__VA_ARGS__)
#define ASSERT(expr) do {                       \
        if (!(expr)) {                          \
            ERROR();                            \
            assert(0 && (expr));                \
        }                                       \
    } while(0)

#define MY_MIME_H264    "video/h264"
#define MY_MIME_H265    "video/h265"
#define MY_MIME_VP8     "video/vp8"
#define MY_MIME_VP9     "video/vp9"
#define MY_MIME_JPEG    "image/jpeg"
#define MY_FOURCC(ch0, ch1, ch2, ch3) \
        ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
         ((uint32_t)(uint8_t)(ch2) << 16)  | ((uint32_t)(uint8_t)(ch3) << 24))

void dbgSetBufferCount(uint32_t count);
void dbgToggleBufferIndex(int index);
