#### note to vim & Makefile
# 1. set expandtab for Makefile in ~/.vimrc
#    autocmd FileType make set tabstop=4 shiftwidth=4 softtabstop=0 expandtab
# 2. use tab only at the begining of make command
#    - temp disable expandtab: set noexpandtab
#    - use tab manually

TARGET=v4l2dec
LOCAL_INSTALL_DIR=${HOME}/MM_Local/

LOCAL_BUILD_FLAGS=                      \
    -g                                  \
    -I./                                \
    -I../../../include/multimedia/      \
    -D__ENABLE_AVFORMAT__ `pkg-config --cflags --libs libavformat libavcodec libavutil`

LOCAL_SOURCE_FILES=                 \
    v4l2decode.cpp                  \
    util.cpp                        \
    input/decodeinput.cpp           \
    input/decodeinputavformat.cpp

all:${TARGET}

${TARGET}:
	g++                         \
        ${LOCAL_SOURCE_FILES}   \
        ${LOCAL_BUILD_FLAGS}    \
        -ldl                    \
        -o ${TARGET}

clean:
	rm -rf ${TARGET}

install:
	cp ${TARGET} ${LOCAL_INSTALL_DIR}/usr/bin/

