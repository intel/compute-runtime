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
enum class CacheRegion : uint16_t;

extern IoctlHelper *ioctlHelperFactory[IGFX_MAX_PRODUCT];

class IoctlHelper {
  public:
    virtual ~IoctlHelper() {}
    static IoctlHelper *get(Drm *drm);
    static uint32_t ioctl(Drm *drm, unsigned long request, void *arg);

    virtual uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) = 0;
    virtual std::unique_ptr<uint8_t[]> translateIfRequired(uint8_t *dataQuery, int32_t length) = 0;
    virtual CacheRegion closAlloc(Drm *drm) = 0;
    virtual uint16_t closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) = 0;
    virtual CacheRegion closFree(Drm *drm, CacheRegion closIndex) = 0;
    virtual int waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                              uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) = 0;
    virtual uint32_t getHwConfigIoctlVal() = 0;
};

class IoctlHelperUpstream : public IoctlHelper {
  public:
    virtual uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) override;
    virtual std::unique_ptr<uint8_t[]> translateIfRequired(uint8_t *dataQuery, int32_t length) override;
    CacheRegion closAlloc(Drm *drm) override;
    uint16_t closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(Drm *drm, CacheRegion closIndex) override;
    int waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) override;
    uint32_t getHwConfigIoctlVal() override;
};

template <PRODUCT_FAMILY gfxProduct>
class IoctlHelperImpl : public IoctlHelperUpstream {
  public:
    static IoctlHelper *get() {
        static IoctlHelperImpl<gfxProduct> instance;
        return &instance;
    }
    uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) override;
    std::unique_ptr<uint8_t[]> translateIfRequired(uint8_t *dataQuery, int32_t length) override;
};

class IoctlHelperPrelim20 : public IoctlHelper {
  public:
    uint32_t createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) override;
    std::unique_ptr<uint8_t[]> translateIfRequired(uint8_t *dataQuery, int32_t length) override;
    CacheRegion closAlloc(Drm *drm) override;
    uint16_t closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(Drm *drm, CacheRegion closIndex) override;
    int waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) override;
    uint32_t getHwConfigIoctlVal() override;
};

} // namespace NEO
