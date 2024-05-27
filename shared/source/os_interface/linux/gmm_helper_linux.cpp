/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"

namespace NEO {
bool GmmHelper::deferMOCSToPatIndex() const {
    return false;
}
} // namespace NEO