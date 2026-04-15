/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/api_specific_config.h"
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

void BuiltInKernelLibImpl::initBuiltinKernel(BufferBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) {
    auto cacheIndex = toBufferCacheIndex(func, mode);
    UNRECOVERABLE_IF(cacheIndex >= maxBufferCacheSize);

    builtins[cacheIndex] = loadBuiltIn(getBaseKernel(func), mode, getKernelName(func));
}

void BuiltInKernelLibImpl::initBuiltinImageKernel(ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) {
    auto cacheIndex = toImageCacheIndex(func, mode);
    UNRECOVERABLE_IF(cacheIndex >= maxImageCacheSize);

    imageBuiltins[cacheIndex] = loadBuiltIn(getBaseKernel(func), mode, getKernelName(func));
}

BuiltInKernelLibImpl::BuiltInKernelLibImpl(Device *device, NEO::BuiltIns *builtInsLib) : device(device), builtInsLib(builtInsLib) {
    if (initBuiltinsAsyncEnabled(device)) {
        this->initAsyncComplete = false;

        auto initFunc = [this]() {
            const auto &compilerProductHelper = this->device->getCompilerProductHelper();

            auto bindlessEnabled = NEO::ApiSpecificConfig::getBindlessMode(*this->device->getNEODevice());
            auto defaultMode = compilerProductHelper.getDefaultBuiltInAddressingMode(bindlessEnabled);

            this->initBuiltinKernel(BufferBuiltIn::fillBufferImmediate, defaultMode);

            this->initAsync.store(true);
        };

        std::thread initAsyncThread(initFunc);
        initAsyncThread.detach();
    }
}

Kernel *BuiltInKernelLibImpl::getFunction(BufferBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) {
    auto cacheIndex = toBufferCacheIndex(func, mode);

    this->ensureInitCompletion();
    if (builtins[cacheIndex].get() == nullptr) {
        initBuiltinKernel(func, mode);
    }

    return builtins[cacheIndex]->func.get();
}

Kernel *BuiltInKernelLibImpl::getImageFunction(ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) {
    auto cacheIndex = toImageCacheIndex(func, mode);

    this->ensureInitCompletion();
    if (imageBuiltins[cacheIndex].get() == nullptr) {
        initBuiltinImageKernel(func, mode);
    }

    return imageBuiltins[cacheIndex]->func.get();
}

std::unique_ptr<BuiltInKernelLibImpl::BuiltInKernelData> BuiltInKernelLibImpl::loadBuiltIn(NEO::BuiltIn::BaseKernel baseKernel, const NEO::BuiltIn::AddressingMode &mode, const char *kernelName) {
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
        builtinCode = builtInsLib->getBuiltinsLib().getBuiltinCode(baseKernel, mode, builtinCodeType, *device->getNEODevice());
        if (!builtinCode.resource.empty()) {
            break;
        }
    }

    if (builtinCode.resource.empty()) {
        return nullptr;
    }

    [[maybe_unused]] ze_result_t res;

    const auto compositeIndex = NEO::BuiltIn::builderIndex(baseKernel, mode);
    if (this->modules.size() <= compositeIndex) {
        this->modules.resize(compositeIndex + 1u);
    }

    if (this->modules[compositeIndex].get() == nullptr) {
        std::unique_ptr<Module> module;
        ze_module_handle_t moduleHandle = {};
        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = builtinCode.type == BuiltInCodeType::binary ? ZE_MODULE_FORMAT_NATIVE : ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<uint8_t *>(&builtinCode.resource[0]);
        moduleDesc.inputSize = builtinCode.resource.size();
        res = device->createModule(&moduleDesc, &moduleHandle, nullptr, ModuleType::builtin);
        UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);

        module.reset(Module::fromHandle(moduleHandle));
        this->modules[compositeIndex] = std::move(module);
    }

    std::unique_ptr<Kernel> kernel;
    ze_kernel_handle_t kernelHandle;
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName;
    res = this->modules[compositeIndex]->createKernel(&kernelDesc, &kernelHandle);
    UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);

    kernel.reset(Kernel::fromHandle(kernelHandle));
    return std::unique_ptr<BuiltInKernelData>(new BuiltInKernelData{modules[compositeIndex].get(), std::move(kernel)});
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
