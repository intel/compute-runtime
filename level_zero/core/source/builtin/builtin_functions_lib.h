/*
 * Copyright (C) 2020-2022 Intel Corporation
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
    CopyBufferBytes = 0u,
    CopyBufferBytesStateless,
    CopyBufferRectBytes2d,
    CopyBufferRectBytes3d,
    CopyBufferToBufferMiddle,
    CopyBufferToBufferMiddleStateless,
    CopyBufferToBufferSide,
    CopyBufferToBufferSideStateless,
    FillBufferImmediate,
    FillBufferImmediateStateless,
    FillBufferSSHOffset,
    FillBufferSSHOffsetStateless,
    FillBufferMiddle,
    FillBufferMiddleStateless,
    FillBufferRightLeftover,
    FillBufferRightLeftoverStateless,
    QueryKernelTimestamps,
    QueryKernelTimestampsWithOffsets,
    COUNT
};

enum class ImageBuiltin : uint32_t {
    CopyBufferToImage3d16Bytes = 0u,
    CopyBufferToImage3d2Bytes,
    CopyBufferToImage3d4Bytes,
    CopyBufferToImage3d8Bytes,
    CopyBufferToImage3dBytes,
    CopyImage3dToBuffer16Bytes,
    CopyImage3dToBuffer2Bytes,
    CopyImage3dToBuffer4Bytes,
    CopyImage3dToBuffer8Bytes,
    CopyImage3dToBufferBytes,
    CopyImageRegion,
    COUNT
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
    MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainUniqueOwnership();

  protected:
    BuiltinFunctionsLib() = default;

    MutexType ownershipMutex;
};

} // namespace L0
