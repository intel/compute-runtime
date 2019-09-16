/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <algorithm>

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

    bool isResident(BufferObject *bo) const {
        return std::find(this->residency.begin(), this->residency.end(), bo) != this->residency.end();
    }

    void makeNonResident(GraphicsAllocation &gfxAllocation) override {
        makeNonResidentResult.called = true;
        makeNonResidentResult.allocation = &gfxAllocation;
        DrmCommandStreamReceiver<GfxFamily>::makeNonResident(gfxAllocation);
    }

    const BufferObject *getResident(BufferObject *bo) const {
        return this->isResident(bo) ? bo : nullptr;
    }

    struct MakeResidentNonResidentResult {
        bool called;
        GraphicsAllocation *allocation;
    };

    MakeResidentNonResidentResult makeNonResidentResult;
    std::vector<BufferObject *> *getResidencyVector() { return &this->residency; }

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
