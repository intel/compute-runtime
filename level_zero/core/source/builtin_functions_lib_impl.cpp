/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/builtin_functions_lib_impl.h"

#include "shared/source/built_ins/built_ins.h"

#include "level_zero/core/source/device.h"
#include "level_zero/core/source/module.h"

namespace L0 {

std::unique_ptr<BuiltinFunctionsLib> BuiltinFunctionsLib::create(Device *device,
                                                                 NEO::BuiltIns *builtins) {
    return std::unique_ptr<BuiltinFunctionsLib>(new BuiltinFunctionsLibImpl(device, builtins));
}

struct BuiltinFunctionsLibImpl::BuiltinData {
    ~BuiltinData() {
        func.reset();
        module.reset();
    }

    std::unique_ptr<Module> module;
    std::unique_ptr<Kernel> func;
};

void BuiltinFunctionsLibImpl::initFunctions() {
    for (uint32_t builtId = 0; builtId < static_cast<uint32_t>(Builtin::COUNT); builtId++) {
        const char *builtinName = nullptr;
        NEO::EBuiltInOps::Type builtin;

        switch (static_cast<Builtin>(builtId)) {
        case Builtin::CopyBufferBytes:
            builtinName = "copyBufferToBufferBytesSingle";
            builtin = NEO::EBuiltInOps::CopyBufferToBuffer;
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
        case Builtin::CopyBufferToBufferSide:
            builtinName = "CopyBufferToBufferSideRegion";
            builtin = NEO::EBuiltInOps::CopyBufferToBuffer;
            break;
        case Builtin::CopyBufferToImage3d16Bytes:
            builtinName = "CopyBufferToImage3d16Bytes";
            builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
            break;
        case Builtin::CopyBufferToImage3d2Bytes:
            builtinName = "CopyBufferToImage3d2Bytes";
            builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
            break;
        case Builtin::CopyBufferToImage3d4Bytes:
            builtinName = "CopyBufferToImage3d4Bytes";
            builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
            break;
        case Builtin::CopyBufferToImage3d8Bytes:
            builtinName = "CopyBufferToImage3d8Bytes";
            builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
            break;
        case Builtin::CopyBufferToImage3dBytes:
            builtinName = "CopyBufferToImage3dBytes";
            builtin = NEO::EBuiltInOps::CopyBufferToImage3d;
            break;
        case Builtin::CopyImage3dToBuffer16Bytes:
            builtinName = "CopyImage3dToBuffer16Bytes";
            builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
            break;
        case Builtin::CopyImage3dToBuffer2Bytes:
            builtinName = "CopyImage3dToBuffer2Bytes";
            builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
            break;
        case Builtin::CopyImage3dToBuffer4Bytes:
            builtinName = "CopyImage3dToBuffer4Bytes";
            builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
            break;
        case Builtin::CopyImage3dToBuffer8Bytes:
            builtinName = "CopyImage3dToBuffer8Bytes";
            builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
            break;
        case Builtin::CopyImage3dToBufferBytes:
            builtinName = "CopyImage3dToBufferBytes";
            builtin = NEO::EBuiltInOps::CopyImage3dToBuffer;
            break;
        case Builtin::CopyImageRegion:
            builtinName = "CopyImageToImage3d";
            builtin = NEO::EBuiltInOps::CopyImageToImage3d;
            break;
        case Builtin::FillBufferImmediate:
            builtinName = "FillBufferImmediate";
            builtin = NEO::EBuiltInOps::FillBuffer;
            break;
        case Builtin::FillBufferSSHOffset:
            builtinName = "FillBufferSSHOffset";
            builtin = NEO::EBuiltInOps::FillBuffer;
            break;
        default:
            continue;
        };

        builtins[builtId] = loadBuiltIn(builtin, builtinName);
    }
}

Kernel *BuiltinFunctionsLibImpl::getFunction(Builtin func) {
    auto builtId = static_cast<uint32_t>(func);
    return builtins[builtId]->func.get();
}

void BuiltinFunctionsLibImpl::initPageFaultFunction() {
    pageFaultBuiltin = loadBuiltIn(NEO::EBuiltInOps::CopyBufferToBuffer, "CopyBufferToBufferSideRegion");
}

Kernel *BuiltinFunctionsLibImpl::getPageFaultFunction() {
    return pageFaultBuiltin->func.get();
}

std::unique_ptr<BuiltinFunctionsLibImpl::BuiltinData> BuiltinFunctionsLibImpl::loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName) {
    auto builtInCode = builtInsLib->getBuiltinsLib().getBuiltinCode(builtin, NEO::BuiltinCode::ECodeType::Binary, *device->getNEODevice());

    ze_result_t res;
    std::unique_ptr<Module> module;
    ze_module_handle_t moduleHandle;
    ze_module_desc_t moduleDesc = {ZE_MODULE_DESC_VERSION_CURRENT};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<uint8_t *>(&builtInCode.resource[0]);
    moduleDesc.inputSize = builtInCode.resource.size();
    res = device->createModule(&moduleDesc, &moduleHandle, nullptr);
    UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);

    module.reset(Module::fromHandle(moduleHandle));

    std::unique_ptr<Kernel> function;
    ze_kernel_handle_t functionHandle;
    ze_kernel_desc_t functionDesc = {ZE_KERNEL_DESC_VERSION_CURRENT};
    functionDesc.pKernelName = builtInName;
    res = module->createKernel(&functionDesc, &functionHandle);
    DEBUG_BREAK_IF(res != ZE_RESULT_SUCCESS);
    UNUSED_VARIABLE(res);
    function.reset(Kernel::fromHandle(functionHandle));
    return std::unique_ptr<BuiltinData>(new BuiltinData{std::move(module), std::move(function)});
}

} // namespace L0
