/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"

#include "shared/source/built_ins/built_ins.h"

namespace NEO {
const char *getAdditionalBuiltinAsString(EBuiltInOps::Type builtin) {
    return nullptr;
}
} // namespace NEO

namespace L0 {

std::unique_lock<BuiltinFunctionsLib::MutexType> BuiltinFunctionsLib::obtainUniqueOwnership() {
    return std::unique_lock<BuiltinFunctionsLib::MutexType>(this->ownershipMutex);
}

void BuiltinFunctionsLibImpl::initBuiltinKernel(Builtin func) {
    const char *builtinName = nullptr;
    NEO::EBuiltInOps::Type builtin;

    switch (func) {
    case Builtin::CopyBufferBytes:
        builtinName = "copyBufferToBufferBytesSingle";
        builtin = NEO::EBuiltInOps::CopyBufferToBuffer;
        break;
    case Builtin::CopyBufferBytesStateless:
        builtinName = "copyBufferToBufferBytesSingle";
        builtin = NEO::EBuiltInOps::CopyBufferToBufferStateless;
        break;
    case Builtin::CopyBufferRectBytes2d:
        builtinName = "CopyBufferRectBytes2d";
        builtin = NEO::EBuiltInOps::CopyBufferRect;
        break;
    case Builtin::CopyBufferRectBytes3d:
        builtinName = "CopyBufferRectBytes3d";
        builtin = NEO::EBuiltInOps::CopyBufferRect;
        break;
    case Builtin::CopyBufferToBufferMiddle:
        builtinName = "CopyBufferToBufferMiddleRegion";
        builtin = NEO::EBuiltInOps::CopyBufferToBuffer;
        break;
    case Builtin::CopyBufferToBufferMiddleStateless:
        builtinName = "CopyBufferToBufferMiddleRegion";
        builtin = NEO::EBuiltInOps::CopyBufferToBufferStateless;
        break;
    case Builtin::CopyBufferToBufferSide:
        builtinName = "CopyBufferToBufferSideRegion";
        builtin = NEO::EBuiltInOps::CopyBufferToBuffer;
        break;
    case Builtin::CopyBufferToBufferSideStateless:
        builtinName = "CopyBufferToBufferSideRegion";
        builtin = NEO::EBuiltInOps::CopyBufferToBufferStateless;
        break;
    case Builtin::FillBufferImmediate:
        builtinName = "FillBufferImmediate";
        builtin = NEO::EBuiltInOps::FillBuffer;
        break;
    case Builtin::FillBufferImmediateStateless:
        builtinName = "FillBufferImmediate";
        builtin = NEO::EBuiltInOps::FillBufferStateless;
        break;
    case Builtin::FillBufferSSHOffset:
        builtinName = "FillBufferSSHOffset";
        builtin = NEO::EBuiltInOps::FillBuffer;
        break;
    case Builtin::FillBufferSSHOffsetStateless:
        builtinName = "FillBufferSSHOffset";
        builtin = NEO::EBuiltInOps::FillBufferStateless;
        break;
    case Builtin::FillBufferMiddle:
        builtinName = "FillBufferMiddle";
        builtin = NEO::EBuiltInOps::FillBuffer;
        break;
    case Builtin::FillBufferMiddleStateless:
        builtinName = "FillBufferMiddle";
        builtin = NEO::EBuiltInOps::FillBufferStateless;
        break;
    case Builtin::FillBufferRightLeftover:
        builtinName = "FillBufferRightLeftover";
        builtin = NEO::EBuiltInOps::FillBuffer;
        break;
    case Builtin::FillBufferRightLeftoverStateless:
        builtinName = "FillBufferRightLeftover";
        builtin = NEO::EBuiltInOps::FillBufferStateless;
        break;
    case Builtin::QueryKernelTimestamps:
        builtinName = "QueryKernelTimestamps";
        builtin = NEO::EBuiltInOps::QueryKernelTimestamps;
        break;
    case Builtin::QueryKernelTimestampsWithOffsets:
        builtinName = "QueryKernelTimestampsWithOffsets";
        builtin = NEO::EBuiltInOps::QueryKernelTimestamps;
        break;
    default:
        UNRECOVERABLE_IF(true);
    };

    auto builtId = static_cast<uint32_t>(func);
    builtins[builtId] = loadBuiltIn(builtin, builtinName);
}

void BuiltinFunctionsLibImpl::initBuiltinImageKernel(ImageBuiltin func) {
    const char *builtinName = nullptr;
    NEO::EBuiltInOps::Type builtin;

    switch (func) {
    case ImageBuiltin::CopyBufferToImage3d16Bytes:
        builtinName = "CopyBufferToImage3d16Bytes";
        builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
        break;
    case ImageBuiltin::CopyBufferToImage3d2Bytes:
        builtinName = "CopyBufferToImage3d2Bytes";
        builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
        break;
    case ImageBuiltin::CopyBufferToImage3d4Bytes:
        builtinName = "CopyBufferToImage3d4Bytes";
        builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
        break;
    case ImageBuiltin::CopyBufferToImage3d8Bytes:
        builtinName = "CopyBufferToImage3d8Bytes";
        builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
        break;
    case ImageBuiltin::CopyBufferToImage3dBytes:
        builtinName = "CopyBufferToImage3dBytes";
        builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
        break;
    case ImageBuiltin::CopyImage3dToBuffer16Bytes:
        builtinName = "CopyImage3dToBuffer16Bytes";
        builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
        break;
    case ImageBuiltin::CopyImage3dToBuffer2Bytes:
        builtinName = "CopyImage3dToBuffer2Bytes";
        builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
        break;
    case ImageBuiltin::CopyImage3dToBuffer4Bytes:
        builtinName = "CopyImage3dToBuffer4Bytes";
        builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
        break;
    case ImageBuiltin::CopyImage3dToBuffer8Bytes:
        builtinName = "CopyImage3dToBuffer8Bytes";
        builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
        break;
    case ImageBuiltin::CopyImage3dToBufferBytes:
        builtinName = "CopyImage3dToBufferBytes";
        builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
        break;
    case ImageBuiltin::CopyImageRegion:
        builtinName = "CopyImageToImage3d";
        builtin = NEO::EBuiltInOps::CopyImageToImage3d;
        break;
    default:
        UNRECOVERABLE_IF(true);
    };

    auto builtId = static_cast<uint32_t>(func);
    imageBuiltins[builtId] = loadBuiltIn(builtin, builtinName);
}

Kernel *BuiltinFunctionsLibImpl::getFunction(Builtin func) {
    auto builtId = static_cast<uint32_t>(func);

    if (builtins[builtId].get() == nullptr) {
        initBuiltinKernel(func);
    }

    return builtins[builtId]->func.get();
}

Kernel *BuiltinFunctionsLibImpl::getImageFunction(ImageBuiltin func) {
    auto builtId = static_cast<uint32_t>(func);

    if (imageBuiltins[builtId].get() == nullptr) {
        initBuiltinImageKernel(func);
    }

    return imageBuiltins[builtId]->func.get();
}

std::unique_ptr<BuiltinFunctionsLibImpl::BuiltinData> BuiltinFunctionsLibImpl::loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName) {
    using BuiltInCodeType = NEO::BuiltinCode::ECodeType;

    StackVec<BuiltInCodeType, 2> supportedTypes{};
    if (!NEO::DebugManager.flags.RebuildPrecompiledKernels.get()) {
        supportedTypes.push_back(BuiltInCodeType::Binary);
    }
    supportedTypes.push_back(BuiltInCodeType::Intermediate);

    NEO::BuiltinCode builtinCode{};

    for (auto &builtinCodeType : supportedTypes) {
        builtinCode = builtInsLib->getBuiltinsLib().getBuiltinCode(builtin, builtinCodeType, *device->getNEODevice());
        if (!builtinCode.resource.empty()) {
            break;
        }
    }

    [[maybe_unused]] ze_result_t res;
    std::unique_ptr<Module> module;
    ze_module_handle_t moduleHandle;
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = builtinCode.type == BuiltInCodeType::Binary ? ZE_MODULE_FORMAT_NATIVE : ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<uint8_t *>(&builtinCode.resource[0]);
    moduleDesc.inputSize = builtinCode.resource.size();
    res = device->createModule(&moduleDesc, &moduleHandle, nullptr, ModuleType::Builtin);
    UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);

    module.reset(Module::fromHandle(moduleHandle));

    std::unique_ptr<Kernel> kernel;
    ze_kernel_handle_t kernelHandle;
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = builtInName;
    res = module->createKernel(&kernelDesc, &kernelHandle);
    DEBUG_BREAK_IF(res != ZE_RESULT_SUCCESS);

    kernel.reset(Kernel::fromHandle(kernelHandle));
    return std::unique_ptr<BuiltinData>(new BuiltinData{std::move(module), std::move(kernel)});
}

} // namespace L0
