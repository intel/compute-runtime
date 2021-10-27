/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace NEO {
class Drm;
class LocalMemoryHelper;

extern LocalMemoryHelper *localMemoryHelperFactory[IGFX_MAX_PRODUCT];

class LocalMemoryHelper {
  public:
    static LocalMemoryHelper *get(PRODUCT_FAMILY product) {
        auto localMemHelper = localMemoryHelperFactory[product];
        if (!localMemHelper) {
            return localMemoryHelperFactory[IGFX_DG1];
        }
        return localMemHelper;
    }
    static uint32_t ioctl(Drm *drm, unsigned long request, void *arg);
    virtual uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) = 0;
    virtual std::unique_ptr<uint8_t[]> translateIfRequired(uint8_t *dataQuery, int32_t length) = 0;
};

template <PRODUCT_FAMILY gfxProduct>
class LocalMemoryHelperImpl : public LocalMemoryHelper {
  public:
    static LocalMemoryHelper *get() {
        static LocalMemoryHelperImpl<gfxProduct> instance;
        return &instance;
    }
    uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) override;
    std::unique_ptr<uint8_t[]> translateIfRequired(uint8_t *dataQuery, int32_t length) override;
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableProductLocalMemoryHelper {
    EnableProductLocalMemoryHelper() {
        LocalMemoryHelper *plocalMemHelper = LocalMemoryHelperImpl<gfxProduct>::get();
        localMemoryHelperFactory[gfxProduct] = plocalMemHelper;
    }
};
} // namespace NEO
