/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

class OsContext;

template <typename GfxFamily, typename Dispatcher>
class OsAgnosticDirectSubmission : public DirectSubmissionHw<GfxFamily, Dispatcher> {
  public:
    OsAgnosticDirectSubmission(const DirectSubmissionInputParams &inputParams);

    ~OsAgnosticDirectSubmission() override;

  protected:
    bool allocateOsResources() override;
    bool submit(uint64_t gpuAddress, size_t size, const ResidencyContainer *allocationsForResidency) override;

    bool handleResidency(const ResidencyContainer *allocationsForResidency) override;
    void ensureRingCompletion() override;
    void handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) override;
    uint64_t updateTagValue(bool requireMonitorFence) override;
    bool dispatchMonitorFenceRequired(bool requireMonitorFence) override;
    void getTagAddressValue(TagData &tagData) override;
    void getTagAddressValueForRingSwitch(TagData &tagData) override;
    bool isCompleted(uint32_t ringBufferIndex) override;
    void dispatchStopRingBufferSection() override;
    size_t dispatchStopRingBufferSectionSize() override;
    TaskCountType *getCompletionValuePointer() override;
    void unblockGpu() override;

    TagData currentTagData{};
    TaskCountType completionFenceValue{};
    OsContext *osContext;
};
} // namespace NEO
