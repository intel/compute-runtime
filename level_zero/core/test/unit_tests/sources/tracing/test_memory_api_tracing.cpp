/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingMemAllocSharedTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Mem.pfnAllocShared =
        [](ze_context_handle_t hContext, const ze_device_mem_alloc_desc_t *deviceDesc, const ze_host_mem_alloc_desc_t *hostDesc, size_t size, size_t alignment, ze_device_handle_t hDevice, void **pptr) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc;
    size_t size = 1024;
    size_t alignment = 4096;
    void *pptr = nullptr;

    prologCbs.Mem.pfnAllocSharedCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnAllocSharedCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemAllocSharedTracing(nullptr, &deviceDesc, &hostDesc, size, alignment, nullptr, &pptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingMemAllocDeviceTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driverDdiTable.coreDdiTable.Mem.pfnAllocDevice =
        [](ze_context_handle_t hContext, const ze_device_mem_alloc_desc_t *deviceDesc, size_t size, size_t alignment, ze_device_handle_t hDevice, void **pptr) -> ze_result_t { return ZE_RESULT_SUCCESS; };

    size_t size = 1024;
    size_t alignment = 4096;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *pptr = nullptr;

    prologCbs.Mem.pfnAllocDeviceCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnAllocDeviceCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemAllocDeviceTracing(nullptr, &deviceDesc, size, alignment, nullptr, &pptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingMemAllocHostTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.Mem.pfnAllocHost =
        [](ze_context_handle_t hContext, const ze_host_mem_alloc_desc_t *hostDesc, size_t size, size_t alignment, void **pptr) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;
    size_t size = 1024;
    size_t alignment = 4096;
    ze_host_mem_alloc_desc_t hostDesc = {};
    void *pptr = nullptr;

    prologCbs.Mem.pfnAllocHostCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnAllocHostCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemAllocHostTracing(nullptr, &hostDesc, size, alignment, &pptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingMemFreeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.Mem.pfnFree =
        [](ze_context_handle_t hContext, void *ptr) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.Mem.pfnFreeCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnFreeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemFreeTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingMemGetAllocPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.Mem.pfnGetAllocProperties =
        [](ze_context_handle_t hContext, const void *ptr, ze_memory_allocation_properties_t *pMemAllocProperties, ze_device_handle_t *phDevice) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.Mem.pfnGetAllocPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnGetAllocPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemGetAllocPropertiesTracing(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingMemGetAddressRangeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.Mem.pfnGetAddressRange =
        [](ze_context_handle_t hContext, const void *ptr, void **pBase, size_t *pSize) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.Mem.pfnGetAddressRangeCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnGetAddressRangeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemGetAddressRangeTracing(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingMemGetIpcHandleTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.Mem.pfnGetIpcHandle =
        [](ze_context_handle_t hContext, const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.Mem.pfnGetIpcHandleCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnGetIpcHandleCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemGetIpcHandleTracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingMemOpenIpcHandleTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.Mem.pfnOpenIpcHandle =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_ipc_mem_handle_t handle, ze_ipc_memory_flags_t flags, void **pptr) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    ze_ipc_mem_handle_t ipchandle = {};

    prologCbs.Mem.pfnOpenIpcHandleCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnOpenIpcHandleCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemOpenIpcHandleTracing(nullptr, nullptr, ipchandle, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingMemCloseIpcHandleTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.Mem.pfnCloseIpcHandle =
        [](ze_context_handle_t hContext, const void *ptr) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.Mem.pfnCloseIpcHandleCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnCloseIpcHandleCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemCloseIpcHandleTracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingPhysicalMemCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.PhysicalMem.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_physical_mem_desc_t *desc, ze_physical_mem_handle_t *phPhysicalMemory) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.PhysicalMem.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.PhysicalMem.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zePhysicalMemCreateTracing(nullptr, nullptr, nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingPhysicalMemDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.PhysicalMem.pfnDestroy =
        [](ze_context_handle_t hContext, ze_physical_mem_handle_t hPhysicalMemory) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.PhysicalMem.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.PhysicalMem.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zePhysicalMemDestroyTracing(nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingVirtualMemFreeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.VirtualMem.pfnFree =
        [](ze_context_handle_t hContext, const void *ptr, size_t size) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.VirtualMem.pfnFreeCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnFreeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemFreeTracing(nullptr, nullptr, 1U);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingVirtualMemGetAccessAttributeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.VirtualMem.pfnGetAccessAttribute =
        [](ze_context_handle_t hContext, const void *ptr, size_t size, ze_memory_access_attribute_t *access, size_t *outSize) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.VirtualMem.pfnGetAccessAttributeCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnGetAccessAttributeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemGetAccessAttributeTracing(nullptr, nullptr, 1U, nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingVirtualMemMapTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.VirtualMem.pfnMap =
        [](ze_context_handle_t hContext, const void *ptr, size_t size, ze_physical_mem_handle_t hPhysicalMemory, size_t offset, ze_memory_access_attribute_t access) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.VirtualMem.pfnMapCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnMapCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemMapTracing(nullptr, nullptr, 1U, nullptr, 1U, ZE_MEMORY_ACCESS_ATTRIBUTE_NONE);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingVirtualMemQueryPageSizeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.VirtualMem.pfnQueryPageSize =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, size_t size, size_t *pagesize) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.VirtualMem.pfnQueryPageSizeCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnQueryPageSizeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemQueryPageSizeTracing(nullptr, nullptr, 1U, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingVirtualMemReserveTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.VirtualMem.pfnReserve =
        [](ze_context_handle_t hContext, const void *pStart, size_t size, void **pptr) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.VirtualMem.pfnReserveCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnReserveCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemReserveTracing(nullptr, nullptr, 1U, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingVirtualMemSetAccessAttributeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.VirtualMem.pfnSetAccessAttribute =
        [](ze_context_handle_t hContext, const void *ptr, size_t size, ze_memory_access_attribute_t access) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.VirtualMem.pfnSetAccessAttributeCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnSetAccessAttributeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemSetAccessAttributeTracing(nullptr, nullptr, 1U, ZE_MEMORY_ACCESS_ATTRIBUTE_NONE);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingVirtualMemUnmapTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driverDdiTable.coreDdiTable.VirtualMem.pfnUnmap =
        [](ze_context_handle_t hContext, const void *ptr, size_t size) -> ze_result_t { return ZE_RESULT_SUCCESS; };
    ze_result_t result = ZE_RESULT_SUCCESS;

    prologCbs.VirtualMem.pfnUnmapCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnUnmapCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemUnmapTracing(nullptr, nullptr, 1U);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

} // namespace ult
} // namespace L0
