/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/ult_hw_config.h"

#include "opencl/source/os_interface/linux/drm_command_stream.h"

using namespace NEO;

template <typename GfxFamily>
class TestedDrmCommandStreamReceiver : public DrmCommandStreamReceiver<GfxFamily> {
  public:
    using CommandStreamReceiver::clearColorAllocation;
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::createPreemptionAllocation;
    using CommandStreamReceiver::flushStamp;
    using CommandStreamReceiver::getTagAddress;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::makeResident;
    using CommandStreamReceiver::taskCount;
    using CommandStreamReceiver::useGpuIdleImplicitFlush;
    using CommandStreamReceiver::useNewResourceImplicitFlush;
    using CommandStreamReceiver::useNotifyEnableForPostSync;
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
};
