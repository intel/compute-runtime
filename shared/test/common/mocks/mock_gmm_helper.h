/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_helper.h"

namespace NEO {
struct MockGmmHelper : GmmHelper {
    using GmmHelper::addressWidth;
    using GmmHelper::l3CacheForDebugDisabled;
};
} // namespace NEO
