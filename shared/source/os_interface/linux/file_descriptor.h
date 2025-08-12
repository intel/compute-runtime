/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

namespace NEO {

class FileDescriptor : NonCopyableAndNonMovableClass {
  public:
    FileDescriptor(const char *file, int flags) : handle(SysCalls::open(file, flags)) {}

    ~FileDescriptor() {
        if (handle >= 0) {
            [[maybe_unused]] auto retVal = SysCalls::close(handle);
            DEBUG_BREAK_IF(retVal != 0);
        }
    }
    operator int() const {
        return handle;
    }

  protected:
    const int handle;
};

static_assert(NEO::NonCopyableAndNonMovable<FileDescriptor>);

} // namespace NEO
