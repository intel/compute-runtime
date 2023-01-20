/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_bdw_and_later.inl"
#include "shared/source/direct_submission/direct_submission_hw.inl"
#include "shared/source/direct_submission/direct_submission_prefetch_mitigation_base.inl"
#include "shared/source/direct_submission/direct_submission_prefetcher_base.inl"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.inl"
#include "shared/source/direct_submission/linux/drm_direct_submission.inl"
#include "shared/source/gen12lp/hw_cmds.h"

namespace NEO {
using GfxFamily = Gen12LpFamily;

template class Dispatcher<GfxFamily>;
template class BlitterDispatcher<GfxFamily>;
template class RenderDispatcher<GfxFamily>;

template class DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>;

template class DrmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>;
} // namespace NEO
