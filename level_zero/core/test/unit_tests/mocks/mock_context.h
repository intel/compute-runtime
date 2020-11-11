/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include "gmock/gmock.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Context> : public ::L0::Context {};

using Context = WhiteBox<::L0::Context>;

template <>
struct Mock<Context> : public Context {
    Mock() = default;
    ~Mock() override = default;

    MOCK_METHOD(ze_result_t,
                destroy,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                getStatus,
                (),
                (override));
    MOCK_METHOD(DriverHandle *,
                getDriverHandle,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                allocHostMem,
                (const ze_host_mem_alloc_desc_t *hostDesc,
                 size_t size,
                 size_t alignment,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                allocDeviceMem,
                (ze_device_handle_t hDevice,
                 const ze_device_mem_alloc_desc_t *deviceDesc,
                 size_t size,
                 size_t alignment,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                allocSharedMem,
                (ze_device_handle_t hDevice,
                 const ze_device_mem_alloc_desc_t *deviceDesc,
                 const ze_host_mem_alloc_desc_t *hostDesc,
                 size_t size,
                 size_t alignment,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                makeMemoryResident,
                (ze_device_handle_t hDevice,
                 void *ptr,
                 size_t size),
                (override));
    MOCK_METHOD(ze_result_t,
                evictMemory,
                (ze_device_handle_t hDevice,
                 void *ptr,
                 size_t size),
                (override));
    MOCK_METHOD(ze_result_t,
                makeImageResident,
                (ze_device_handle_t hDevice,
                 ze_image_handle_t hImage),
                (override));
    MOCK_METHOD(ze_result_t,
                evictImage,
                (ze_device_handle_t hDevice,
                 ze_image_handle_t hImage),
                (override));
    MOCK_METHOD(ze_result_t,
                freeMem,
                (const void *ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                getMemAllocProperties,
                (const void *ptr,
                 ze_memory_allocation_properties_t *pMemAllocProperties,
                 ze_device_handle_t *phDevice),
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
                 ze_ipc_memory_flags_t flags,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                createModule,
                (ze_device_handle_t hDevice,
                 const ze_module_desc_t *desc,
                 ze_module_handle_t *phModule,
                 ze_module_build_log_handle_t *phBuildLog),
                (override));
    MOCK_METHOD(ze_result_t,
                createSampler,
                (ze_device_handle_t hDevice,
                 const ze_sampler_desc_t *pDesc,
                 ze_sampler_handle_t *phSampler),
                (override));
    MOCK_METHOD(ze_result_t,
                createCommandQueue,
                (ze_device_handle_t hDevice,
                 const ze_command_queue_desc_t *desc,
                 ze_command_queue_handle_t *commandQueue),
                (override));
    MOCK_METHOD(ze_result_t,
                createCommandList,
                (ze_device_handle_t hDevice,
                 const ze_command_list_desc_t *desc,
                 ze_command_list_handle_t *commandList),
                (override));
    MOCK_METHOD(ze_result_t,
                createCommandListImmediate,
                (ze_device_handle_t hDevice,
                 const ze_command_queue_desc_t *desc,
                 ze_command_list_handle_t *commandList),
                (override));
    MOCK_METHOD(ze_result_t,
                activateMetricGroups,
                (zet_device_handle_t hDevice,
                 uint32_t count,
                 zet_metric_group_handle_t *phMetricGroups),
                (override));
    MOCK_METHOD(ze_result_t,
                reserveVirtualMem,
                (const void *pStart,
                 size_t size,
                 void **pptr),
                (override));
    MOCK_METHOD(ze_result_t,
                freeVirtualMem,
                (const void *ptr,
                 size_t size),
                (override));
    MOCK_METHOD(ze_result_t,
                queryVirtualMemPageSize,
                (ze_device_handle_t hDevice,
                 size_t size,
                 size_t *pagesize),
                (override));
    MOCK_METHOD(ze_result_t,
                createPhysicalMem,
                (ze_device_handle_t hDevice,
                 ze_physical_mem_desc_t *desc,
                 ze_physical_mem_handle_t *phPhysicalMemory),
                (override));
    MOCK_METHOD(ze_result_t,
                destroyPhysicalMem,
                (ze_physical_mem_handle_t hPhysicalMemory),
                (override));
    MOCK_METHOD(ze_result_t,
                mapVirtualMem,
                (const void *ptr,
                 size_t size,
                 ze_physical_mem_handle_t hPhysicalMemory,
                 size_t offset,
                 ze_memory_access_attribute_t access),
                (override));
    MOCK_METHOD(ze_result_t,
                unMapVirtualMem,
                (const void *ptr,
                 size_t size),
                (override));
    MOCK_METHOD(ze_result_t,
                setVirtualMemAccessAttribute,
                (const void *ptr,
                 size_t size,
                 ze_memory_access_attribute_t access),
                (override));
    MOCK_METHOD(ze_result_t,
                getVirtualMemAccessAttribute,
                (const void *ptr,
                 size_t size,
                 ze_memory_access_attribute_t *access,
                 size_t *outSize),
                (override));
    MOCK_METHOD(ze_result_t,
                openEventPoolIpcHandle,
                (ze_ipc_event_pool_handle_t hIpc,
                 ze_event_pool_handle_t *phEventPool),
                (override));
    MOCK_METHOD(ze_result_t,
                createEventPool,
                (const ze_event_pool_desc_t *desc,
                 uint32_t numDevices,
                 ze_device_handle_t *phDevices,
                 ze_event_pool_handle_t *phEventPool),
                (override));
    MOCK_METHOD(ze_result_t,
                createImage,
                (ze_device_handle_t hDevice,
                 const ze_image_desc_t *desc,
                 ze_image_handle_t *phImage),
                (override));
};

} // namespace ult
} // namespace L0
