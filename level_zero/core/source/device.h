/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/driver.h"
#include "level_zero/core/source/driver_handle.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

struct _ze_device_handle_t {};
struct DeviceInfo;
namespace NEO {
class Device;
class MemoryManager;
} // namespace NEO

namespace L0 {
struct DriverHandle;
struct BuiltinFunctionsLib;
struct ExecutionEnvironment;
struct MetricContext;

struct Device : _ze_device_handle_t {
    virtual uint32_t getRootDeviceIndex() = 0;
    virtual ze_result_t canAccessPeer(ze_device_handle_t hPeerDevice, ze_bool_t *value) = 0;
    virtual ze_result_t copyCommandList(ze_command_list_handle_t hCommandList,
                                        ze_command_list_handle_t *phCommandList) = 0;

    virtual ze_result_t createCommandList(const ze_command_list_desc_t *desc,
                                          ze_command_list_handle_t *commandList) = 0;

    virtual ze_result_t createCommandListImmediate(const ze_command_queue_desc_t *desc,
                                                   ze_command_list_handle_t *commandList) = 0;

    virtual ze_result_t createCommandQueue(const ze_command_queue_desc_t *desc,
                                           ze_command_queue_handle_t *commandQueue) = 0;

    virtual ze_result_t createEventPool(const ze_event_pool_desc_t *desc,
                                        ze_event_pool_handle_t *phEventPool) = 0;
    virtual ze_result_t createImage(const ze_image_desc_t *desc, ze_image_handle_t *phImage) = 0;

    virtual ze_result_t createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                     ze_module_build_log_handle_t *buildLog) = 0;
    virtual ze_result_t createSampler(const ze_sampler_desc_t *pDesc,
                                      ze_sampler_handle_t *phSampler) = 0;
    virtual ze_result_t evictImage(ze_image_handle_t hImage) = 0;
    virtual ze_result_t evictMemory(void *ptr, size_t size) = 0;
    virtual ze_result_t
    getComputeProperties(ze_device_compute_properties_t *pComputeProperties) = 0;
    virtual ze_result_t getP2PProperties(ze_device_handle_t hPeerDevice,
                                         ze_device_p2p_properties_t *pP2PProperties) = 0;
    virtual ze_result_t getKernelProperties(ze_device_kernel_properties_t *pKernelProperties) = 0;
    virtual ze_result_t getMemoryProperties(uint32_t *pCount, ze_device_memory_properties_t *pMemProperties) = 0;
    virtual ze_result_t getMemoryAccessProperties(ze_device_memory_access_properties_t *pMemAccessProperties) = 0;
    virtual ze_result_t getProperties(ze_device_properties_t *pDeviceProperties) = 0;
    virtual ze_result_t getSubDevices(uint32_t *pCount, ze_device_handle_t *phSubdevices) = 0;
    virtual ze_result_t makeImageResident(ze_image_handle_t hImage) = 0;
    virtual ze_result_t makeMemoryResident(void *ptr, size_t size) = 0;
    virtual ze_result_t setIntermediateCacheConfig(ze_cache_config_t cacheConfig) = 0;
    virtual ze_result_t setLastLevelCacheConfig(ze_cache_config_t cacheConfig) = 0;
    virtual ze_result_t getCacheProperties(ze_device_cache_properties_t *pCacheProperties) = 0;
    virtual ze_result_t imageGetProperties(const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties) = 0;
    virtual ze_result_t getDeviceImageProperties(ze_device_image_properties_t *pDeviceImageProperties) = 0;

    virtual ze_result_t systemBarrier() = 0;

    virtual ze_result_t registerCLMemory(cl_context context, cl_mem mem, void **ptr) = 0;
    virtual ze_result_t registerCLProgram(cl_context context, cl_program program,
                                          ze_module_handle_t *phModule) = 0;
    virtual ze_result_t registerCLCommandQueue(cl_context context, cl_command_queue commandQueue,
                                               ze_command_queue_handle_t *phCommandQueue) = 0;
    virtual ~Device() = default;

    virtual void *getExecEnvironment() = 0;
    virtual BuiltinFunctionsLib *getBuiltinFunctionsLib() = 0;
    virtual uint32_t getMOCS(bool l3enabled, bool l1enabled) = 0;
    virtual uint32_t getMaxNumHwThreads() const = 0;

    virtual NEO::HwHelper &getHwHelper() = 0;
    virtual bool isMultiDeviceCapable() const = 0;
    virtual const NEO::HardwareInfo &getHwInfo() const = 0;
    virtual NEO::OSInterface &getOsInterface() = 0;
    virtual uint32_t getPlatformInfo() const = 0;
    virtual MetricContext &getMetricContext() = 0;

    virtual ze_result_t activateMetricGroups(uint32_t count,
                                             zet_metric_group_handle_t *phMetricGroups) = 0;
    virtual void activateMetricGroups() = 0;

    virtual DriverHandle *getDriverHandle() = 0;
    virtual void setDriverHandle(DriverHandle *driverHandle) = 0;

    static Device *fromHandle(ze_device_handle_t handle) { return static_cast<Device *>(handle); }

    inline ze_device_handle_t toHandle() { return this; }

    static Device *create(DriverHandle *driverHandle, NEO::Device *neoDevice);

    virtual NEO::PreemptionMode getDevicePreemptionMode() const = 0;
    virtual const DeviceInfo &getDeviceInfo() const = 0;
    virtual NEO::Device *getNEODevice() = 0;
};

} // namespace L0
