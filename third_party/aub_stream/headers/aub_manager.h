/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace AubDump {

class AubStreamer;
struct AubFileStream;
struct PhysicalAddressAllocator;
struct GGTT;
struct PML4;
struct PDP4;
using PPGTT = typename std::conditional<sizeof(void *) == 8, PML4, PDP4>::type;

class AubManager {
  public:
    AubManager(uint32_t devicesCount, bool localMemorySupported, std::string &aubFileName);
    virtual ~AubManager();

    AubManager &operator=(const AubManager &) = delete;
    AubManager(const AubManager &) = delete;

    GGTT *getGGTT(uint32_t index) {
        return ggtts.at(index).get();
    }

    PPGTT *getPPGTT(uint32_t index) {
        return ppgtts.at(index).get();
    }

    PhysicalAddressAllocator *getPysicalAddressAllocator() {
        return physicalAddressAllocator.get();
    }

    AubFileStream *getAubFileStream() {
        return aubFileStream.get();
    }

    AubStreamer *createAubStreamer(uint32_t gfxFamily, uint32_t device, uint32_t engine);

  protected:
    void initialize(uint32_t devicesCount, bool localMemorySupported);

    uint32_t devicesCount = 0;

    std::unique_ptr<AubFileStream> aubFileStream;
    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;

    std::vector<std::unique_ptr<PPGTT>> ppgtts;
    std::vector<std::unique_ptr<GGTT>> ggtts;
};

} // namespace AubDump
