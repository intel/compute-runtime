/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/direct_submission/direct_submission_hw.inl"
#include "core/direct_submission/dispatchers/blitter_dispatcher.inl"
#include "core/direct_submission/dispatchers/render_dispatcher.inl"
#include "core/direct_submission/windows/wddm_direct_submission.inl"
#include "core/helpers/hw_cmds.h"

namespace NEO {
template class BlitterDispatcher<BDWFamily>;
template class RenderDispatcher<BDWFamily>;

template class DirectSubmissionHw<BDWFamily>;
template class WddmDirectSubmission<BDWFamily>;
} // namespace NEO
