/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/aubstream.h"

namespace aub_stream_stubs {
uint16_t tbxServerPort = 4321;
std::string tbxServerIp = "127.0.0.1";
bool tbxFrontdoorMode = false;
} // namespace aub_stream_stubs

namespace aub_stream {

AubManager *AubManager::create(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSizeInGB, uint32_t stepping, bool localMemorySupported, uint32_t streamMode, uint64_t gpuAddressSpace) {
    return nullptr;
}

extern "C" {
void injectMMIOList(MMIOList mmioList){};
void setTbxServerPort(uint16_t port) { aub_stream_stubs::tbxServerPort = port; };
void setTbxServerIp(std::string server) {
    // better to avoid reassigning global variables which assume memory allocations since
    // we could step into false-positive memory leak detection with embedded leak check helper
    if (aub_stream_stubs::tbxServerIp != server)
        aub_stream_stubs::tbxServerIp = server;
};
void setTbxFrontdoorMode(bool frontdoor) { aub_stream_stubs::tbxFrontdoorMode = frontdoor; }
}

} // namespace aub_stream
