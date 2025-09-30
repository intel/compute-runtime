/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/windows/gmm_memory.h"

namespace NEO {
GmmMemory *GmmMemory::create(GmmClientContext *gmmClientContext) {
    return new GmmMemory(gmmClientContext);
}
} // namespace NEO
