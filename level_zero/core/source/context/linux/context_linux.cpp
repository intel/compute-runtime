/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/core/source/context/context.h"

#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <sys/resource.h>

namespace L0 {

constexpr rlim_t maxOpaqueHandlePreallocation = 4096;

void Context::initOpaqueHandleResourcesImpl() {
    struct rlimit rlim;
    if (NEO::SysCalls::getrlimit(RLIMIT_NOFILE, &rlim)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "getrlimit RLIMIT_NOFILE failed, no preallocation of fds for opaque ipc handles.\n");
        return;
    }

    // Use the minimum of ulimit and a reasonable maximum
    rlim_t availableFDs = std::min(rlim.rlim_cur / 10, maxOpaqueHandlePreallocation);
    int32_t overrideCount = NEO::debugManager.flags.IpcFdPreallocationCount.get();
    if (overrideCount >= 1 && static_cast<rlim_t>(overrideCount) <= rlim.rlim_cur) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "IpcFdPreallocationCount override active: using %d fds (ulimit: %lld)\n",
                     overrideCount, static_cast<long long>(rlim.rlim_cur));
        availableFDs = static_cast<rlim_t>(overrideCount);
    } else {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Using default FD preallocation: %lld fds (ulimit: %lld, max: %lld)\n",
                     static_cast<long long>(availableFDs), static_cast<long long>(rlim.rlim_cur),
                     static_cast<long long>(maxOpaqueHandlePreallocation));
    }
    auto tempFds = std::make_unique<int[]>(availableFDs);

    rlim_t i;
    for (i = 0; i < availableFDs; ++i) {
        tempFds[i] = NEO::SysCalls::open("/dev/null", O_RDONLY);
        if (tempFds[i] < 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "open() failed at fd index %lld, using partial preallocation\n", static_cast<long long>(i));
            break;
        }
    }
    availableFDs = i;

    for (i = 0; i < availableFDs; ++i) {
        NEO::SysCalls::close(tempFds[i]);
    }

    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                 "preallocation of fds for opaque ipc handles completed with %lld fds\n", static_cast<long long>(availableFDs));
}

} // namespace L0
