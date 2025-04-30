/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <memory>
#include <string>
#include <vector>

struct _ze_driver_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_DRIVER> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_driver_handle_t>);

namespace NEO {
class Device;
class MemoryManager;
class SVMAllocsManager;
class GraphicsAllocation;
struct SvmAllocationData;
} // namespace NEO

namespace L0 {
struct Device;
struct L0EnvVariables;

struct BaseDriver : _ze_driver_handle_t {
    virtual ~BaseDriver() = default;
    virtual ze_result_t getExtensionFunctionAddress(const char *pFuncName, void **pfunc) = 0;
    static BaseDriver *fromHandle(ze_driver_handle_t handle) { return static_cast<BaseDriver *>(handle); }
};

struct DriverHandle : BaseDriver, NEO::NonCopyableAndNonMovableClass {
    virtual ze_result_t createContext(const ze_context_desc_t *desc,
                                      uint32_t numDevices,
                                      ze_device_handle_t *phDevices,
                                      ze_context_handle_t *phContext) = 0;
    virtual ze_result_t getDevice(uint32_t *pCount, ze_device_handle_t *phDevices) = 0;
    virtual ze_result_t getProperties(ze_driver_properties_t *properties) = 0;
    virtual ze_result_t getApiVersion(ze_api_version_t *version) = 0;
    virtual ze_result_t getIPCProperties(ze_driver_ipc_properties_t *pIPCProperties) = 0;
    virtual ze_result_t getExtensionProperties(uint32_t *pCount,
                                               ze_driver_extension_properties_t *pExtensionProperties) = 0;

    virtual NEO::MemoryManager *getMemoryManager() = 0;
    virtual void setMemoryManager(NEO::MemoryManager *memoryManager) = 0;
    virtual ze_result_t checkMemoryAccessFromDevice(Device *device, const void *ptr) = 0;
    virtual bool findAllocationDataForRange(const void *buffer,
                                            size_t size,
                                            NEO::SvmAllocationData *&allocData) = 0;
    virtual std::vector<NEO::SvmAllocationData *> findAllocationsWithinRange(const void *buffer,
                                                                             size_t size,
                                                                             bool *allocationRangeCovered) = 0;

    virtual NEO::SVMAllocsManager *getSvmAllocsManager() = 0;
    virtual ze_result_t sysmanEventsListen(uint32_t timeout, uint32_t count, zes_device_handle_t *phDevices,
                                           uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) = 0;
    virtual ze_result_t sysmanEventsListenEx(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices,
                                             uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) = 0;
    virtual ze_result_t importExternalPointer(void *ptr, size_t size) = 0;
    virtual ze_result_t releaseImportedPointer(void *ptr) = 0;
    virtual ze_result_t getHostPointerBaseAddress(void *ptr, void **baseAddress) = 0;

    virtual NEO::GraphicsAllocation *findHostPointerAllocation(void *ptr, size_t size, uint32_t rootDeviceIndex) = 0;
    virtual NEO::GraphicsAllocation *getDriverSystemMemoryAllocation(void *ptr,
                                                                     size_t size,
                                                                     uint32_t rootDeviceIndex,
                                                                     uintptr_t *gpuAddress) = 0;
    virtual ze_result_t fabricVertexGetExp(uint32_t *pCount, ze_fabric_vertex_handle_t *phDevices) = 0;
    virtual uint32_t getEventMaxPacketCount(uint32_t numDevices, ze_device_handle_t *deviceHandles) const = 0;
    virtual uint32_t getEventMaxKernelCount(uint32_t numDevices, ze_device_handle_t *deviceHandles) const = 0;

    virtual ze_result_t loadRTASLibrary() = 0;
    virtual ze_result_t createRTASBuilder(const ze_rtas_builder_exp_desc_t *desc, ze_rtas_builder_exp_handle_t *phBuilder) = 0;
    virtual ze_result_t createRTASParallelOperation(ze_rtas_parallel_operation_exp_handle_t *phParallelOperation) = 0;
    virtual ze_result_t formatRTASCompatibilityCheck(ze_rtas_format_exp_t rtasFormatA, ze_rtas_format_exp_t rtasFormatB) = 0;

    virtual int setErrorDescription(const std::string &str) = 0;
    virtual ze_result_t getErrorDescription(const char **ppString) = 0;
    virtual ze_result_t clearErrorDescription() = 0;

    virtual ze_context_handle_t getDefaultContext() const = 0;

    static DriverHandle *fromHandle(ze_driver_handle_t handle) { return static_cast<DriverHandle *>(handle); }
    inline ze_driver_handle_t toHandle() { return this; }

    static DriverHandle *create(std::vector<std::unique_ptr<NEO::Device>> devices, const L0EnvVariables &envVariables, ze_result_t *returnValue);
};

static_assert(NEO::NonCopyableAndNonMovable<DriverHandle>);

} // namespace L0
