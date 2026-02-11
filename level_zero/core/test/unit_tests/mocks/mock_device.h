/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {
namespace ult {

struct Device : public ::L0::Device {
    using Base = L0::Device;
    using Base::adjustCommandQueueDesc;
    using Base::createRequiredLibModule;
    using Base::debugSession;
    using Base::deviceInOrderCounterAllocator;
    using Base::freeMemoryAllocation;
    using Base::getRequiredLibBinaryOptionalSearchPaths;
    using Base::getRequiredLibDirPath;
    using Base::hostInOrderCounterAllocator;
    using Base::implicitScalingCapable;
    using Base::inOrderTimestampAllocator;
    using Base::queryPeerAccess;
    using Base::requiredLibsOptionalSearchPaths;
    using Base::requiredLibsRegistry;
    using Base::resourcesReleased;
    using Base::subDeviceCopyEngineGroups;
    using Base::syncDispatchTokenAllocation;
    using L0::Device::getNEODevice;
    using L0::Device::neoDevice;
};

struct MockDevice : public Device {
    using BaseClass = Device;

    MockDevice() : Device() {
        resourcesReleased = true; // Prevent releaseResources() from being called
    }
    MockDevice(NEO::Device *device) : Device() {
        device->incRefInternal();
        neoDevice = device;
        allocationsForReuse = std::make_unique<NEO::AllocationsList>();
    }
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
    ADDMETHOD_NOBASE(getCsrForOrdinalAndIndex, ze_result_t, ZE_RESULT_SUCCESS, (NEO::CommandStreamReceiver * *csr, uint32_t ordinal, uint32_t index, ze_command_queue_priority_t priority, std::optional<int> priorityLevel, bool allocateInterrupt));
    ADDMETHOD_NOBASE(getCsrForLowPriority, ze_result_t, ZE_RESULT_SUCCESS, (NEO::CommandStreamReceiver * *csr, bool copyOnly));
    ADDMETHOD_NOBASE(getDebugProperties, ze_result_t, ZE_RESULT_SUCCESS, (zet_device_debug_properties_t * properties));
    ADDMETHOD_NOBASE(getDebugSession, DebugSession *, nullptr, (const zet_debug_config_t &config));
    ADDMETHOD_NOBASE_VOIDRETURN(removeDebugSession, ());
    ADDMETHOD_NOBASE_VOIDRETURN(bcsSplitReleaseResources, ());
    ADDMETHOD_NOBASE(obtainReusableAllocation, NEO::GraphicsAllocation *, nullptr, (size_t requiredSize, NEO::AllocationType type))
    ADDMETHOD_NOBASE_VOIDRETURN(storeReusableAllocation, (NEO::GraphicsAllocation & alloc));
    ADDMETHOD_NOBASE(getFabricVertex, ze_result_t, ZE_RESULT_SUCCESS, (ze_fabric_vertex_handle_t * phVertex));
    ADDMETHOD_CONST_NOBASE(getEventMaxPacketCount, uint32_t, 8, ())
    ADDMETHOD_CONST_NOBASE(getEventMaxKernelCount, uint32_t, 3, ())
    ADDMETHOD_NOBASE(getAggregatedCopyOffloadIncrementValue, uint32_t, 0, ())
    ADDMETHOD_CONST_NOBASE(getBufferFromFile, NEO::BuiltinResourceT, std::vector<char>{'X'}, (const std::string &dirPath, const std::string &fileName))
    ADDMETHOD_NOBASE(doCreateRequiredLibModule, ::L0::Module *, nullptr, (NEO::BuiltinResourceT & reqLibBuff, ::L0::ModuleBuildLog *buildLog, ze_result_t &result))
    ADDMETHOD(getRequiredLibBinaryOptionalSearchPaths, std::span<const std::string_view>, false, {}, (), ())
    ADDMETHOD_CONST_NOBASE(getRequiredLibBinaryDefaultSearchPaths, std::span<const std::string_view>, {}, ())

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

struct MockDeviceImp : public L0::Device {
    using BaseClass = L0::Device;
    using BaseClass::adjustCommandQueueDesc;
    using BaseClass::debugSession;
    using BaseClass::deviceInOrderCounterAllocator;
    using BaseClass::freeMemoryAllocation;
    using BaseClass::getNEODevice;
    using BaseClass::hostInOrderCounterAllocator;
    using BaseClass::implicitScalingCapable;
    using BaseClass::inOrderTimestampAllocator;
    using BaseClass::neoDevice;
    using BaseClass::queryPeerAccess;
    using BaseClass::subDeviceCopyEngineGroups;
    using BaseClass::syncDispatchTokenAllocation;

    MockDeviceImp(NEO::Device *device) {
        device->incRefInternal();
        BaseClass::neoDevice = device;
        BaseClass::allocationsForReuse = std::make_unique<NEO::AllocationsList>();
    }

    ADDMETHOD(createRequiredLibModule, ::L0::Module *, true, nullptr, (const std::string &libName, ModuleBuildLog *buildLog, ze_result_t &result), (libName, buildLog, result));
};

} // namespace ult
} // namespace L0
