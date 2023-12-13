/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/direct_submission/windows/wddm_direct_submission.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {
template <typename GfxFamily, typename Dispatcher>
inline std::unique_ptr<DirectSubmissionHw<GfxFamily, Dispatcher>> DirectSubmissionHw<GfxFamily, Dispatcher>::create(const DirectSubmissionInputParams &inputParams) {
    if (inputParams.rootDeviceEnvironment.osInterface->getDriverModel()->getDriverModelType() == DriverModelType::drm) {
        return std::make_unique<DrmDirectSubmission<GfxFamily, Dispatcher>>(inputParams);
    } else {
        return std::make_unique<WddmDirectSubmission<GfxFamily, Dispatcher>>(inputParams);
    }
}
} // namespace NEO
