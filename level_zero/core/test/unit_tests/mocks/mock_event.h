/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Event> : public ::L0::Event {
    using BaseClass = ::L0::Event;
    using BaseClass::csr;
    using BaseClass::hostAddress;
    using BaseClass::l3FlushAppliedOnKernel;
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

class MockEvent : public ::L0::Event {
  public:
    using ::L0::Event::l3FlushAppliedOnKernel;
    MockEvent() {
        mockAllocation.reset(new NEO::MockGraphicsAllocation(0,
                                                             NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                                             reinterpret_cast<void *>(0x1234),
                                                             0x1000,
                                                             0,
                                                             sizeof(uint32_t),
                                                             NEO::MemoryPool::System4KBPages,
                                                             NEO::MemoryManager::maxOsContextCount));
        this->timestampSizeInDw = 1;
        this->contextStartOffset = 0;
        this->contextEndOffset = 4;
        this->globalStartOffset = 8;
        this->globalEndOffset = 12;
        this->singlePacketSize = 16;
    }
    NEO::GraphicsAllocation &getAllocation(L0::Device *device) override {
        return *mockAllocation.get();
    }

    uint64_t getGpuAddress(L0::Device *device) override {
        return mockAllocation->getGpuAddress();
    }

    ze_result_t destroy() override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t hostSignal() override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t hostSynchronize(uint64_t timeout) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t queryStatus() override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t reset() override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t queryTimestampsExp(L0::Device *device, uint32_t *pCount, ze_kernel_timestamp_result_t *pTimestamps) override {
        return ZE_RESULT_SUCCESS;
    }
    uint32_t getPacketsUsedInLastKernel() override { return 1; }
    uint32_t getPacketsInUse() override { return 1; }
    void resetPackets() override {}
    void setPacketsInUse(uint32_t value) override {}
    uint64_t getPacketAddress(L0::Device *) override { return 0; }

    std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
};

} // namespace ult
} // namespace L0
