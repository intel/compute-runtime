/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

namespace L0 {

void ExternalSemaphoreImp::semaphoreWait(const ExternalSemaphoreOperationData &operationData) {
    for (auto [semaphore, value] : operationData.semaphores) {
        bool result = semaphore->neoExternalSemaphore->enqueueWait(&value);
        PRINT_STRING(NEO::debugManager.flags.PrintExternalSemaphoreOperationResults.get(), stdout,
                     "ExternalSemaphoreImp::semaphoreWait semaphore=%p value=%llu result=%d\n",
                     static_cast<void *>(semaphore),
                     static_cast<unsigned long long>(value),
                     static_cast<int>(result));
    }
};

void ExternalSemaphoreImp::semaphoreSignal(const ExternalSemaphoreOperationData &operationData) {
    for (auto [semaphore, value] : operationData.semaphores) {
        bool result = semaphore->neoExternalSemaphore->enqueueSignal(&value);
        PRINT_STRING(NEO::debugManager.flags.PrintExternalSemaphoreOperationResults.get(), stdout,
                     "ExternalSemaphoreImp::semaphoreSignal semaphore=%p value=%llu result=%d\n",
                     static_cast<void *>(semaphore),
                     static_cast<unsigned long long>(value),
                     static_cast<int>(result));
    }
};

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

    std::lock_guard<std::mutex> lock(driverHandle->externalSemaphoreControllerMutex);
    if (driverHandle->externalSemaphoreController == nullptr) {
        driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();
        driverHandle->externalSemaphoreController->startThread();
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

    this->neoExternalSemaphore = NEO::ExternalSemaphore::create(this->device->getOsInterface(), externalSemaphoreType, handle, fd, name);
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
    if (eventPools.find(hDevice) == eventPools.end()) {
        eventPools.emplace(hDevice, ReusableEventPools{});
    }

    ReusableEventPools &devPools = eventPools[hDevice];
    EventPool *pool = nullptr;
    uint32_t idx = 0;
    if (devPools.eventSlots) {
        // Event slots available, check for reusable slots before using new pool slot.
        if (devPools.reuseEvent.empty()) {
            // No reusable events, use available slot in most recently created pool.
            pool = devPools.pools.back();
            idx = poolCapacity - devPools.eventSlots;
        } else { // Reuse event from the reuse stack.
            auto slot = devPools.reuseEvent.top();
            devPools.reuseEvent.pop();
            pool = slot.pool;
            idx = slot.eventIdx;
        }
    } else { // No available event slots --> create new pool.
        auto driver = L0::Device::fromHandle(hDevice)->getDriverHandle();
        auto context = L0::Context::fromHandle(hContext);

        ze_event_pool_desc_t desc{};
        desc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
        desc.count = poolCapacity;
        desc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        auto result = ZE_RESULT_SUCCESS;
        pool = EventPool::create(driver, context, 1, &hDevice, &desc, result);
        if (!pool) {
            return result;
        }
        devPools.pools.push_back(pool);
        devPools.eventSlots += poolCapacity;
        idx = 0u;
    }
    ze_event_desc_t desc{};
    desc.index = idx;
    desc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    desc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t hEvent{};
    ze_result_t result = pool->createEvent(&desc, &hEvent);
    if (result == ZE_RESULT_SUCCESS) {
        devPools.eventSlots--;
        *phEvent = hEvent;
    }
    return result;
}

void ExternalSemaphoreController::processProxyEvents() {
    for (auto it = proxyEvents.begin(); it != proxyEvents.end();) {
        Event *event = it->event;
        ExternalSemaphoreImp *extSemaphore = static_cast<ExternalSemaphoreImp *>(it->extSemaphore);
        uint64_t fenceValue = it->fenceValue;
        SemaphoreOperation operation = it->operation;

        auto neoSemaphore = extSemaphore->neoExternalSemaphore.get();

        if (operation == SemaphoreOperation::Wait) {
            if (neoSemaphore->getState() == NEO::ExternalSemaphore::SemaphoreState::Initial) {
                neoSemaphore->enqueueWait(&fenceValue);
            }
            if (neoSemaphore->getState() == NEO::ExternalSemaphore::SemaphoreState::Signaled) {
                event->hostSignal(false);
                it = proxyEvents.erase(it);
                if (syncEvents.find(neoSemaphore) != syncEvents.end() &&
                    syncEvents[neoSemaphore] != event) {
                    releaseEvent(syncEvents[neoSemaphore]);
                }
                syncEvents[neoSemaphore] = event;
            } else {
                ++it;
            }
        } else {
            // Timeout value doesn't matter here since event should already be
            //   signaled on host side when processing Signal operation.
            ze_result_t ret = event->hostSynchronize(20);
            if (ret == ZE_RESULT_SUCCESS) {
                neoSemaphore->enqueueSignal(&fenceValue);
                releaseEvent(event);
                it = proxyEvents.erase(it);
                auto syncEventIt = syncEvents.find(neoSemaphore);
                if (syncEventIt != syncEvents.end()) {
                    if (syncEventIt->second != event) {
                        releaseEvent(syncEventIt->second);
                    }
                    syncEvents.erase(syncEventIt);
                }
            } else {
                ++it;
            }
        }
    }
}

void ExternalSemaphoreController::releaseEvent(Event *event) {
    if (Device *dev = event->getDevice(); dev) {
        if (auto devPools = eventPools.find(dev->toHandle()); devPools != eventPools.end()) {
            ReusableEventPools &pools = devPools->second;
            pools.reuseEvent.emplace(event->peekEventPool(), event->getPoolIndex());
            event->destroy();
            pools.eventSlots++;
            return;
        }
    }
    event->destroy();
}

void ExternalSemaphoreController::runController() {
    while (true) {
        std::unique_lock<std::mutex> lock(semControllerMutex);
        semControllerCv.wait(lock, [this] { return (!proxyEvents.empty() || !continueRunning); });

        if (!continueRunning) {
            lock.unlock();
            break;
        } else {
            processProxyEvents();

            lock.unlock();
            semControllerCv.notify_all();
        }
    }
}

void ExternalSemaphoreController::releaseResources() {
    {
        std::lock_guard<std::mutex> lock(this->semControllerMutex);
        this->continueRunning = false;
        this->semControllerCv.notify_one();
    }

    joinThread();

    for (auto proxyEvent : proxyEvents) {
        proxyEvent.event->destroy();
    }
    proxyEvents.clear();

    for (auto &syncEvent : syncEvents) {
        syncEvent.second->destroy();
    }
    syncEvents.clear();

    for (auto &devPools : eventPools) {
        for (auto &pool : devPools.second.pools) {
            pool->destroy();
        }
    }
}

} // namespace L0
