/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls_wrapper.h"

namespace NEO {
namespace SysCalls {
BOOL closeHandle(HANDLE hObject) {
    return TRUE;
}
} // namespace SysCalls
} // namespace NEO