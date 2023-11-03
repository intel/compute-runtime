/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/aubstream.h"

#include <limits>

namespace aub_stream_stubs {
uint16_t tbxServerPort = 4321;
std::string tbxServerIp = "127.0.0.1";
bool tbxFrontdoorMode = false;
uint32_t aubStreamCaller = std::numeric_limits<uint32_t>::max();
} // namespace aub_stream_stubs

namespace aub_stream {

extern "C" {
void injectMMIOList(MMIOList mmioList){};
void setTbxServerPort(uint16_t port) { aub_stream_stubs::tbxServerPort = port; };
void setTbxServerIp(std::string server) {
    // better to avoid reassigning global variables which assume memory allocations since
    // we could step into false-positive memory leak detection with embedded leak check helper
    if (aub_stream_stubs::tbxServerIp != server)
        aub_stream_stubs::tbxServerIp = server;
};
void setTbxFrontdoorMode(bool frontdoor) { aub_stream_stubs::tbxFrontdoorMode = frontdoor; };
void setAubStreamCaller(uint32_t caller) { aub_stream_stubs::aubStreamCaller = caller; };
}

} // namespace aub_stream
