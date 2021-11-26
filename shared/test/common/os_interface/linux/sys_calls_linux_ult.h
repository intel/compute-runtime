/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <iostream>

namespace NEO {
namespace SysCalls {

extern int (*sysCallsOpen)(const char *pathname, int flags);
extern ssize_t (*sysCallsPread)(int fd, void *buf, size_t count, off_t offset);
extern int (*sysCallsReadlink)(const char *path, char *buf, size_t bufsize);

} // namespace SysCalls
} // namespace NEO
