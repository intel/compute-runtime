/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/kernel/kernel.h"

namespace L0 {

BuiltInKernelLibImpl::BuiltInKernelData::~BuiltInKernelData() {
    func->destroy();
    func.release();
}
BuiltInKernelLibImpl::BuiltInKernelData::BuiltInKernelData() = default;
BuiltInKernelLibImpl::BuiltInKernelData::BuiltInKernelData(Module *module, std::unique_ptr<L0::Kernel> &&ker) : module(module), func(std::move(ker)) {}
std::unique_lock<BuiltInKernelLib::MutexType> BuiltInKernelLib::obtainUniqueOwnership() {
    return std::unique_lock<BuiltInKernelLib::MutexType>(this->ownershipMutex);
}

void BuiltInKernelLibImpl::initBuiltinKernel(BufferBuiltIn func) {
    const char *kernelName = nullptr;
    NEO::BuiltIn::Group builtInGroup;

    switch (func) {
    case BufferBuiltIn::copyBufferBytes:
        kernelName = "copyBufferToBufferBytesSingle";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBuffer;
        break;
    case BufferBuiltIn::copyBufferBytesStateless:
        kernelName = "copyBufferToBufferBytesSingle";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferStateless;
        break;
    case BufferBuiltIn::copyBufferBytesWideStateless:
        kernelName = "copyBufferToBufferBytesSingle";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferWideStateless;
        break;
    case BufferBuiltIn::copyBufferBytesStatelessHeapless:
        kernelName = "copyBufferToBufferBytesSingle";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferStatelessHeapless;
        break;
    case BufferBuiltIn::copyBufferBytesWideStatelessHeapless:
        kernelName = "copyBufferToBufferBytesSingle";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferWideStatelessHeapless;
        break;
    case BufferBuiltIn::copyBufferRectBytes2d:
        kernelName = "CopyBufferRectBytes2d";
        builtInGroup = NEO::BuiltIn::Group::copyBufferRect;
        break;
    case BufferBuiltIn::copyBufferRectBytes2dStateless:
        kernelName = "CopyBufferRectBytes2d";
        builtInGroup = NEO::BuiltIn::Group::copyBufferRectStateless;
        break;
    case BufferBuiltIn::copyBufferRectBytes2dStatelessHeapless:
        kernelName = "CopyBufferRectBytes2d";
        builtInGroup = NEO::BuiltIn::Group::copyBufferRectStatelessHeapless;
        break;
    case BufferBuiltIn::copyBufferRectBytes3d:
        kernelName = "CopyBufferRectBytes3d";
        builtInGroup = NEO::BuiltIn::Group::copyBufferRect;
        break;
    case BufferBuiltIn::copyBufferRectBytes3dStateless:
        kernelName = "CopyBufferRectBytes3d";
        builtInGroup = NEO::BuiltIn::Group::copyBufferRectStateless;
        break;
    case BufferBuiltIn::copyBufferRectBytes3dStatelessHeapless:
        kernelName = "CopyBufferRectBytes3d";
        builtInGroup = NEO::BuiltIn::Group::copyBufferRectStatelessHeapless;
        break;
    case BufferBuiltIn::copyBufferToBufferMiddle:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBuffer;
        break;
    case BufferBuiltIn::copyBufferToBufferMiddleStateless:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferStateless;
        break;
    case BufferBuiltIn::copyBufferToBufferMiddleWideStateless:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferWideStateless;
        break;
    case BufferBuiltIn::copyBufferToBufferMiddleStatelessHeapless:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferStatelessHeapless;
        break;
    case BufferBuiltIn::copyBufferToBufferMiddleWideStatelessHeapless:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferWideStatelessHeapless;
        break;
    case BufferBuiltIn::copyBufferToBufferSide:
        kernelName = "CopyBufferToBufferSideRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBuffer;
        break;
    case BufferBuiltIn::copyBufferToBufferSideStateless:
        kernelName = "CopyBufferToBufferSideRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferStateless;
        break;
    case BufferBuiltIn::copyBufferToBufferSideWideStateless:
        kernelName = "CopyBufferToBufferSideRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferWideStateless;
        break;
    case BufferBuiltIn::copyBufferToBufferSideStatelessHeapless:
        kernelName = "CopyBufferToBufferSideRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferStatelessHeapless;
        break;
    case BufferBuiltIn::copyBufferToBufferSideWideStatelessHeapless:
        kernelName = "CopyBufferToBufferSideRegion";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToBufferWideStatelessHeapless;
        break;
    case BufferBuiltIn::fillBufferImmediate:
        kernelName = "FillBufferImmediate";
        builtInGroup = NEO::BuiltIn::Group::fillBuffer;
        break;
    case BufferBuiltIn::fillBufferImmediateStateless:
        kernelName = "FillBufferImmediate";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStateless;
        break;
    case BufferBuiltIn::fillBufferImmediateWideStateless:
        kernelName = "FillBufferImmediate";
        builtInGroup = NEO::BuiltIn::Group::fillBufferWideStateless;
        break;
    case BufferBuiltIn::fillBufferImmediateStatelessHeapless:
        kernelName = "FillBufferImmediate";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStatelessHeapless;
        break;
    case BufferBuiltIn::fillBufferImmediateWideStatelessHeapless:
        kernelName = "FillBufferImmediate";
        builtInGroup = NEO::BuiltIn::Group::fillBufferWideStatelessHeapless;
        break;
    case BufferBuiltIn::fillBufferImmediateLeftOver:
        kernelName = "FillBufferImmediateLeftOver";
        builtInGroup = NEO::BuiltIn::Group::fillBuffer;
        break;
    case BufferBuiltIn::fillBufferImmediateLeftOverStateless:
        kernelName = "FillBufferImmediateLeftOver";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStateless;
        break;
    case BufferBuiltIn::fillBufferImmediateLeftOverWideStateless:
        kernelName = "FillBufferImmediateLeftOver";
        builtInGroup = NEO::BuiltIn::Group::fillBufferWideStateless;
        break;
    case BufferBuiltIn::fillBufferImmediateLeftOverStatelessHeapless:
        kernelName = "FillBufferImmediateLeftOver";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStatelessHeapless;
        break;
    case BufferBuiltIn::fillBufferImmediateLeftOverWideStatelessHeapless:
        kernelName = "FillBufferImmediateLeftOver";
        builtInGroup = NEO::BuiltIn::Group::fillBufferWideStatelessHeapless;
        break;
    case BufferBuiltIn::fillBufferSSHOffset:
        kernelName = "FillBufferSSHOffset";
        builtInGroup = NEO::BuiltIn::Group::fillBuffer;
        break;
    case BufferBuiltIn::fillBufferSSHOffsetStateless:
        kernelName = "FillBufferSSHOffset";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStateless;
        break;
    case BufferBuiltIn::fillBufferSSHOffsetStatelessHeapless:
        kernelName = "FillBufferSSHOffset";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStatelessHeapless;
        break;
    case BufferBuiltIn::fillBufferMiddle:
        kernelName = "FillBufferMiddle";
        builtInGroup = NEO::BuiltIn::Group::fillBuffer;
        break;
    case BufferBuiltIn::fillBufferMiddleStateless:
        kernelName = "FillBufferMiddle";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStateless;
        break;
    case BufferBuiltIn::fillBufferMiddleWideStateless:
        kernelName = "FillBufferMiddle";
        builtInGroup = NEO::BuiltIn::Group::fillBufferWideStateless;
        break;
    case BufferBuiltIn::fillBufferMiddleStatelessHeapless:
        kernelName = "FillBufferMiddle";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStatelessHeapless;
        break;
    case BufferBuiltIn::fillBufferMiddleWideStatelessHeapless:
        kernelName = "FillBufferMiddle";
        builtInGroup = NEO::BuiltIn::Group::fillBufferWideStatelessHeapless;
        break;
    case BufferBuiltIn::fillBufferRightLeftover:
        kernelName = "FillBufferRightLeftover";
        builtInGroup = NEO::BuiltIn::Group::fillBuffer;
        break;
    case BufferBuiltIn::fillBufferRightLeftoverStateless:
        kernelName = "FillBufferRightLeftover";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStateless;
        break;
    case BufferBuiltIn::fillBufferRightLeftoverWideStateless:
        kernelName = "FillBufferRightLeftover";
        builtInGroup = NEO::BuiltIn::Group::fillBufferWideStateless;
        break;
    case BufferBuiltIn::fillBufferRightLeftoverStatelessHeapless:
        kernelName = "FillBufferRightLeftover";
        builtInGroup = NEO::BuiltIn::Group::fillBufferStatelessHeapless;
        break;
    case BufferBuiltIn::fillBufferRightLeftoverWideStatelessHeapless:
        kernelName = "FillBufferRightLeftover";
        builtInGroup = NEO::BuiltIn::Group::fillBufferWideStatelessHeapless;
        break;
    case BufferBuiltIn::queryKernelTimestamps:
        kernelName = "QueryKernelTimestamps";
        builtInGroup = NEO::BuiltIn::Group::queryKernelTimestamps;
        break;
    case BufferBuiltIn::queryKernelTimestampsStateless:
        kernelName = "QueryKernelTimestamps";
        builtInGroup = NEO::BuiltIn::Group::queryKernelTimestampsStateless;
        break;
    case BufferBuiltIn::queryKernelTimestampsStatelessHeapless:
        kernelName = "QueryKernelTimestamps";
        builtInGroup = NEO::BuiltIn::Group::queryKernelTimestampsStatelessHeapless;
        break;
    case BufferBuiltIn::queryKernelTimestampsWithOffsets:
        kernelName = "QueryKernelTimestampsWithOffsets";
        builtInGroup = NEO::BuiltIn::Group::queryKernelTimestamps;
        break;
    case BufferBuiltIn::queryKernelTimestampsWithOffsetsStateless:
        kernelName = "QueryKernelTimestampsWithOffsets";
        builtInGroup = NEO::BuiltIn::Group::queryKernelTimestampsStateless;
        break;
    case BufferBuiltIn::queryKernelTimestampsWithOffsetsStatelessHeapless:
        kernelName = "QueryKernelTimestampsWithOffsets";
        builtInGroup = NEO::BuiltIn::Group::queryKernelTimestampsStatelessHeapless;
        break;
    default:
        UNRECOVERABLE_IF(true);
    };

    auto builtId = static_cast<uint32_t>(func);
    builtins[builtId] = loadBuiltIn(builtInGroup, kernelName);
}

void BuiltInKernelLibImpl::initBuiltinImageKernel(ImageBuiltIn func) {
    const char *kernelName = nullptr;
    NEO::BuiltIn::Group builtInGroup;

    switch (func) {
    case ImageBuiltIn::copyBufferToImage3d16Bytes:
        kernelName = "CopyBufferToImage3d16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3d;
        break;
    case ImageBuiltIn::copyBufferToImage3d16BytesAligned:
        kernelName = "CopyBufferToImage3d16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3d;
        break;
    case ImageBuiltIn::copyBufferToImage3d16BytesStateless:
        kernelName = "CopyBufferToImage3d16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d16BytesWideStateless:
        kernelName = "CopyBufferToImage3d16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d16BytesWideStatelessHeapless:
        kernelName = "CopyBufferToImage3d16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d16BytesAlignedStateless:
        kernelName = "CopyBufferToImage3d16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d16BytesAlignedWideStateless:
        kernelName = "CopyBufferToImage3d16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d16BytesAlignedWideStatelessHeapless:
        kernelName = "CopyBufferToImage3d16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d16BytesStatelessHeapless:
        kernelName = "CopyBufferToImage3d16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d16BytesAlignedStatelessHeapless:
        kernelName = "CopyBufferToImage3d16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d2Bytes:
        kernelName = "CopyBufferToImage3d2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3d;
        break;
    case ImageBuiltIn::copyBufferToImage3d2BytesStateless:
        kernelName = "CopyBufferToImage3d2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d2BytesWideStateless:
        kernelName = "CopyBufferToImage3d2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d2BytesStatelessHeapless:
        kernelName = "CopyBufferToImage3d2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d2BytesWideStatelessHeapless:
        kernelName = "CopyBufferToImage3d2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d4Bytes:
        kernelName = "CopyBufferToImage3d4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3d;
        break;
    case ImageBuiltIn::copyBufferToImage3d4BytesStateless:
        kernelName = "CopyBufferToImage3d4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d4BytesWideStateless:
        kernelName = "CopyBufferToImage3d4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d4BytesStatelessHeapless:
        kernelName = "CopyBufferToImage3d4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d4BytesWideStatelessHeapless:
        kernelName = "CopyBufferToImage3d4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d3To4Bytes:
        kernelName = "CopyBufferToImage3d3To4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3d;
        break;
    case ImageBuiltIn::copyBufferToImage3d3To4BytesStateless:
        kernelName = "CopyBufferToImage3d3To4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d3To4BytesWideStateless:
        kernelName = "CopyBufferToImage3d3To4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d3To4BytesStatelessHeapless:
        kernelName = "CopyBufferToImage3d3To4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d3To4BytesWideStatelessHeapless:
        kernelName = "CopyBufferToImage3d3To4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d8Bytes:
        kernelName = "CopyBufferToImage3d8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3d;
        break;
    case ImageBuiltIn::copyBufferToImage3d8BytesStateless:
        kernelName = "CopyBufferToImage3d8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d8BytesWideStateless:
        kernelName = "CopyBufferToImage3d8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d8BytesStatelessHeapless:
        kernelName = "CopyBufferToImage3d8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d8BytesWideStatelessHeapless:
        kernelName = "CopyBufferToImage3d8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d6To8Bytes:
        kernelName = "CopyBufferToImage3d6To8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3d;
        break;
    case ImageBuiltIn::copyBufferToImage3d6To8BytesStateless:
        kernelName = "CopyBufferToImage3d6To8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d6To8BytesWideStateless:
        kernelName = "CopyBufferToImage3d6To8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3d6To8BytesStatelessHeapless:
        kernelName = "CopyBufferToImage3d6To8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3d6To8BytesWideStatelessHeapless:
        kernelName = "CopyBufferToImage3d6To8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3dBytes:
        kernelName = "CopyBufferToImage3dBytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3d;
        break;
    case ImageBuiltIn::copyBufferToImage3dBytesStateless:
        kernelName = "CopyBufferToImage3dBytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3dBytesWideStateless:
        kernelName = "CopyBufferToImage3dBytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltIn::copyBufferToImage3dBytesStatelessHeapless:
        kernelName = "CopyBufferToImage3dBytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltIn::copyBufferToImage3dBytesWideStatelessHeapless:
        kernelName = "CopyBufferToImage3dBytes";
        builtInGroup = NEO::BuiltIn::Group::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16Bytes:
        kernelName = "CopyImage3dToBuffer16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16BytesAligned:
        kernelName = "CopyImage3dToBuffer16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16BytesStateless:
        kernelName = "CopyImage3dToBuffer16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16BytesWideStateless:
        kernelName = "CopyImage3dToBuffer16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16BytesAlignedStateless:
        kernelName = "CopyImage3dToBuffer16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16BytesAlignedWideStateless:
        kernelName = "CopyImage3dToBuffer16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16BytesStatelessHeapless:
        kernelName = "CopyImage3dToBuffer16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16BytesWideStatelessHeapless:
        kernelName = "CopyImage3dToBuffer16Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16BytesAlignedStatelessHeapless:
        kernelName = "CopyImage3dToBuffer16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer16BytesAlignedWideStatelessHeapless:
        kernelName = "CopyImage3dToBuffer16BytesAligned";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer2Bytes:
        kernelName = "CopyImage3dToBuffer2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBuffer2BytesStateless:
        kernelName = "CopyImage3dToBuffer2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer2BytesWideStateless:
        kernelName = "CopyImage3dToBuffer2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer2BytesStatelessHeapless:
        kernelName = "CopyImage3dToBuffer2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer2BytesWideStatelessHeapless:
        kernelName = "CopyImage3dToBuffer2Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer3Bytes:
        kernelName = "CopyImage3dToBuffer3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBuffer3BytesStateless:
        kernelName = "CopyImage3dToBuffer3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer3BytesWideStateless:
        kernelName = "CopyImage3dToBuffer3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer3BytesStatelessHeapless:
        kernelName = "CopyImage3dToBuffer3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer3BytesWideStatelessHeapless:
        kernelName = "CopyImage3dToBuffer3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4Bytes:
        kernelName = "CopyImage3dToBuffer4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4BytesStateless:
        kernelName = "CopyImage3dToBuffer4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4BytesWideStateless:
        kernelName = "CopyImage3dToBuffer4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4BytesStatelessHeapless:
        kernelName = "CopyImage3dToBuffer4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4BytesWideStatelessHeapless:
        kernelName = "CopyImage3dToBuffer4Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4To3Bytes:
        kernelName = "CopyImage3dToBuffer4To3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4To3BytesStateless:
        kernelName = "CopyImage3dToBuffer4To3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4To3BytesWideStateless:
        kernelName = "CopyImage3dToBuffer4To3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4To3BytesStatelessHeapless:
        kernelName = "CopyImage3dToBuffer4To3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer4To3BytesWideStatelessHeapless:
        kernelName = "CopyImage3dToBuffer4To3Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer6Bytes:
        kernelName = "CopyImage3dToBuffer6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBuffer6BytesStateless:
        kernelName = "CopyImage3dToBuffer6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer6BytesWideStateless:
        kernelName = "CopyImage3dToBuffer6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer6BytesStatelessHeapless:
        kernelName = "CopyImage3dToBuffer6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer6BytesWideStatelessHeapless:
        kernelName = "CopyImage3dToBuffer6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8Bytes:
        kernelName = "CopyImage3dToBuffer8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8BytesStateless:
        kernelName = "CopyImage3dToBuffer8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8BytesWideStateless:
        kernelName = "CopyImage3dToBuffer8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8BytesStatelessHeapless:
        kernelName = "CopyImage3dToBuffer8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8BytesWideStatelessHeapless:
        kernelName = "CopyImage3dToBuffer8Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8To6Bytes:
        kernelName = "CopyImage3dToBuffer8To6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8To6BytesStateless:
        kernelName = "CopyImage3dToBuffer8To6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8To6BytesWideStateless:
        kernelName = "CopyImage3dToBuffer8To6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8To6BytesStatelessHeapless:
        kernelName = "CopyImage3dToBuffer8To6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBuffer8To6BytesWideStatelessHeapless:
        kernelName = "CopyImage3dToBuffer8To6Bytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBufferBytes:
        kernelName = "CopyImage3dToBufferBytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBuffer;
        break;
    case ImageBuiltIn::copyImage3dToBufferBytesStateless:
        kernelName = "CopyImage3dToBufferBytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStateless;
        break;
    case ImageBuiltIn::copyImage3dToBufferBytesWideStateless:
        kernelName = "CopyImage3dToBufferBytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltIn::copyImage3dToBufferBytesStatelessHeapless:
        kernelName = "CopyImage3dToBufferBytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltIn::copyImage3dToBufferBytesWideStatelessHeapless:
        kernelName = "CopyImage3dToBufferBytes";
        builtInGroup = NEO::BuiltIn::Group::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltIn::copyImageRegion:
        kernelName = "CopyImage3dToImage3d";
        builtInGroup = NEO::BuiltIn::Group::copyImageToImage3d;
        break;
    case ImageBuiltIn::copyImageRegionHeapless:
        kernelName = "CopyImage3dToImage3d";
        builtInGroup = NEO::BuiltIn::Group::copyImageToImage3dHeapless;
        break;
    default:
        UNRECOVERABLE_IF(true);
    };

    auto builtId = static_cast<uint32_t>(func);
    imageBuiltins[builtId] = loadBuiltIn(builtInGroup, kernelName);
}

BuiltInKernelLibImpl::BuiltInKernelLibImpl(Device *device, NEO::BuiltIns *builtInsLib) : device(device), builtInsLib(builtInsLib) {
    if (initBuiltinsAsyncEnabled(device)) {
        this->initAsyncComplete = false;

        auto initFunc = [this]() {
            const auto &compilerProductHelper = this->device->getCompilerProductHelper();

            if (compilerProductHelper.isHeaplessModeEnabled(this->device->getHwInfo())) {
                this->initBuiltinKernel(BufferBuiltIn::fillBufferImmediateStatelessHeapless);
            } else if (compilerProductHelper.isForceToStatelessRequired()) {
                this->initBuiltinKernel(BufferBuiltIn::fillBufferImmediateStateless);
            } else {
                this->initBuiltinKernel(BufferBuiltIn::fillBufferImmediate);
            }

            this->initAsync.store(true);
        };

        std::thread initAsyncThread(initFunc);
        initAsyncThread.detach();
    }
}

Kernel *BuiltInKernelLibImpl::getFunction(BufferBuiltIn func) {
    auto builtId = static_cast<uint32_t>(func);

    this->ensureInitCompletion();
    if (builtins[builtId].get() == nullptr) {
        initBuiltinKernel(func);
    }

    return builtins[builtId]->func.get();
}

Kernel *BuiltInKernelLibImpl::getImageFunction(ImageBuiltIn func) {
    auto builtId = static_cast<uint32_t>(func);

    this->ensureInitCompletion();
    if (imageBuiltins[builtId].get() == nullptr) {
        initBuiltinImageKernel(func);
    }

    return imageBuiltins[builtId]->func.get();
}

std::unique_ptr<BuiltInKernelLibImpl::BuiltInKernelData> BuiltInKernelLibImpl::loadBuiltIn(NEO::BuiltIn::Group builtInGroup, const char *kernelName) {
    using BuiltInCodeType = NEO::BuiltIn::CodeType;

    if (!NEO::BuiltIn::EmbeddedStorageRegistry::exists) {
        return nullptr;
    }

    StackVec<BuiltInCodeType, 2> supportedTypes{};
    bool requiresRebuild = !device->getNEODevice()->getExecutionEnvironment()->isOneApiPvcWaEnv();
    if (!requiresRebuild && !NEO::debugManager.flags.RebuildPrecompiledKernels.get()) {
        supportedTypes.push_back(BuiltInCodeType::binary);
    }

    supportedTypes.push_back(BuiltInCodeType::intermediate);

    NEO::BuiltIn::Code builtinCode{};

    for (auto &builtinCodeType : supportedTypes) {
        builtinCode = builtInsLib->getBuiltinsLib().getBuiltinCode(builtInGroup, builtinCodeType, *device->getNEODevice());
        if (!builtinCode.resource.empty()) {
            break;
        }
    }

    if (builtinCode.resource.empty()) {
        return nullptr;
    }

    [[maybe_unused]] ze_result_t res;

    const auto groupIndex = NEO::BuiltIn::toIndex(builtInGroup);
    if (this->modules.size() <= groupIndex) {
        this->modules.resize(groupIndex + 1u);
    }

    if (this->modules[groupIndex].get() == nullptr) {
        std::unique_ptr<Module> module;
        ze_module_handle_t moduleHandle = {};
        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = builtinCode.type == BuiltInCodeType::binary ? ZE_MODULE_FORMAT_NATIVE : ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<uint8_t *>(&builtinCode.resource[0]);
        moduleDesc.inputSize = builtinCode.resource.size();
        res = device->createModule(&moduleDesc, &moduleHandle, nullptr, ModuleType::builtin);
        UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);

        module.reset(Module::fromHandle(moduleHandle));
        this->modules[groupIndex] = std::move(module);
    }

    std::unique_ptr<Kernel> kernel;
    ze_kernel_handle_t kernelHandle;
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName;
    res = this->modules[groupIndex]->createKernel(&kernelDesc, &kernelHandle);
    UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);

    kernel.reset(Kernel::fromHandle(kernelHandle));
    return std::unique_ptr<BuiltInKernelData>(new BuiltInKernelData{modules[groupIndex].get(), std::move(kernel)});
}

void BuiltInKernelLibImpl::ensureInitCompletion() {
    this->ensureInitCompletionImpl();
}

void BuiltInKernelLibImpl::ensureInitCompletionImpl() {
    if (!this->initAsyncComplete) {
        while (!this->initAsync.load()) {
        }
        this->initAsyncComplete = true;
    }
}

} // namespace L0
