/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

void ExternalSemaphoreOperationData::add(const ExternalSemaphoreImp &semaphore, uint64_t fenceValue) {
    operations.push_back({semaphore.neoExternalSemaphore.get(), fenceValue});
}

namespace {
void printResults(const char *operationName, const ExternalSemaphoreOperationData &operationData, bool result) {
    for (const auto &operation : operationData.operations) {
        PRINT_STRING(NEO::debugManager.flags.PrintExternalSemaphoreOperationResults.get(), stdout,
                     "ExternalSemaphoreImp::%s semaphore=%p handle=0x%x value=%llu result=%d\n",
                     operationName, static_cast<const void *>(operation.semaphore), operation.semaphore->getSyncHandle(),
                     static_cast<unsigned long long>(operation.fenceValue), static_cast<int>(result));
    }
}
} // namespace

void ExternalSemaphoreImp::semaphoreWait(NEO::DriverModel &driverModel, const ExternalSemaphoreOperationData &operationData) {
    const bool result = driverModel.waitExternalSemaphoresFromCpu(operationData.operations);
    printResults("semaphoreWait", operationData, result);
}

void ExternalSemaphoreImp::semaphoreSignal(NEO::DriverModel &driverModel, const ExternalSemaphoreOperationData &operationData) {
    const bool result = driverModel.signalExternalSemaphoresFromCpu(operationData.operations);
    printResults("semaphoreSignal", operationData, result);
}

ze_result_t
ExternalSemaphore::importExternalSemaphore(ze_device_handle_t device, const ze_external_semaphore_ext_desc_t *semaphoreDesc, ze_external_semaphore_ext_handle_t *phSemaphore) {
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

ze_result_t ExternalSemaphoreImp::initialize(ze_device_handle_t device, const ze_external_semaphore_ext_desc_t *semaphoreDesc) {
    this->device = Device::fromHandle(device);
    NEO::ExternalSemaphore::Type externalSemaphoreType;
    void *handle = nullptr;
    const char *name = nullptr;
    int fd = 0;

    if (semaphoreDesc->pNext != nullptr) {
        const ze_base_desc_t *extendedDesc =
            reinterpret_cast<const ze_base_desc_t *>(semaphoreDesc->pNext);
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC) {
            const ze_external_semaphore_win32_ext_desc_t *extendedSemaphoreDesc =
                reinterpret_cast<const ze_external_semaphore_win32_ext_desc_t *>(extendedDesc);
            handle = extendedSemaphoreDesc->handle;
            name = extendedSemaphoreDesc->name;
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_FD_EXT_DESC) {
            const ze_external_semaphore_fd_ext_desc_t *extendedSemaphoreDesc =
                reinterpret_cast<const ze_external_semaphore_fd_ext_desc_t *>(extendedDesc);
            fd = extendedSemaphoreDesc->fd;
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    } else {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    switch (semaphoreDesc->flags) {
    case ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_OPAQUE_FD:
        externalSemaphoreType = NEO::ExternalSemaphore::Type::OpaqueFd;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_OPAQUE_WIN32:
        externalSemaphoreType = NEO::ExternalSemaphore::Type::OpaqueWin32;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_OPAQUE_WIN32_KMT:
        externalSemaphoreType = NEO::ExternalSemaphore::Type::OpaqueWin32Kmt;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE:
        externalSemaphoreType = NEO::ExternalSemaphore::Type::D3d12Fence;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D11_FENCE:
        externalSemaphoreType = NEO::ExternalSemaphore::Type::D3d11Fence;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_KEYED_MUTEX:
        externalSemaphoreType = NEO::ExternalSemaphore::Type::KeyedMutex;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_KEYED_MUTEX_KMT:
        externalSemaphoreType = NEO::ExternalSemaphore::Type::KeyedMutexKmt;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_VK_TIMELINE_SEMAPHORE_FD:
        externalSemaphoreType = NEO::ExternalSemaphore::Type::TimelineSemaphoreFd;
        break;
    case ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_VK_TIMELINE_SEMAPHORE_WIN32:
        externalSemaphoreType = NEO::ExternalSemaphore::Type::TimelineSemaphoreWin32;
        break;
    default:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    NEO::ExternalSemaphore::ImportResult importResult = NEO::ExternalSemaphore::ImportResult::success;
    this->neoExternalSemaphore = NEO::ExternalSemaphore::create(this->device->getOsInterface(), externalSemaphoreType, handle, fd, name, importResult);
    if (!this->neoExternalSemaphore) {
        return (importResult == NEO::ExternalSemaphore::ImportResult::invalidResource) ? ZE_RESULT_ERROR_INVALID_ARGUMENT
                                                                                       : ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ExternalSemaphoreImp::releaseExternalSemaphore() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

bool ExternalSemaphoreImp::areImportedToDevice(Device &device, uint32_t numSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores) {
    const auto osInterface = device.getOsInterface();
    for (uint32_t i = 0; i < numSemaphores; ++i) {
        if (static_cast<ExternalSemaphoreImp *>(hSemaphores[i])->neoExternalSemaphore->osInterface != osInterface) {
            return false;
        }
    }
    return true;
}

} // namespace L0
