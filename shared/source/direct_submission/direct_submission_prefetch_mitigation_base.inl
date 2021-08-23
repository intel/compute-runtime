/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchPrefetchMitigation() {
    uint32_t *prefetchNoop = static_cast<uint32_t *>(ringCommandStream.getSpace(prefetchSize));
    size_t i = 0u;
    while (i < prefetchNoops) {
        *prefetchNoop = 0u;
        prefetchNoop++;
        i++;
    }
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizePrefetchMitigation() {
    return prefetchSize;
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeDisablePrefetcher() {
    return 0u;
}

} // namespace NEO