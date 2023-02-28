/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchDisablePrefetcher(bool disable) {

    if (isDisablePrefetcherRequired) {
        EncodeDummyBlitWaArgs waArgs{false};
        EncodeMiArbCheck<GfxFamily>::programWithWa(ringCommandStream, disable, waArgs);
    }
}

} // namespace NEO
