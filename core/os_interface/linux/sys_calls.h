/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
namespace SysCalls {
int close(int fileDescriptor);
int open(const char *file, int flags);
int ioctl(int fileDescriptor, unsigned long int request, void *arg);
} // namespace SysCalls
} // namespace NEO
