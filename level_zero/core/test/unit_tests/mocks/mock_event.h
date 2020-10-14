/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include "gmock/gmock.h"

#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Event> : public ::L0::Event {
    using BaseClass = ::L0::Event;
    using BaseClass::allocation;
};

using Event = WhiteBox<::L0::Event>;

template <>
struct WhiteBox<::L0::EventPool> : public ::L0::EventPool {
    using BaseClass = ::L0::EventPool;
};

using EventPool = WhiteBox<::L0::EventPool>;

template <>
struct Mock<Event> : public Event {
    Mock();
    ~Mock() override;

    MOCK_METHOD3(create, L0::Event *(::L0::EventPool *eventPool, const ze_event_desc_t *desc, ::L0::Device *device));
    MOCK_METHOD0(destroy, ze_result_t());
    MOCK_METHOD0(hostSignal, ze_result_t());
    MOCK_METHOD1(hostSynchronize, ze_result_t(uint64_t timeout));
    MOCK_METHOD0(queryStatus, ze_result_t());
    MOCK_METHOD0(reset, ze_result_t());
    MOCK_METHOD1(queryKernelTimestamp, ze_result_t(ze_kernel_timestamp_result_t *dstptr));

    // Fake an allocation for event memory
    alignas(16) uint32_t memory = -1;
    NEO::GraphicsAllocation mockAllocation;

    using Event::allocation;
};

template <>
struct Mock<EventPool> : public EventPool {
    Mock();
    ~Mock() override;

    MOCK_METHOD0(destroy, ze_result_t());
    MOCK_METHOD0(getPoolSize, size_t());
    MOCK_METHOD0(getPoolUsedCount, uint32_t());
    MOCK_METHOD1(getIpcHandle, ze_result_t(ze_ipc_event_pool_handle_t *pIpcHandle));
    MOCK_METHOD0(closeIpcHandle, ze_result_t());
    MOCK_METHOD2(createEvent, ze_result_t(const ze_event_desc_t *desc, ze_event_handle_t *phEvent));
    MOCK_METHOD2(reserveEventFromPool, ze_result_t(int index, ::L0::Event *event));
    MOCK_METHOD1(releaseEventToPool, ze_result_t(::L0::Event *event));
    MOCK_METHOD0(getDevice, Device *());
    MOCK_METHOD0(getEventSize, uint32_t());

    std::vector<int> pool;

    using EventPool::eventPoolAllocations;
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
