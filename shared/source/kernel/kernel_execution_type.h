/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
enum class KernelExecutionType {
    defaultType = 0x0u,
    concurrent = 0x1u,
    notApplicable = 0x2u
};
} // namespace NEO
