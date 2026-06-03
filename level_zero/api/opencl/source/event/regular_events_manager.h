/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>

#include <cstdint>
#include <mutex>
#include <vector>

namespace NEO {
namespace LEO {

class RegularEventsManager {
  public:
    RegularEventsManager(ze_context_handle_t contextHandle, std::vector<ze_device_handle_t> devices);
    ~RegularEventsManager();

    RegularEventsManager(const RegularEventsManager &) = delete;
    RegularEventsManager &operator=(const RegularEventsManager &) = delete;
    RegularEventsManager(RegularEventsManager &&) = delete;
    RegularEventsManager &operator=(RegularEventsManager &&) = delete;

    ze_event_handle_t obtainEvent(bool timestamp);

    void returnEvent(ze_event_handle_t event, bool timestamp);

  protected:
    static constexpr uint32_t eventCountInPool = 8192u;

    struct EventPoolGroup {
        std::vector<ze_event_pool_handle_t> pools{};
        std::vector<ze_event_handle_t> freeEvents{};
        uint32_t createdFromLatestPool = eventCountInPool;
    };

    ze_event_handle_t createEvent(EventPoolGroup &group, bool timestamp);
    EventPoolGroup &getGroup(bool timestamp) { return timestamp ? timestampGroup : hostVisibleGroup; }

    ze_context_handle_t contextHandle = nullptr;
    std::vector<ze_device_handle_t> devices{};
    EventPoolGroup hostVisibleGroup{};
    EventPoolGroup timestampGroup{};
    std::mutex mtx{};
};

} // namespace LEO
} // namespace NEO
