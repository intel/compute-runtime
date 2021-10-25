/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"

namespace NEO {
namespace SysCalls {
BOOL closeHandle(HANDLE hObject);
}
} // namespace NEO
