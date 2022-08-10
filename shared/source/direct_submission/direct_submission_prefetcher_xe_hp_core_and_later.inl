/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchDisablePrefetcher(bool disable) {
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;

    if (isDisablePrefetcherRequired) {
        MI_ARB_CHECK arbCheck = GfxFamily::cmdInitArbCheck;
        arbCheck.setPreFetchDisable(disable);

        EncodeMiArbCheck<GfxFamily>::adjust(arbCheck);

        MI_ARB_CHECK *arbCheckSpace = ringCommandStream.getSpaceForCmd<MI_ARB_CHECK>();
        *arbCheckSpace = arbCheck;
    }
}

} // namespace NEO
