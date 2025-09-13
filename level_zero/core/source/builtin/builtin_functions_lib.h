/*
 * Copyright (C) 2020-2025 Intel Corporation
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
    copyBufferBytesStatelessHeapless,
    copyBufferRectBytes2d,
    copyBufferRectBytes2dStateless,
    copyBufferRectBytes3d,
    copyBufferRectBytes3dStateless,
    copyBufferToBufferMiddle,
    copyBufferToBufferMiddleStateless,
    copyBufferToBufferMiddleStatelessHeapless,
    copyBufferToBufferSide,
    copyBufferToBufferSideStateless,
    copyBufferToBufferSideStatelessHeapless,
    fillBufferImmediate,
    fillBufferImmediateStateless,
    fillBufferImmediateStatelessHeapless,
    fillBufferImmediateLeftOver,
    fillBufferImmediateLeftOverStateless,
    fillBufferImmediateLeftOverStatelessHeapless,
    fillBufferSSHOffset,
    fillBufferSSHOffsetStateless,
    fillBufferSSHOffsetStatelessHeapless,
    fillBufferMiddle,
    fillBufferMiddleStateless,
    fillBufferMiddleStatelessHeapless,
    fillBufferRightLeftover,
    fillBufferRightLeftoverStateless,
    fillBufferRightLeftoverStatelessHeapless,
    queryKernelTimestamps,
    queryKernelTimestampsWithOffsets,
    count
};

enum class ImageBuiltin : uint32_t {
    copyBufferToImage3d16Bytes = 0u,
    copyBufferToImage3d16BytesStateless,
    copyBufferToImage3d16BytesHeapless,
    copyBufferToImage3d2Bytes,
    copyBufferToImage3d2BytesStateless,
    copyBufferToImage3d2BytesHeapless,
    copyBufferToImage3d4Bytes,
    copyBufferToImage3d4BytesStateless,
    copyBufferToImage3d4BytesHeapless,
    copyBufferToImage3d3To4Bytes,
    copyBufferToImage3d3To4BytesStateless,
    copyBufferToImage3d3To4BytesHeapless,
    copyBufferToImage3d8Bytes,
    copyBufferToImage3d8BytesStateless,
    copyBufferToImage3d8BytesHeapless,
    copyBufferToImage3d6To8Bytes,
    copyBufferToImage3d6To8BytesStateless,
    copyBufferToImage3d6To8BytesHeapless,
    copyBufferToImage3dBytes,
    copyBufferToImage3dBytesStateless,
    copyBufferToImage3dBytesHeapless,
    copyImage3dToBuffer16Bytes,
    copyImage3dToBuffer16BytesStateless,
    copyImage3dToBuffer16BytesHeapless,
    copyImage3dToBuffer2Bytes,
    copyImage3dToBuffer2BytesStateless,
    copyImage3dToBuffer2BytesHeapless,
    copyImage3dToBuffer3Bytes,
    copyImage3dToBuffer3BytesStateless,
    copyImage3dToBuffer3BytesHeapless,
    copyImage3dToBuffer4Bytes,
    copyImage3dToBuffer4BytesStateless,
    copyImage3dToBuffer4BytesHeapless,
    copyImage3dToBuffer4To3Bytes,
    copyImage3dToBuffer4To3BytesStateless,
    copyImage3dToBuffer4To3BytesHeapless,
    copyImage3dToBuffer6Bytes,
    copyImage3dToBuffer6BytesStateless,
    copyImage3dToBuffer6BytesHeapless,
    copyImage3dToBuffer8Bytes,
    copyImage3dToBuffer8BytesStateless,
    copyImage3dToBuffer8BytesHeapless,
    copyImage3dToBuffer8To6Bytes,
    copyImage3dToBuffer8To6BytesStateless,
    copyImage3dToBuffer8To6BytesHeapless,
    copyImage3dToBufferBytes,
    copyImage3dToBufferBytesStateless,
    copyImage3dToBufferBytesHeapless,
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
constexpr Builtin adjustBuiltinType(const bool isStateless, const bool isHeapless) {
    return type;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferBytes>(const bool isStateless, const bool isHeapless) {
    if (isHeapless) {
        return Builtin::copyBufferBytesStatelessHeapless;
    } else if (isStateless) {
        return Builtin::copyBufferBytesStateless;
    }
    return Builtin::copyBufferBytes;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferRectBytes2d>(const bool isStateless, const bool isHeapless) {
    if (isStateless) {
        return Builtin::copyBufferRectBytes2dStateless;
    }
    return Builtin::copyBufferRectBytes2d;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferRectBytes3d>(const bool isStateless, const bool isHeapless) {
    if (isStateless) {
        return Builtin::copyBufferRectBytes3dStateless;
    }
    return Builtin::copyBufferRectBytes3d;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferToBufferMiddle>(const bool isStateless, const bool isHeapless) {
    if (isHeapless) {
        return Builtin::copyBufferToBufferMiddleStatelessHeapless;
    } else if (isStateless) {
        return Builtin::copyBufferToBufferMiddleStateless;
    }
    return Builtin::copyBufferToBufferMiddle;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::copyBufferToBufferSide>(const bool isStateless, const bool isHeapless) {
    if (isHeapless) {
        return Builtin::copyBufferToBufferSideStatelessHeapless;
    } else if (isStateless) {
        return Builtin::copyBufferToBufferSideStateless;
    }
    return Builtin::copyBufferToBufferSide;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferImmediate>(const bool isStateless, const bool isHeapless) {
    if (isHeapless) {
        return Builtin::fillBufferImmediateStatelessHeapless;
    } else if (isStateless) {
        return Builtin::fillBufferImmediateStateless;
    }
    return Builtin::fillBufferImmediate;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferImmediateLeftOver>(const bool isStateless, const bool isHeapless) {
    if (isHeapless) {
        return Builtin::fillBufferImmediateLeftOverStatelessHeapless;
    } else if (isStateless) {
        return Builtin::fillBufferImmediateLeftOverStateless;
    }
    return Builtin::fillBufferImmediateLeftOver;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferSSHOffset>(const bool isStateless, const bool isHeapless) {
    if (isHeapless) {
        return Builtin::fillBufferSSHOffsetStatelessHeapless;
    } else if (isStateless) {
        return Builtin::fillBufferSSHOffsetStateless;
    }
    return Builtin::fillBufferSSHOffset;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferMiddle>(const bool isStateless, const bool isHeapless) {
    if (isHeapless) {
        return Builtin::fillBufferMiddleStatelessHeapless;
    } else if (isStateless) {
        return Builtin::fillBufferMiddleStateless;
    }
    return Builtin::fillBufferMiddle;
}

template <>
constexpr Builtin adjustBuiltinType<Builtin::fillBufferRightLeftover>(const bool isStateless, const bool isHeapless) {
    if (isHeapless) {
        return Builtin::fillBufferRightLeftoverStatelessHeapless;
    } else if (isStateless) {
        return Builtin::fillBufferRightLeftoverStateless;
    }
    return Builtin::fillBufferRightLeftover;
}

template <ImageBuiltin type>
constexpr ImageBuiltin adjustImageBuiltinType(const bool isStateless, const bool isHeapless) {
    return type;
}

#define DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(type)                                                           \
    template <>                                                                                          \
    constexpr ImageBuiltin adjustImageBuiltinType<type>(const bool isStateless, const bool isHeapless) { \
        if (isHeapless) {                                                                                \
            return type##Heapless;                                                                       \
        } else if (isStateless) {                                                                        \
            return type##Stateless;                                                                      \
        } else {                                                                                         \
            return type;                                                                                 \
        }                                                                                                \
    }

DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d16Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d2Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d4Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d3To4Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d8Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3d6To8Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyBufferToImage3dBytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer16Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer2Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer3Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer4Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer4To3Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer6Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer8Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBuffer8To6Bytes);
DEFINE_ADJUST_IMAGE_BUILTIN_TYPE(ImageBuiltin::copyImage3dToBufferBytes);

template <>
constexpr ImageBuiltin adjustImageBuiltinType<ImageBuiltin::copyImageRegion>(const bool isStateless, const bool isHeapless) {
    if (isHeapless) {
        return ImageBuiltin::copyImageRegionHeapless;
    }
    return ImageBuiltin::copyImageRegion;
}

} // namespace BuiltinTypeHelper

} // namespace L0
