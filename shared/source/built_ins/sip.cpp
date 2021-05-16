/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/io_functions.h"

namespace NEO {

const size_t SipKernel::maxDbgSurfaceSize = 0x1800000; // proper value should be taken from compiler when it's ready

SipClassType SipKernel::classType = SipClassType::Init;

SipKernel::~SipKernel() = default;

SipKernel::SipKernel(SipKernelType type, GraphicsAllocation *sipAlloc, std::vector<char> ssah) : stateSaveAreaHeader(ssah), sipAllocation(sipAlloc), type(type) {
}

GraphicsAllocation *SipKernel::getSipAllocation() const {
    return sipAllocation;
}

const std::vector<char> &SipKernel::getStateSaveAreaHeader() const {
    return stateSaveAreaHeader;
}

SipKernelType SipKernel::getSipKernelType(Device &device) {
    bool debuggingEnabled = device.getDebugger() != nullptr || device.isDebuggerActive();
    auto &hwHelper = HwHelper::get(device.getHardwareInfo().platform.eRenderCoreFamily);
    return hwHelper.getSipKernelType(debuggingEnabled);
}

bool SipKernel::initBuiltinsSipKernel(SipKernelType type, Device &device) {
    device.getBuiltIns()->getSipKernel(type, device);
    return true;
}

bool SipKernel::initRawBinaryFromFileKernel(SipKernelType type, Device &device, std::string &fileName) {
    uint32_t sipIndex = static_cast<uint32_t>(type);
    uint32_t rootDeviceIndex = device.getRootDeviceIndex();

    if (device.getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->sipKernels[sipIndex].get() != nullptr) {
        return true;
    }

    FILE *fileDescriptor = nullptr;
    long int size = 0;
    size_t bytesRead = 0u;

    fileDescriptor = IoFunctions::fopenPtr(fileName.c_str(), "rb");
    if (fileDescriptor == NULL) {
        return false;
    }

    IoFunctions::fseekPtr(fileDescriptor, 0, SEEK_END);
    size = IoFunctions::ftellPtr(fileDescriptor);
    IoFunctions::rewindPtr(fileDescriptor);

    void *alignedBuffer = alignedMalloc(size, MemoryConstants::pageSize);

    bytesRead = IoFunctions::freadPtr(alignedBuffer, 1, size, fileDescriptor);
    IoFunctions::fclosePtr(fileDescriptor);
    if (static_cast<long int>(bytesRead) != size || bytesRead == 0u) {
        alignedFree(alignedBuffer);
        return false;
    }

    const auto allocType = GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL;
    AllocationProperties properties = {rootDeviceIndex, bytesRead, allocType, device.getDeviceBitfield()};
    properties.flags.use32BitFrontWindow = false;

    auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    if (sipAllocation == nullptr) {
        alignedFree(alignedBuffer);
        return false;
    }

    auto &hwInfo = device.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    MemoryTransferHelper::transferMemoryToAllocation(hwHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *sipAllocation),
                                                     device, sipAllocation, 0, alignedBuffer,
                                                     bytesRead);

    alignedFree(alignedBuffer);

    std::vector<char> emptyStateSaveAreaHeader;
    device.getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->sipKernels[sipIndex] =
        std::make_unique<SipKernel>(type, sipAllocation, std::move(emptyStateSaveAreaHeader));

    return true;
}

void SipKernel::freeSipKernels(RootDeviceEnvironment *rootDeviceEnvironment, MemoryManager *memoryManager) {
    for (auto &sipKernel : rootDeviceEnvironment->sipKernels) {
        if (sipKernel.get()) {
            memoryManager->freeGraphicsMemory(sipKernel->getSipAllocation());
        }
    }
}

void SipKernel::selectSipClassType(std::string &fileName) {
    const std::string unknown("unk");
    if (fileName.compare(unknown) == 0) {
        SipKernel::classType = SipClassType::Builtins;
    } else {
        SipKernel::classType = SipClassType::RawBinaryFromFile;
    }
}

bool SipKernel::initSipKernelImpl(SipKernelType type, Device &device) {
    std::string fileName = DebugManager.flags.LoadBinarySipFromFile.get();
    SipKernel::selectSipClassType(fileName);

    if (SipKernel::classType == SipClassType::RawBinaryFromFile) {
        return SipKernel::initRawBinaryFromFileKernel(type, device, fileName);
    }
    return SipKernel::initBuiltinsSipKernel(type, device);
}

const SipKernel &SipKernel::getSipKernelImpl(Device &device) {
    auto sipType = SipKernel::getSipKernelType(device);

    if (SipKernel::classType == SipClassType::RawBinaryFromFile) {
        return *device.getRootDeviceEnvironment().sipKernels[static_cast<uint32_t>(sipType)].get();
    }
    return device.getBuiltIns()->getSipKernel(sipType, device);
}

} // namespace NEO
