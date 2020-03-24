/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
class ClDevice;

namespace TestChecks {
bool supportsSvm(const ClDevice *pClDevice);
} // namespace TestChecks

} // namespace NEO

#include "shared/test/unit_test/test_macros/test_checks.h"
