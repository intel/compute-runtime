/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/tbx/tbx_sockets.h"

namespace NEO {

class MockTbxSockets : public TbxSockets {
  public:
    MockTbxSockets(){};
    ~MockTbxSockets() override = default;

    bool init(const std::string &hostNameOrIp, uint16_t port) override { return true; };
    void close() override{};

    bool writeGTT(uint32_t gttOffset, uint64_t entry) override { return true; };

    bool readMemory(uint64_t offset, void *data, size_t size) override { return true; };
    bool writeMemory(uint64_t offset, const void *data, size_t size, uint32_t type) override {
        typeCapturedFromWriteMemory = type;
        return true;
    };

    bool readMMIO(uint32_t offset, uint32_t *data) override { return true; };
    bool writeMMIO(uint32_t offset, uint32_t data) override { return true; };

    uint32_t typeCapturedFromWriteMemory = 0;
};
} // namespace NEO
