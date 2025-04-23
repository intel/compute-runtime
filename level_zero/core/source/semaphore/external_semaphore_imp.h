/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/external_semaphore.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/semaphore/external_semaphore.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace L0 {

class ExternalSemaphoreImp : public ExternalSemaphore {
  public:
    ze_result_t initialize(ze_device_handle_t device, const ze_external_semaphore_ext_desc_t *semaphoreDesc);
    ze_result_t releaseExternalSemaphore() override;

    std::unique_ptr<NEO::ExternalSemaphore> neoExternalSemaphore;

  protected:
    Device *device = nullptr;
    const ze_external_semaphore_ext_desc_t *desc;
};

class ExternalSemaphoreController : NEO::NonCopyableAndNonMovableClass {
  public:
    enum SemaphoreOperation {
        Wait,
        Signal
    };

    static std::unique_ptr<ExternalSemaphoreController> create();

    ~ExternalSemaphoreController() {
        releaseResources();
    }

    void startThread() {
        if (!extSemThread.joinable()) {
            extSemThread = std::thread(&ExternalSemaphoreController::runController, this);
        }
    }

    void joinThread() {
        if (extSemThread.joinable()) {
            extSemThread.join();
        }
    }

    void releaseResources() {
        {
            std::lock_guard<std::mutex> lock(this->semControllerMutex);
            this->continueRunning = false;
            this->semControllerCv.notify_one();
        }

        joinThread();

        for (auto it = proxyEvents.begin(); it != proxyEvents.end(); ++it) {
            Event *event = std::get<0>(*it);
            event->destroy();
        }

        for (auto event : processedProxyEvents) {
            event->destroy();
        }

        for (auto &eventPools : eventPoolsMap) {
            for (auto &eventPool : eventPools.second) {
                eventPool->destroy();
            }
        }
    }

    ze_result_t allocateProxyEvent(ze_device_handle_t hDevice, ze_context_handle_t hContext, ze_event_handle_t *phEvent);
    void processProxyEvents();

    std::mutex semControllerMutex;
    std::condition_variable semControllerCv;

    std::unordered_map<ze_device_handle_t, std::vector<EventPool *>> eventPoolsMap;
    std::unordered_map<ze_device_handle_t, size_t> eventsCreatedFromLatestPoolMap;
    const size_t maxEventCountInPool = 20u;
    std::vector<std::tuple<Event *, ExternalSemaphore *, uint64_t, SemaphoreOperation>> proxyEvents;
    std::vector<Event *> processedProxyEvents;
    bool continueRunning = true;

  private:
    void runController();

    std::thread extSemThread;
};

static_assert(NEO::NonCopyableAndNonMovable<ExternalSemaphoreController>);

} // namespace L0
