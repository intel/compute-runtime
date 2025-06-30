/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
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
    UNRECOVERABLE_IF(kernelId >= static_cast<uint32_t>(SipKernelType::count));
    auto &sipBuiltIn = this->sipKernels[kernelId];

    auto initializer = [&] {
        std::vector<char> sipBinary;
        std::vector<char> stateSaveAreaHeader;
        auto compilerInterface = device.getCompilerInterface();
        UNRECOVERABLE_IF(compilerInterface == nullptr);

        auto ret = compilerInterface->getSipKernelBinary(device, type, sipBinary, stateSaveAreaHeader);

        UNRECOVERABLE_IF(ret != TranslationOutput::ErrorCode::success);
        UNRECOVERABLE_IF(sipBinary.size() == 0);

        if (NEO::debugManager.flags.DumpSipHeaderFile.get() != "unk") {
            std::string name = NEO::debugManager.flags.DumpSipHeaderFile.get() + "_header.bin";
            writeDataToFile(name.c_str(), std::string_view(stateSaveAreaHeader.data(), stateSaveAreaHeader.size()));
        }

        const auto allocType = AllocationType::kernelIsaInternal;

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
        sipBuiltIn.first.reset(new SipKernel(type, sipAllocation, std::move(stateSaveAreaHeader), std::move(sipBinary)));

        if (rootDeviceEnvironment.executionEnvironment.getDebuggingMode() == DebuggingMode::offline) {
            sipBuiltIn.first->parseBinaryForContextId();
        }

        if (type == SipKernelType::csr) {
            rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.requiredPreemptionSurfaceSize = sipBuiltIn.first->getStateSaveAreaSize(&device);
        }
    };
    std::call_once(sipBuiltIn.second, initializer);
    UNRECOVERABLE_IF(sipBuiltIn.first == nullptr);
    return *sipBuiltIn.first;
}

const SipKernel &BuiltIns::getSipKernel(Device &device, OsContext *context) {
    const uint32_t contextId = context->getContextId();
    const SipKernelType type = SipKernelType::dbgBindless;

    auto &bindlessSip = this->getSipKernel(type, device);
    auto copySuccess = false;

    auto initializer = [&] {
        UNRECOVERABLE_IF(bindlessSip.getBinary().size() == 0);

        auto binarySize = alignUp(bindlessSip.getBinary().size(), sizeof(uint32_t)) / sizeof(uint32_t);
        auto binary = std::make_unique_for_overwrite<uint32_t[]>(binarySize);
        memcpy_s(binary.get(), binarySize * sizeof(uint32_t), bindlessSip.getBinary().data(), bindlessSip.getBinary().size());

        const auto allocType = AllocationType::kernelIsaInternal;

        AllocationProperties properties = {device.getRootDeviceIndex(), bindlessSip.getBinary().size(), allocType, device.getDeviceBitfield()};
        properties.flags.use32BitFrontWindow = false;

        auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

        if (sipAllocation) {

            for (uint32_t deviceIndex = 0; deviceIndex < context->getDeviceBitfield().size(); deviceIndex++) {
                if (!context->getDeviceBitfield().test(deviceIndex)) {
                    continue;
                }

                if (bindlessSip.getCtxOffset() != 0) {
                    binary[bindlessSip.getCtxOffset()] = static_cast<uint32_t>(context->getOfflineDumpContextId(deviceIndex) & 0xFFFFFFFF);
                    binary[bindlessSip.getPidOffset()] = static_cast<uint32_t>((context->getOfflineDumpContextId(deviceIndex) >> 32) & 0xFFFFFFFF);
                }

                auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();
                auto &productHelper = device.getProductHelper();

                DeviceBitfield copyBitfield{};
                copyBitfield.set(deviceIndex);
                copySuccess = MemoryTransferHelper::transferMemoryToAllocationBanks(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *sipAllocation),
                                                                                    device, sipAllocation, 0, binary.get(), bindlessSip.getBinary().size(), copyBitfield);
                DEBUG_BREAK_IF(!copySuccess);
            }
        }

        std::vector<char> stateSaveAreaHeader;
        stateSaveAreaHeader.assign(bindlessSip.getStateSaveAreaHeader().begin(), bindlessSip.getStateSaveAreaHeader().end());
        perContextSipKernels[contextId].first = std::make_unique<SipKernel>(type, sipAllocation, std::move(stateSaveAreaHeader));
    };
    std::call_once(perContextSipKernels[contextId].second, initializer);

    return *perContextSipKernels[contextId].first;
}

void BuiltIns::freeSipKernels(MemoryManager *memoryManager) {
    for (auto &sipKernel : sipKernels) {
        if (sipKernel.first.get()) {
            memoryManager->freeGraphicsMemory(sipKernel.first->getSipAllocation());
        }
    }

    for (const auto &pair : perContextSipKernels) {
        const auto sipKernel = pair.second.first.get();
        if (sipKernel) {
            memoryManager->freeGraphicsMemory(sipKernel->getSipAllocation());
        }
    }
}

} // namespace NEO
