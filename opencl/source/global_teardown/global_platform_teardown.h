/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
extern volatile bool wasPlatformTeardownCalled;

void globalPlatformTeardown();
void globalPlatformSetup();
} // namespace NEO
