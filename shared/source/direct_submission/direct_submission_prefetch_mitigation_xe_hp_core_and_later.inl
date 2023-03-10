/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchPrefetchMitigation() {
    dispatchStartSection(ringCommandStream.getCurrentGpuAddressPosition() + getSizeStartSection());
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizePrefetchMitigation() {
    return getSizeStartSection();
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeDisablePrefetcher() {
    EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&this->rootDeviceEnvironment)};
    return EncodeMiArbCheck<GfxFamily>::getCommandSizeWithWa(waArgs);
}

} // namespace NEO