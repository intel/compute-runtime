/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_in_ops_base.h"

#include <memory>
#include <mutex>

namespace NEO {
class BuiltIns;
} // namespace NEO

namespace L0 {
struct Device;
struct Kernel;

enum class BufferBuiltIn : uint32_t {
    copyBufferBytes = 0u,
    copyBufferRectBytes2d,
    copyBufferRectBytes3d,
    copyBufferToBufferMiddle,
    copyBufferToBufferSide,
    fillBufferImmediate,
    fillBufferImmediateLeftOver,
    fillBufferSSHOffset,
    fillBufferMiddle,
    fillBufferRightLeftover,
    queryKernelTimestamps,
    queryKernelTimestampsWithOffsets,
    count
};

enum class ImageBuiltIn : uint32_t {
    copyBufferToImage3d16Bytes = 0u,
    copyBufferToImage3d16BytesAligned,
    copyBufferToImage3d2Bytes,
    copyBufferToImage3d4Bytes,
    copyBufferToImage3d3To4Bytes,
    copyBufferToImage3d8Bytes,
    copyBufferToImage3d6To8Bytes,
    copyBufferToImage3dBytes,
    copyImage3dToBuffer16Bytes,
    copyImage3dToBuffer16BytesAligned,
    copyImage3dToBuffer2Bytes,
    copyImage3dToBuffer3Bytes,
    copyImage3dToBuffer4Bytes,
    copyImage3dToBuffer4To3Bytes,
    copyImage3dToBuffer6Bytes,
    copyImage3dToBuffer8Bytes,
    copyImage3dToBuffer8To6Bytes,
    copyImage3dToBufferBytes,
    copyImageRegion,
    fillImage3d,
    count
};

constexpr const char *getKernelName(BufferBuiltIn func) {
    switch (func) {
    case BufferBuiltIn::copyBufferBytes:
        return "copyBufferToBufferBytesSingle";
    case BufferBuiltIn::copyBufferRectBytes2d:
        return "CopyBufferRectBytes2d";
    case BufferBuiltIn::copyBufferRectBytes3d:
        return "CopyBufferRectBytes3d";
    case BufferBuiltIn::copyBufferToBufferMiddle:
        return "CopyBufferToBufferMiddleRegion";
    case BufferBuiltIn::copyBufferToBufferSide:
        return "CopyBufferToBufferSideRegion";
    case BufferBuiltIn::fillBufferImmediate:
        return "FillBufferImmediate";
    case BufferBuiltIn::fillBufferImmediateLeftOver:
        return "FillBufferImmediateLeftOver";
    case BufferBuiltIn::fillBufferSSHOffset:
        return "FillBufferSSHOffset";
    case BufferBuiltIn::fillBufferMiddle:
        return "FillBufferMiddle";
    case BufferBuiltIn::fillBufferRightLeftover:
        return "FillBufferRightLeftover";
    case BufferBuiltIn::queryKernelTimestamps:
        return "QueryKernelTimestamps";
    case BufferBuiltIn::queryKernelTimestampsWithOffsets:
        return "QueryKernelTimestampsWithOffsets";
    default:
        return "";
    }
}

constexpr NEO::BuiltIn::BaseKernel getBaseKernel(BufferBuiltIn func) {
    switch (func) {
    case BufferBuiltIn::copyBufferBytes:
    case BufferBuiltIn::copyBufferToBufferMiddle:
    case BufferBuiltIn::copyBufferToBufferSide:
        return NEO::BuiltIn::BaseKernel::copyBufferToBuffer;
    case BufferBuiltIn::copyBufferRectBytes2d:
    case BufferBuiltIn::copyBufferRectBytes3d:
        return NEO::BuiltIn::BaseKernel::copyBufferRect;
    case BufferBuiltIn::fillBufferImmediate:
    case BufferBuiltIn::fillBufferImmediateLeftOver:
    case BufferBuiltIn::fillBufferSSHOffset:
    case BufferBuiltIn::fillBufferMiddle:
    case BufferBuiltIn::fillBufferRightLeftover:
        return NEO::BuiltIn::BaseKernel::fillBuffer;
    case BufferBuiltIn::queryKernelTimestamps:
    case BufferBuiltIn::queryKernelTimestampsWithOffsets:
        return NEO::BuiltIn::BaseKernel::copyKernelTimestamps;
    default:
        return NEO::BuiltIn::BaseKernel::copyBufferToBuffer;
    }
}

constexpr const char *getKernelName(ImageBuiltIn func) {
    switch (func) {
    case ImageBuiltIn::copyBufferToImage3d16Bytes:
        return "CopyBufferToImage3d16Bytes";
    case ImageBuiltIn::copyBufferToImage3d16BytesAligned:
        return "CopyBufferToImage3d16BytesAligned";
    case ImageBuiltIn::copyBufferToImage3d2Bytes:
        return "CopyBufferToImage3d2Bytes";
    case ImageBuiltIn::copyBufferToImage3d4Bytes:
        return "CopyBufferToImage3d4Bytes";
    case ImageBuiltIn::copyBufferToImage3d3To4Bytes:
        return "CopyBufferToImage3d3To4Bytes";
    case ImageBuiltIn::copyBufferToImage3d8Bytes:
        return "CopyBufferToImage3d8Bytes";
    case ImageBuiltIn::copyBufferToImage3d6To8Bytes:
        return "CopyBufferToImage3d6To8Bytes";
    case ImageBuiltIn::copyBufferToImage3dBytes:
        return "CopyBufferToImage3dBytes";
    case ImageBuiltIn::copyImage3dToBuffer16Bytes:
        return "CopyImage3dToBuffer16Bytes";
    case ImageBuiltIn::copyImage3dToBuffer16BytesAligned:
        return "CopyImage3dToBuffer16BytesAligned";
    case ImageBuiltIn::copyImage3dToBuffer2Bytes:
        return "CopyImage3dToBuffer2Bytes";
    case ImageBuiltIn::copyImage3dToBuffer3Bytes:
        return "CopyImage3dToBuffer3Bytes";
    case ImageBuiltIn::copyImage3dToBuffer4Bytes:
        return "CopyImage3dToBuffer4Bytes";
    case ImageBuiltIn::copyImage3dToBuffer4To3Bytes:
        return "CopyImage3dToBuffer4To3Bytes";
    case ImageBuiltIn::copyImage3dToBuffer6Bytes:
        return "CopyImage3dToBuffer6Bytes";
    case ImageBuiltIn::copyImage3dToBuffer8Bytes:
        return "CopyImage3dToBuffer8Bytes";
    case ImageBuiltIn::copyImage3dToBuffer8To6Bytes:
        return "CopyImage3dToBuffer8To6Bytes";
    case ImageBuiltIn::copyImage3dToBufferBytes:
        return "CopyImage3dToBufferBytes";
    case ImageBuiltIn::copyImageRegion:
        return "CopyImage3dToImage3d";
    case ImageBuiltIn::fillImage3d:
        return "FillImage3d";
    default:
        return "";
    }
}

constexpr NEO::BuiltIn::BaseKernel getBaseKernel(ImageBuiltIn func) {
    switch (func) {
    case ImageBuiltIn::copyBufferToImage3d16Bytes:
    case ImageBuiltIn::copyBufferToImage3d16BytesAligned:
    case ImageBuiltIn::copyBufferToImage3d2Bytes:
    case ImageBuiltIn::copyBufferToImage3d4Bytes:
    case ImageBuiltIn::copyBufferToImage3d3To4Bytes:
    case ImageBuiltIn::copyBufferToImage3d8Bytes:
    case ImageBuiltIn::copyBufferToImage3d6To8Bytes:
    case ImageBuiltIn::copyBufferToImage3dBytes:
        return NEO::BuiltIn::BaseKernel::copyBufferToImage3d;
    case ImageBuiltIn::copyImage3dToBuffer16Bytes:
    case ImageBuiltIn::copyImage3dToBuffer16BytesAligned:
    case ImageBuiltIn::copyImage3dToBuffer2Bytes:
    case ImageBuiltIn::copyImage3dToBuffer3Bytes:
    case ImageBuiltIn::copyImage3dToBuffer4Bytes:
    case ImageBuiltIn::copyImage3dToBuffer4To3Bytes:
    case ImageBuiltIn::copyImage3dToBuffer6Bytes:
    case ImageBuiltIn::copyImage3dToBuffer8Bytes:
    case ImageBuiltIn::copyImage3dToBuffer8To6Bytes:
    case ImageBuiltIn::copyImage3dToBufferBytes:
        return NEO::BuiltIn::BaseKernel::copyImage3dToBuffer;
    case ImageBuiltIn::copyImageRegion:
        return NEO::BuiltIn::BaseKernel::copyImageToImage3d;
    case ImageBuiltIn::fillImage3d:
        return NEO::BuiltIn::BaseKernel::fillImage3d;
    default:
        return NEO::BuiltIn::BaseKernel::copyBufferToImage3d;
    }
}

struct BuiltInKernelLib {
    using MutexType = std::mutex;
    virtual ~BuiltInKernelLib() = default;
    static std::unique_ptr<BuiltInKernelLib> create(Device *device,
                                                    NEO::BuiltIns *builtins);

    virtual Kernel *getFunction(BufferBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) = 0;
    virtual Kernel *getImageFunction(ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) = 0;
    virtual void initBuiltinKernel(BufferBuiltIn builtId, const NEO::BuiltIn::AddressingMode &mode) = 0;
    virtual void initBuiltinImageKernel(ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) = 0;
    virtual void ensureInitCompletion() = 0;
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainUniqueOwnership();

  protected:
    BuiltInKernelLib() = default;

    MutexType ownershipMutex;
};

} // namespace L0
