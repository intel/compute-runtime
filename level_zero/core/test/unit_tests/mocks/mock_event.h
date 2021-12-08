/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Event> : public ::L0::Event {
    using BaseClass = ::L0::Event;
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

    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(hostSignal, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(hostSynchronize, ze_result_t, ZE_RESULT_SUCCESS, (uint64_t timeout));
    ADDMETHOD_NOBASE(queryStatus, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(reset, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(queryKernelTimestamp, ze_result_t, ZE_RESULT_SUCCESS, (ze_kernel_timestamp_result_t * dstptr));
    ADDMETHOD_NOBASE(queryTimestampsExp, ze_result_t, ZE_RESULT_SUCCESS, (::L0::Device * device, uint32_t *pCount, ze_kernel_timestamp_result_t *pTimestamps));

    // Fake an allocation for event memory
    alignas(16) uint32_t memory = -1;
    NEO::GraphicsAllocation mockAllocation;
};

template <>
struct Mock<EventPool> : public EventPool {
    Mock() = default;

    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getIpcHandle, ze_result_t, ZE_RESULT_SUCCESS, (ze_ipc_event_pool_handle_t * pIpcHandle));
    ADDMETHOD_NOBASE(closeIpcHandle, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(createEvent, ze_result_t, ZE_RESULT_SUCCESS, (const ze_event_desc_t *desc, ze_event_handle_t *phEvent));
    ADDMETHOD_NOBASE(getDevice, Device *, nullptr, ());
    ADDMETHOD_NOBASE(getEventSize, uint32_t, 0u, ());

    using EventPool::eventPoolAllocations;
};

} // namespace ult
} // namespace L0
