/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

namespace AdditionalKernelExecInfo {
inline constexpr uint32_t DisableOverdispatch = 0u;
inline constexpr uint32_t NotSet = 1u;
inline constexpr uint32_t NotApplicable = 2u;
} // namespace AdditionalKernelExecInfo

} // namespace NEO