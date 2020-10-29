/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/os_interface/linux/drm_command_stream.h"

using namespace NEO;

template <typename GfxFamily>
class TestedDrmCommandStreamReceiver : public DrmCommandStreamReceiver<GfxFamily> {
  public:
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::makeResident;
    using CommandStreamReceiver::useGpuIdleImplicitFlush;
    using CommandStreamReceiver::useNewResourceImplicitFlush;
    using DrmCommandStreamReceiver<GfxFamily>::residency;
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
};
