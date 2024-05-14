/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/aubstream.h"

namespace aub_stream_stubs {
uint16_t tbxServerPort = 4321;
std::string tbxServerIp = "127.0.0.1";
bool tbxFrontdoorMode = false;
aub_stream::MMIOList mmioListInjected;
} // namespace aub_stream_stubs

namespace aub_stream {

void injectMMIOListLegacy(MMIOList mmioList) {
    aub_stream_stubs::mmioListInjected = mmioList;
    aub_stream_stubs::mmioListInjected.shrink_to_fit();
}
void setTbxServerPort(uint16_t port) {
    aub_stream_stubs::tbxServerPort = port;
}
void setTbxServerIpLegacy(std::string server) {
    // better to avoid reassigning global variables which assume memory allocations since
    // we could step into false-positive memory leak detection with embedded leak check helper
    if (aub_stream_stubs::tbxServerIp != server) {
        aub_stream_stubs::tbxServerIp = server;
    }
}
void setTbxFrontdoorMode(bool frontdoor) {
    aub_stream_stubs::tbxFrontdoorMode = frontdoor;
}
void setAubStreamCaller(uint32_t caller) {
}

} // namespace aub_stream
