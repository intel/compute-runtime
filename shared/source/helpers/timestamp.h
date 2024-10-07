/*
 * Copyright (C) 2024 Intel Corporation
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

    ss << "[" << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S") << "] ";

    return ss.str();
}
} // namespace TimestampHelper
