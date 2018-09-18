/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#define PATH_SEPARATOR '/'
#define __cdecl
namespace OCLRT {
class PerfProfiler;
extern thread_local PerfProfiler *gPerfProfiler;
}; // namespace OCLRT
