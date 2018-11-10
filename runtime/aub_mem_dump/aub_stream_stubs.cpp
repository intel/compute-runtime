/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "third_party/aub_stream/headers/aub_manager.h"

namespace AubDump {

AubManager *AubManager::create(uint32_t gfxFamily, uint32_t devicesCount, size_t memoryBankSizeInGB, bool localMemorySupported, std::string &aubFileName) {
    return nullptr;
}

} // namespace AubDump
