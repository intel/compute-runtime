/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
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
struct WhiteBox<::L0::DriverHandleImp> : public ::L0::DriverHandleImp {
    using ::L0::DriverHandleImp::devices;
    using ::L0::DriverHandleImp::enableProgramDebugging;
    using ::L0::DriverHandleImp::svmAllocsManager;
};

using DriverHandle = WhiteBox<::L0::DriverHandleImp>;
template <>
struct Mock<DriverHandle> : public DriverHandle {
    Mock();
    ~Mock() override;

    ADDMETHOD_NOBASE(getProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_driver_properties_t * properties))
    ADDMETHOD_NOBASE(getApiVersion, ze_result_t, ZE_RESULT_SUCCESS, (ze_api_version_t * version))
    ADDMETHOD_NOBASE(getIPCProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_driver_ipc_properties_t * pIPCProperties))
    ADDMETHOD_NOBASE(importExternalPointer, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (void *ptr, size_t size))
    ADDMETHOD_NOBASE(releaseImportedPointer, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (void *ptr))
    ADDMETHOD_NOBASE(getHostPointerBaseAddress, ze_result_t, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, (void *ptr, void **baseAddress))
    ADDMETHOD_NOBASE(findHostPointerAllocation, NEO::GraphicsAllocation *, nullptr, (void *ptr, size_t size, uint32_t rootDeviceIndex))
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
