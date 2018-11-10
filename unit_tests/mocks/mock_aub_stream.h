/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/gen_common/aub_mapper_base.h"

namespace OCLRT {

struct MockAubStreamMockMmioWrite : AubMemDump::AubStream {
    void open(const char *filePath) override{};
    void close() override{};
    bool init(uint32_t stepping, uint32_t device) override { return true; };
    void writeMemory(uint64_t physAddress, const void *memory, size_t sizeToDumpThisIteration, uint32_t addressSpace, uint32_t hint) override{};
    void writeMemoryWriteHeader(uint64_t physAddress, size_t size, uint32_t addressSpace, uint32_t hint) override{};
    void writePTE(uint64_t physAddress, uint64_t entry, uint32_t addressSpace) override{};
    void writeGTT(uint32_t offset, uint64_t entry) override{};
    void registerPoll(uint32_t registerOffset, uint32_t mask, uint32_t value, bool pollNotEqual, uint32_t timeoutAction) override{};

    void writeMMIOImpl(uint32_t offset, uint32_t value) override {
        mmioList.push_back(std::make_pair(offset, value));
    }
    bool isOnMmioList(const MMIOPair &mmio) {
        bool mmioFound = false;
        for (auto &mmioPair : mmioList) {
            if (mmioPair.first == mmio.first && mmioPair.second == mmio.second) {
                mmioFound = true;
                break;
            }
        }
        return mmioFound;
    }

    std::vector<std::pair<uint32_t, uint32_t>> mmioList;
};
} // namespace OCLRT
