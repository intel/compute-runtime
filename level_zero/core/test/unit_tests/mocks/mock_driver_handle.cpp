/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

using MockDriverHandle = Mock<L0::ult::DriverHandle>;

Mock<DriverHandle>::Mock() {
    this->devices.push_back(new Mock<Device>);
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

    phDevices[0] = this->devices.front();

    return ZE_RESULT_SUCCESS;
}

ze_result_t Mock<DriverHandle>::allocDeviceMem(ze_device_handle_t hDevice, const ze_device_mem_alloc_desc_t *deviceDesc,
                                               size_t size, size_t alignment, void **ptr) {
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);

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
        this->rootDeviceIndices.push_back(neoDevice->getRootDeviceIndex());
        this->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        auto device = Device::create(this, neoDevice.release(), false, &returnValue);
        this->devices.push_back(device);
    }
    this->rootDeviceIndices.remove_duplicates();
}

Mock<DriverHandle>::~Mock(){};

} // namespace ult
} // namespace L0
