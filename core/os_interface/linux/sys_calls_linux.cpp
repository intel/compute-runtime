/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/sys_calls.h"

#include <fcntl.h>
#include <unistd.h>

namespace NEO {
namespace SysCalls {
int close(int fileDescriptor) {
    return ::close(fileDescriptor);
}
int open(const char *file, int flags) {
    return ::open(file, flags);
}
} // namespace SysCalls
} // namespace NEO
