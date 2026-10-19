/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <regex>
#include <string>

inline bool containsRegex(const std::string &str, const std::string &regex) {
    return std::regex_search(str, std::regex(regex));
}
