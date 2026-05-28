/*
 * Copyright (C) 2024-2026 Intel Corporation
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
#include <stack>
#include <thread>

namespace L0 {
struct Device;

class ExternalSemaphoreImp : public ExternalSemaphore {
  public:
    ze_result_t initialize(ze_device_handle_t device, const ze_external_semaphore_ext_desc_t *semaphoreDesc);
    ze_result_t releaseExternalSemaphore() override;

    ExternalSemaphore *toBase() { return static_cast<ExternalSemaphore *>(this); }

    std::unique_ptr<NEO::ExternalSemaphore> neoExternalSemaphore;

  protected:
    Device *device = nullptr;
};

class ExternalSemaphoreController : NEO::NonCopyableAndNonMovableClass {
  public:
    enum SemaphoreOperation {
        Wait,
        Signal
    };

    struct ProxyEvent {
        Event *event;
        ExternalSemaphore *extSemaphore;
        uint64_t fenceValue;
        SemaphoreOperation operation;
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

    ze_result_t allocateProxyEvent(ze_device_handle_t hDevice, ze_context_handle_t hContext, ze_event_handle_t *phEvent);

    std::mutex semControllerMutex;
    std::condition_variable semControllerCv;

    std::vector<ProxyEvent> proxyEvents;
    static constexpr uint32_t poolCapacity = 32u;

  protected:
    struct ReuseEvent {
        EventPool *pool = nullptr;
        uint32_t eventIdx = 0u;
    };

    struct ReusableEventPools {
        std::vector<EventPool *> pools{};
        uint32_t eventSlots = 0u;
        std::stack<ReuseEvent> reuseEvent{};
    };

    void processProxyEvents();
    void releaseEvent(Event *event);
    void runController();
    void releaseResources();

    std::unordered_map<ze_device_handle_t, ReusableEventPools> eventPools;
    std::unordered_map<NEO::ExternalSemaphore *, Event *> syncEvents;
    bool continueRunning = true;

    std::thread extSemThread;
};

static_assert(NEO::NonCopyableAndNonMovable<ExternalSemaphoreController>);

} // namespace L0
