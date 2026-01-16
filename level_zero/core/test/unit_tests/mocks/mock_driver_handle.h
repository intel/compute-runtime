/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace NEO {
class Device;
class GraphicsAllocation;
class SVMAllocsManager;
} // namespace NEO

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::DriverHandle> : public ::L0::DriverHandle {
    using ::L0::DriverHandle::devices;
    using ::L0::DriverHandle::enableProgramDebugging;
    using ::L0::DriverHandle::ipcSocketServer;
    using ::L0::DriverHandle::stagingBufferManager;
    using ::L0::DriverHandle::svmAllocsManager;
};

using DriverHandle = WhiteBox<::L0::DriverHandle>;
template <>
struct Mock<DriverHandle> : public DriverHandle {
    Mock();
    ~Mock() override;

    ADDMETHOD_NOBASE(getProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_driver_properties_t * properties))
    ADDMETHOD_NOBASE(getApiVersion, ze_result_t, ZE_RESULT_SUCCESS, (ze_api_version_t * version))

    ze_result_t getIPCPropertiesResult = ZE_RESULT_SUCCESS;
    uint32_t getIPCPropertiesCalled = 0u;
    bool callRealGetIPCProperties = false;
    ze_result_t getIPCProperties(ze_driver_ipc_properties_t *pIPCProperties) override {
        getIPCPropertiesCalled++;
        if (callRealGetIPCProperties && getIPCPropertiesResult == ZE_RESULT_SUCCESS) {
            return L0::DriverHandle::getIPCProperties(pIPCProperties);
        }
        return getIPCPropertiesResult;
    }

    ADDMETHOD_NOBASE(importExternalPointer, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (void *ptr, size_t size))
    ADDMETHOD_NOBASE(releaseImportedPointer, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (void *ptr))
    ADDMETHOD_NOBASE(getHostPointerBaseAddress, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (void *ptr, void **baseAddress))
    ADDMETHOD_NOBASE(findHostPointerAllocation, NEO::GraphicsAllocation *, nullptr, (void *ptr, size_t size, uint32_t rootDeviceIndex))
    ADDMETHOD_NOBASE(importFdHandle, void *, nullptr, (NEO::Device * neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAlloc, NEO::SvmAllocationData &mappedPeerAllocData))
    ADDMETHOD_CONST_NOBASE(getEventMaxPacketCount, uint32_t, 8, (uint32_t, ze_device_handle_t *))
    ADDMETHOD_CONST_NOBASE(getEventMaxKernelCount, uint32_t, 3, (uint32_t, ze_device_handle_t *))

    void setupDevices(std::vector<std::unique_ptr<NEO::Device>> devices);

    ze_result_t freeMem(const void *ptr);
    ze_result_t getDevice(uint32_t *pCount,
                          ze_device_handle_t *phDevices) override;
    NEO::MemoryManager *getMemoryManager() override;
    NEO::SVMAllocsManager *getSvmAllocManager();
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment,
                               void **ptr);

    NEO::GraphicsAllocation *getDriverSystemMemoryAllocation(void *ptr,
                                                             size_t size,
                                                             uint32_t rootDeviceIndex,
                                                             uintptr_t *gpuAddress) override;
};
} // namespace ult
} // namespace L0
