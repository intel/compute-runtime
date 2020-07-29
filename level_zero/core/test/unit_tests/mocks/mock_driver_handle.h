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

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::DriverHandle> : public ::L0::DriverHandleImp {
    using ::L0::DriverHandleImp::enableProgramDebugging;
};

using DriverHandle = WhiteBox<::L0::DriverHandle>;

template <>
struct Mock<DriverHandle> : public DriverHandleImp {
    Mock();
    ~Mock() override;

    MOCK_METHOD(ze_result_t,
                getDevice,
                (uint32_t * pCount,
                 ze_device_handle_t *phDevices),
                (override));
    MOCK_METHOD(ze_result_t,
                getProperties,
                (ze_driver_properties_t * properties),
                (override));
    MOCK_METHOD(ze_result_t,
                getApiVersion,
                (ze_api_version_t * version),
                (override));
    MOCK_METHOD(ze_result_t,
                getIPCProperties,
                (ze_driver_ipc_properties_t * pIPCProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                getMemAllocProperties,
                (const void *ptr,
                 ze_memory_allocation_properties_t *pMemAllocProperties,
                 ze_device_handle_t *phDevice),
                (override));
    MOCK_METHOD(ze_result_t,
                createContext,
                (const ze_context_desc_t *desc,
                 ze_context_handle_t *phContext),
                (override));
    MOCK_METHOD(ze_result_t,
                getExtensionProperties,
                (uint32_t * pCount,
                 ze_driver_extension_properties_t *pExtensionProperties),
                (override));
    MOCK_METHOD(NEO::MemoryManager *,
                getMemoryManager,
                (),
                (override));
    MOCK_METHOD(void,
                setMemoryManager,
                (NEO::MemoryManager *),
                (override));
    MOCK_METHOD(ze_result_t,
                getMemAddressRange,
                (const void *ptr,
                 void **pBase,
                 size_t *pSize),
                (override));
    MOCK_METHOD(ze_result_t,
                getIpcMemHandle,
                (const void *ptr,
                 ze_ipc_mem_handle_t *pIpcHandle),
                (override));
    MOCK_METHOD(ze_result_t,
                closeIpcMemHandle,
                (const void *ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                openIpcMemHandle,
                (ze_device_handle_t hDevice,
                 ze_ipc_mem_handle_t handle,
                 ze_ipc_memory_flag_t flags,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                createEventPool,
                (const ze_event_pool_desc_t *desc,
                 uint32_t numDevices,
                 ze_device_handle_t *phDevices,
                 ze_event_pool_handle_t *phEventPool),
                (override));
    MOCK_METHOD(ze_result_t,
                allocHostMem,
                (ze_host_mem_alloc_flags_t flags,
                 size_t size,
                 size_t alignment,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                allocDeviceMem,
                (ze_device_handle_t hDevice,
                 ze_device_mem_alloc_flags_t flags,
                 size_t size,
                 size_t alignment,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                allocSharedMem,
                (ze_device_handle_t hDevice,
                 ze_device_mem_alloc_flags_t deviceFlags,
                 ze_host_mem_alloc_flags_t hostFlags,
                 size_t size,
                 size_t alignment,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                freeMem,
                (const void *ptr),
                (override));
    MOCK_METHOD(NEO::SVMAllocsManager *,
                getSvmAllocsManager,
                (),
                (override));
    uint32_t num_devices = 1;
    Mock<Device> device;

    void setupDevices(std::vector<std::unique_ptr<NEO::Device>> devices);

    ze_result_t doFreeMem(const void *ptr);
    ze_result_t doGetDevice(uint32_t *pCount,
                            ze_device_handle_t *phDevices);
    NEO::MemoryManager *doGetMemoryManager();
    NEO::SVMAllocsManager *doGetSvmAllocManager();
    ze_result_t doAllocHostMem(ze_host_mem_alloc_flags_t flags,
                               size_t size,
                               size_t alignment,
                               void **ptr);
    ze_result_t doAllocDeviceMem(ze_device_handle_t hDevice,
                                 ze_device_mem_alloc_flags_t flags,
                                 size_t size,
                                 size_t alignment,
                                 void **ptr);
};

} // namespace ult
} // namespace L0
