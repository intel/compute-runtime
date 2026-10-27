/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/os_agnostic_direct_submission.h"

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
struct MockOsAgnosticDirectSubmission : public OsAgnosticDirectSubmission<GfxFamily, Dispatcher> {
    using BaseClass = OsAgnosticDirectSubmission<GfxFamily, Dispatcher>;
    using BaseClass::currentRingBuffer;
    using BaseClass::getSizeSwitchRingBufferSection;
    using BaseClass::OsAgnosticDirectSubmission;
    using BaseClass::ringBuffers;
    using BaseClass::ringCommandStream;
};
} // namespace NEO
