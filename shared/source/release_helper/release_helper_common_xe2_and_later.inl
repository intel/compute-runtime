/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {
template <>
bool ReleaseHelperHw<release>::shouldAdjustDepth() const {
    return true;
}
} // namespace NEO
