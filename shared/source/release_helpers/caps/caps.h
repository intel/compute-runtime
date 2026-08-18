/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct Caps {
    bool isDotProductAccumulateSystolicSupported = false;

    constexpr bool operator==(const Caps &) const = default;
};

} // namespace NEO
