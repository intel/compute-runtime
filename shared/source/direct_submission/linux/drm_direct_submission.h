/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
class DrmDirectSubmission : public DirectSubmissionHw<GfxFamily, Dispatcher> {
  public:
    using DirectSubmissionHw<GfxFamily, Dispatcher>::ringCommandStream;
    using DirectSubmissionHw<GfxFamily, Dispatcher>::switchRingBuffersAllocations;

    DrmDirectSubmission(const DirectSubmissionInputParams &inputParams);

    ~DrmDirectSubmission() override;

    TaskCountType *getCompletionValuePointer() override;

  protected:
    bool allocateOsResources() override;
    bool submit(uint64_t gpuAddress, size_t size) override;

    bool handleResidency() override;
    void handleStopRingBuffer() override;

    void ensureRingCompletion() override;
    void handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) override;
    uint64_t updateTagValue(bool requireMonitorFence) override;
    void getTagAddressValue(TagData &tagData) override;
    bool isCompleted(uint32_t ringBufferIndex) override;
    bool isCompletionFenceSupported();

    MOCKABLE_VIRTUAL void wait(TaskCountType taskCountToWait);

    TagData currentTagData{};
    volatile TagAddressType *tagAddress;
    TaskCountType completionFenceValue{};
};
} // namespace NEO
