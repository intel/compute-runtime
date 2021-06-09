/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include "gmock/gmock.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::CommandQueue> : public ::L0::CommandQueueImp {
    using BaseClass = ::L0::CommandQueueImp;
    using BaseClass::buffers;
    using BaseClass::commandStream;
    using BaseClass::csr;
    using BaseClass::device;
    using BaseClass::preemptionCmdSyncProgramming;
    using BaseClass::printfFunctionContainer;
    using BaseClass::submitBatchBuffer;
    using BaseClass::synchronizeByPollingForTaskCount;
    using CommandQueue::commandQueuePreemptionMode;
    using CommandQueue::internalUsage;

    WhiteBox(Device *device, NEO::CommandStreamReceiver *csr,
             const ze_command_queue_desc_t *desc);
    ~WhiteBox() override;
};
using CommandQueue = WhiteBox<::L0::CommandQueue>;

static ze_command_queue_desc_t default_cmd_queue_desc = {};
template <>
struct Mock<CommandQueue> : public CommandQueue {
    Mock(L0::Device *device = nullptr, NEO::CommandStreamReceiver *csr = nullptr, const ze_command_queue_desc_t *desc = &default_cmd_queue_desc);
    ~Mock() override;

    MOCK_METHOD(ze_result_t,
                createFence,
                (const ze_fence_desc_t *desc,
                 ze_fence_handle_t *phFence),
                (override));
    MOCK_METHOD(ze_result_t,
                destroy,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                executeCommandLists,
                (uint32_t numCommandLists,
                 ze_command_list_handle_t *phCommandLists,
                 ze_fence_handle_t hFence,
                 bool performMigration),
                (override));
    MOCK_METHOD(ze_result_t,
                executeCommands,
                (uint32_t numCommands,
                 void *phCommands,
                 ze_fence_handle_t hFence),
                (override));
    MOCK_METHOD(ze_result_t,
                synchronize,
                (uint64_t timeout),
                (override));
    MOCK_METHOD(void,
                dispatchTaskCountWrite,
                (NEO::LinearStream & commandStream,
                 bool flushDataCache),
                (override));
    MOCK_METHOD(bool,
                getPreemptionCmdProgramming,
                (),
                (override));
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockCommandQueueHw : public L0::CommandQueueHw<gfxCoreFamily> {
    using BaseClass = ::L0::CommandQueueHw<gfxCoreFamily>;
    using BaseClass::commandStream;
    using BaseClass::printfFunctionContainer;
    using L0::CommandQueue::internalUsage;
    using L0::CommandQueue::preemptionCmdSyncProgramming;
    using L0::CommandQueueImp::csr;

    MockCommandQueueHw(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : L0::CommandQueueHw<gfxCoreFamily>(device, csr, desc) {
    }
    ze_result_t synchronize(uint64_t timeout) override {
        synchronizedCalled++;
        return ZE_RESULT_SUCCESS;
    }

    void submitBatchBuffer(size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr) override {
        residencyContainerSnapshot = residencyContainer;
        BaseClass::submitBatchBuffer(offset, residencyContainer, endingCmdPtr);
    }

    uint32_t synchronizedCalled = 0;
    NEO::ResidencyContainer residencyContainerSnapshot;
};

} // namespace ult
} // namespace L0
