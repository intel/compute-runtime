/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/options.h"

namespace aub_stream {

MMIOList injectMMIOList;
std::string tbxServerIp = "127.0.0.1";
uint16_t tbxServerPort = 4321;

AubManager *AubManager::create(uint32_t gfxFamily, uint32_t devicesCount, uint64_t memoryBankSizeInGB, bool localMemorySupported, const std::string &aubFileName, uint32_t streamMode) {
    return nullptr;
}

} // namespace aub_stream
