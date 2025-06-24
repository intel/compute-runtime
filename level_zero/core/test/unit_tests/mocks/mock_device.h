/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {
namespace ult {

struct Device : public ::L0::Device {
    using Base = L0::Device;
    using Base::implicitScalingCapable;
    using L0::Device::getNEODevice;
    using L0::Device::neoDevice;
};

struct MockDevice : public Device {
    ~MockDevice() override = default;

    ADDMETHOD_NOBASE(canAccessPeer, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hPeerDevice, ze_bool_t *value));
    ADDMETHOD_NOBASE(createCommandList, ze_result_t, ZE_RESULT_SUCCESS, (const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList));
    ADDMETHOD_NOBASE(createCommandListImmediate, ze_result_t, ZE_RESULT_SUCCESS, (const ze_command_queue_desc_t *desc, ze_command_list_handle_t *commandList));
    ADDMETHOD_NOBASE(createCommandQueue, ze_result_t, ZE_RESULT_SUCCESS, (const ze_command_queue_desc_t *desc, ze_command_queue_handle_t *commandQueue));
    ADDMETHOD_NOBASE(createImage, ze_result_t, ZE_RESULT_SUCCESS, (const ze_image_desc_t *desc, ze_image_handle_t *phImage));
    ADDMETHOD_NOBASE(createModule, ze_result_t, ZE_RESULT_SUCCESS, (const ze_module_desc_t *desc, ze_module_handle_t *module, ze_module_build_log_handle_t *buildLog, ModuleType type));
    ADDMETHOD_NOBASE(createSampler, ze_result_t, ZE_RESULT_SUCCESS, (const ze_sampler_desc_t *pDesc, ze_sampler_handle_t *phSampler));
    ADDMETHOD_NOBASE(getStatus, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getComputeProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_compute_properties_t * pComputeProperties));
    ADDMETHOD_NOBASE(getP2PProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t hPeerDevice, ze_device_p2p_properties_t *pP2PProperties));
    ADDMETHOD_NOBASE(getKernelProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_module_properties_t * pKernelProperties));
    ADDMETHOD_NOBASE(getPciProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_pci_ext_properties_t * pPciProperties));
    ADDMETHOD_NOBASE(getVectorWidthPropertiesExt, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCount, ze_device_vector_width_properties_ext_t *pVectorWidthProperties));
    ADDMETHOD_NOBASE(getMemoryProperties, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCount, ze_device_memory_properties_t *pMemProperties));
    ADDMETHOD_NOBASE(getMemoryAccessProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_memory_access_properties_t * pMemAccessProperties));
    ADDMETHOD_NOBASE(getProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_properties_t * pDeviceProperties));
    ADDMETHOD_NOBASE(getSubDevices, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCount, ze_device_handle_t *phSubdevices));
    ADDMETHOD_NOBASE(getCacheProperties, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCount, ze_device_cache_properties_t *pCacheProperties));
    ADDMETHOD_NOBASE(reserveCache, ze_result_t, ZE_RESULT_SUCCESS, (size_t cacheLevel, size_t cacheReservationSize));
    ADDMETHOD_NOBASE(setCacheAdvice, ze_result_t, ZE_RESULT_SUCCESS, (void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion));
    ADDMETHOD_NOBASE(imageGetProperties, ze_result_t, ZE_RESULT_SUCCESS, (const ze_image_desc_t *desc, ze_image_properties_t *pImageProperties));
    ADDMETHOD_NOBASE(getCommandQueueGroupProperties, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCount, ze_command_queue_group_properties_t *pCommandQueueGroupProperties));
    ADDMETHOD_NOBASE(getDeviceImageProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_image_properties_t * pDeviceImageProperties));
    ADDMETHOD_NOBASE(getExternalMemoryProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_external_memory_properties_t * pExternalMemoryProperties));
    ADDMETHOD_NOBASE(getGlobalTimestamps, ze_result_t, ZE_RESULT_SUCCESS, (uint64_t * hostTimestamp, uint64_t *deviceTimestamp));
    ADDMETHOD_NOBASE(systemBarrier, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(synchronize, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getRootDevice, ze_result_t, ZE_RESULT_SUCCESS, (ze_device_handle_t * phRootDevice));
    // Runtime internal methods
    ADDMETHOD_NOBASE_REFRETURN(getGfxCoreHelper, NEO::GfxCoreHelper &, ());
    ADDMETHOD_NOBASE_REFRETURN(getL0GfxCoreHelper, L0GfxCoreHelper &, ());
    ADDMETHOD_NOBASE_REFRETURN(getProductHelper, NEO::ProductHelper &, ());
    ADDMETHOD_NOBASE_REFRETURN(getCompilerProductHelper, NEO::CompilerProductHelper &, ());
    ADDMETHOD_NOBASE(getBuiltinFunctionsLib, BuiltinFunctionsLib *, nullptr, ());
    ADDMETHOD_CONST_NOBASE(getMaxNumHwThreads, uint32_t, 16u, ());
    ADDMETHOD_NOBASE(activateMetricGroupsDeferred, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t count, zet_metric_group_handle_t *phMetricGroups));
    ADDMETHOD_NOBASE_REFRETURN(getOsInterface, NEO::OSInterface *, ());
    ADDMETHOD_CONST_NOBASE(getPlatformInfo, uint32_t, 0u, ());
    ADDMETHOD_NOBASE_REFRETURN(getMetricDeviceContext, MetricDeviceContext &, ());
    ADDMETHOD_CONST_NOBASE_REFRETURN(getHwInfo, const NEO::HardwareInfo &, ());
    ADDMETHOD_NOBASE(getDriverHandle, L0::DriverHandle *, nullptr, ());
    ADDMETHOD_NOBASE_VOIDRETURN(setDriverHandle, (L0::DriverHandle *));
    ADDMETHOD_CONST_NOBASE(getDevicePreemptionMode, NEO::PreemptionMode, NEO::PreemptionMode::Initial, ());
    ADDMETHOD_CONST_NOBASE_REFRETURN(getDeviceInfo, const NEO::DeviceInfo &, ());
    ADDMETHOD_NOBASE_VOIDRETURN(activateMetricGroups, ());
    ADDMETHOD_CONST_NOBASE(getDebugSurface, NEO::GraphicsAllocation *, nullptr, ());
    ADDMETHOD_NOBASE(allocateManagedMemoryFromHostPtr, NEO::GraphicsAllocation *, nullptr, (void *buffer, size_t size, struct L0::CommandList *commandList));
    ADDMETHOD_NOBASE(allocateMemoryFromHostPtr, NEO::GraphicsAllocation *, nullptr, (const void *buffer, size_t size, bool hostCopyAllowed));
    ADDMETHOD_NOBASE_VOIDRETURN(setSysmanHandle, (SysmanDevice *));
    ADDMETHOD_NOBASE(getSysmanHandle, SysmanDevice *, nullptr, ());
    ADDMETHOD_NOBASE(getCsrForOrdinalAndIndex, ze_result_t, ZE_RESULT_SUCCESS, (NEO::CommandStreamReceiver * *csr, uint32_t ordinal, uint32_t index, ze_command_queue_priority_t priority, int priorityLevel, bool allocateInterrupt));
    ADDMETHOD_NOBASE(getCsrForLowPriority, ze_result_t, ZE_RESULT_SUCCESS, (NEO::CommandStreamReceiver * *csr, bool copyOnly));
    ADDMETHOD_NOBASE(getDebugProperties, ze_result_t, ZE_RESULT_SUCCESS, (zet_device_debug_properties_t * properties));
    ADDMETHOD_NOBASE(getDebugSession, DebugSession *, nullptr, (const zet_debug_config_t &config));
    ADDMETHOD_NOBASE_VOIDRETURN(removeDebugSession, ());
    ADDMETHOD_NOBASE(obtainReusableAllocation, NEO::GraphicsAllocation *, nullptr, (size_t requiredSize, NEO::AllocationType type))
    ADDMETHOD_NOBASE_VOIDRETURN(storeReusableAllocation, (NEO::GraphicsAllocation & alloc));
    ADDMETHOD_NOBASE(getFabricVertex, ze_result_t, ZE_RESULT_SUCCESS, (ze_fabric_vertex_handle_t * phVertex));
    ADDMETHOD_CONST_NOBASE(getEventMaxPacketCount, uint32_t, 8, ())
    ADDMETHOD_CONST_NOBASE(getEventMaxKernelCount, uint32_t, 3, ())

    DebugSession *createDebugSession(const zet_debug_config_t &config, ze_result_t &result, bool isRootAttach) override {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return nullptr;
    }

    uint32_t getMOCS(bool l3enabled, bool l1enabled) override {
        if (l3enabled && !l1enabled) {
            return 2;
        }
        return 0;
    }
};

struct MockDeviceImp : public L0::DeviceImp {
    using Base = L0::DeviceImp;
    using Base::adjustCommandQueueDesc;
    using Base::debugSession;
    using Base::deviceInOrderCounterAllocator;
    using Base::getNEODevice;
    using Base::hostInOrderCounterAllocator;
    using Base::implicitScalingCapable;
    using Base::inOrderTimestampAllocator;
    using Base::neoDevice;
    using Base::queuePriorityHigh;
    using Base::queuePriorityLow;
    using Base::subDeviceCopyEngineGroups;
    using Base::syncDispatchTokenAllocation;

    MockDeviceImp(NEO::Device *device) {
        device->incRefInternal();
        Base::neoDevice = device;
        Base::allocationsForReuse = std::make_unique<NEO::AllocationsList>();
    }
};

} // namespace ult
} // namespace L0
