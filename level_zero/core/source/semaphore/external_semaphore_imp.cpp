/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"

#include "shared/source/device/device.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

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

    auto driverHandle = Device::fromHandle(device)->getDriverHandle();
    auto driverHandleImp = static_cast<DriverHandleImp *>(driverHandle);

    std::lock_guard<std::mutex> lock(driverHandleImp->externalSemaphoreControllerMutex);
    if (driverHandleImp->externalSemaphoreController == nullptr) {
        driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();
        driverHandleImp->externalSemaphoreController->startThread();
    }

    *phSemaphore = externalSemaphore;

    return result;
}

ze_result_t ExternalSemaphoreImp::initialize(ze_device_handle_t device, const ze_external_semaphore_ext_desc_t *semaphoreDesc) {
    this->device = Device::fromHandle(device);
    auto deviceImp = static_cast<DeviceImp *>(this->device);
    this->desc = semaphoreDesc;
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

    this->neoExternalSemaphore = NEO::ExternalSemaphore::create(deviceImp->getOsInterface(), externalSemaphoreType, handle, fd, name);
    if (!this->neoExternalSemaphore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ExternalSemaphoreImp::releaseExternalSemaphore() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

std::unique_ptr<ExternalSemaphoreController> ExternalSemaphoreController::create() {
    return std::make_unique<ExternalSemaphoreController>();
}

ze_result_t ExternalSemaphoreController::allocateProxyEvent(ze_device_handle_t hDevice, ze_context_handle_t hContext, ze_event_handle_t *phEvent) {
    if (this->eventPoolsMap.find(hDevice) == this->eventPoolsMap.end()) {
        this->eventPoolsMap[hDevice] = std::vector<EventPool *>();
        this->eventsCreatedFromLatestPoolMap[hDevice] = 0u;
    }

    if (this->eventPoolsMap[hDevice].empty() ||
        this->eventsCreatedFromLatestPoolMap[hDevice] + 1 > this->maxEventCountInPool) {
        ze_result_t result;
        ze_event_pool_desc_t desc{};
        desc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
        desc.count = static_cast<uint32_t>(this->maxEventCountInPool);
        desc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        auto driverHandle = L0::Device::fromHandle(hDevice)->getDriverHandle();
        auto pool = EventPool::create(driverHandle, L0::Context::fromHandle(hContext), 1, &hDevice, &desc, result);
        if (!pool) {
            return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        this->eventPoolsMap[hDevice].push_back(pool);
        this->eventsCreatedFromLatestPoolMap[hDevice] = 0u;
    }

    auto pool = this->eventPoolsMap[hDevice][this->eventPoolsMap[hDevice].size() - 1];
    ze_event_desc_t desc{};
    desc.index = static_cast<uint32_t>(this->eventsCreatedFromLatestPoolMap[hDevice]++);
    desc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    desc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t hEvent{};
    pool->createEvent(&desc, &hEvent);

    *phEvent = hEvent;

    return ZE_RESULT_SUCCESS;
}

void ExternalSemaphoreController::processProxyEvents() {
    for (auto it = proxyEvents.rbegin(); it != proxyEvents.rend();) {
        Event *event = std::get<0>(*it);
        ExternalSemaphoreImp *externalSemaphoreImp = static_cast<ExternalSemaphoreImp *>(std::get<1>(*it));
        uint64_t fenceValue = std::get<2>(*it);
        SemaphoreOperation operation = std::get<3>(*it);

        if (operation == SemaphoreOperation::Wait) {
            if (externalSemaphoreImp->neoExternalSemaphore->getState() == NEO::ExternalSemaphore::SemaphoreState::Initial) {
                externalSemaphoreImp->neoExternalSemaphore->enqueueWait(&fenceValue);
            }
            if (externalSemaphoreImp->neoExternalSemaphore->getState() == NEO::ExternalSemaphore::SemaphoreState::Signaled) {
                event->hostSignal(false);
                it = std::vector<std::tuple<Event *, ExternalSemaphore *, uint64_t, SemaphoreOperation>>::reverse_iterator(this->proxyEvents.erase((++it).base()));
                this->processedProxyEvents.push_back(event);
            } else {
                ++it;
            }
        } else {
            ze_result_t ret = event->hostSynchronize(std::numeric_limits<uint64_t>::max());
            if (ret == ZE_RESULT_SUCCESS) {
                externalSemaphoreImp->neoExternalSemaphore->enqueueSignal(&fenceValue);
            }

            event->destroy();
            it = std::vector<std::tuple<Event *, ExternalSemaphore *, uint64_t, SemaphoreOperation>>::reverse_iterator(this->proxyEvents.erase((++it).base()));
        }
    }
}

void ExternalSemaphoreController::runController() {
    while (true) {
        std::unique_lock<std::mutex> lock(this->semControllerMutex);
        this->semControllerCv.wait(lock, [this] { return (!this->proxyEvents.empty() || !this->continueRunning); });

        if (!this->continueRunning) {
            lock.unlock();
            break;
        } else {
            this->processProxyEvents();

            lock.unlock();
            this->semControllerCv.notify_all();
        }
    }
}

} // namespace L0
