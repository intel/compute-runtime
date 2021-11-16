/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.inl"
#include "shared/source/direct_submission/direct_submission_prefetch_mitigation_xe_hp_core_and_later.inl"
#include "shared/source/direct_submission/direct_submission_prefetcher_pvc_and_later.inl"
#include "shared/source/direct_submission/direct_submission_xe_hp_core_and_later.inl"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.inl"
#include "shared/source/direct_submission/windows/wddm_direct_submission.inl"

#include "hw_cmds.h"

namespace NEO {
using GfxFamily = XE_HPC_COREFamily;

template class Dispatcher<GfxFamily>;
template class BlitterDispatcher<GfxFamily>;
template class RenderDispatcher<GfxFamily>;

template class DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>;

template class WddmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class WddmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>;
} // namespace NEO
