/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.inl"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.inl"
#include "shared/source/direct_submission/linux/drm_direct_submission.inl"
#include "shared/source/helpers/hw_cmds.h"

namespace NEO {
template class BlitterDispatcher<SKLFamily>;
template class RenderDispatcher<SKLFamily>;

template class DirectSubmissionHw<SKLFamily>;
template class DrmDirectSubmission<SKLFamily>;
} // namespace NEO
