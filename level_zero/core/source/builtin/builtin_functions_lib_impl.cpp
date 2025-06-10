/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/os_interface.h"

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
        kernelName = "copyBufferToBufferBytesSingleStateless";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStateless;
        break;
    case Builtin::copyBufferBytesStatelessHeapless:
        kernelName = "copyBufferToBufferBytesSingleStateless";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStatelessHeapless;
        break;
    case Builtin::copyBufferRectBytes2d:
        kernelName = "CopyBufferRectBytes2d";
        builtin = NEO::EBuiltInOps::copyBufferRect;
        break;
    case Builtin::copyBufferRectBytes3d:
        kernelName = "CopyBufferRectBytes3d";
        builtin = NEO::EBuiltInOps::copyBufferRect;
        break;
    case Builtin::copyBufferToBufferMiddle:
        kernelName = "CopyBufferToBufferMiddleRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBuffer;
        break;
    case Builtin::copyBufferToBufferMiddleStateless:
        kernelName = "CopyBufferToBufferMiddleRegionStateless";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStateless;
        break;
    case Builtin::copyBufferToBufferMiddleStatelessHeapless:
        kernelName = "CopyBufferToBufferMiddleRegionStateless";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStatelessHeapless;
        break;
    case Builtin::copyBufferToBufferSide:
        kernelName = "CopyBufferToBufferSideRegion";
        builtin = NEO::EBuiltInOps::copyBufferToBuffer;
        break;
    case Builtin::copyBufferToBufferSideStateless:
        kernelName = "CopyBufferToBufferSideRegionStateless";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStateless;
        break;
    case Builtin::copyBufferToBufferSideStatelessHeapless:
        kernelName = "CopyBufferToBufferSideRegionStateless";
        builtin = NEO::EBuiltInOps::copyBufferToBufferStatelessHeapless;
        break;
    case Builtin::fillBufferImmediate:
        kernelName = "FillBufferImmediate";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferImmediateStateless:
        kernelName = "FillBufferImmediateStateless";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferImmediateStatelessHeapless:
        kernelName = "FillBufferImmediateStateless";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
        break;
    case Builtin::fillBufferImmediateLeftOver:
        kernelName = "FillBufferImmediateLeftOver";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferImmediateLeftOverStateless:
        kernelName = "FillBufferImmediateLeftOverStateless";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferImmediateLeftOverStatelessHeapless:
        kernelName = "FillBufferImmediateLeftOverStateless";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
        break;
    case Builtin::fillBufferSSHOffset:
        kernelName = "FillBufferSSHOffset";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferSSHOffsetStateless:
        kernelName = "FillBufferSSHOffsetStateless";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferSSHOffsetStatelessHeapless:
        kernelName = "FillBufferSSHOffsetStateless";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
        break;
    case Builtin::fillBufferMiddle:
        kernelName = "FillBufferMiddle";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferMiddleStateless:
        kernelName = "FillBufferMiddleStateless";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferMiddleStatelessHeapless:
        kernelName = "FillBufferMiddleStateless";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
        break;
    case Builtin::fillBufferRightLeftover:
        kernelName = "FillBufferRightLeftover";
        builtin = NEO::EBuiltInOps::fillBuffer;
        break;
    case Builtin::fillBufferRightLeftoverStateless:
        kernelName = "FillBufferRightLeftoverStateless";
        builtin = NEO::EBuiltInOps::fillBufferStateless;
        break;
    case Builtin::fillBufferRightLeftoverStatelessHeapless:
        kernelName = "FillBufferRightLeftoverStateless";
        builtin = NEO::EBuiltInOps::fillBufferStatelessHeapless;
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
    case ImageBuiltin::copyBufferToImage3d16BytesHeapless:
        builtinName = "CopyBufferToImage3d16BytesStateless";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d2Bytes:
        builtinName = "CopyBufferToImage3d2Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d2BytesHeapless:
        builtinName = "CopyBufferToImage3d2BytesStateless";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d4Bytes:
        builtinName = "CopyBufferToImage3d4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d4BytesHeapless:
        builtinName = "CopyBufferToImage3d4BytesStateless";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d3To4Bytes:
        builtinName = "CopyBufferToImage3d3To4Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d3To4BytesHeapless:
        builtinName = "CopyBufferToImage3d3To4BytesStateless";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d8Bytes:
        builtinName = "CopyBufferToImage3d8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d8BytesHeapless:
        builtinName = "CopyBufferToImage3d8BytesStateless";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3d6To8Bytes:
        builtinName = "CopyBufferToImage3d6To8Bytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3d6To8BytesHeapless:
        builtinName = "CopyBufferToImage3d6To8BytesStateless";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dHeapless;
        break;
    case ImageBuiltin::copyBufferToImage3dBytes:
        builtinName = "CopyBufferToImage3dBytes";
        builtin = NEO::EBuiltInOps::copyBufferToImage3d;
        break;
    case ImageBuiltin::copyBufferToImage3dBytesHeapless:
        builtinName = "CopyBufferToImage3dBytesStateless";
        builtin = NEO::EBuiltInOps::copyBufferToImage3dHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer16Bytes:
        builtinName = "CopyImage3dToBuffer16Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer16BytesHeapless:
        builtinName = "CopyImage3dToBuffer16BytesStateless";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer2Bytes:
        builtinName = "CopyImage3dToBuffer2Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer2BytesHeapless:
        builtinName = "CopyImage3dToBuffer2BytesStateless";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer3Bytes:
        builtinName = "CopyImage3dToBuffer3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer3BytesHeapless:
        builtinName = "CopyImage3dToBuffer3BytesStateless";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4Bytes:
        builtinName = "CopyImage3dToBuffer4Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer4BytesHeapless:
        builtinName = "CopyImage3dToBuffer4BytesStateless";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer4To3Bytes:
        builtinName = "CopyImage3dToBuffer4To3Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer4To3BytesHeapless:
        builtinName = "CopyImage3dToBuffer4To3BytesStateless";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer6Bytes:
        builtinName = "CopyImage3dToBuffer6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer6BytesHeapless:
        builtinName = "CopyImage3dToBuffer6BytesStateless";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8Bytes:
        builtinName = "CopyImage3dToBuffer8Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer8BytesHeapless:
        builtinName = "CopyImage3dToBuffer8BytesStateless";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferHeapless;
        break;
    case ImageBuiltin::copyImage3dToBuffer8To6Bytes:
        builtinName = "CopyImage3dToBuffer8To6Bytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBuffer8To6BytesHeapless:
        builtinName = "CopyImage3dToBuffer8To6BytesStateless";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferHeapless;
        break;
    case ImageBuiltin::copyImage3dToBufferBytes:
        builtinName = "CopyImage3dToBufferBytes";
        builtin = NEO::EBuiltInOps::copyImage3dToBuffer;
        break;
    case ImageBuiltin::copyImage3dToBufferBytesHeapless:
        builtinName = "CopyImage3dToBufferBytesStateless";
        builtin = NEO::EBuiltInOps::copyImage3dToBufferHeapless;
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
    if (!NEO::debugManager.flags.RebuildPrecompiledKernels.get()) {
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
        ze_module_handle_t moduleHandle;
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
    DEBUG_BREAK_IF(res != ZE_RESULT_SUCCESS);

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
