/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "direct_submission/direct_submission_hw.inl"
#include "direct_submission/dispatchers/blitter_dispatcher.inl"
#include "direct_submission/dispatchers/render_dispatcher.inl"
#include "direct_submission/linux/drm_direct_submission.inl"
#include "helpers/hw_cmds.h"

namespace NEO {
template class BlitterDispatcher<TGLLPFamily>;
template class RenderDispatcher<TGLLPFamily>;

template class DirectSubmissionHw<TGLLPFamily>;
template class DrmDirectSubmission<TGLLPFamily>;
} // namespace NEO
