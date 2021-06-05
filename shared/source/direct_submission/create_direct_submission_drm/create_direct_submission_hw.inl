/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/linux/drm_direct_submission.h"

namespace NEO {
template <typename GfxFamily, typename Dispatcher>
inline std::unique_ptr<DirectSubmissionHw<GfxFamily, Dispatcher>> DirectSubmissionHw<GfxFamily, Dispatcher>::create(Device &device, OsContext &osContext) {
    return std::make_unique<DrmDirectSubmission<GfxFamily, Dispatcher>>(device, osContext);
}
} // namespace NEO
