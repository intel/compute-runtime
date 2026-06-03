/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/event/regular_events_manager.h"

#include <utility>

namespace NEO {
namespace LEO {

RegularEventsManager::RegularEventsManager(ze_context_handle_t contextHandle, std::vector<ze_device_handle_t> devices)
    : contextHandle(contextHandle), devices(std::move(devices)) {}

RegularEventsManager::~RegularEventsManager() {
    for (auto *group : {&hostVisibleGroup, &timestampGroup}) {
        for (auto event : group->freeEvents) {
            zeEventDestroy(event);
        }
        for (auto pool : group->pools) {
            zeEventPoolDestroy(pool);
        }
    }
}

ze_event_handle_t RegularEventsManager::obtainEvent(bool timestamp) {
    std::lock_guard<std::mutex> lock(this->mtx);

    auto &group = this->getGroup(timestamp);
    if (!group.freeEvents.empty()) {
        auto event = group.freeEvents.back();
        group.freeEvents.pop_back();
        zeEventHostReset(event);
        return event;
    }

    return this->createEvent(group, timestamp);
}

void RegularEventsManager::returnEvent(ze_event_handle_t event, bool timestamp) {
    std::lock_guard<std::mutex> lock(this->mtx);

    if (zeEventQueryStatus(event) == ZE_RESULT_SUCCESS) {
        this->getGroup(timestamp).freeEvents.push_back(event);
    } else {
        zeEventDestroy(event);
    }
}

ze_event_handle_t RegularEventsManager::createEvent(EventPoolGroup &group, bool timestamp) {
    if (group.createdFromLatestPool >= eventCountInPool) {
        ze_event_pool_flags_t poolFlags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        if (timestamp) {
            poolFlags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
        }
        ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, poolFlags, eventCountInPool};
        zeEventPoolCreate(this->contextHandle, &eventPoolDesc, static_cast<uint32_t>(this->devices.size()), this->devices.data(), &group.pools.emplace_back());
        group.createdFromLatestPool = 0u;
    }

    ze_event_handle_t event{};
    ze_event_desc_t eventDesc{ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, group.createdFromLatestPool++,
                              ZE_EVENT_SCOPE_FLAG_HOST | ZE_EVENT_SCOPE_FLAG_DEVICE,
                              ZE_EVENT_SCOPE_FLAG_HOST | ZE_EVENT_SCOPE_FLAG_DEVICE};
    zeEventCreate(group.pools.back(), &eventDesc, &event);

    return event;
}

} // namespace LEO
} // namespace NEO
