/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/test/common/helpers/ult_hw_config.h"

using namespace NEO;

template <typename GfxFamily>
class TestedDrmCommandStreamReceiver : public DrmCommandStreamReceiver<GfxFamily> {
  public:
    using CommandStreamReceiver::activePartitions;
    using CommandStreamReceiver::clearColorAllocation;
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::createPreemptionAllocation;
    using CommandStreamReceiver::flushStamp;
    using CommandStreamReceiver::getTagAddress;
    using CommandStreamReceiver::getTagAllocation;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::latestSentTaskCount;
    using CommandStreamReceiver::makeResident;
    using CommandStreamReceiver::postSyncWriteOffset;
    using CommandStreamReceiver::tagAddress;
    using CommandStreamReceiver::taskCount;
    using CommandStreamReceiver::useGpuIdleImplicitFlush;
    using CommandStreamReceiver::useNewResourceImplicitFlush;
    using CommandStreamReceiver::useNotifyEnableForPostSync;
    using DrmCommandStreamReceiver<GfxFamily>::exec;
    using DrmCommandStreamReceiver<GfxFamily>::residency;
    using DrmCommandStreamReceiver<GfxFamily>::useContextForUserFenceWait;
    using DrmCommandStreamReceiver<GfxFamily>::useUserFenceWait;
    using CommandStreamReceiverHw<GfxFamily>::directSubmission;
    using CommandStreamReceiverHw<GfxFamily>::blitterDirectSubmission;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::lastSentSliceCount;

    TestedDrmCommandStreamReceiver(gemCloseWorkerMode mode,
                                   ExecutionEnvironment &executionEnvironment,
                                   const DeviceBitfield deviceBitfield)
        : DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, 0, deviceBitfield, mode) {
    }
    TestedDrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                                   uint32_t rootDeviceIndex,
                                   const DeviceBitfield deviceBitfield)
        : DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield, gemCloseWorkerMode::gemCloseWorkerInactive) {
    }

    void overrideDispatchPolicy(DispatchMode overrideValue) {
        this->dispatchMode = overrideValue;
    }

    void makeNonResident(GraphicsAllocation &gfxAllocation) override {
        makeNonResidentResult.called = true;
        makeNonResidentResult.allocation = &gfxAllocation;
        DrmCommandStreamReceiver<GfxFamily>::makeNonResident(gfxAllocation);
    }

    struct MakeResidentNonResidentResult {
        bool called = false;
        GraphicsAllocation *allocation = nullptr;
    };

    MakeResidentNonResidentResult makeNonResidentResult;

    SubmissionAggregator *peekSubmissionAggregator() const {
        return this->submissionAggregator.get();
    }

    void overrideSubmissionAggregator(SubmissionAggregator *newSubmissionsAggregator) {
        this->submissionAggregator.reset(newSubmissionsAggregator);
    }

    std::vector<drm_i915_gem_exec_object2> &getExecStorage() {
        return this->execObjectsStorage;
    }

    bool createPreemptionAllocation() override {
        if (ultHwConfig.csrBaseCallCreatePreemption) {
            return CommandStreamReceiver::createPreemptionAllocation();
        } else {
            return ultHwConfig.csrCreatePreemptionReturnValue;
        }
    }

    struct WaitUserFenceResult {
        uint32_t called = 0u;
        uint32_t waitValue = 0u;
        int returnValue = 0;
        bool callParent = true;
    };

    WaitUserFenceResult waitUserFenceResult;

    int waitUserFence(uint32_t waitValue) override {
        waitUserFenceResult.called++;
        waitUserFenceResult.waitValue = waitValue;

        if (waitUserFenceResult.callParent) {
            return DrmCommandStreamReceiver<GfxFamily>::waitUserFence(waitValue);
        } else {
            return waitUserFenceResult.returnValue;
        }
    }

    int flushInternal(const BatchBuffer &batchBuffer, const ResidencyContainer &allocationsForResidency) override {
        if (callHwFlush) {
            return DrmCommandStreamReceiver<GfxFamily>::flushInternal(batchBuffer, allocationsForResidency);
        }
        return 0;
    }

    void readBackAllocation(void *source) override {
        latestReadBackAddress = source;
        DrmCommandStreamReceiver<GfxFamily>::readBackAllocation(source);
    }

    void *latestReadBackAddress = nullptr;
    bool callHwFlush = true;
};

template <typename GfxFamily>
class TestedDrmCommandStreamReceiverWithFailingExec : public TestedDrmCommandStreamReceiver<GfxFamily> {
  public:
    TestedDrmCommandStreamReceiverWithFailingExec(gemCloseWorkerMode mode,
                                                  ExecutionEnvironment &executionEnvironment,
                                                  const DeviceBitfield deviceBitfield)
        : TestedDrmCommandStreamReceiver<GfxFamily>(mode,
                                                    executionEnvironment,
                                                    deviceBitfield) {
    }
    TestedDrmCommandStreamReceiverWithFailingExec(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield)
        : TestedDrmCommandStreamReceiver<GfxFamily>(executionEnvironment,
                                                    rootDeviceIndex,
                                                    deviceBitfield) {
    }

    int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) override {
        return -1;
    }
};
