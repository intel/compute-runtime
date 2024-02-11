/*
 * Copyright (C) 2020-2024 Intel Corporation
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
    copyBufferRectBytes3d,
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
    copyBufferToImage3d2Bytes,
    copyBufferToImage3d4Bytes,
    copyBufferToImage3d8Bytes,
    copyBufferToImage3dBytes,
    copyImage3dToBuffer16Bytes,
    copyImage3dToBuffer2Bytes,
    copyImage3dToBuffer4Bytes,
    copyImage3dToBuffer8Bytes,
    copyImage3dToBufferBytes,
    copyImageRegion,
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
} // namespace BuiltinTypeHelper

} // namespace L0
