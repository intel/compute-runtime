/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {

namespace SysCalls {

unsigned int getProcessId();
unsigned int getCurrentProcessId();

unsigned long getNumThreads();

void exit(int code);

bool isShutdownInProgress();
} // namespace SysCalls

} // namespace NEO
