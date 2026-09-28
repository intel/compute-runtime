/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"

#include "opencl/source/sharings/unified/unified_sharing.h"

namespace NEO {

void UnifiedSharing::consumeImportedHandle(const UnifiedSharingMemoryDescription &description) {
    if (description.type == UnifiedSharingHandleType::linuxFd) {
        SysCalls::close(static_cast<int>(reinterpret_cast<uintptr_t>(description.handle)));
    }
}

} // namespace NEO
