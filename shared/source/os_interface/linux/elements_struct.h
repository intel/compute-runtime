/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {

struct ElementsStruct {
    time_t lastAccessTime;
    size_t fileSize;
    std::string path;
};

} // namespace NEO
