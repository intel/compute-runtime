/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace NEO {
namespace SysCalls {
int close(int fileDescriptor) {
    return ::close(fileDescriptor);
}
int open(const char *file, int flags) {
    return ::open(file, flags);
}
int ioctl(int fileDescriptor, unsigned long int request, void *arg) {
    return ::ioctl(fileDescriptor, request, arg);
}
} // namespace SysCalls
} // namespace NEO
