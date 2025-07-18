/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
extern volatile bool wasPlatformTeardownCalled;

void globalPlatformTeardown(bool processTermination);
void globalPlatformSetup();
} // namespace NEO
