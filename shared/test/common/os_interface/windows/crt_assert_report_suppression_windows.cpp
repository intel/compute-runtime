/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/crt_assert_report_suppression.h"

#include <crtdbg.h>

CrtAssertReportSuppression::CrtAssertReportSuppression() : previousReportMode(_CrtSetReportMode(_CRT_ASSERT, 0)) {}

CrtAssertReportSuppression::~CrtAssertReportSuppression() {
    _CrtSetReportMode(_CRT_ASSERT, previousReportMode);
}
