/*
 * Copyright (C) 2020 Intel Corporation
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

    DrmDirectSubmission(Device &device,
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
