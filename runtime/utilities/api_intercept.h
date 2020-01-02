/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#define SYSTEM_ENTER()
#define SYSTEM_LEAVE(id)
#define WAIT_ENTER()
#define WAIT_LEAVE()

#if KMD_PROFILING == 1
#undef SYSTEM_ENTER
#undef SYSTEM_LEAVE
#undef WAIT_ENTER
#undef WAIT_LEAVE

#define SYSTEM_ENTER()      \
    PerfProfiler::create(); \
    gPerfProfiler->systemEnter();

#define SYSTEM_LEAVE(id) \
    gPerfProfiler->systemLeave(id);
#define WAIT_ENTER() \
    SYSTEM_ENTER()
#define WAIT_LEAVE() \
    SYSTEM_LEAVE(0)
#endif
