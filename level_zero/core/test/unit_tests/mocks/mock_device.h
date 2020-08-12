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

#include "gmock/gmock.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Device> : public ::L0::Device {};

using Device = WhiteBox<::L0::Device>;

template <>
struct Mock<Device> : public Device {
    Mock();
    ~Mock() override;

    MOCK_METHOD(uint32_t,
                getRootDeviceIndex,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                canAccessPeer,
                (ze_device_handle_t hPeerDevice,
                 ze_bool_t *value),
                (override));
    MOCK_METHOD(ze_result_t,
                createCommandList,
                (const ze_command_list_desc_t *desc,
                 ze_command_list_handle_t *commandList),
                (override));

    MOCK_METHOD(ze_result_t,
                createCommandListImmediate,
                (const ze_command_queue_desc_t *desc,
                 ze_command_list_handle_t *commandList),
                (override));

    MOCK_METHOD(ze_result_t,
                createCommandQueue,
                (const ze_command_queue_desc_t *desc,
                 ze_command_queue_handle_t *commandQueue),
                (override));
    MOCK_METHOD(ze_result_t,
                createImage,
                (const ze_image_desc_t *desc,
                 ze_image_handle_t *phImage),
                (override));

    MOCK_METHOD(ze_result_t,
                createModule,
                (const ze_module_desc_t *desc,
                 ze_module_handle_t *module,
                 ze_module_build_log_handle_t *buildLog),
                (override));
    MOCK_METHOD(ze_result_t,
                createSampler,
                (const ze_sampler_desc_t *pDesc,
                 ze_sampler_handle_t *phSampler),
                (override));
    MOCK_METHOD(ze_result_t,
                getComputeProperties,
                (ze_device_compute_properties_t * pComputeProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                getP2PProperties,
                (ze_device_handle_t hPeerDevice,
                 ze_device_p2p_properties_t *pP2PProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                getKernelProperties,
                (ze_device_module_properties_t * pKernelProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                getMemoryProperties,
                (uint32_t * pCount,
                 ze_device_memory_properties_t *pMemProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                getMemoryAccessProperties,
                (ze_device_memory_access_properties_t * pMemAccessProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                getProperties,
                (ze_device_properties_t * pDeviceProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                getSubDevices,
                (uint32_t * pCount,
                 ze_device_handle_t *phSubdevices),
                (override));
    MOCK_METHOD(ze_result_t,
                setIntermediateCacheConfig,
                (ze_cache_config_flags_t CacheConfig),
                (override));
    MOCK_METHOD(ze_result_t,
                setLastLevelCacheConfig,
                (ze_cache_config_flags_t CacheConfig),
                (override));
    MOCK_METHOD(ze_result_t,
                getCacheProperties,
                (uint32_t * pCount,
                 ze_device_cache_properties_t *pCacheProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                imageGetProperties,
                (const ze_image_desc_t *desc,
                 ze_image_properties_t *pImageProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                getCommandQueueGroupProperties,
                (uint32_t * pCount,
                 ze_command_queue_group_properties_t *pCommandQueueGroupProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                getDeviceImageProperties,
                (ze_device_image_properties_t * pDeviceImageProperties),
                (override));
    MOCK_METHOD(ze_result_t,
                systemBarrier,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                registerCLMemory,
                (cl_context context,
                 cl_mem mem,
                 void **ptr),
                (override));
    MOCK_METHOD(ze_result_t,
                registerCLProgram,
                (cl_context context,
                 cl_program program,
                 ze_module_handle_t *phModule),
                (override));
    MOCK_METHOD(ze_result_t,
                registerCLCommandQueue,
                (cl_context context,
                 cl_command_queue commandQueue,
                 ze_command_queue_handle_t *phCommandQueue),
                (override));
    // Runtime internal methods
    MOCK_METHOD(void *,
                getExecEnvironment,
                (),
                (override));
    MOCK_METHOD(NEO::HwHelper &,
                getHwHelper,
                (),
                (override));
    MOCK_METHOD(bool,
                isMultiDeviceCapable,
                (),
                (const, override));
    MOCK_METHOD(BuiltinFunctionsLib *,
                getBuiltinFunctionsLib,
                (),
                (override));
    MOCK_METHOD(uint32_t,
                getMOCS,
                (bool l3enabled,
                 bool l1enabled),
                (override));
    MOCK_METHOD(uint32_t,
                getMaxNumHwThreads,
                (),
                (const, override));

    MOCK_METHOD(ze_result_t,
                activateMetricGroups,
                (uint32_t count,
                 zet_metric_group_handle_t *phMetricGroups),
                (override));
    MOCK_METHOD(NEO::OSInterface &,
                getOsInterface,
                (),
                (override));
    MOCK_METHOD(uint32_t,
                getPlatformInfo,
                (),
                (const, override));
    MOCK_METHOD(MetricContext &,
                getMetricContext,
                (),
                (override));
    MOCK_METHOD(const NEO::HardwareInfo &,
                getHwInfo,
                (),
                (const, override));
    MOCK_METHOD(L0::DriverHandle *,
                getDriverHandle,
                (),
                (override));
    MOCK_METHOD(void,
                setDriverHandle,
                (L0::DriverHandle *),
                (override));

    MOCK_METHOD(NEO::PreemptionMode,
                getDevicePreemptionMode,
                (),
                (const, override));
    MOCK_METHOD(const NEO::DeviceInfo &,
                getDeviceInfo,
                (),
                (const, override));
    MOCK_METHOD(NEO::Device *,
                getNEODevice,
                (),
                (override));
    MOCK_METHOD(void,
                activateMetricGroups,
                (),
                (override));
    MOCK_METHOD(NEO::GraphicsAllocation *,
                getDebugSurface,
                (),
                (const, override));
    MOCK_METHOD(NEO::GraphicsAllocation *,
                allocateManagedMemoryFromHostPtr,
                (void *buffer,
                 size_t size,
                 struct L0::CommandList *commandList),
                (override));
    MOCK_METHOD(NEO::GraphicsAllocation *,
                allocateMemoryFromHostPtr,
                (const void *buffer,
                 size_t size),
                (override));
    MOCK_METHOD(void,
                setSysmanHandle,
                (SysmanDevice *),
                (override));
    MOCK_METHOD(SysmanDevice *,
                getSysmanHandle,
                (),
                (override));
    ze_result_t getCsrForOrdinalAndIndex(NEO::CommandStreamReceiver **csr, uint32_t ordinal, uint32_t index) override {
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t mapOrdinalForAvailableEngineGroup(uint32_t *ordinal) override {
        return ZE_RESULT_SUCCESS;
    }
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
