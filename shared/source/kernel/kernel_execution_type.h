/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
enum class KernelExecutionType {
    Default = 0x0u,
    Concurrent = 0x1u,
    NotApplicable = 0x2u
};
} // namespace NEO
