/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/windows_wrapper.h"
namespace NEO {

namespace SysCalls {

MMRESULT timeBeginPeriod(UINT period);
MMRESULT timeEndPeriod(UINT period);
} // namespace SysCalls
} // namespace NEO
