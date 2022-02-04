/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "compiler_options.h"

#include <cstdint>
#include <sstream>

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
        auto compilerInteface = device.getCompilerInterface();
        UNRECOVERABLE_IF(compilerInteface == nullptr);

        auto ret = compilerInteface->getSipKernelBinary(device, type, sipBinary, stateSaveAreaHeader);

        UNRECOVERABLE_IF(ret != TranslationOutput::ErrorCode::Success);
        UNRECOVERABLE_IF(sipBinary.size() == 0);

        const auto allocType = AllocationType::KERNEL_ISA_INTERNAL;

        AllocationProperties properties = {device.getRootDeviceIndex(), sipBinary.size(), allocType, device.getDeviceBitfield()};
        properties.flags.use32BitFrontWindow = false;

        auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

        auto &hwInfo = device.getHardwareInfo();
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

        if (sipAllocation) {
            MemoryTransferHelper::transferMemoryToAllocation(hwHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *sipAllocation),
                                                             device, sipAllocation, 0, sipBinary.data(),
                                                             sipBinary.size());
        }
        sipBuiltIn.first.reset(new SipKernel(type, sipAllocation, std::move(stateSaveAreaHeader)));
    };
    std::call_once(sipBuiltIn.second, initializer);
    UNRECOVERABLE_IF(sipBuiltIn.first == nullptr);
    return *sipBuiltIn.first;
}

void BuiltIns::freeSipKernels(MemoryManager *memoryManager) {
    for (auto &sipKernel : sipKernels) {
        if (sipKernel.first.get()) {
            memoryManager->freeGraphicsMemory(sipKernel.first->getSipAllocation());
        }
    }
}

} // namespace NEO
