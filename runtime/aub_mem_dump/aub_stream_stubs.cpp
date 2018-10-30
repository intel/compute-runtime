/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/aub_streamer.h"

namespace AubDump {

struct AubFileStream {};
struct PhysicalAddressAllocator {};
struct GGTT {};
struct PML4 {};
struct PDP4 {};

AubManager::AubManager(uint32_t devicesCount, bool localMemorySupported, std::string &aubFileName) {
}

void AubManager::initialize(uint32_t devicesCount, bool localMemorySupported) {
}

AubManager::~AubManager() {
}

AubStreamer *AubManager::createAubStreamer(uint32_t gfxFamily, uint32_t device, uint32_t engine) {
    return nullptr;
}

AubStreamer::~AubStreamer() {
}

void AubStreamer::writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, size_t pageSize) {
}

void AubStreamer::expectMemory(uint64_t gfxAddress, const void *memory, size_t size) {
}

void AubStreamer::dumpBuffer(uint64_t gfxAddress, size_t size) {
}
void AubStreamer::dumpImage(uint64_t gfxAddress, size_t size) {
}

void AubStreamer::submit(uint64_t batchBufferGfxAddress, const void *batchBuffer, size_t size, uint32_t memoryBank) {
}

void AubStreamer::pollForCompletion() {
}

} // namespace AubDump
