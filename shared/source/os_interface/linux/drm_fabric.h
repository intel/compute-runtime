/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/graphics_allocation.h"

#include <memory>

namespace NEO {

class Drm;

class DrmFabric {
  public:
    virtual ~DrmFabric() = default;

    virtual bool isAvailable() const = 0;

    virtual InternalHandleStatus fdToHandle(int32_t dmabufFd, void *reservedHandleData) = 0;
    virtual InternalHandleStatus handleToFd(const void *reservedHandleData, int32_t &dmabufFd) = 0;
    virtual InternalHandleStatus handleClose(const void *reservedHandleData) = 0;

    static std::unique_ptr<DrmFabric> create(Drm &drm);
};

class DrmFabricStub : public DrmFabric {
  public:
    bool isAvailable() const override { return false; }
    InternalHandleStatus fdToHandle(int32_t dmabufFd, void *reservedHandleData) override { return InternalHandleStatus::unsupported; }
    InternalHandleStatus handleToFd(const void *reservedHandleData, int32_t &dmabufFd) override { return InternalHandleStatus::unsupported; }
    InternalHandleStatus handleClose(const void *reservedHandleData) override { return InternalHandleStatus::unsupported; }
};

} // namespace NEO
