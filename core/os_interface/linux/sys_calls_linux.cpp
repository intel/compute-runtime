/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/sys_calls.h"

#include <unistd.h>

namespace NEO {
namespace SysCalls {
int close(int fileDescriptor) {
    return ::close(fileDescriptor);
}
} // namespace SysCalls
} // namespace NEO
