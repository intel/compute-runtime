/*
 * Copyright (C) 2025 Intel Corporation
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

  protected:
    DeviceCapsReader() = default;
    DeviceCapsReader(uint32_t offset) : offset(offset) {}
    uint32_t offset = 0;
};

class DeviceCapsReaderTbx : public DeviceCapsReader {
  public:
    static std::unique_ptr<DeviceCapsReaderTbx> create(aub_stream::AubManager &aubManager, uint32_t offset) {
        return std::unique_ptr<DeviceCapsReaderTbx>(new DeviceCapsReaderTbx(aubManager, offset));
    }

    uint32_t operator[](size_t offsetDw) const override {
        return aubManager.readMMIO(static_cast<uint32_t>(offsetDw * sizeof(uint32_t)));
    }

  protected:
    DeviceCapsReaderTbx(aub_stream::AubManager &aubManager, uint32_t offset) : DeviceCapsReader(offset), aubManager(aubManager) {}
    aub_stream::AubManager &aubManager;
};

} // namespace NEO
