/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"

#include "shared/source/device/device.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

ze_result_t ExternalSemaphore::importExternalSemaphore(ze_device_handle_t device, const ze_intel_external_semaphore_exp_desc_t *semaphoreDesc, ze_intel_external_semaphore_exp_handle_t *phSemaphore) {
    auto externalSemaphore = new ExternalSemaphoreImp();
    if (externalSemaphore == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    auto result = externalSemaphore->initialize(device, semaphoreDesc);
    if (result != ZE_RESULT_SUCCESS) {
        delete externalSemaphore;
        return result;
    }

    *phSemaphore = externalSemaphore;

    return result;
}

ze_result_t ExternalSemaphoreImp::initialize(ze_device_handle_t device, const ze_intel_external_semaphore_exp_desc_t *semaphoreDesc) {
    auto deviceImp = Device::fromHandle(device);
    this->neoDevice = Device::fromHandle(device)->getNEODevice();
    this->desc = semaphoreDesc;
    NEO::ExternalSemaphoreHelper::Type externalSemaphoreType;
    void *handle = nullptr;
    int fd = 0;

    if (semaphoreDesc->pNext != nullptr) {
        const ze_base_desc_t *extendedDesc =
            reinterpret_cast<const ze_base_desc_t *>(semaphoreDesc->pNext);
        if (extendedDesc->stype == ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC) { // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
            const ze_intel_external_semaphore_win32_exp_desc_t *extendedSemaphoreDesc =
                reinterpret_cast<const ze_intel_external_semaphore_win32_exp_desc_t *>(extendedDesc);
            handle = extendedSemaphoreDesc->handle;
        } else if (extendedDesc->stype == ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_FD_EXP_DESC) { // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
            const ze_intel_external_semaphore_desc_fd_exp_desc_t *extendedSemaphoreDesc =
                reinterpret_cast<const ze_intel_external_semaphore_desc_fd_exp_desc_t *>(extendedDesc);
            fd = extendedSemaphoreDesc->fd;
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    } else {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    switch (semaphoreDesc->flags) {
    case ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_FD:
        externalSemaphoreType = NEO::ExternalSemaphoreHelper::Type::OpaqueFd;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32:
        externalSemaphoreType = NEO::ExternalSemaphoreHelper::Type::OpaqueWin32;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32_KMT:
        externalSemaphoreType = NEO::ExternalSemaphoreHelper::Type::OpaqueWin32Kmt;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_D3D12_FENCE:
        externalSemaphoreType = NEO::ExternalSemaphoreHelper::Type::D3d12Fence;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_D3D11_FENCE:
        externalSemaphoreType = NEO::ExternalSemaphoreHelper::Type::D3d11Fence;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_KEYED_MUTEX:
        externalSemaphoreType = NEO::ExternalSemaphoreHelper::Type::KeyedMutex;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_KEYED_MUTEX_KMT:
        externalSemaphoreType = NEO::ExternalSemaphoreHelper::Type::KeyedMutexKmt;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_TIMELINE_SEMAPHORE_FD:
        externalSemaphoreType = NEO::ExternalSemaphoreHelper::Type::TimelineSemaphoreFd;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_TIMELINE_SEMAPHORE_WIN32:
        externalSemaphoreType = NEO::ExternalSemaphoreHelper::Type::TimelineSemaphoreWin32;
        break;
    default:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    this->neoExternalSemaphore = NEO::ExternalSemaphore::create(&deviceImp->getOsInterface(), externalSemaphoreType, handle, fd);
    if (!this->neoExternalSemaphore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ExternalSemaphoreImp::releaseExternalSemaphore() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

ze_result_t ExternalSemaphoreImp::appendWait(const NEO::CommandStreamReceiver *csr) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ExternalSemaphoreImp::appendSignal(const NEO::CommandStreamReceiver *csr) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
