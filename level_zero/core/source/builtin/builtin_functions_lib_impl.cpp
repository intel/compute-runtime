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

BuiltinFunctionsLibImpl::BuiltinData::~BuiltinData() {
    func->destroy();
    func.release();
}
BuiltinFunctionsLibImpl::BuiltinData::BuiltinData() = default;
BuiltinFunctionsLibImpl::BuiltinData::BuiltinData(Module *module, std::unique_ptr<L0::Kernel> &&ker) : module(module), func(std::move(ker)) {}
std::unique_lock<BuiltinFunctionsLib::MutexType> BuiltinFunctionsLib::obtainUniqueOwnership() {
    return std::unique_lock<BuiltinFunctionsLib::MutexType>(this->ownershipMutex);
}

void BuiltinFunctionsLibImpl::initBuiltinKernel(Builtin func) {
    const char *kernelName = nullptr;
    NEO::EBuiltInOps::Type builtin;

    switch (func) {
    case Builtin::copyBufferBytes:
        kernelName = "copyBufferToBufferBytesSingle";
        builtin = NEO::EBuiltInOps::copyBufferToBuffer;
        break;
    case Builtin::copyBufferBytesStateless:
        kernelName = "copyBufferToBufferBytesSingle";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStateless;
        break;
    case Builtin::copyBufferBytesWideStateless:
        kernelName = "copyBufferToBufferBytesSingle";
        builtin = NEO::EBuiltInOps::copyBufferToBufferWideStateless;
        break;
    case Builtin::copyBufferBytesStatelessHeapless:
        kernelName = "copyBufferToBufferBytesSingle";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStatelessHeapless;
        break;
    case Builtin::copyBufferBytesWideStatelessHeapless:
        kernelName = "copyBufferToBufferBytesSingle";
        builtin = NEO::EBuiltInOps::copyBufferToBufferWideStatelessHeapless;
        break;
    case Builtin::copyBufferRectBytes2d:
        kernelName = "CopyBufferRectBytes2d";
        builtin = NEO::EBuiltInOps::copyBufferRect;
        break;
    case Builtin::copyBufferRectBytes2dStateless:
        kernelName = "CopyBufferRectBytes2d";
        builtin = NEO::EBuiltInOps::copyBufferRectStateless;
        break;
    case Builtin::copyBufferRectBytes2dStatelessHeapless:
        kernelName = "CopyBufferRectBytes2d";
        builtin = NEO::EBuiltInOps::copyBufferRectStatelessHeapless;
        break;
    case Builtin::copyBufferRectBytes3d:
        kernelName = "CopyBufferRectBytes3d";
        builtin = NEO::EBuiltInOps::copyBufferRect;
        break;
    case Builtin::copyBufferRectBytes3dStateless:
        kernelName = "CopyBufferRectBytes3d";
        builtin = NEO::EBuiltInOps::copyBufferRectStateless;
        break;
    case Builtin::copyBufferRectBytes3dStatelessHeapless:
        kernelName = "CopyBufferRectBytes3d";
        builtin = NEO::EBuiltInOps::copyBufferRectStatelessHeapless;
        break;
    case Builtin::copyBufferToBufferMiddle:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBuffer;
        break;
    case Builtin::copyBufferToBufferMiddleStateless:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStateless;
        break;
    case Builtin::copyBufferToBufferMiddleWideStateless:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBufferWideStateless;
        break;
    case Builtin::copyBufferToBufferMiddleStatelessHeapless:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStatelessHeapless;
        break;
    case Builtin::copyBufferToBufferMiddleWideStatelessHeapless:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBufferWideStatelessHeapless;
        break;
    case Builtin::copyBufferToBufferSide:
        kernelName = "CopyBufferToBufferSideRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBuffer;
        break;
    case Builtin::copyBufferToBufferSideStateless:
        kernelName = "CopyBufferToBufferSideRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStateless;
        break;
    case Builtin::copyBufferToBufferSideWideStateless:
        kernelName = "CopyBufferToBufferSideRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBufferWideStateless;
        break;
    case Builtin::copyBufferToBufferSideStatelessHeapless:
        kernelName = "CopyBufferToBufferSideRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStatelessHeapless;
        break;
    case Builtin::copyBufferToBufferSideWideStatelessHeapless:
        kernelName = "CopyBufferToBufferSideRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBufferWideStatelessHeapless;
        break;
    case Builtin::fillBufferImmediate:
        kernelName = "FillBufferImmediate";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferImmediateStateless:
        kernelName = "FillBufferImmediate";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferImmediateWideStateless:
        kernelName = "FillBufferImmediate";
        builtin = NEO::EBuiltInOps::fillBufferWideStateless;
        break;
    case Builtin::fillBufferImmediateStatelessHeapless:
        kernelName = "FillBufferImmediate";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
        break;
    case Builtin::fillBufferImmediateWideStatelessHeapless:
        kernelName = "FillBufferImmediate";
        builtin = NEO::EBuiltInOps::fillBufferWideStatelessHeapless;
        break;
    case Builtin::fillBufferImmediateLeftOver:
        kernelName = "FillBufferImmediateLeftOver";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferImmediateLeftOverStateless:
        kernelName = "FillBufferImmediateLeftOver";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferImmediateLeftOverWideStateless:
        kernelName = "FillBufferImmediateLeftOver";
        builtin = NEO::EBuiltInOps::fillBufferWideStateless;
        break;
    case Builtin::fillBufferImmediateLeftOverStatelessHeapless:
        kernelName = "FillBufferImmediateLeftOver";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
        break;
    case Builtin::fillBufferImmediateLeftOverWideStatelessHeapless:
        kernelName = "FillBufferImmediateLeftOver";
        builtin = NEO::EBuiltInOps::fillBufferWideStatelessHeapless;
        break;
    case Builtin::fillBufferSSHOffset:
        kernelName = "FillBufferSSHOffset";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferSSHOffsetStateless:
        kernelName = "FillBufferSSHOffset";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferSSHOffsetStatelessHeapless:
        kernelName = "FillBufferSSHOffset";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
        break;
    case Builtin::fillBufferMiddle:
        kernelName = "FillBufferMiddle";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferMiddleStateless:
        kernelName = "FillBufferMiddle";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferMiddleWideStateless:
        kernelName = "FillBufferMiddle";
        builtin = NEO::EBuiltInOps::fillBufferWideStateless;
        break;
    case Builtin::fillBufferMiddleStatelessHeapless:
        kernelName = "FillBufferMiddle";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
        break;
    case Builtin::fillBufferMiddleWideStatelessHeapless:
        kernelName = "FillBufferMiddle";
        builtin = NEO::EBuiltInOps::fillBufferWideStatelessHeapless;
        break;
    case Builtin::fillBufferRightLeftover:
        kernelName = "FillBufferRightLeftover";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferRightLeftoverStateless:
        kernelName = "FillBufferRightLeftover";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferRightLeftoverWideStateless:
        kernelName = "FillBufferRightLeftover";
        builtin = NEO::EBuiltInOps::fillBufferWideStateless;
        break;
    case Builtin::fillBufferRightLeftoverStatelessHeapless:
        kernelName = "FillBufferRightLeftover";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
        break;
    case Builtin::fillBufferRightLeftoverWideStatelessHeapless:
        kernelName = "FillBufferRightLeftover";
        builtin = NEO::EBuiltInOps::fillBufferWideStatelessHeapless;
        break;
    case Builtin::queryKernelTimestamps:
        kernelName = "QueryKernelTimestamps";
        builtin = NEO::EBuiltInOps::queryKernelTimestamps;
        break;
    case Builtin::queryKernelTimestampsWithOffsets:
        kernelName = "QueryKernelTimestampsWithOffsets";
        builtin = NEO::EBuiltInOps::queryKernelTimestamps;
        break;
    default:
        UNRECOVERABLE_IF(true);
    };

    auto builtId = static_cast<uint32_t>(func);
    builtins[builtId] = loadBuiltIn(builtin, kernelName);
}

void BuiltinFunctionsLibImpl::initBuiltinImageKernel(ImageBuiltin func) {
    const char *builtinName = nullptr;
    NEO::EBuiltInOps::Type builtin;

    switch (func) {
    case ImageBuiltin::copyBufferToImage3d16Bytes:
        builtinName = "CopyBufferToImage3d16Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d16BytesAligned:
        builtinName = "CopyBufferToImage3d16BytesAligned";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d16BytesStateless:
        builtinName = "CopyBufferToImage3d16Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d16BytesWideStateless:
        builtinName = "CopyBufferToImage3d16Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d16BytesWideStatelessHeapless:
        builtinName = "CopyBufferToImage3d16Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d16BytesAlignedStateless:
        builtinName = "CopyBufferToImage3d16BytesAligned";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d16BytesAlignedWideStateless:
        builtinName = "CopyBufferToImage3d16BytesAligned";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d16BytesAlignedWideStatelessHeapless:
        builtinName = "CopyBufferToImage3d16BytesAligned";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d16BytesStatelessHeapless:
        builtinName = "CopyBufferToImage3d16Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d16BytesAlignedStatelessHeapless:
        builtinName = "CopyBufferToImage3d16BytesAligned";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d2Bytes:
        builtinName = "CopyBufferToImage3d2Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d2BytesStateless:
        builtinName = "CopyBufferToImage3d2Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d2BytesWideStateless:
        builtinName = "CopyBufferToImage3d2Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d2BytesStatelessHeapless:
        builtinName = "CopyBufferToImage3d2Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d2BytesWideStatelessHeapless:
        builtinName = "CopyBufferToImage3d2Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d4Bytes:
        builtinName = "CopyBufferToImage3d4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d4BytesStateless:
        builtinName = "CopyBufferToImage3d4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d4BytesWideStateless:
        builtinName = "CopyBufferToImage3d4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d4BytesStatelessHeapless:
        builtinName = "CopyBufferToImage3d4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d4BytesWideStatelessHeapless:
        builtinName = "CopyBufferToImage3d4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d3To4Bytes:
        builtinName = "CopyBufferToImage3d3To4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d3To4BytesStateless:
        builtinName = "CopyBufferToImage3d3To4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d3To4BytesWideStateless:
        builtinName = "CopyBufferToImage3d3To4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d3To4BytesStatelessHeapless:
        builtinName = "CopyBufferToImage3d3To4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d3To4BytesWideStatelessHeapless:
        builtinName = "CopyBufferToImage3d3To4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d8Bytes:
        builtinName = "CopyBufferToImage3d8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d8BytesStateless:
        builtinName = "CopyBufferToImage3d8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d8BytesWideStateless:
        builtinName = "CopyBufferToImage3d8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d8BytesStatelessHeapless:
        builtinName = "CopyBufferToImage3d8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d8BytesWideStatelessHeapless:
        builtinName = "CopyBufferToImage3d8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d6To8Bytes:
        builtinName = "CopyBufferToImage3d6To8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d6To8BytesStateless:
        builtinName = "CopyBufferToImage3d6To8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d6To8BytesWideStateless:
        builtinName = "CopyBufferToImage3d6To8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltin::copyBufferToImage3d6To8BytesStatelessHeapless:
        builtinName = "CopyBufferToImage3d6To8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d6To8BytesWideStatelessHeapless:
        builtinName = "CopyBufferToImage3d6To8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3dBytes:
        builtinName = "CopyBufferToImage3dBytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3dBytesStateless:
        builtinName = "CopyBufferToImage3dBytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStateless;
        break;
    case ImageBuiltin::copyBufferToImage3dBytesWideStateless:
        builtinName = "CopyBufferToImage3dBytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStateless;
        break;
    case ImageBuiltin::copyBufferToImage3dBytesStatelessHeapless:
        builtinName = "CopyBufferToImage3dBytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dStatelessHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3dBytesWideStatelessHeapless:
        builtinName = "CopyBufferToImage3dBytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer16Bytes:
        builtinName = "CopyImage3dToBuffer16Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesAligned:
        builtinName = "CopyImage3dToBuffer16BytesAligned";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesStateless:
        builtinName = "CopyImage3dToBuffer16Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesWideStateless:
        builtinName = "CopyImage3dToBuffer16Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesAlignedStateless:
        builtinName = "CopyImage3dToBuffer16BytesAligned";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesAlignedWideStateless:
        builtinName = "CopyImage3dToBuffer16BytesAligned";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesStatelessHeapless:
        builtinName = "CopyImage3dToBuffer16Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesWideStatelessHeapless:
        builtinName = "CopyImage3dToBuffer16Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesAlignedStatelessHeapless:
        builtinName = "CopyImage3dToBuffer16BytesAligned";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesAlignedWideStatelessHeapless:
        builtinName = "CopyImage3dToBuffer16BytesAligned";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer2Bytes:
        builtinName = "CopyImage3dToBuffer2Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer2BytesStateless:
        builtinName = "CopyImage3dToBuffer2Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer2BytesWideStateless:
        builtinName = "CopyImage3dToBuffer2Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer2BytesStatelessHeapless:
        builtinName = "CopyImage3dToBuffer2Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer2BytesWideStatelessHeapless:
        builtinName = "CopyImage3dToBuffer2Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer3Bytes:
        builtinName = "CopyImage3dToBuffer3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer3BytesStateless:
        builtinName = "CopyImage3dToBuffer3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer3BytesWideStateless:
        builtinName = "CopyImage3dToBuffer3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer3BytesStatelessHeapless:
        builtinName = "CopyImage3dToBuffer3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer3BytesWideStatelessHeapless:
        builtinName = "CopyImage3dToBuffer3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4Bytes:
        builtinName = "CopyImage3dToBuffer4Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer4BytesStateless:
        builtinName = "CopyImage3dToBuffer4Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4BytesWideStateless:
        builtinName = "CopyImage3dToBuffer4Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4BytesStatelessHeapless:
        builtinName = "CopyImage3dToBuffer4Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4BytesWideStatelessHeapless:
        builtinName = "CopyImage3dToBuffer4Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4To3Bytes:
        builtinName = "CopyImage3dToBuffer4To3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer4To3BytesStateless:
        builtinName = "CopyImage3dToBuffer4To3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4To3BytesWideStateless:
        builtinName = "CopyImage3dToBuffer4To3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4To3BytesStatelessHeapless:
        builtinName = "CopyImage3dToBuffer4To3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4To3BytesWideStatelessHeapless:
        builtinName = "CopyImage3dToBuffer4To3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer6Bytes:
        builtinName = "CopyImage3dToBuffer6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer6BytesStateless:
        builtinName = "CopyImage3dToBuffer6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer6BytesWideStateless:
        builtinName = "CopyImage3dToBuffer6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer6BytesStatelessHeapless:
        builtinName = "CopyImage3dToBuffer6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer6BytesWideStatelessHeapless:
        builtinName = "CopyImage3dToBuffer6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8Bytes:
        builtinName = "CopyImage3dToBuffer8Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer8BytesStateless:
        builtinName = "CopyImage3dToBuffer8Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8BytesWideStateless:
        builtinName = "CopyImage3dToBuffer8Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8BytesStatelessHeapless:
        builtinName = "CopyImage3dToBuffer8Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8BytesWideStatelessHeapless:
        builtinName = "CopyImage3dToBuffer8Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8To6Bytes:
        builtinName = "CopyImage3dToBuffer8To6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer8To6BytesStateless:
        builtinName = "CopyImage3dToBuffer8To6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8To6BytesWideStateless:
        builtinName = "CopyImage3dToBuffer8To6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8To6BytesStatelessHeapless:
        builtinName = "CopyImage3dToBuffer8To6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8To6BytesWideStatelessHeapless:
        builtinName = "CopyImage3dToBuffer8To6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBufferBytes:
        builtinName = "CopyImage3dToBufferBytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBufferBytesStateless:
        builtinName = "CopyImage3dToBufferBytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStateless;
        break;
    case ImageBuiltin::copyImage3dToBufferBytesWideStateless:
        builtinName = "CopyImage3dToBufferBytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStateless;
        break;
    case ImageBuiltin::copyImage3dToBufferBytesStatelessHeapless:
        builtinName = "CopyImage3dToBufferBytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferStatelessHeapless;
        break;
    case ImageBuiltin::copyImage3dToBufferBytesWideStatelessHeapless:
        builtinName = "CopyImage3dToBufferBytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferWideStatelessHeapless;
        break;
    case ImageBuiltin::copyImageRegion:
        builtinName = "CopyImage3dToImage3d";
        builtin = NEO::EBuiltInOps::copyImageToImage3d;
        break;
    case ImageBuiltin::copyImageRegionHeapless:
        builtinName = "CopyImage3dToImage3d";
        builtin = NEO::EBuiltInOps::copyImageToImage3dHeapless;
        break;
    default:
        UNRECOVERABLE_IF(true);
    };

    auto builtId = static_cast<uint32_t>(func);
    imageBuiltins[builtId] = loadBuiltIn(builtin, builtinName);
}

BuiltinFunctionsLibImpl::BuiltinFunctionsLibImpl(Device *device, NEO::BuiltIns *builtInsLib) : device(device), builtInsLib(builtInsLib) {
    if (initBuiltinsAsyncEnabled(device)) {
        this->initAsyncComplete = false;

        auto initFunc = [this]() {
            const auto &compilerProductHelper = this->device->getCompilerProductHelper();

            if (compilerProductHelper.isHeaplessModeEnabled(this->device->getHwInfo())) {
                this->initBuiltinKernel(Builtin::fillBufferImmediateStatelessHeapless);
            } else if (compilerProductHelper.isForceToStatelessRequired()) {
                this->initBuiltinKernel(Builtin::fillBufferImmediateStateless);
            } else {
                this->initBuiltinKernel(Builtin::fillBufferImmediate);
            }

            this->initAsync.store(true);
        };

        std::thread initAsyncThread(initFunc);
        initAsyncThread.detach();
    }
}

Kernel *BuiltinFunctionsLibImpl::getFunction(Builtin func) {
    auto builtId = static_cast<uint32_t>(func);

    this->ensureInitCompletion();
    if (builtins[builtId].get() == nullptr) {
        initBuiltinKernel(func);
    }

    return builtins[builtId]->func.get();
}

Kernel *BuiltinFunctionsLibImpl::getImageFunction(ImageBuiltin func) {
    auto builtId = static_cast<uint32_t>(func);

    this->ensureInitCompletion();
    if (imageBuiltins[builtId].get() == nullptr) {
        initBuiltinImageKernel(func);
    }

    return imageBuiltins[builtId]->func.get();
}

std::unique_ptr<BuiltinFunctionsLibImpl::BuiltinData> BuiltinFunctionsLibImpl::loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName) {
    using BuiltInCodeType = NEO::BuiltinCode::ECodeType;

    if (!NEO::EmbeddedStorageRegistry::exists) {
        return nullptr;
    }

    StackVec<BuiltInCodeType, 2> supportedTypes{};
    bool requiresRebuild = !device->getNEODevice()->getExecutionEnvironment()->isOneApiPvcWaEnv();
    if (!requiresRebuild && !NEO::debugManager.flags.RebuildPrecompiledKernels.get()) {
        supportedTypes.push_back(BuiltInCodeType::binary);
    }

    supportedTypes.push_back(BuiltInCodeType::intermediate);

    NEO::BuiltinCode builtinCode{};

    for (auto &builtinCodeType : supportedTypes) {
        builtinCode = builtInsLib->getBuiltinsLib().getBuiltinCode(builtin, builtinCodeType, *device->getNEODevice());
        if (!builtinCode.resource.empty()) {
            break;
        }
    }

    if (builtinCode.resource.empty()) {
        return nullptr;
    }

    [[maybe_unused]] ze_result_t res;

    if (this->modules.size() <= builtin) {
        this->modules.resize(builtin + 1u);
    }

    if (this->modules[builtin].get() == nullptr) {
        std::unique_ptr<Module> module;
        ze_module_handle_t moduleHandle = {};
        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = builtinCode.type == BuiltInCodeType::binary ? ZE_MODULE_FORMAT_NATIVE : ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<uint8_t *>(&builtinCode.resource[0]);
        moduleDesc.inputSize = builtinCode.resource.size();
        res = device->createModule(&moduleDesc, &moduleHandle, nullptr, ModuleType::builtin);
        UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);

        module.reset(Module::fromHandle(moduleHandle));
        this->modules[builtin] = std::move(module);
    }

    std::unique_ptr<Kernel> kernel;
    ze_kernel_handle_t kernelHandle;
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = builtInName;
    res = this->modules[builtin]->createKernel(&kernelDesc, &kernelHandle);
    UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);

    kernel.reset(Kernel::fromHandle(kernelHandle));
    return std::unique_ptr<BuiltinData>(new BuiltinData{modules[builtin].get(), std::move(kernel)});
}

void BuiltinFunctionsLibImpl::ensureInitCompletion() {
    this->ensureInitCompletionImpl();
}

void BuiltinFunctionsLibImpl::ensureInitCompletionImpl() {
    if (!this->initAsyncComplete) {
        while (!this->initAsync.load()) {
        }
        this->initAsyncComplete = true;
    }
}

} // namespace L0
