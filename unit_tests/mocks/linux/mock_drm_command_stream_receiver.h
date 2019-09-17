/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

template <typename GfxFamily>
class TestedDrmCommandStreamReceiver : public DrmCommandStreamReceiver<GfxFamily> {
  public:
    using CommandStreamReceiver::commandStream;
    using DrmCommandStreamReceiver<GfxFamily>::makeResidentBufferObjects;
    using DrmCommandStreamReceiver<GfxFamily>::residency;

    TestedDrmCommandStreamReceiver(gemCloseWorkerMode mode, ExecutionEnvironment &executionEnvironment)
        : DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, mode) {
    }
    TestedDrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment)
        : DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, gemCloseWorkerMode::gemCloseWorkerInactive) {
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
