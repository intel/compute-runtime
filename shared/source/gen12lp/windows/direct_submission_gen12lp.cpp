/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.inl"
#include "shared/source/direct_submission/direct_submission_prefetch_mitigation_base.inl"
#include "shared/source/direct_submission/direct_submission_prefetcher_base.inl"
#include "shared/source/direct_submission/direct_submission_relaxed_ordering.inl"
#include "shared/source/direct_submission/direct_submission_tgllp_and_later.inl"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.inl"
#include "shared/source/direct_submission/windows/wddm_direct_submission.inl"
#include "shared/source/gen12lp/hw_cmds_base.h"

namespace NEO {
using GfxFamily = Gen12LpFamily;

template class Dispatcher<GfxFamily>;
template class BlitterDispatcher<GfxFamily>;
template class RenderDispatcher<GfxFamily>;

template class DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>;

template class WddmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class WddmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>;
} // namespace NEO
