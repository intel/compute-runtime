/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

#ifndef WIN32
#pragma pack(4)
#else
#pragma pack(push, 4)
#endif
namespace AubMemDump {
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
    uint32_t dwordLength : 16,
        subOp : 7,
        opcode : 6,
        type : 3;
};
static_assert(4 == sizeof(AubCmdHdr), "Invalid size for AubCmdHdr");

struct AubCmdDumpBmpHd {
    AubCmdHdr header;
    uint32_t xMin;
    uint32_t yMin;
    uint32_t bufferPitch;
    uint32_t bitsPerPixel : 8,
        format : 8,
        reserved0 : 16;
    uint32_t xSize;
    uint32_t ySize;
    uint64_t baseAddr;
    uint32_t secure : 1,
        useFence : 1,
        tileOn : 1,
        walkY : 1,
        usePPGTT : 1,
        use32BitDump : 1,
        useFullFormat : 1,
        reserved1 : 25;
    uint32_t directoryHandle;
    uint64_t getBaseAddr() const {
        return getMisalignedUint64(&this->baseAddr);
    }
    void setBaseAddr(const uint64_t baseAddr) {
        setMisalignedUint64(&this->baseAddr, baseAddr);
    }
};
static_assert(44 == sizeof(AubCmdDumpBmpHd), "Invalid size for AubCmdDumpBmpHd");
struct AubPpgttContextCreate {
    AubCmdHdr header;
    uint32_t handle;
    uint32_t advancedContext : 1,
        sixtyFourBit : 1,
        reserved31To2 : 30;
    uint64_t pageDirPointer[4];
};
static_assert(44 == sizeof(AubPpgttContextCreate), "Invalid size for AubPpgttContextCreate");

struct AubCaptureBinaryDumpHD {
    AubCmdHdr header;
    uint64_t baseAddr;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint32_t surfaceType : 4,
        gttType : 2,
        reserved31To6 : 26;
    uint32_t directoryHandle;
    uint32_t reservedDW1;
    uint32_t reservedDW2;
    char outputFile[4];
    uint64_t getBaseAddr() const {
        return getMisalignedUint64(&this->baseAddr);
    }
    void setBaseAddr(const uint64_t baseAddr) {
        setMisalignedUint64(&this->baseAddr, baseAddr);
    }
    uint64_t getWidth() const {
        return getMisalignedUint64(&this->width);
    }
    void setWidth(const uint64_t width) {
        setMisalignedUint64(&this->width, width);
    }
    uint64_t getHeight() const {
        return getMisalignedUint64(&this->height);
    }
    void setHeight(const uint64_t height) {
        setMisalignedUint64(&this->height, height);
    }
    uint64_t getPitch() const {
        return getMisalignedUint64(&this->pitch);
    }
    void setPitch(const uint64_t pitch) {
        setMisalignedUint64(&this->pitch, pitch);
    }
};
static_assert(56 == sizeof(AubCaptureBinaryDumpHD), "Invalid size for AubCaptureBinaryDumpHD");
} // namespace AubMemDump
#ifndef WIN32
#pragma pack()
#else
#pragma pack(pop)
#endif
