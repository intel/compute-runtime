/*
 * Copyright (c) 2017, Intel Corporation
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

struct AubBinaryDump {
    AubCmdHdr Header;
    char OutputFile[40];
    uint32_t Height;
    uint32_t Width;
    uint64_t BaseAddr;
    uint32_t SurfaceType : 4,
        Pitch : 28;
    uint32_t GttType : 2,
        Reserved_31_2 : 30;
    uint32_t DirectoryHandle;
};
static_assert(72 == sizeof(AubBinaryDump), "Invalid size for AubBinaryDump");

#ifndef WIN32
#pragma pack()
#else
#pragma pack(pop)
#endif
