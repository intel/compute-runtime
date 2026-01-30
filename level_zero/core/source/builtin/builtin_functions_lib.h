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

enum class Builtin : uint32_t {
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
    queryKernelTimestampsWithOffsets,
    count
};

enum class ImageBuiltin : uint32_t {
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
    count
};

struct BuiltinFunctionsLib {
    using MutexType = std::mutex;
    virtual ~BuiltinFunctionsLib() = default;
    static std::unique_ptr<BuiltinFunctionsLib> create(Device *device,
                                                       NEO::BuiltIns *builtins);

    virtual Kernel *getFunction(Builtin func) = 0;
    virtual Kernel *getImageFunction(ImageBuiltin func) = 0;
    virtual void initBuiltinKernel(Builtin builtId) = 0;
    virtual void initBuiltinImageKernel(ImageBuiltin func) = 0;
    virtual void ensureInitCompletion() = 0;
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainUniqueOwnership();

  protected:
    BuiltinFunctionsLib() = default;

    MutexType ownershipMutex;
};

namespace BuiltinTypeHelper {

template <Builtin type>
constexpr Builtin adjustBuiltinType(const bool isStateless, const bool isHeapless, const bool isWideness = false) {
    return type;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferBytes>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? Builtin::copyBufferBytesWideStatelessHeapless
                          : Builtin::copyBufferBytesStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? Builtin::copyBufferBytesWideStateless
                          : Builtin::copyBufferBytesStateless;
    }

    return Builtin::copyBufferBytes;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferRectBytes2d>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return Builtin::copyBufferRectBytes2dStatelessHeapless;
    } else if (isStateless) {
        return Builtin::copyBufferRectBytes2dStateless;
    }

    return Builtin::copyBufferRectBytes2d;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferRectBytes3d>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return Builtin::copyBufferRectBytes3dStatelessHeapless;
    } else if (isStateless) {
        return Builtin::copyBufferRectBytes3dStateless;
    }

    return Builtin::copyBufferRectBytes3d;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferToBufferMiddle>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? Builtin::copyBufferToBufferMiddleWideStatelessHeapless
                          : Builtin::copyBufferToBufferMiddleStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? Builtin::copyBufferToBufferMiddleWideStateless
                          : Builtin::copyBufferToBufferMiddleStateless;
    }

    return Builtin::copyBufferToBufferMiddle;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferToBufferSide>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? Builtin::copyBufferToBufferSideWideStatelessHeapless
                          : Builtin::copyBufferToBufferSideStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? Builtin::copyBufferToBufferSideWideStateless
                          : Builtin::copyBufferToBufferSideStateless;
    }

    return Builtin::copyBufferToBufferSide;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferImmediate>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? Builtin::fillBufferImmediateWideStatelessHeapless
                          : Builtin::fillBufferImmediateStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? Builtin::fillBufferImmediateWideStateless
                          : Builtin::fillBufferImmediateStateless;
    }

    return Builtin::fillBufferImmediate;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferImmediateLeftOver>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? Builtin::fillBufferImmediateLeftOverWideStatelessHeapless
                          : Builtin::fillBufferImmediateLeftOverStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? Builtin::fillBufferImmediateLeftOverWideStateless
                          : Builtin::fillBufferImmediateLeftOverStateless;
    }

    return Builtin::fillBufferImmediateLeftOver;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferSSHOffset>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return Builtin::fillBufferSSHOffsetStatelessHeapless;
    } else if (isStateless) {
        return Builtin::fillBufferSSHOffsetStateless;
    }

    return Builtin::fillBufferSSHOffset;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferMiddle>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? Builtin::fillBufferMiddleWideStatelessHeapless
                          : Builtin::fillBufferMiddleStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? Builtin::fillBufferMiddleWideStateless
                          : Builtin::fillBufferMiddleStateless;
    }

    return Builtin::fillBufferMiddle;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferRightLeftover>(
    bool isStateless, bool isHeapless, bool isWideness) {

    if (isHeapless) {
        return isWideness ? Builtin::fillBufferRightLeftoverWideStatelessHeapless
                          : Builtin::fillBufferRightLeftoverStatelessHeapless;
    } else if (isStateless) {
        return isWideness ? Builtin::fillBufferRightLeftoverWideStateless
                          : Builtin::fillBufferRightLeftoverStateless;
    }

    return Builtin::fillBufferRightLeftover;
}

template <ImageBuiltin type>
constexpr ImageBuiltin adjustImageBuiltinType(const bool isStateless, const bool isHeapless, const bool isWideness = false) {
    return type;
}

#define DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(type)                \
    template <>                                               \
    constexpr ImageBuiltin adjustImageBuiltinType<type>(      \
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

DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d16Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d16BytesAligned);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d2Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d4Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d3To4Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d8Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d6To8Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3dBytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer16Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer16BytesAligned);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer2Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer3Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer4Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer4To3Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer6Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer8Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer8To6Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBufferBytes);

template <>
constexpr ImageBuiltin adjustImageBuiltinType<ImageBuiltin::copyImageRegion>(const bool isStateless, const bool isHeapless, const bool isWideness) {
    if (isHeapless) {
        return ImageBuiltin::copyImageRegionHeapless;
    }
    return ImageBuiltin::copyImageRegion;
}

} // namespace BuiltinTypeHelper

} // namespace L0
