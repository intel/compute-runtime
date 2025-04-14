/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"

#include <chrono>

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
    bool submit(uint64_t gpuAddress, size_t size, const ResidencyContainer *allocationsForResidency) override;

    bool handleResidency() override;
    void handleRingRestartForUllsLightResidency(const ResidencyContainer *allocationsForResidency) override;
    void handleResidencyContainerForUllsLightNewRingAllocation(ResidencyContainer *allocationsForResidency) override;
    void handleStopRingBuffer() override;

    void ensureRingCompletion() override;
    void handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) override;
    void dispatchStopRingBufferSection() override;
    size_t dispatchStopRingBufferSectionSize() override;
    uint64_t updateTagValue(bool requireMonitorFence) override;
    void getTagAddressValue(TagData &tagData) override;
    void getTagAddressValueForRingSwitch(TagData &tagData) override;
    bool isCompleted(uint32_t ringBufferIndex) override;
    bool isCompletionFenceSupported();
    bool isGpuHangDetected(std::chrono::high_resolution_clock::time_point &lastHangCheckTime);
    MOCKABLE_VIRTUAL std::chrono::steady_clock::time_point getCpuTimePoint();

    MOCKABLE_VIRTUAL void wait(TaskCountType taskCountToWait);

    TagData currentTagData{};
    TaskCountType completionFenceValue{};
    std::chrono::microseconds gpuHangCheckPeriod{CommonConstants::gpuHangCheckTimeInUS};

    constexpr static size_t ullsLightTimeout = 2'000'000;
    std::chrono::steady_clock::time_point lastUllsLightExecTimestamp{};
    int boHandleForExec = 0;

    std::vector<BufferObject *> residency{};
    std::vector<ExecObject> execObjectsStorage{};
};
} // namespace NEO
