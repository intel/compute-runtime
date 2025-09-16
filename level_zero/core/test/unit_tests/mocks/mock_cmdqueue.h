/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include <cstddef>
#include <optional>

namespace NEO {
class GraphicsAllocation;
class LinearStream;
class ScratchSpaceController;
enum class SubmissionStatus : uint32_t;
enum class WaitStatus;
} // namespace NEO

namespace L0 {
struct Device;

namespace ult {
template <typename Type>
struct Mock;
template <typename Type>
struct WhiteBox;

template <>
struct WhiteBox<::L0::CommandQueue> : public ::L0::CommandQueueImp {
    using BaseClass = ::L0::CommandQueueImp;
    using BaseClass::buffers;
    using BaseClass::cmdListWithAssertExecuted;
    using BaseClass::commandStream;
    using BaseClass::csr;
    using BaseClass::desc;
    using BaseClass::device;
    using BaseClass::firstCmdListStream;
    using BaseClass::forceBbStartJump;
    using BaseClass::preemptionCmdSyncProgramming;
    using BaseClass::printfKernelContainer;
    using BaseClass::startingCmdBuffer;
    using BaseClass::submitBatchBuffer;
    using BaseClass::synchronizeByPollingForTaskCount;
    using BaseClass::taskCount;
    using CommandQueue::activeSubDevices;
    using CommandQueue::cmdListHeapAddressModel;
    using CommandQueue::dispatchCmdListBatchBufferAsPrimary;
    using CommandQueue::doubleSbaWa;
    using CommandQueue::frontEndStateTracking;
    using CommandQueue::heaplessModeEnabled;
    using CommandQueue::heaplessStateInitEnabled;
    using CommandQueue::internalUsage;
    using CommandQueue::partitionCount;
    using CommandQueue::patchingPreamble;
    using CommandQueue::pipelineSelectStateTracking;
    using CommandQueue::saveWaitForPreamble;
    using CommandQueue::stateBaseAddressTracking;
    using CommandQueue::stateComputeModeTracking;

    WhiteBox(Device *device, NEO::CommandStreamReceiver *csr,
             const ze_command_queue_desc_t *desc);
    ~WhiteBox() override;
};
using CommandQueue = WhiteBox<::L0::CommandQueue>;

static ze_command_queue_desc_t defaultCmdqueueDesc = {};
template <>
struct Mock<CommandQueue> : public CommandQueue {
    Mock(L0::Device *device = nullptr, NEO::CommandStreamReceiver *csr = nullptr, const ze_command_queue_desc_t *desc = &defaultCmdqueueDesc);
    ~Mock() override;

    using CommandQueue::isCopyOnlyCommandQueue;

    ADDMETHOD_NOBASE(createFence, ze_result_t, ZE_RESULT_SUCCESS, (const ze_fence_desc_t *desc, ze_fence_handle_t *phFence));
    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(executeCommandLists, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists, ze_fence_handle_t hFence, bool performMigration, NEO::LinearStream *parentImmediateCommandlistLinearStream, std::unique_lock<std::mutex> *outerLockForIndirect));
    ADDMETHOD_NOBASE(synchronize, ze_result_t, ZE_RESULT_SUCCESS, (uint64_t timeout));
    ADDMETHOD_NOBASE(getPreemptionCmdProgramming, bool, false, ());
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockCommandQueueHw : public L0::CommandQueueHw<gfxCoreFamily> {
    using BaseClass = ::L0::CommandQueueHw<gfxCoreFamily>;
    using BaseClass::commandStream;
    using BaseClass::estimateStreamSizeForExecuteCommandListsRegularHeapless;
    using BaseClass::executeCommandListsRegularHeapless;
    using BaseClass::forceBbStartJump;
    using BaseClass::prepareAndSubmitBatchBuffer;
    using BaseClass::printfKernelContainer;
    using BaseClass::startingCmdBuffer;
    using L0::CommandQueue::activeSubDevices;
    using L0::CommandQueue::cmdListHeapAddressModel;
    using L0::CommandQueue::dispatchCmdListBatchBufferAsPrimary;
    using L0::CommandQueue::doubleSbaWa;
    using L0::CommandQueue::frontEndStateTracking;
    using L0::CommandQueue::heaplessModeEnabled;
    using L0::CommandQueue::heaplessStateInitEnabled;
    using L0::CommandQueue::internalUsage;
    using L0::CommandQueue::partitionCount;
    using L0::CommandQueue::patchingPreamble;
    using L0::CommandQueue::pipelineSelectStateTracking;
    using L0::CommandQueue::preemptionCmdSyncProgramming;
    using L0::CommandQueue::saveWaitForPreamble;
    using L0::CommandQueue::stateBaseAddressTracking;
    using L0::CommandQueue::stateComputeModeTracking;
    using L0::CommandQueueImp::csr;
    using typename BaseClass::CommandListExecutionContext;

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
        if (submitBatchBufferReturnValue.has_value()) {
            return *submitBatchBufferReturnValue;
        }
        if (this->startingCmdBuffer == nullptr) {
            this->startingCmdBuffer = &this->commandStream;
        }
        return BaseClass::submitBatchBuffer(offset, residencyContainer, endingCmdPtr, isCooperative);
    }

    ze_result_t executeCommandListsRegular(CommandListExecutionContext &ctx,
                                           uint32_t numCommandLists,
                                           ze_command_list_handle_t *commandListHandles,
                                           ze_fence_handle_t hFence,
                                           NEO::LinearStream *parentImmediateCommandlistLinearStream) override {
        recordedGlobalStatelessAllocation = ctx.globalStatelessAllocation;
        recordedScratchController = ctx.scratchSpaceController;
        recordedLockScratchController = ctx.lockScratchController;
        return BaseClass::executeCommandListsRegular(ctx, numCommandLists, commandListHandles, hFence, parentImmediateCommandlistLinearStream);
    }

    ze_result_t initialize(bool copyOnly, bool isInternal, bool immediateCmdListQueue) override {
        auto returnCode = BaseClass::initialize(copyOnly, isInternal, immediateCmdListQueue);

        if (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
            this->csr->createGlobalStatelessHeap();
        }
        return returnCode;
    }

    void handleIndirectAllocationResidency(UnifiedMemoryControls unifiedMemoryControls, std::unique_lock<std::mutex> &lockForIndirect, bool performMigration) override {
        handleIndirectAllocationResidencyCalledTimes++;
        BaseClass::handleIndirectAllocationResidency(unifiedMemoryControls, lockForIndirect, performMigration);
    }

    NEO::GraphicsAllocation *recordedGlobalStatelessAllocation = nullptr;
    NEO::ScratchSpaceController *recordedScratchController = nullptr;
    uint32_t synchronizedCalled = 0;
    NEO::ResidencyContainer residencyContainerSnapshot;
    ze_result_t synchronizeReturnValue{ZE_RESULT_SUCCESS};
    std::optional<NEO::WaitStatus> reserveLinearStreamSizeReturnValue{};
    std::optional<NEO::SubmissionStatus> submitBatchBufferReturnValue{};
    uint32_t handleIndirectAllocationResidencyCalledTimes = 0;
    bool recordedLockScratchController = false;
};

struct Deleter {
    void operator()(CommandQueueImp *cmdQ) {
        cmdQ->destroy();
    }
};

} // namespace ult
} // namespace L0
