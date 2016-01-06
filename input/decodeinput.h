/*
 *  decodeinput.h - decode test input
 *
 *  Copyright (C) 2011-2014 Intel Corporation
 *    Author: Changzhi Wei<changzhix.wei@intel.com>
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
#ifndef decodeinput_h
#define decodeinput_h

#include <stdint.h>
#include <assert.h>
#include "util.h"

class DecodeInput {
public:
    DecodeInput();
    virtual ~DecodeInput() {}
    static DecodeInput * create(const char* fileName);
    virtual bool isEOS() = 0;
    virtual const char * getMimeType() = 0;
    virtual bool getNextDecodeUnit(uint8_t* &data, uint32_t &size, int64_t &timeStamp, uint32_t &flags) = 0;
    virtual bool getCodecData(uint8_t* &data, uint32_t &size) = 0;
    virtual bool getResolution(uint32_t &width, uint32_t &height);

protected:
    virtual bool initInput(const char* fileName) = 0;
    virtual void setResolution(uint32_t width, uint32_t height);
    uint16_t m_width;
    uint16_t m_height;

};
#endif

