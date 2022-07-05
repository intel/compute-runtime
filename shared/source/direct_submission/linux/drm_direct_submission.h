/*
 * Copyright (C) 2020-2022 Intel Corporation
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

    uint32_t *getCompletionValuePointer() override;

  protected:
    bool allocateOsResources() override;
    bool submit(uint64_t gpuAddress, size_t size) override;

    bool handleResidency() override;
    bool isNewResourceHandleNeeded();
    void handleNewResourcesSubmission() override;
    size_t getSizeNewResourceHandler() override;
    void handleStopRingBuffer() override;
    void handleSwitchRingBuffers() override;
    uint64_t updateTagValue() override;
    void getTagAddressValue(TagData &tagData) override;
    bool isCompleted(uint32_t ringBufferIndex) override;
    bool isCompletionFenceSupported();

    MOCKABLE_VIRTUAL void wait(uint32_t taskCountToWait);

    TagData currentTagData{};
    volatile uint32_t *tagAddress;
    uint32_t completionFenceValue{};
};
} // namespace NEO
