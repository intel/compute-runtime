/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/os_interface/windows/windows_defs.h"

struct COMMAND_BUFFER_HEADER_REC; // NOLINT(readability-identifier-naming), forward declaration from sharedata_wrapper.h
typedef struct COMMAND_BUFFER_HEADER_REC COMMAND_BUFFER_HEADER;

namespace NEO {

class OsContextWin;
class Wddm;

template <typename GfxFamily, typename Dispatcher>
class WddmDirectSubmission : public DirectSubmissionHw<GfxFamily, Dispatcher> {
  public:
    WddmDirectSubmission(const DirectSubmissionInputParams &inputParams);

    ~WddmDirectSubmission() override;

    void flushMonitorFence(bool notifyKmd) override;
    void unblockPagingFenceSemaphore(uint64_t pagingFenceValue) override;

  protected:
    bool allocateOsResources() override;
    bool submit(uint64_t gpuAddress, size_t size, const ResidencyContainer *allocationsForResidency) override;

    bool handleResidency() override;
    void handleCompletionFence(uint64_t completionValue, MonitoredFence &fence);
    void ensureRingCompletion() override;
    void handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) override;
    void handleStopRingBuffer() override;
    uint64_t updateTagValue(bool requireMonitorFence) override;
    bool dispatchMonitorFenceRequired(bool requireMonitorFence) override;
    MOCKABLE_VIRTUAL uint64_t updateTagValueImpl(uint32_t completionBufferIndex);
    MOCKABLE_VIRTUAL void updateTagValueImplForSwitchRingBuffer(uint32_t completionBufferIndex);
    void getTagAddressValue(TagData &tagData) override;
    void getTagAddressValueForRingSwitch(TagData &tagData) override;
    bool isCompleted(uint32_t ringBufferIndex) override;
    MOCKABLE_VIRTUAL void updateMonitorFenceValueForResidencyList(ResidencyContainer *allocationsForResidency);
    void makeGlobalFenceAlwaysResident() override;
    void dispatchStopRingBufferSection() override;
    size_t dispatchStopRingBufferSectionSize() override;

    TagData ringBufferEndCompletionTagData{};
    OsContextWin *osContextWin;
    Wddm *wddm;
    MonitoredFence ringFence;
    std::unique_ptr<COMMAND_BUFFER_HEADER> commandBufferHeader;
};
} // namespace NEO
