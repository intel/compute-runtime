/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/device/device.h"
#include "shared/source/device/device_info.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"
#include "level_zero/tools/source/metrics/metric.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Device> : public ::L0::Device {};

using Device = WhiteBox<::L0::Device>;

template <>
struct Mock<Device> : public Device {
    Mock();
    ~Mock() override;

    MOCK_METHOD0(getRootDeviceIndex, uint32_t());
    MOCK_METHOD2(canAccessPeer, ze_result_t(ze_device_handle_t hPeerDevice, ze_bool_t *value));
    MOCK_METHOD2(copyCommandList, ze_result_t(ze_command_list_handle_t hCommandList,
                                              ze_command_list_handle_t *phCommandList));
    MOCK_METHOD2(createCommandList, ze_result_t(const ze_command_list_desc_t *desc,
                                                ze_command_list_handle_t *commandList));

    MOCK_METHOD2(createCommandListImmediate, ze_result_t(const ze_command_queue_desc_t *desc,
                                                         ze_command_list_handle_t *commandList));

    MOCK_METHOD2(createCommandQueue, ze_result_t(const ze_command_queue_desc_t *desc,
                                                 ze_command_queue_handle_t *commandQueue));

    MOCK_METHOD2(createEventPool,
                 ze_result_t(const ze_event_pool_desc_t *desc, ze_event_pool_handle_t *eventPool));
    MOCK_METHOD2(createImage, ze_result_t(const ze_image_desc_t *desc, ze_image_handle_t *phImage));

    MOCK_METHOD3(createModule, ze_result_t(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                           ze_module_build_log_handle_t *buildLog));
    MOCK_METHOD2(createSampler,
                 ze_result_t(const ze_sampler_desc_t *pDesc, ze_sampler_handle_t *phSampler));
    MOCK_METHOD1(evictImage, ze_result_t(ze_image_handle_t hImage));
    MOCK_METHOD2(evictMemory, ze_result_t(void *ptr, size_t size));
    MOCK_METHOD1(getComputeProperties,
                 ze_result_t(ze_device_compute_properties_t *pComputeProperties));
    MOCK_METHOD2(getP2PProperties, ze_result_t(ze_device_handle_t hPeerDevice,
                                               ze_device_p2p_properties_t *pP2PProperties));
    MOCK_METHOD1(getKernelProperties, ze_result_t(ze_device_kernel_properties_t *pKernelProperties));
    MOCK_METHOD2(getMemoryProperties, ze_result_t(uint32_t *pCount, ze_device_memory_properties_t *pMemProperties));
    MOCK_METHOD1(getMemoryAccessProperties, ze_result_t(ze_device_memory_access_properties_t *pMemAccessProperties));
    MOCK_METHOD1(getProperties, ze_result_t(ze_device_properties_t *pDeviceProperties));
    MOCK_METHOD2(getSubDevice, ze_result_t(uint32_t ordinal, ze_device_handle_t *phSubDevice));
    MOCK_METHOD2(getSubDevices, ze_result_t(uint32_t *pCount, ze_device_handle_t *phSubdevices));
    MOCK_METHOD1(makeImageResident, ze_result_t(ze_image_handle_t hImage));
    MOCK_METHOD2(makeMemoryResident, ze_result_t(void *ptr, size_t size));
    MOCK_METHOD1(setIntermediateCacheConfig, ze_result_t(ze_cache_config_t CacheConfig));
    MOCK_METHOD1(setLastLevelCacheConfig, ze_result_t(ze_cache_config_t CacheConfig));
    MOCK_METHOD1(getCacheProperties, ze_result_t(ze_device_cache_properties_t *pCacheProperties));

    MOCK_METHOD2(imageGetProperties,
                 ze_result_t(const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties));
    MOCK_METHOD1(getDeviceImageProperties,
                 ze_result_t(ze_device_image_properties_t *pDeviceImageProperties));
    MOCK_METHOD0(systemBarrier, ze_result_t());
    MOCK_METHOD3(registerCLMemory, ze_result_t(cl_context context, cl_mem mem, void **ptr));
    MOCK_METHOD3(registerCLProgram,
                 ze_result_t(cl_context context, cl_program program, ze_module_handle_t *phModule));
    MOCK_METHOD3(registerCLCommandQueue,
                 ze_result_t(cl_context context, cl_command_queue command_queue,
                             ze_command_queue_handle_t *phCommandQueue));
    // Runtime internal methods
    MOCK_METHOD0(getMemoryManager, NEO::MemoryManager *());
    MOCK_METHOD0(getExecEnvironment, void *());
    MOCK_METHOD0(getHwHelper, NEO::HwHelper &());
    MOCK_CONST_METHOD0(isMultiDeviceCapable, bool());
    MOCK_METHOD0(getBuiltinFunctionsLib, BuiltinFunctionsLib *());
    MOCK_METHOD2(getMOCS, uint32_t(bool l3enabled, bool l1enabled));
    MOCK_CONST_METHOD0(getMaxNumHwThreads, uint32_t());

    MOCK_METHOD2(activateMetricGroups,
                 ze_result_t(uint32_t count, zet_metric_group_handle_t *phMetricGroups));
    MOCK_METHOD0(getOsInterface, NEO::OSInterface &());
    MOCK_CONST_METHOD0(getPlatformInfo, uint32_t());
    MOCK_METHOD0(getMetricContext, MetricContext &());
    MOCK_CONST_METHOD0(getHwInfo, const NEO::HardwareInfo &());
    MOCK_METHOD0(getDriverHandle, L0::DriverHandle *());
    MOCK_METHOD1(setDriverHandle, void(L0::DriverHandle *));

    MOCK_CONST_METHOD0(getDevicePreemptionMode, NEO::PreemptionMode());
    MOCK_CONST_METHOD0(getDeviceInfo, const NEO::DeviceInfo &());
    MOCK_METHOD0(getNEODevice, NEO::Device *());
    MOCK_METHOD0(activateMetricGroups, void());
    MOCK_CONST_METHOD0(getDebugSurface, NEO::GraphicsAllocation *());
};

template <>
struct Mock<L0::DeviceImp> : public L0::DeviceImp {
    using Base = L0::DeviceImp;

    explicit Mock(NEO::Device *device, NEO::ExecutionEnvironment *execEnv) {
        device->incRefInternal();
        Base::execEnvironment = execEnv;
        Base::neoDevice = device;
    }
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
