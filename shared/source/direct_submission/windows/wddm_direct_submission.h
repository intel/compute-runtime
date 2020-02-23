/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/os_interface/windows/windows_defs.h"

struct COMMAND_BUFFER_HEADER_REC;

namespace NEO {

class OsContextWin;
class Wddm;

template <typename GfxFamily>
class WddmDirectSubmission : public DirectSubmissionHw<GfxFamily> {
  public:
    WddmDirectSubmission(Device &device,
                         std::unique_ptr<Dispatcher> cmdDispatcher,
                         OsContext &osContext);

    ~WddmDirectSubmission();

  protected:
    bool allocateOsResources(DirectSubmissionAllocations &allocations) override;
    bool submit(uint64_t gpuAddress, size_t size) override;

    bool handleResidency() override;
    void handleCompletionRingBuffer(uint64_t completionValue, MonitoredFence &fence);
    uint64_t switchRingBuffers() override;
    uint64_t updateTagValue() override;
    void getTagAddressValue(TagData &tagData) override;

    OsContextWin *osContextWin;
    Wddm *wddm;
    MonitoredFence ringFence;
    std::unique_ptr<COMMAND_BUFFER_HEADER_REC> commandBufferHeader;
};
} // namespace NEO
