/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace TimestampHelper {
static inline std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    tm timeInfo = *std::localtime(&nowTime);

    std::stringstream ss;

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", &timeInfo);
    ss << buffer;

    return ss.str();
}
} // namespace TimestampHelper
