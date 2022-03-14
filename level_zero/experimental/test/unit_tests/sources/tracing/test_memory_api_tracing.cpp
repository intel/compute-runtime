/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(zeAPITracingRuntimeTests, WhenCallingMemAllocSharedTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Mem.pfnAllocShared =
        [](ze_context_handle_t hContext, const ze_device_mem_alloc_desc_t *deviceDesc, const ze_host_mem_alloc_desc_t *hostDesc, size_t size, size_t alignment, ze_device_handle_t hDevice, void **pptr) { return ZE_RESULT_SUCCESS; };

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc;
    size_t size = 1024;
    size_t alignment = 4096;
    void *pptr = nullptr;

    prologCbs.Mem.pfnAllocSharedCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnAllocSharedCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemAllocShared_Tracing(nullptr, &deviceDesc, &hostDesc, size, alignment, nullptr, &pptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingMemAllocDeviceTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Mem.pfnAllocDevice =
        [](ze_context_handle_t hContext, const ze_device_mem_alloc_desc_t *deviceDesc, size_t size, size_t alignment, ze_device_handle_t hDevice, void **pptr) { return ZE_RESULT_SUCCESS; };

    size_t size = 1024;
    size_t alignment = 4096;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *pptr = nullptr;

    prologCbs.Mem.pfnAllocDeviceCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnAllocDeviceCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemAllocDevice_Tracing(nullptr, &deviceDesc, size, alignment, nullptr, &pptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingMemAllocHostTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.Mem.pfnAllocHost =
        [](ze_context_handle_t hContext, const ze_host_mem_alloc_desc_t *hostDesc, size_t size, size_t alignment, void **pptr) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;
    size_t size = 1024;
    size_t alignment = 4096;
    ze_host_mem_alloc_desc_t hostDesc = {};
    void *pptr = nullptr;

    prologCbs.Mem.pfnAllocHostCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnAllocHostCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemAllocHost_Tracing(nullptr, &hostDesc, size, alignment, &pptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingMemFreeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.Mem.pfnFree =
        [](ze_context_handle_t hContext, void *ptr) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.Mem.pfnFreeCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnFreeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemFree_Tracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingMemGetAllocPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.Mem.pfnGetAllocProperties =
        [](ze_context_handle_t hContext, const void *ptr, ze_memory_allocation_properties_t *pMemAllocProperties, ze_device_handle_t *phDevice) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.Mem.pfnGetAllocPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnGetAllocPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemGetAllocProperties_Tracing(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingMemGetAddressRangeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.Mem.pfnGetAddressRange =
        [](ze_context_handle_t hContext, const void *ptr, void **pBase, size_t *pSize) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.Mem.pfnGetAddressRangeCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnGetAddressRangeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemGetAddressRange_Tracing(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingMemGetIpcHandleTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.Mem.pfnGetIpcHandle =
        [](ze_context_handle_t hContext, const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.Mem.pfnGetIpcHandleCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnGetIpcHandleCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemGetIpcHandle_Tracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingMemOpenIpcHandleTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.Mem.pfnOpenIpcHandle =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_ipc_mem_handle_t handle, ze_ipc_memory_flags_t flags, void **pptr) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    ze_ipc_mem_handle_t ipchandle = {};

    prologCbs.Mem.pfnOpenIpcHandleCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnOpenIpcHandleCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemOpenIpcHandle_Tracing(nullptr, nullptr, ipchandle, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingMemCloseIpcHandleTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.Mem.pfnCloseIpcHandle =
        [](ze_context_handle_t hContext, const void *ptr) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.Mem.pfnCloseIpcHandleCb = genericPrologCallbackPtr;
    epilogCbs.Mem.pfnCloseIpcHandleCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeMemCloseIpcHandle_Tracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingPhysicalMemCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.PhysicalMem.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_physical_mem_desc_t *desc, ze_physical_mem_handle_t *phPhysicalMemory) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.PhysicalMem.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.PhysicalMem.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zePhysicalMemCreate_Tracing(nullptr, nullptr, nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingPhysicalMemDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.PhysicalMem.pfnDestroy =
        [](ze_context_handle_t hContext, ze_physical_mem_handle_t hPhysicalMemory) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.PhysicalMem.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.PhysicalMem.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zePhysicalMemDestroy_Tracing(nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingVirtualMemFreeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.VirtualMem.pfnFree =
        [](ze_context_handle_t hContext, const void *ptr, size_t size) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.VirtualMem.pfnFreeCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnFreeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemFree_Tracing(nullptr, nullptr, 1U);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingVirtualMemGetAccessAttributeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.VirtualMem.pfnGetAccessAttribute =
        [](ze_context_handle_t hContext, const void *ptr, size_t size, ze_memory_access_attribute_t *access, size_t *outSize) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.VirtualMem.pfnGetAccessAttributeCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnGetAccessAttributeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemGetAccessAttribute_Tracing(nullptr, nullptr, 1U, nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingVirtualMemMapTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.VirtualMem.pfnMap =
        [](ze_context_handle_t hContext, const void *ptr, size_t size, ze_physical_mem_handle_t hPhysicalMemory, size_t offset, ze_memory_access_attribute_t access) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.VirtualMem.pfnMapCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnMapCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemMap_Tracing(nullptr, nullptr, 1U, nullptr, 1U, ZE_MEMORY_ACCESS_ATTRIBUTE_NONE);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingVirtualMemQueryPageSizeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.VirtualMem.pfnQueryPageSize =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, size_t size, size_t *pagesize) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.VirtualMem.pfnQueryPageSizeCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnQueryPageSizeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemQueryPageSize_Tracing(nullptr, nullptr, 1U, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingVirtualMemReserveTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.VirtualMem.pfnReserve =
        [](ze_context_handle_t hContext, const void *pStart, size_t size, void **pptr) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.VirtualMem.pfnReserveCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnReserveCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemReserve_Tracing(nullptr, nullptr, 1U, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingVirtualMemSetAccessAttributeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.VirtualMem.pfnSetAccessAttribute =
        [](ze_context_handle_t hContext, const void *ptr, size_t size, ze_memory_access_attribute_t access) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.VirtualMem.pfnSetAccessAttributeCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnSetAccessAttributeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemSetAccessAttribute_Tracing(nullptr, nullptr, 1U, ZE_MEMORY_ACCESS_ATTRIBUTE_NONE);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingVirtualMemUnmapTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    driver_ddiTable.core_ddiTable.VirtualMem.pfnUnmap =
        [](ze_context_handle_t hContext, const void *ptr, size_t size) { return ZE_RESULT_SUCCESS; };
    ze_result_t result;

    prologCbs.VirtualMem.pfnUnmapCb = genericPrologCallbackPtr;
    epilogCbs.VirtualMem.pfnUnmapCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeVirtualMemUnmap_Tracing(nullptr, nullptr, 1U);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

} // namespace ult
} // namespace L0
