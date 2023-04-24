/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

namespace SysCalls {

unsigned int getProcessId();

unsigned long getNumThreads();

void exit(int code);

} // namespace SysCalls

} // namespace NEO
