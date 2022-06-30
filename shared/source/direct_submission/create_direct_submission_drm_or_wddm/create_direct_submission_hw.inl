/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/direct_submission/windows/wddm_direct_submission.h"

namespace NEO {
template <typename GfxFamily, typename Dispatcher>
inline std::unique_ptr<DirectSubmissionHw<GfxFamily, Dispatcher>> DirectSubmissionHw<GfxFamily, Dispatcher>::create(const DirectSubmissionInputParams &inputParams) {
    if (inputParams.rootDeviceEnvironment.osInterface->getDriverModel()->getDriverModelType() == DriverModelType::DRM) {
        return std::make_unique<DrmDirectSubmission<GfxFamily, Dispatcher>>(inputParams);
    } else {
        return std::make_unique<WddmDirectSubmission<GfxFamily, Dispatcher>>(inputParams);
    }
}
} // namespace NEO
