/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.inl"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.inl"

#include "hw_cmds.h"

namespace NEO {
using GfxFamily = XeHpFamily;

template class Dispatcher<GfxFamily>;
template class BlitterDispatcher<GfxFamily>;
template class RenderDispatcher<GfxFamily>;

template <>
void DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>::dispatchPrefetchMitigation() {
    auto addressToJump = ptrOffset(ringCommandStream.getSpace(0u), getSizeStartSection());
    dispatchStartSection(getCommandBufferPositionGpuAddress(addressToJump));
}

template <>
size_t DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>::getSizePrefetchMitigation() {
    return getSizeStartSection();
}

template <>
void DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>::dispatchDisablePrefetcher(bool disable) {
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;

    MI_ARB_CHECK arbCheck = GfxFamily::cmdInitArbCheck;
    arbCheck.setPreFetchDisable(disable);
    MI_ARB_CHECK *arbCheckSpace = ringCommandStream.getSpaceForCmd<MI_ARB_CHECK>();
    *arbCheckSpace = arbCheck;
}

template <>
size_t DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>::getSizeDisablePrefetcher() {
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;
    return sizeof(MI_ARB_CHECK);
}

template <>
void DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>::dispatchPrefetchMitigation() {
    auto addressToJump = ptrOffset(ringCommandStream.getSpace(0u), getSizeStartSection());
    dispatchStartSection(getCommandBufferPositionGpuAddress(addressToJump));
}

template <>
size_t DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>::getSizePrefetchMitigation() {
    return getSizeStartSection();
}

template <>
void DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>::dispatchDisablePrefetcher(bool disable) {
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;

    MI_ARB_CHECK arbCheck = GfxFamily::cmdInitArbCheck;
    arbCheck.setPreFetchDisable(disable);
    MI_ARB_CHECK *arbCheckSpace = ringCommandStream.getSpaceForCmd<MI_ARB_CHECK>();
    *arbCheckSpace = arbCheck;
}

template <>
size_t DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>::getSizeDisablePrefetcher() {
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;
    return sizeof(MI_ARB_CHECK);
}

template class DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>;

} // namespace NEO
