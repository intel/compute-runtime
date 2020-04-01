/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_driver_handle.h"

namespace L0 {
namespace ult {

using MockDriverHandle = Mock<L0::ult::DriverHandle>;
using namespace testing;
using ::testing::Invoke;
using ::testing::Return;

Mock<DriverHandle>::Mock() {
    EXPECT_CALL(*this, getDevice)
        .WillRepeatedly(testing::Invoke(this, &MockDriverHandle::doGetDevice));
    EXPECT_CALL(*this, getMemoryManager)
        .WillRepeatedly(Invoke(this, &MockDriverHandle::doGetMemoryManager));
    EXPECT_CALL(*this, getSvmAllocsManager)
        .WillRepeatedly(Invoke(this, &MockDriverHandle::doGetSvmAllocManager));
    EXPECT_CALL(*this, allocHostMem)
        .WillRepeatedly(Invoke(this, &MockDriverHandle::doAllocHostMem));
    EXPECT_CALL(*this, allocDeviceMem)
        .WillRepeatedly(Invoke(this, &MockDriverHandle::doAllocDeviceMem));
    EXPECT_CALL(*this, freeMem)
        .WillRepeatedly(Invoke(this, &MockDriverHandle::doFreeMem));
};

NEO::MemoryManager *Mock<DriverHandle>::doGetMemoryManager() {
    return memoryManager;
}

NEO::SVMAllocsManager *Mock<DriverHandle>::doGetSvmAllocManager() {
    return svmAllocsManager;
}

ze_result_t Mock<DriverHandle>::doGetDevice(uint32_t *pCount, ze_device_handle_t *phDevices) {
    if (*pCount == 0) { // User wants to know number of devices
        *pCount = this->num_devices;
        return ZE_RESULT_SUCCESS;
    }

    if (phDevices == nullptr) // User is expected to allocate space
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    phDevices[0] = &this->device;

    return ZE_RESULT_SUCCESS;
}

ze_result_t Mock<DriverHandle>::doAllocHostMem(ze_host_mem_alloc_flag_t flags, size_t size, size_t alignment,
                                               void **ptr) {
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY);

    auto allocation = svmAllocsManager->createUnifiedMemoryAllocation(0u, size, unifiedMemoryProperties);

    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    *ptr = allocation;

    return ZE_RESULT_SUCCESS;
}

ze_result_t Mock<DriverHandle>::doAllocDeviceMem(ze_device_handle_t hDevice, ze_device_mem_alloc_flag_t flags, size_t size, size_t alignment, void **ptr) {
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY);

    auto allocation = svmAllocsManager->createUnifiedMemoryAllocation(0u, size, unifiedMemoryProperties);

    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    *ptr = allocation;

    return ZE_RESULT_SUCCESS;
}

ze_result_t Mock<DriverHandle>::doFreeMem(const void *ptr) {
    auto allocation = svmAllocsManager->getSVMAllocs()->get(ptr);
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
        auto device = Device::create(this, neoDevice.release());
        this->devices.push_back(device);
    }
}

Mock<DriverHandle>::~Mock(){};

} // namespace ult
} // namespace L0
