/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#pragma once

#include <cstdint>
#include <type_traits>

#ifndef WIN32
#pragma pack(4)
#else
#pragma pack(push, 4)
#endif

inline void setMisalignedUint64(uint64_t *address, const uint64_t value) {
    uint32_t *addressBits = reinterpret_cast<uint32_t *>(address);
    addressBits[0] = static_cast<uint32_t>(value);
    addressBits[1] = static_cast<uint32_t>(value >> 32);
}

inline uint64_t getMisalignedUint64(const uint64_t *address) {
    const uint32_t *addressBits = reinterpret_cast<const uint32_t *>(address);
    return static_cast<uint64_t>(static_cast<uint64_t>(addressBits[1]) << 32) | addressBits[0];
}

struct AubCmdHdr {
    uint32_t DwordLength : 16,
        SubOp : 7,
        Opcode : 6,
        Type : 3;
};
static_assert(4 == sizeof(AubCmdHdr), "Invalid size for AubCmdHdr");

struct AubCmdDumpBmpHd {
    AubCmdHdr Header;
    uint32_t Xmin;
    uint32_t Ymin;
    uint32_t BufferPitch;
    uint32_t BitsPerPixel : 8,
        Format : 8,
        Reserved_0 : 16;
    uint32_t Xsize;
    uint32_t Ysize;
    uint64_t BaseAddr;
    uint32_t Secure : 1,
        UseFence : 1,
        TileOn : 1,
        WalkY : 1,
        UsePPGTT : 1,
        Use32BitDump : 1,
        UseFullFormat : 1,
        Reserved_1 : 25;
    uint32_t DirectoryHandle;
    uint64_t getBaseAddr() const {
        return getMisalignedUint64(&this->BaseAddr);
    }
    void setBaseAddr(const uint64_t baseAddr) {
        setMisalignedUint64(&this->BaseAddr, baseAddr);
    }
};
static_assert(44 == sizeof(AubCmdDumpBmpHd), "Invalid size for AubCmdDumpBmpHd");

struct AubPpgttContextCreate {
    AubCmdHdr Header;
    uint32_t Handle;
    uint32_t AdvancedContext : 1,
        SixtyFourBit : 1,
        Reserved_31_2 : 30;
    uint64_t PageDirPointer[4];
};
static_assert(44 == sizeof(AubPpgttContextCreate), "Invalid size for AubPpgttContextCreate");

struct AubCaptureBinaryDumpHD {
    AubCmdHdr Header;
    uint64_t BaseAddr;
    uint64_t Width;
    uint64_t Height;
    uint64_t Pitch;
    uint32_t SurfaceType : 4,
        GttType : 2,
        Reserved_31_6 : 26;
    uint32_t DirectoryHandle;
    uint32_t ReservedDW1;
    uint32_t ReservedDW2;
    char OutputFile[4];
    uint64_t getBaseAddr() const {
        return getMisalignedUint64(&this->BaseAddr);
    }
    void setBaseAddr(const uint64_t baseAddr) {
        setMisalignedUint64(&this->BaseAddr, baseAddr);
    }
    uint64_t getWidth() const {
        return getMisalignedUint64(&this->Width);
    }
    void setWidth(const uint64_t width) {
        setMisalignedUint64(&this->Width, width);
    }
    uint64_t getHeight() const {
        return getMisalignedUint64(&this->Height);
    }
    void setHeight(const uint64_t height) {
        setMisalignedUint64(&this->Height, height);
    }
    uint64_t getPitch() const {
        return getMisalignedUint64(&this->Pitch);
    }
    void setPitch(const uint64_t pitch) {
        setMisalignedUint64(&this->Pitch, pitch);
    }
};
static_assert(56 == sizeof(AubCaptureBinaryDumpHD), "Invalid size for AubCaptureBinaryDumpHD");

#ifndef WIN32
#pragma pack()
#else
#pragma pack(pop)
#endif
