/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/sys_calls.h"

#include <string>

namespace NEO {

struct ElementsStruct {
    FILETIME lastAccessTime;
    uint64_t fileSize;
    std::string path;
};

} // namespace NEO
