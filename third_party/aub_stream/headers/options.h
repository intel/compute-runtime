/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>
#include <cstdint>
#include <ostream>
#include <vector>

namespace aub_stream {

using MMIOPair = std::pair<uint32_t, uint32_t>;
using MMIOList = std::vector<MMIOPair>;

extern "C" void injectMMIOList(MMIOList mmioList);
extern "C" void setTbxServerPort(uint16_t port);
extern "C" void setTbxServerIp(std::string server);

} // namespace aub_stream
