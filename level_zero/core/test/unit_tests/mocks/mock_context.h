/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Context> : public ::L0::Context {};

using Context = WhiteBox<::L0::Context>;

template <>
struct WhiteBox<::L0::ContextImp> : public ::L0::ContextImp {
    using ::L0::ContextImp::devices;
    using ::L0::ContextImp::numDevices;
};

template <>
struct Mock<Context> : public Context {
    Mock() = default;
    ~Mock() override = default;

    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getStatus, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getDriverHandle, DriverHandle *, nullptr, ());
    ADDMETHOD_NOBASE(allocHostMem, ze_result_t, ZE_RESULT_SUCCESS, (const ze_host_mem_alloc_desc_t *hostDesc, size_t size, size_t alignment, void **ptr));
    ADDMETHOD_NOBASE(allocDeviceMem, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, const ze_device_mem_alloc_desc_t *deviceDesc, size_t size, size_t alignment, void **ptr));
    ADDMETHOD_NOBASE(allocSharedMem, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, const ze_device_mem_alloc_desc_t *deviceDesc, const ze_host_mem_alloc_desc_t *hostDesc, size_t size, size_t alignment, void **ptr));
    ADDMETHOD_NOBASE(makeMemoryResident, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, void *ptr, size_t size));
    ADDMETHOD_NOBASE(evictMemory, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, void *ptr, size_t size));
    ADDMETHOD_NOBASE(makeImageResident, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, ze_image_handle_t hImage));
    ADDMETHOD_NOBASE(evictImage, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, ze_image_handle_t hImage));
    ADDMETHOD_NOBASE(freeMem, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr));
    ADDMETHOD_NOBASE(getMemAllocProperties, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr, ze_memory_allocation_properties_t *pMemAllocProperties, ze_device_handle_t *phDevice));
    ADDMETHOD_NOBASE(getMemAddressRange, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr, void **pBase, size_t *pSize));
    ADDMETHOD_NOBASE(getIpcMemHandle, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr, ze_ipc_mem_handle_t *pIpcHandle));
    ADDMETHOD_NOBASE(closeIpcMemHandle, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr));
    ADDMETHOD_NOBASE(openIpcMemHandle, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, const ze_ipc_mem_handle_t &handle, ze_ipc_memory_flags_t flags, void **ptr));
    ADDMETHOD_NOBASE(createModule, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, const ze_module_desc_t *desc, ze_module_handle_t *phModule, ze_module_build_log_handle_t *phBuildLog));
    ADDMETHOD_NOBASE(createSampler, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, const ze_sampler_desc_t *pDesc, ze_sampler_handle_t *phSampler));
    ADDMETHOD_NOBASE(createCommandQueue, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, const ze_command_queue_desc_t *desc, ze_command_queue_handle_t *commandQueue));
    ADDMETHOD_NOBASE(createCommandList, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList));
    ADDMETHOD_NOBASE(createCommandListImmediate, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, const ze_command_queue_desc_t *desc, ze_command_list_handle_t *commandList));
    ADDMETHOD_NOBASE(activateMetricGroups, ze_result_t, ZE_RESULT_SUCCESS, (zet_device_handle_t hDevice, uint32_t count, zet_metric_group_handle_t *phMetricGroups));
    ADDMETHOD_NOBASE(reserveVirtualMem, ze_result_t, ZE_RESULT_SUCCESS, (const void *pStart, size_t size, void **pptr));
    ADDMETHOD_NOBASE(freeVirtualMem, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr, size_t size));
    ADDMETHOD_NOBASE(queryVirtualMemPageSize, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, size_t size, size_t *pagesize));
    ADDMETHOD_NOBASE(createPhysicalMem, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, ze_physical_mem_desc_t *desc, ze_physical_mem_handle_t *phPhysicalMemory));
    ADDMETHOD_NOBASE(destroyPhysicalMem, ze_result_t, ZE_RESULT_SUCCESS, (ze_physical_mem_handle_t hPhysicalMemory));
    ADDMETHOD_NOBASE(mapVirtualMem, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr, size_t size, ze_physical_mem_handle_t hPhysicalMemory, size_t offset, ze_memory_access_attribute_t access));
    ADDMETHOD_NOBASE(unMapVirtualMem, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr, size_t size));
    ADDMETHOD_NOBASE(setVirtualMemAccessAttribute, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr, size_t size, ze_memory_access_attribute_t access));
    ADDMETHOD_NOBASE(getVirtualMemAccessAttribute, ze_result_t, ZE_RESULT_SUCCESS, (const void *ptr, size_t size, ze_memory_access_attribute_t *access, size_t *outSize));
    ADDMETHOD_NOBASE(openEventPoolIpcHandle, ze_result_t, ZE_RESULT_SUCCESS, (const ze_ipc_event_pool_handle_t &ipcEventPoolHandle, ze_event_pool_handle_t *eventPoolHandle));
    ADDMETHOD_NOBASE(createEventPool, ze_result_t, ZE_RESULT_SUCCESS, (const ze_event_pool_desc_t *desc, uint32_t numDevices, ze_device_handle_t *phDevices, ze_event_pool_handle_t *phEventPool));
    ADDMETHOD_NOBASE(createImage, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hDevice, const ze_image_desc_t *desc, ze_image_handle_t *phImage));
};

struct ContextShareableMock : public L0::ContextImp {
    ContextShareableMock(L0::DriverHandle *driverHandle) : L0::ContextImp(driverHandle) {}
    bool isShareableMemory(const void *pNext, bool exportableMemory, NEO::Device *neoDevice) override {
        return true;
    }
};
} // namespace ult
} // namespace L0
