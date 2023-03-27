/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"

#include <cstdint>

namespace NEO {

BuiltIns::BuiltIns() {
    builtinsLib.reset(new BuiltinsLib());
}

BuiltIns::~BuiltIns() = default;

const SipKernel &BuiltIns::getSipKernel(SipKernelType type, Device &device) {
    uint32_t kernelId = static_cast<uint32_t>(type);
    UNRECOVERABLE_IF(kernelId >= static_cast<uint32_t>(SipKernelType::COUNT));
    auto &sipBuiltIn = this->sipKernels[kernelId];

    auto initializer = [&] {
        std::vector<char> sipBinary;
        std::vector<char> stateSaveAreaHeader;
        auto compilerInterface = device.getCompilerInterface();
        UNRECOVERABLE_IF(compilerInterface == nullptr);

        auto ret = compilerInterface->getSipKernelBinary(device, type, sipBinary, stateSaveAreaHeader);

        UNRECOVERABLE_IF(ret != TranslationOutput::ErrorCode::Success);
        UNRECOVERABLE_IF(sipBinary.size() == 0);

        const auto allocType = AllocationType::KERNEL_ISA_INTERNAL;

        AllocationProperties properties = {device.getRootDeviceIndex(), sipBinary.size(), allocType, device.getDeviceBitfield()};
        properties.flags.use32BitFrontWindow = false;

        auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

        auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();
        auto &productHelper = device.getProductHelper();

        if (sipAllocation) {
            MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *sipAllocation),
                                                             device, sipAllocation, 0, sipBinary.data(),
                                                             sipBinary.size());
        }
        sipBuiltIn.first.reset(new SipKernel(type, sipAllocation, std::move(stateSaveAreaHeader)));
    };
    std::call_once(sipBuiltIn.second, initializer);
    UNRECOVERABLE_IF(sipBuiltIn.first == nullptr);
    return *sipBuiltIn.first;
}

const SipKernel &BuiltIns::getSipKernel(Device &device, OsContext *context) {
    const uint32_t contextId = context->getContextId();
    const SipKernelType type = SipKernelType::DbgBindless;

    auto initializer = [&] {
        std::vector<char> sipBinary;
        std::vector<char> stateSaveAreaHeader;
        auto compilerInterface = device.getCompilerInterface();
        UNRECOVERABLE_IF(compilerInterface == nullptr);

        auto ret = compilerInterface->getSipKernelBinary(device, type, sipBinary, stateSaveAreaHeader);

        UNRECOVERABLE_IF(ret != TranslationOutput::ErrorCode::Success);
        UNRECOVERABLE_IF(sipBinary.size() == 0);

        const auto allocType = AllocationType::KERNEL_ISA_INTERNAL;

        AllocationProperties properties = {device.getRootDeviceIndex(), sipBinary.size(), allocType, device.getDeviceBitfield()};
        properties.flags.use32BitFrontWindow = false;

        auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

        auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();
        auto &productHelper = device.getProductHelper();

        if (sipAllocation) {
            MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *sipAllocation),
                                                             device, sipAllocation, 0, sipBinary.data(),
                                                             sipBinary.size());
        }
        perContextSipKernels.first[contextId] = std::make_unique<SipKernel>(type, sipAllocation, std::move(stateSaveAreaHeader));
    };
    std::call_once(perContextSipKernels.second, initializer);

    return *perContextSipKernels.first[contextId];
}

void BuiltIns::freeSipKernels(MemoryManager *memoryManager) {
    for (auto &sipKernel : sipKernels) {
        if (sipKernel.first.get()) {
            memoryManager->freeGraphicsMemory(sipKernel.first->getSipAllocation());
        }
    }

    for (const auto &ctxToSipKernelMap : perContextSipKernels.first) {
        if (ctxToSipKernelMap.second.get()) {
            memoryManager->freeGraphicsMemory(ctxToSipKernelMap.second->getSipAllocation());
        }
    }
}

} // namespace NEO
