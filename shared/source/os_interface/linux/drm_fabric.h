/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>

namespace NEO {

class Drm;

class DrmFabric {
  public:
    virtual ~DrmFabric() = default;

    virtual bool isAvailable() const = 0;

    virtual int fdToHandle(int32_t dmabufFd, void *reservedHandleData) = 0;
    virtual int handleToFd(const void *reservedHandleData, int32_t &dmabufFd) = 0;
    virtual int handleClose(const void *reservedHandleData) = 0;

    static std::unique_ptr<DrmFabric> create(Drm &drm);
};

class DrmFabricStub : public DrmFabric {
  public:
    bool isAvailable() const override { return false; }
    int fdToHandle(int32_t dmabufFd, void *reservedHandleData) override { return -1; }
    int handleToFd(const void *reservedHandleData, int32_t &dmabufFd) override { return -1; }
    int handleClose(const void *reservedHandleData) override { return -1; }
};

} // namespace NEO
