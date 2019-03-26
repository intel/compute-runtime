/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

namespace NEO {

class TbxSockets {
  protected:
    TbxSockets() = default;

  public:
    virtual ~TbxSockets() = default;
    virtual bool init(const std::string &hostNameOrIp, uint16_t port) = 0;
    virtual void close() = 0;

    virtual bool writeGTT(uint32_t offset, uint64_t entry) = 0;

    virtual bool readMemory(uint64_t addr, void *memory, size_t size) = 0;
    virtual bool writeMemory(uint64_t addr, const void *memory, size_t size, uint32_t type) = 0;

    virtual bool readMMIO(uint32_t offset, uint32_t *value) = 0;
    virtual bool writeMMIO(uint32_t offset, uint32_t value) = 0;

    static TbxSockets *create();
};
} // namespace NEO
