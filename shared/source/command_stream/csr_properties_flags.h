/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

namespace AdditionalKernelExecInfo {
inline constexpr uint32_t disableOverdispatch = 0u;
inline constexpr uint32_t notSet = 1u;
inline constexpr uint32_t notApplicable = 2u;
} // namespace AdditionalKernelExecInfo

} // namespace NEO
