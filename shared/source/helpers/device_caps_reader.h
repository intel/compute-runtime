/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "aubstream/aub_manager.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace NEO {

class DeviceCapsReader {
  public:
    virtual ~DeviceCapsReader() = default;
    virtual uint32_t operator[](size_t offsetDw) const = 0;
    uint32_t getOffset() const { return offset; }
    uint32_t getStride() const { return stride; }

  protected:
    DeviceCapsReader() = default;
    DeviceCapsReader(uint32_t offset, uint32_t stride) : offset(offset), stride(stride) {}
    uint32_t offset = 0;
    uint32_t stride = 0;
};

class DeviceCapsReaderTbx : public DeviceCapsReader {
  public:
    static std::unique_ptr<DeviceCapsReaderTbx> create(aub_stream::AubManager &aubManager, uint32_t offset, uint32_t stride) {
        return std::unique_ptr<DeviceCapsReaderTbx>(new DeviceCapsReaderTbx(aubManager, offset, stride));
    }

    uint32_t operator[](size_t offsetDw) const override {
        return aubManager.readMMIO(static_cast<uint32_t>(offsetDw * sizeof(uint32_t)));
    }

  protected:
    DeviceCapsReaderTbx(aub_stream::AubManager &aubManager, uint32_t offset, uint32_t stride) : DeviceCapsReader(offset, stride), aubManager(aubManager) {}
    aub_stream::AubManager &aubManager;
};

} // namespace NEO
