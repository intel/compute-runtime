/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

class OsContextWin;
class Wddm;

template <typename GfxFamily>
class DrmDirectSubmission : public DirectSubmissionHw<GfxFamily> {
  public:
    using DirectSubmissionHw<GfxFamily>::ringCommandStream;
    using DirectSubmissionHw<GfxFamily>::switchRingBuffersAllocations;

    DrmDirectSubmission(Device &device,
                        std::unique_ptr<Dispatcher> cmdDispatcher,
                        OsContext &osContext);

  protected:
    bool allocateOsResources(DirectSubmissionAllocations &allocations) override;
    bool submit(uint64_t gpuAddress, size_t size) override;

    bool handleResidency() override;
    uint64_t switchRingBuffers() override;
    uint64_t updateTagValue() override;
    void getTagAddressValue(TagData &tagData) override;
};
} // namespace NEO
