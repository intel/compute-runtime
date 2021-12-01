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
class IoctlHelper;

extern IoctlHelper *ioctlHelperFactory[IGFX_MAX_PRODUCT];

class IoctlHelper {
  public:
    static IoctlHelper *get(PRODUCT_FAMILY product);
    static uint32_t ioctl(Drm *drm, unsigned long request, void *arg);

    virtual uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) = 0;
    virtual std::unique_ptr<uint8_t[]> translateIfRequired(uint8_t *dataQuery, int32_t length) = 0;
};

template <PRODUCT_FAMILY gfxProduct>
class IoctlHelperImpl : public IoctlHelper {
  public:
    static IoctlHelper *get() {
        static IoctlHelperImpl<gfxProduct> instance;
        return &instance;
    }
    uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) override;
    std::unique_ptr<uint8_t[]> translateIfRequired(uint8_t *dataQuery, int32_t length) override;
};

class IoctlHelperDefault : public IoctlHelper {
  public:
    static IoctlHelper *get() {
        static IoctlHelperDefault instance;
        return &instance;
    }
    uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) override;
    std::unique_ptr<uint8_t[]> translateIfRequired(uint8_t *dataQuery, int32_t length) override;
};

} // namespace NEO
