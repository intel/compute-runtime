/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/test/common/fixtures/captured_api.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include <level_zero/ze_api.h>

namespace NEO {
namespace LEO {
namespace ult {

struct FreeMemExtArgs {
    ze_driver_memory_free_policy_ext_flags_t freePolicy;
    void *ptr;

    FreeMemExtArgs(const ze_memory_free_ext_desc_t *freeDesc, void *ptr)
        : freePolicy(freeDesc != nullptr ? freeDesc->freePolicy : static_cast<ze_driver_memory_free_policy_ext_flags_t>(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_FORCE_UINT32)),
          ptr(ptr) {}
};

struct AllocDeviceMemArgs {
    ze_device_handle_t hDevice;
    size_t size;
    bool fdImport = false;
    int fd = -1;
    bool win32Import = false;
    void *win32Handle = nullptr;
    const void *win32Name = nullptr;

    AllocDeviceMemArgs(ze_device_handle_t hDevice, const ze_device_mem_alloc_desc_t *deviceDesc, size_t size)
        : hDevice(hDevice), size(size) {
        auto extendedDesc = deviceDesc != nullptr ? static_cast<const ze_base_desc_t *>(deviceDesc->pNext) : nullptr;
        if (extendedDesc == nullptr) {
            return;
        }
        switch (extendedDesc->stype) {
        case ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD:
            fdImport = true;
            fd = static_cast<const ze_external_memory_import_fd_t *>(static_cast<const void *>(extendedDesc))->fd;
            break;
        case ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32:
            win32Import = true;
            win32Handle = static_cast<const ze_external_memory_import_win32_handle_t *>(static_cast<const void *>(extendedDesc))->handle;
            win32Name = static_cast<const ze_external_memory_import_win32_handle_t *>(static_cast<const void *>(extendedDesc))->name;
            break;
        default:
            break;
        }
    }
};

struct CapturingContext : public L0::ult::Mock<L0::Context> {
    using BaseClass = L0::ult::Mock<L0::Context>;

    CapturingContext(L0::DriverHandle *driverHandle, ze_device_handle_t deviceHandle) : BaseClass(driverHandle) {
        this->devices[0] = deviceHandle;
        this->numDevices = 1;
    }

    ze_result_t freeMemExt(const ze_memory_free_ext_desc_t *freeDesc, void *ptr) override {
        this->freeMemExtArgs.records.push_back(FreeMemExtArgs{freeDesc, ptr});
        return freeMemExtResult;
    }

    ze_result_t allocDeviceMem(ze_device_handle_t hDevice, const ze_device_mem_alloc_desc_t *deviceDesc, size_t size, size_t alignment, void **ptr) override {
        this->allocDeviceMemArgs.records.push_back(AllocDeviceMemArgs{hDevice, deviceDesc, size});
        if (allocDeviceMemResult == ZE_RESULT_SUCCESS) {
            *ptr = allocDeviceMemPtr;
        }
        return allocDeviceMemResult;
    }

    CapturedApi<FreeMemExtArgs> freeMemExtArgs{};
    CapturedApi<AllocDeviceMemArgs> allocDeviceMemArgs{};
    void *allocDeviceMemPtr = nullptr;
    ze_result_t allocDeviceMemResult = ZE_RESULT_SUCCESS;
    ze_result_t freeMemExtResult = ZE_RESULT_SUCCESS;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
