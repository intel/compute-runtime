/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/common_types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace NEO {

struct alignas(1) ImplicitArgsHeader {
    uint8_t structSize;
    uint8_t structVersion;
};

static_assert(sizeof(ImplicitArgsHeader) == (2 * sizeof(uint8_t)));

struct alignas(32) ImplicitArgsV0 {
    ImplicitArgsHeader header;
    uint8_t numWorkDim;
    uint8_t simdWidth;
    uint32_t localSizeX;
    uint32_t localSizeY;
    uint32_t localSizeZ;
    uint64_t globalSizeX;
    uint64_t globalSizeY;
    uint64_t globalSizeZ;
    uint64_t printfBufferPtr;
    uint64_t globalOffsetX;
    uint64_t globalOffsetY;
    uint64_t globalOffsetZ;
    uint64_t localIdTablePtr;
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
    uint32_t padding0;
    uint64_t rtGlobalBufferPtr;
    uint64_t assertBufferPtr;
    uint8_t reserved[16];

    static constexpr uint8_t getSize() { return static_cast<uint8_t>((offsetof(ImplicitArgsV0, reserved))); }
    static constexpr uint8_t getAlignedSize() {
        return static_cast<uint8_t>(alignUp(sizeof(ImplicitArgsV0), 64));
    }
};

static_assert(std::alignment_of_v<ImplicitArgsV0> == 32, "Implicit args size need to be aligned to 32");
static_assert(sizeof(ImplicitArgsV0) == (32 * sizeof(uint32_t)));
static_assert(ImplicitArgsV0::getSize() == (28 * sizeof(uint32_t)));
static_assert(NEO::TypeTraits::isPodV<ImplicitArgsV0>);

struct alignas(32) ImplicitArgsV1 {
    ImplicitArgsHeader header;
    uint8_t numWorkDim;
    uint8_t padding0;
    uint32_t localSizeX;
    uint32_t localSizeY;
    uint32_t localSizeZ;
    uint64_t globalSizeX;
    uint64_t globalSizeY;
    uint64_t globalSizeZ;
    uint64_t printfBufferPtr;
    uint64_t globalOffsetX;
    uint64_t globalOffsetY;
    uint64_t globalOffsetZ;
    uint64_t localIdTablePtr;
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
    uint32_t padding1;
    uint64_t rtGlobalBufferPtr;
    uint64_t assertBufferPtr;
    uint8_t reserved[44];

    static constexpr uint8_t getSize() { return static_cast<uint8_t>(offsetof(ImplicitArgsV1, reserved)); }
    static constexpr uint8_t getAlignedSize() { return sizeof(ImplicitArgsV1); }
};

static_assert(std::alignment_of_v<ImplicitArgsV1> == 32, "Implicit args size need to be aligned to 32");
static_assert(sizeof(ImplicitArgsV1) == (40 * sizeof(uint32_t)));
static_assert(ImplicitArgsV1::getSize() == (28 * sizeof(uint32_t)));
static_assert(NEO::TypeTraits::isPodV<ImplicitArgsV1>);

struct alignas(32) ImplicitArgs {
    union {
        ImplicitArgsV0 v0;
        ImplicitArgsV1 v1;
    };

    void initializeHeader(uint32_t version) {
        if (version == 0) {
            v0.header.structSize = ImplicitArgsV0::getSize();
            v0.header.structVersion = 0;
        } else if (version == 1) {
            v1.header.structSize = NEO::ImplicitArgsV1::getSize();
            v1.header.structVersion = 1;
        }
    }

    uint8_t getSize() const {
        if (v0.header.structVersion == 0) {
            return v0.header.structSize;

        } else if (v1.header.structVersion == 1) {
            return v1.header.structSize;
        }

        DEBUG_BREAK_IF(true);
        return 0;
    }

    uint8_t getAlignedSize() const {
        if (v0.header.structVersion == 0) {
            return ImplicitArgsV0::getAlignedSize();

        } else if (v1.header.structVersion == 1) {
            return ImplicitArgsV1::getAlignedSize();
        }

        DEBUG_BREAK_IF(true);
        return 0;
    }

    void setNumWorkDim(uint32_t numWorkDim) {
        if (v0.header.structVersion == 0) {
            v0.numWorkDim = numWorkDim;

        } else if (v1.header.structVersion == 1) {
            v1.numWorkDim = numWorkDim;
        }
    }

    void setSimdWidth(uint32_t simd) {
        if (v0.header.structVersion == 0) {
            v0.simdWidth = simd;
        }
    }

    std::optional<uint32_t> getSimdWidth() const {
        if (v0.header.structVersion == 0) {
            return v0.simdWidth;
        }
        return std::nullopt;
    }

    void setLocalSize(uint32_t x, uint32_t y, uint32_t z) {
        if (v0.header.structVersion == 0) {
            v0.localSizeX = x;
            v0.localSizeY = y;
            v0.localSizeZ = z;

        } else if (v1.header.structVersion == 1) {
            v1.localSizeX = x;
            v1.localSizeY = y;
            v1.localSizeZ = z;
        }
    }

    void getLocalSize(uint32_t &x, uint32_t &y, uint32_t &z) const {
        if (v0.header.structVersion == 0) {
            x = v0.localSizeX;
            y = v0.localSizeY;
            z = v0.localSizeZ;

        } else if (v1.header.structVersion == 1) {
            x = v1.localSizeX;
            y = v1.localSizeY;
            z = v1.localSizeZ;
        }
    }

    void setGlobalSize(uint32_t x, uint32_t y, uint32_t z) {
        if (v0.header.structVersion == 0) {
            v0.globalSizeX = x;
            v0.globalSizeY = y;
            v0.globalSizeZ = z;

        } else if (v1.header.structVersion == 1) {
            v1.globalSizeX = x;
            v1.globalSizeY = y;
            v1.globalSizeZ = z;
        }
    }

    void setGlobalOffset(uint32_t x, uint32_t y, uint32_t z) {
        if (v0.header.structVersion == 0) {
            v0.globalOffsetX = x;
            v0.globalOffsetY = y;
            v0.globalOffsetZ = z;

        } else if (v1.header.structVersion == 1) {
            v1.globalOffsetX = x;
            v1.globalOffsetY = y;
            v1.globalOffsetZ = z;
        }
    }
    void setGroupCount(uint32_t x, uint32_t y, uint32_t z) {
        if (v0.header.structVersion == 0) {
            v0.groupCountX = x;
            v0.groupCountY = y;
            v0.groupCountZ = z;

        } else if (v1.header.structVersion == 1) {
            v1.groupCountX = x;
            v1.groupCountY = y;
            v1.groupCountZ = z;
        }
    }

    void setLocalIdTablePtr(uint64_t address) {
        if (v0.header.structVersion == 0) {
            v0.localIdTablePtr = address;

        } else if (v1.header.structVersion == 1) {
            v1.localIdTablePtr = address;
        }
    }
    void setPrintfBuffer(uint64_t address) {
        if (v0.header.structVersion == 0) {
            v0.printfBufferPtr = address;

        } else if (v1.header.structVersion == 1) {
            v1.printfBufferPtr = address;
        }
    }

    void setRtGlobalBufferPtr(uint64_t address) {
        if (v0.header.structVersion == 0) {
            v0.rtGlobalBufferPtr = address;

        } else if (v1.header.structVersion == 1) {
            v1.rtGlobalBufferPtr = address;
        }
    }

    void setAssertBufferPtr(uint64_t address) {
        if (v0.header.structVersion == 0) {
            v0.assertBufferPtr = address;

        } else if (v1.header.structVersion == 1) {
            v1.assertBufferPtr = address;
        }
    }
};

static_assert(NEO::TypeTraits::isPodV<ImplicitArgs>);

} // namespace NEO
