/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

using MockDriverHandle = Mock<L0::ult::DriverHandle>;

Mock<DriverHandle>::Mock() {
    this->devices.push_back(new MockDevice);
};

NEO::MemoryManager *Mock<DriverHandle>::getMemoryManager() {
    return memoryManager;
}

NEO::SVMAllocsManager *Mock<DriverHandle>::getSvmAllocManager() {
    return svmAllocsManager;
}

ze_result_t Mock<DriverHandle>::getDevice(uint32_t *pCount, ze_device_handle_t *phDevices) {
    if (*pCount == 0) { // User wants to know number of devices
        *pCount = static_cast<uint32_t>(this->devices.size());
        return ZE_RESULT_SUCCESS;
    }

    if (phDevices == nullptr) // User is expected to allocate space
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    auto numDevices = std::min(pCount ? *pCount : 1u, static_cast<uint32_t>(this->devices.size()));
    for (auto i = 0u; i < numDevices; i++) {
        phDevices[i] = this->devices[i];
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t Mock<DriverHandle>::allocDeviceMem(ze_device_handle_t hDevice, const ze_device_mem_alloc_desc_t *deviceDesc,
                                               size_t size, size_t alignment, void **ptr) {
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, alignment, rootDeviceIndices, deviceBitfields);

    auto allocation = svmAllocsManager->createUnifiedMemoryAllocation(size, unifiedMemoryProperties);

    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    *ptr = allocation;

    return ZE_RESULT_SUCCESS;
}

ze_result_t Mock<DriverHandle>::freeMem(const void *ptr) {
    auto allocation = svmAllocsManager->getSVMAlloc(ptr);
    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    svmAllocsManager->freeSVMAlloc(const_cast<void *>(ptr));
    if (svmAllocsManager->getSvmMapOperation(ptr)) {
        svmAllocsManager->removeSvmMapOperation(ptr);
    }
    return ZE_RESULT_SUCCESS;
}

void Mock<DriverHandle>::setupDevices(std::vector<std::unique_ptr<NEO::Device>> neoDevices) {
    this->numDevices = static_cast<uint32_t>(neoDevices.size());
    for (auto &neoDevice : neoDevices) {
        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        this->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        this->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        auto device = Device::create(this, neoDevice.release(), false, &returnValue);
        this->devices.push_back(device);
    }
}

NEO::GraphicsAllocation *Mock<DriverHandle>::getDriverSystemMemoryAllocation(void *ptr,
                                                                             size_t size,
                                                                             uint32_t rootDeviceIndex,
                                                                             uintptr_t *gpuAddress) {
    auto svmData = svmAllocsManager->getSVMAlloc(ptr);
    if (svmData != nullptr) {
        if (gpuAddress != nullptr) {
            *gpuAddress = reinterpret_cast<uintptr_t>(ptr);
        }
        return svmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
    }
    return nullptr;
}
Mock<DriverHandle>::~Mock(){};

} // namespace ult
} // namespace L0
