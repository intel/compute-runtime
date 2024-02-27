/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchDisablePrefetcher(bool disable) {

    if (isDisablePrefetcherRequired) {
        EncodeMiArbCheck<GfxFamily>::program(ringCommandStream, disable);
    }
}

} // namespace NEO
