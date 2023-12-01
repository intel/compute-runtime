/*
 * Copyright (C) 2020-2023 Intel Corporation
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
    copyBufferRectBytes2d,
    copyBufferRectBytes3d,
    copyBufferToBufferMiddle,
    copyBufferToBufferMiddleStateless,
    copyBufferToBufferSide,
    copyBufferToBufferSideStateless,
    fillBufferImmediate,
    fillBufferImmediateStateless,
    fillBufferImmediateLeftOver,
    fillBufferImmediateLeftOverStateless,
    fillBufferSSHOffset,
    fillBufferSSHOffsetStateless,
    fillBufferMiddle,
    fillBufferMiddleStateless,
    fillBufferRightLeftover,
    fillBufferRightLeftoverStateless,
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

} // namespace L0
