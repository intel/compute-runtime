/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace aub_stream {
namespace mode {
constexpr uint32_t aubFile = 0;
constexpr uint32_t tbx = 1;
constexpr uint32_t aubFileAndTbx = 2;
} // namespace mode
} // namespace aub_stream
