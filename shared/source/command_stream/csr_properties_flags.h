/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

namespace AdditionalKernelExecInfo {
constexpr uint32_t DisableOverdispatch = 0u;
constexpr uint32_t NotSet = 1u;
constexpr uint32_t NotApplicable = 2u;
} // namespace AdditionalKernelExecInfo

} // namespace NEO