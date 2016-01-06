// debug use
#include "util.h"
#include <vector>
#include <string>
#include <string.h>
#include <stdint.h>
#include <algorithm>


static std::vector<bool> s_bufferStatus; // true means in surface, false means in v4l2codec

void dbgSetBufferCount(uint32_t count)
{
    uint32_t i;

    for (i=0; i<count; i++) {
        s_bufferStatus.push_back(true);
    }
}

static void dbgDumpBufferStatus()
{
    std::string bufferStatus;
    std::string bufferInSurface, bufferInV4l2codec;
    char t[10];
    uint32_t i;

    for (i = 0; i < s_bufferStatus.size(); i++) {
        sprintf(t, "%d, ", i);
        if (s_bufferStatus[i]) {
            bufferStatus.append("+");
            bufferInSurface.append(t);
        } else {
            bufferStatus.append("-");
            bufferInV4l2codec.append(t);
        }
    }

    DEBUG("buffer status: (%s), buffer in surface: (%s), buffer in v4l2codec: (%s)",
        bufferStatus.c_str(), bufferInSurface.c_str(), bufferInV4l2codec.c_str());

}
void dbgToggleBufferIndex(int index)
{
    // FIXME, lock
    s_bufferStatus[index] = !s_bufferStatus[index];
    dbgDumpBufferStatus();
}

