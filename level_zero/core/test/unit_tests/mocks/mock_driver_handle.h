/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::DriverHandleImp> : public ::L0::DriverHandleImp {
};

using DriverHandle = WhiteBox<::L0::DriverHandleImp>;

template <>
struct Mock<DriverHandle> : public DriverHandleImp {
    Mock();
    ~Mock() override;

    MOCK_METHOD2(getDevice, ze_result_t(uint32_t *pCount, ze_device_handle_t *phDevices));
    MOCK_METHOD1(getProperties, ze_result_t(ze_driver_properties_t *properties));
    MOCK_METHOD1(getApiVersion, ze_result_t(ze_api_version_t *version));
    MOCK_METHOD1(getIPCProperties, ze_result_t(ze_driver_ipc_properties_t *pIPCProperties));
    MOCK_METHOD3(getMemAllocProperties, ze_result_t(const void *ptr,
                                                    ze_memory_allocation_properties_t *pMemAllocProperties,
                                                    ze_device_handle_t *phDevice));

    MOCK_METHOD0(getMemoryManager, NEO::MemoryManager *());
    MOCK_METHOD1(setMemoryManager, void(NEO::MemoryManager *));
    MOCK_METHOD3(getMemAddressRange, ze_result_t(const void *ptr, void **pBase, size_t *pSize));
    MOCK_METHOD2(getIpcMemHandle, ze_result_t(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle));
    MOCK_METHOD1(closeIpcMemHandle, ze_result_t(const void *ptr));
    MOCK_METHOD4(openIpcMemHandle, ze_result_t(ze_device_handle_t hDevice, ze_ipc_mem_handle_t handle,
                                               ze_ipc_memory_flag_t flags, void **ptr));
    MOCK_METHOD4(createEventPool, ze_result_t(const ze_event_pool_desc_t *desc,
                                              uint32_t numDevices,
                                              ze_device_handle_t *phDevices,
                                              ze_event_pool_handle_t *phEventPool));
    MOCK_METHOD4(allocHostMem, ze_result_t(ze_host_mem_alloc_flag_t flags, size_t size, size_t alignment, void **ptr));
    MOCK_METHOD5(allocDeviceMem, ze_result_t(ze_device_handle_t hDevice, ze_device_mem_alloc_flag_t flags, size_t size, size_t alignment, void **ptr));
    MOCK_METHOD1(freeMem, ze_result_t(const void *ptr));

    MOCK_METHOD0(getSvmAllocsManager, NEO::SVMAllocsManager *());
    uint32_t num_devices = 1;
    Mock<Device> device;

    void setupDevices(std::vector<std::unique_ptr<NEO::Device>> devices);

    ze_result_t doFreeMem(const void *ptr);
    ze_result_t doGetDevice(uint32_t *pCount, ze_device_handle_t *phDevices);
    NEO::MemoryManager *doGetMemoryManager();
    NEO::SVMAllocsManager *doGetSvmAllocManager();
    ze_result_t doAllocHostMem(ze_host_mem_alloc_flag_t flags, size_t size, size_t alignment, void **ptr);
    ze_result_t doAllocDeviceMem(ze_device_handle_t hDevice, ze_device_mem_alloc_flag_t flags, size_t size, size_t alignment, void **ptr);
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
