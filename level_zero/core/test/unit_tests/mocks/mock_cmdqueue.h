/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include <cstddef>
#include <optional>

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
    using BaseClass::taskCount;
    using CommandQueue::activeSubDevices;
    using CommandQueue::internalUsage;
    using CommandQueue::partitionCount;

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

    ADDMETHOD_NOBASE(createFence, ze_result_t, ZE_RESULT_SUCCESS, (const ze_fence_desc_t *desc, ze_fence_handle_t *phFence));
    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(executeCommandLists, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists, ze_fence_handle_t hFence, bool performMigration));
    ADDMETHOD_NOBASE(executeCommands, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t numCommands, void *phCommands, ze_fence_handle_t hFence));
    ADDMETHOD_NOBASE(synchronize, ze_result_t, ZE_RESULT_SUCCESS, (uint64_t timeout));
    ADDMETHOD_NOBASE(getPreemptionCmdProgramming, bool, false, ());
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockCommandQueueHw : public L0::CommandQueueHw<gfxCoreFamily> {
    using BaseClass = ::L0::CommandQueueHw<gfxCoreFamily>;
    using BaseClass::commandStream;
    using BaseClass::printfFunctionContainer;
    using L0::CommandQueue::activeSubDevices;
    using L0::CommandQueue::internalUsage;
    using L0::CommandQueue::partitionCount;
    using L0::CommandQueue::preemptionCmdSyncProgramming;
    using L0::CommandQueueImp::csr;

    MockCommandQueueHw(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : L0::CommandQueueHw<gfxCoreFamily>(device, csr, desc) {
    }
    ze_result_t synchronize(uint64_t timeout) override {
        synchronizedCalled++;
        return synchronizeReturnValue;
    }

    NEO::WaitStatus reserveLinearStreamSize(size_t size) override {
        if (reserveLinearStreamSizeReturnValue.has_value()) {
            return *reserveLinearStreamSizeReturnValue;
        }

        return BaseClass::reserveLinearStreamSize(size);
    }

    NEO::SubmissionStatus submitBatchBuffer(size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr, bool isCooperative) override {
        residencyContainerSnapshot = residencyContainer;
        return BaseClass::submitBatchBuffer(offset, residencyContainer, endingCmdPtr, isCooperative);
    }

    uint32_t synchronizedCalled = 0;
    NEO::ResidencyContainer residencyContainerSnapshot;
    ze_result_t synchronizeReturnValue{ZE_RESULT_SUCCESS};
    std::optional<NEO::WaitStatus> reserveLinearStreamSizeReturnValue{};
};

struct Deleter {
    void operator()(CommandQueueImp *cmdQ) {
        cmdQ->destroy();
    }
};

} // namespace ult
} // namespace L0