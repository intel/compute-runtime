/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/direct_submission/direct_submission_hw_diagnostic_mode.h"

namespace NEO {

struct MockDirectSubmissionDiagnosticsCollector : public DirectSubmissionDiagnosticsCollector {
    using BaseClass = DirectSubmissionDiagnosticsCollector;
    using BaseClass::DirectSubmissionDiagnosticsCollector;
    using BaseClass::executionList;
};

} // namespace NEO
