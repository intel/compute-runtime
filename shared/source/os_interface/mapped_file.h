/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace NEO {

class MappedFile {
  public:
    static std::unique_ptr<MappedFile> open(const std::string &filePath);

    virtual ~MappedFile() = default;

    uint8_t *getBase() const {
        return this->base;
    }

    uint64_t getSize() const {
        return this->size;
    }

  protected:
    MappedFile(uint8_t *base, uint64_t size) : base(base), size(size) {}

    uint8_t *base = nullptr;
    uint64_t size = 0;
};

} // namespace NEO
