/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm.h"

#include <cstddef>

namespace NEO {

class GmmHandleAllocator {
  public:
    virtual ~GmmHandleAllocator() = default;

    virtual void *createHandle(const GMM_RESOURCE_INFO *gmmResourceInfo) {
        return nullptr;
    }
    virtual void destroyHandle(void *handle) {
    }
    virtual size_t getHandleSize() {
        return 0;
    }
};

} // namespace NEO