/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::DriverHandle> : public ::L0::DriverHandleImp {
    using ::L0::DriverHandleImp::enableProgramDebugging;
};

using DriverHandle = WhiteBox<::L0::DriverHandle>;
#define ADDMETHOD_NOBASE(funcName, retType, defaultReturn, funcParams) \
    retType funcName##Result = defaultReturn;                          \
    uint32_t funcName##Called = 0u;                                    \
    retType funcName funcParams override {                             \
        funcName##Called++;                                            \
        return funcName##Result;                                       \
    }

template <>
struct Mock<DriverHandle> : public DriverHandleImp {
    Mock();
    ~Mock() override;

    ADDMETHOD_NOBASE(getProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_driver_properties_t * properties))
    ADDMETHOD_NOBASE(getApiVersion, ze_result_t, ZE_RESULT_SUCCESS, (ze_api_version_t * version))
    ADDMETHOD_NOBASE(getIPCProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_driver_ipc_properties_t * pIPCProperties))

    uint32_t num_devices = 1;
    Mock<Device> device;

    void setupDevices(std::vector<std::unique_ptr<NEO::Device>> devices);

    ze_result_t freeMem(const void *ptr);
    ze_result_t getDevice(uint32_t *pCount,
                          ze_device_handle_t *phDevices) override;
    NEO::MemoryManager *getMemoryManager() override;
    NEO::SVMAllocsManager *getSvmAllocManager();
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment,
                               void **ptr);

    ze_result_t importExternalPointer(void *ptr, size_t size) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t releaseImportedPointer(void *ptr) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t getHostPointerBaseAddress(void *ptr, void **baseAddress) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    NEO::GraphicsAllocation *findHostPointerAllocation(void *ptr, size_t size, uint32_t rootDeviceIndex) override {
        return nullptr;
    }
    NEO::GraphicsAllocation *getDriverSystemMemoryAllocation(void *ptr,
                                                             size_t size,
                                                             uint32_t rootDeviceIndex,
                                                             uintptr_t *gpuAddress) override {
        auto svmData = svmAllocsManager->getSVMAlloc(ptr);
        if (svmData != nullptr) {
            if (gpuAddress != nullptr) {
                *gpuAddress = reinterpret_cast<uintptr_t>(ptr);
            }
            return svmData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
        }
        return nullptr;
    }
};
#undef ADDMETHOD_NOBASE
} // namespace ult
} // namespace L0
