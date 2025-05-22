/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.inl"
#include "shared/source/direct_submission/direct_submission_prefetch_mitigation_xe_hp_core_and_later.inl"
#include "shared/source/direct_submission/direct_submission_prefetcher_pvc_and_later.inl"
#include "shared/source/direct_submission/direct_submission_relaxed_ordering.inl"
#include "shared/source/direct_submission/direct_submission_xe_hp_core_and_later.inl"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.inl"
#include "shared/source/direct_submission/linux/drm_direct_submission.inl"
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {
using GfxFamily = Xe3CoreFamily;

template class Dispatcher<GfxFamily>;
template class BlitterDispatcher<GfxFamily>;
template class RenderDispatcher<GfxFamily>;

template class DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>;

template class DrmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class DrmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>;
} // namespace NEO
