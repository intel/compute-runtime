/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "os_inc.h"

#include <string>

namespace NEO {
inline std::string joinPath(const std::string &lhs, const std::string &rhs) {
    if (lhs.size() == 0) {
        return rhs;
    }

    if (rhs.size() == 0) {
        return lhs;
    }

    if (*lhs.rbegin() == PATH_SEPARATOR) {
        return lhs + rhs;
    }

    return lhs + PATH_SEPARATOR + rhs;
}

inline std::string addSuffixBeforeExtension(const std::string &fileName, const std::string &suffix) {
    auto suffixPosition = fileName.find_last_of("./\\");
    if (suffixPosition == std::string::npos || fileName[suffixPosition] != '.') {
        suffixPosition = fileName.size();
    }
    return std::string(fileName).insert(suffixPosition, suffix);
}

bool pathExists(const std::string &path);
} // namespace NEO
