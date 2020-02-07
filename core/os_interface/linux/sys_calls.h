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
} // namespace SysCalls
} // namespace NEO
