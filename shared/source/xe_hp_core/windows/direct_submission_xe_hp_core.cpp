/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/dispatcher.inl"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.inl"
#include "shared/source/direct_submission/windows/wddm_direct_submission.inl"

#include "hw_cmds.h"

namespace NEO {
using GfxFamily = XeHpFamily;

template class WddmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>>;
template class WddmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>;
} // namespace NEO
