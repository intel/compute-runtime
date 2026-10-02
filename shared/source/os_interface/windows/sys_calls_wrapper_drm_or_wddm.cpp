/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/windows/sys_calls_wrapper.h"

namespace NEO {
namespace SysCalls {
BOOL closeHandle(HANDLE hObject) {
    if (hObject == nullptr) {
        return TRUE;
    }

    return close(static_cast<int>(castToUint64(hObject))) == 0 ? TRUE : FALSE;
}
} // namespace SysCalls
} // namespace NEO
