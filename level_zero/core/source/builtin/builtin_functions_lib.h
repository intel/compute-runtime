/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>
#include <mutex>

namespace NEO {
class BuiltIns;
} // namespace NEO

namespace L0 {
struct Device;
struct Kernel;

// Each value maps to a specific kernel function
// Variants of the same builtIn (e.g. stateless, wideStateless, heapless, wideHeapless) select
// alternative compilations of the same kernel with different addressing modes or compilation options
enum class BufferBuiltIn : uint32_t {
    copyBufferBytes = 0u,
    copyBufferBytesStateless,
    copyBufferBytesWideStateless,
    copyBufferBytesStatelessHeapless,
    copyBufferBytesWideStatelessHeapless,
    copyBufferRectBytes2d,
    copyBufferRectBytes2dStateless,
    copyBufferRectBytes2dStatelessHeapless,
    copyBufferRectBytes3d,
    copyBufferRectBytes3dStateless,
    copyBufferRectBytes3dStatelessHeapless,
    copyBufferToBufferMiddle,
    copyBufferToBufferMiddleStateless,
    copyBufferToBufferMiddleWideStateless,
    copyBufferToBufferMiddleStatelessHeapless,
    copyBufferToBufferMiddleWideStatelessHeapless,
    copyBufferToBufferSide,
    copyBufferToBufferSideStateless,
    copyBufferToBufferSideWideStateless,
    copyBufferToBufferSideStatelessHeapless,
    copyBufferToBufferSideWideStatelessHeapless,
    fillBufferImmediate,
    fillBufferImmediateStateless,
    fillBufferImmediateWideStateless,
    fillBufferImmediateStatelessHeapless,
    fillBufferImmediateWideStatelessHeapless,
    fillBufferImmediateLeftOver,
    fillBufferImmediateLeftOverStateless,
    fillBufferImmediateLeftOverWideStateless,
    fillBufferImmediateLeftOverStatelessHeapless,
    fillBufferImmediateLeftOverWideStatelessHeapless,
    fillBufferSSHOffset,
    fillBufferSSHOffsetStateless,
    fillBufferSSHOffsetStatelessHeapless,
    fillBufferMiddle,
    fillBufferMiddleStateless,
    fillBufferMiddleWideStateless,
    fillBufferMiddleStatelessHeapless,
    fillBufferMiddleWideStatelessHeapless,
    fillBufferRightLeftover,
    fillBufferRightLeftoverStateless,
    fillBufferRightLeftoverWideStateless,
    fillBufferRightLeftoverStatelessHeapless,
    fillBufferRightLeftoverWideStatelessHeapless,
    queryKernelTimestamps,
    queryKernelTimestampsStateless,
    queryKernelTimestampsStatelessHeapless,
    queryKernelTimestampsWithOffsets,
    queryKernelTimestampsWithOffsetsStateless,
    queryKernelTimestampsWithOffsetsStatelessHeapless,
    count
};

enum class ImageBuiltIn : uint32_t {
    copyBufferToImage3d16Bytes = 0u,
    copyBufferToImage3d16BytesAligned,
    copyBufferToImage3d16BytesStateless,
    copyBufferToImage3d16BytesWideStateless,
    copyBufferToImage3d16BytesAlignedStateless,
    copyBufferToImage3d16BytesAlignedWideStateless,
    copyBufferToImage3d16BytesStatelessHeapless,
    copyBufferToImage3d16BytesWideStatelessHeapless,
    copyBufferToImage3d16BytesAlignedStatelessHeapless,
    copyBufferToImage3d16BytesAlignedWideStatelessHeapless,
    copyBufferToImage3d2Bytes,
    copyBufferToImage3d2BytesStateless,
    copyBufferToImage3d2BytesWideStateless,
    copyBufferToImage3d2BytesStatelessHeapless,
    copyBufferToImage3d2BytesWideStatelessHeapless,
    copyBufferToImage3d4Bytes,
    copyBufferToImage3d4BytesStateless,
    copyBufferToImage3d4BytesWideStateless,
    copyBufferToImage3d4BytesStatelessHeapless,
    copyBufferToImage3d4BytesWideStatelessHeapless,
    copyBufferToImage3d3To4Bytes,
    copyBufferToImage3d3To4BytesStateless,
    copyBufferToImage3d3To4BytesWideStateless,
    copyBufferToImage3d3To4BytesStatelessHeapless,
    copyBufferToImage3d3To4BytesWideStatelessHeapless,
    copyBufferToImage3d8Bytes,
    copyBufferToImage3d8BytesStateless,
    copyBufferToImage3d8BytesWideStateless,
    copyBufferToImage3d8BytesStatelessHeapless,
    copyBufferToImage3d8BytesWideStatelessHeapless,
    copyBufferToImage3d6To8Bytes,
    copyBufferToImage3d6To8BytesStateless,
    copyBufferToImage3d6To8BytesWideStateless,
    copyBufferToImage3d6To8BytesStatelessHeapless,
    copyBufferToImage3d6To8BytesWideStatelessHeapless,
    copyBufferToImage3dBytes,
    copyBufferToImage3dBytesStateless,
    copyBufferToImage3dBytesWideStateless,
    copyBufferToImage3dBytesStatelessHeapless,
    copyBufferToImage3dBytesWideStatelessHeapless,
    copyImage3dToBuffer16Bytes,
    copyImage3dToBuffer16BytesAligned,
    copyImage3dToBuffer16BytesStateless,
    copyImage3dToBuffer16BytesWideStateless,
    copyImage3dToBuffer16BytesAlignedStateless,
    copyImage3dToBuffer16BytesAlignedWideStateless,
    copyImage3dToBuffer16BytesStatelessHeapless,
    copyImage3dToBuffer16BytesWideStatelessHeapless,
    copyImage3dToBuffer16BytesAlignedStatelessHeapless,
    copyImage3dToBuffer16BytesAlignedWideStatelessHeapless,
    copyImage3dToBuffer2Bytes,
    copyImage3dToBuffer2BytesStateless,
    copyImage3dToBuffer2BytesWideStateless,
    copyImage3dToBuffer2BytesStatelessHeapless,
    copyImage3dToBuffer2BytesWideStatelessHeapless,
    copyImage3dToBuffer3Bytes,
    copyImage3dToBuffer3BytesStateless,
    copyImage3dToBuffer3BytesWideStateless,
    copyImage3dToBuffer3BytesStatelessHeapless,
    copyImage3dToBuffer3BytesWideStatelessHeapless,
    copyImage3dToBuffer4Bytes,
    copyImage3dToBuffer4BytesStateless,
    copyImage3dToBuffer4BytesWideStateless,
    copyImage3dToBuffer4BytesStatelessHeapless,
    copyImage3dToBuffer4BytesWideStatelessHeapless,
    copyImage3dToBuffer4To3Bytes,
    copyImage3dToBuffer4To3BytesStateless,
    copyImage3dToBuffer4To3BytesWideStateless,
    copyImage3dToBuffer4To3BytesStatelessHeapless,
    copyImage3dToBuffer4To3BytesWideStatelessHeapless,
    copyImage3dToBuffer6Bytes,
    copyImage3dToBuffer6BytesStateless,
    copyImage3dToBuffer6BytesWideStateless,
    copyImage3dToBuffer6BytesStatelessHeapless,
    copyImage3dToBuffer6BytesWideStatelessHeapless,
    copyImage3dToBuffer8Bytes,
    copyImage3dToBuffer8BytesStateless,
    copyImage3dToBuffer8BytesWideStateless,
    copyImage3dToBuffer8BytesStatelessHeapless,
    copyImage3dToBuffer8BytesWideStatelessHeapless,
    copyImage3dToBuffer8To6Bytes,
    copyImage3dToBuffer8To6BytesStateless,
    copyImage3dToBuffer8To6BytesWideStateless,
    copyImage3dToBuffer8To6BytesStatelessHeapless,
    copyImage3dToBuffer8To6BytesWideStatelessHeapless,
    copyImage3dToBufferBytes,
    copyImage3dToBufferBytesStateless,
    copyImage3dToBufferBytesWideStateless,
    copyImage3dToBufferBytesStatelessHeapless,
    copyImage3dToBufferBytesWideStatelessHeapless,
    copyImageRegion,
    copyImageRegionHeapless,
    fillImage3d,
    fillImage3dHeapless,
    count
};

struct BuiltInKernelLib {
    using MutexType = std::mutex;
    virtual ~BuiltInKernelLib() = default;
    static std::unique_ptr<BuiltInKernelLib> create(Device *device,
                                                    NEO::BuiltIns *builtins);

    virtual Kernel *getFunction(BufferBuiltIn func) = 0;
    virtual Kernel *getImageFunction(ImageBuiltIn func) = 0;
    virtual void initBuiltinKernel(BufferBuiltIn builtId) = 0;
    virtual void initBuiltinImageKernel(ImageBuiltIn func) = 0;
    virtual void ensureInitCompletion() = 0;
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainUniqueOwnership();

  protected:
    BuiltInKernelLib() = default;

    MutexType ownershipMutex;
};

namespace BuiltInHelper {

template <BufferBuiltIn type>
constexpr BufferBuiltIn adjustBufferBuiltIn(const bool isStateless, const bool isHeapless, const bool isWideness = false) {
    return type;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::copyBufferBytes>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? BufferBuiltIn::copyBufferBytesWideStatelessHeapless
                          : BufferBuiltIn::copyBufferBytesStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? BufferBuiltIn::copyBufferBytesWideStateless
                          : BufferBuiltIn::copyBufferBytesStateless;
    }

    return BufferBuiltIn::copyBufferBytes;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::copyBufferRectBytes2d>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return BufferBuiltIn::copyBufferRectBytes2dStatelessHeapless;
    } else if (isStateless) {
        return BufferBuiltIn::copyBufferRectBytes2dStateless;
    }

    return BufferBuiltIn::copyBufferRectBytes2d;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::copyBufferRectBytes3d>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return BufferBuiltIn::copyBufferRectBytes3dStatelessHeapless;
    } else if (isStateless) {
        return BufferBuiltIn::copyBufferRectBytes3dStateless;
    }

    return BufferBuiltIn::copyBufferRectBytes3d;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::copyBufferToBufferMiddle>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? BufferBuiltIn::copyBufferToBufferMiddleWideStatelessHeapless
                          : BufferBuiltIn::copyBufferToBufferMiddleStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? BufferBuiltIn::copyBufferToBufferMiddleWideStateless
                          : BufferBuiltIn::copyBufferToBufferMiddleStateless;
    }

    return BufferBuiltIn::copyBufferToBufferMiddle;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::copyBufferToBufferSide>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? BufferBuiltIn::copyBufferToBufferSideWideStatelessHeapless
                          : BufferBuiltIn::copyBufferToBufferSideStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? BufferBuiltIn::copyBufferToBufferSideWideStateless
                          : BufferBuiltIn::copyBufferToBufferSideStateless;
    }

    return BufferBuiltIn::copyBufferToBufferSide;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::fillBufferImmediate>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? BufferBuiltIn::fillBufferImmediateWideStatelessHeapless
                          : BufferBuiltIn::fillBufferImmediateStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? BufferBuiltIn::fillBufferImmediateWideStateless
                          : BufferBuiltIn::fillBufferImmediateStateless;
    }

    return BufferBuiltIn::fillBufferImmediate;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::fillBufferImmediateLeftOver>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? BufferBuiltIn::fillBufferImmediateLeftOverWideStatelessHeapless
                          : BufferBuiltIn::fillBufferImmediateLeftOverStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? BufferBuiltIn::fillBufferImmediateLeftOverWideStateless
                          : BufferBuiltIn::fillBufferImmediateLeftOverStateless;
    }

    return BufferBuiltIn::fillBufferImmediateLeftOver;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::fillBufferSSHOffset>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return BufferBuiltIn::fillBufferSSHOffsetStatelessHeapless;
    } else if (isStateless) {
        return BufferBuiltIn::fillBufferSSHOffsetStateless;
    }

    return BufferBuiltIn::fillBufferSSHOffset;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::fillBufferMiddle>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? BufferBuiltIn::fillBufferMiddleWideStatelessHeapless
                          : BufferBuiltIn::fillBufferMiddleStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? BufferBuiltIn::fillBufferMiddleWideStateless
                          : BufferBuiltIn::fillBufferMiddleStateless;
    }

    return BufferBuiltIn::fillBufferMiddle;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::fillBufferRightLeftover>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? BufferBuiltIn::fillBufferRightLeftoverWideStatelessHeapless
                          : BufferBuiltIn::fillBufferRightLeftoverStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? BufferBuiltIn::fillBufferRightLeftoverWideStateless
                          : BufferBuiltIn::fillBufferRightLeftoverStateless;
    }

    return BufferBuiltIn::fillBufferRightLeftover;
}
template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::queryKernelTimestamps>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return BufferBuiltIn::queryKernelTimestampsStatelessHeapless;
    } else if (isStateless) {
        return BufferBuiltIn::queryKernelTimestampsStateless;
    }

    return BufferBuiltIn::queryKernelTimestamps;
}

template <>
constexpr BufferBuiltIn adjustBufferBuiltIn<BufferBuiltIn::queryKernelTimestampsWithOffsets>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return BufferBuiltIn::queryKernelTimestampsWithOffsetsStatelessHeapless;
    } else if (isStateless) {
        return BufferBuiltIn::queryKernelTimestampsWithOffsetsStateless;
    }

    return BufferBuiltIn::queryKernelTimestampsWithOffsets;
}

template <ImageBuiltIn type>
constexpr ImageBuiltIn adjustImageBuiltIn(const bool isStateless, const bool isHeapless, const bool isWideness = false) {
    return type;
}

#define DEFINE_ADJUST_IMAGE_BUILT_IN(type)                    \
    template <>                                               \
    constexpr ImageBuiltIn adjustImageBuiltIn<type>(          \
        bool isStateless, bool isHeapless, bool isWideness) { \
                                                              \
        if (isHeapless) {                                     \
            return isWideness ? type##WideStatelessHeapless   \
                              : type##StatelessHeapless;      \
        } else if (isStateless) {                             \
            return isWideness ? type##WideStateless           \
                              : type##Stateless;              \
        }                                                     \
                                                              \
        return type;                                          \
    }

DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyBufferToImage3d16Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyBufferToImage3d16BytesAligned);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyBufferToImage3d2Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyBufferToImage3d4Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyBufferToImage3d3To4Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyBufferToImage3d8Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyBufferToImage3d6To8Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyBufferToImage3dBytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBuffer16Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBuffer16BytesAligned);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBuffer2Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBuffer3Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBuffer4Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBuffer4To3Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBuffer6Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBuffer8Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBuffer8To6Bytes);
DEFINE_ADJUST_IMAGE_BUILT_IN(ImageBuiltIn::copyImage3dToBufferBytes);

template <>
constexpr ImageBuiltIn adjustImageBuiltIn<ImageBuiltIn::copyImageRegion>(const bool isStateless, const bool isHeapless, const bool isWideness) {
    if (isHeapless) {
        return ImageBuiltIn::copyImageRegionHeapless;
    }
    return ImageBuiltIn::copyImageRegion;
}

template <>
constexpr ImageBuiltIn adjustImageBuiltIn<ImageBuiltIn::fillImage3d>(const bool isStateless, const bool isHeapless, const bool isWideness) {
    if (isHeapless) {
        return ImageBuiltIn::fillImage3dHeapless;
    }
    return ImageBuiltIn::fillImage3d;
}

} // namespace BuiltInHelper

} // namespace L0
