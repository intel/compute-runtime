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

namespace dumpFormat {
constexpr uint32_t bin = 0;
constexpr uint32_t bmp = 1;
constexpr uint32_t tre = 2;
} // namespace dumpFormat

namespace mode {
constexpr uint32_t aubFile = 0;
constexpr uint32_t tbx = 1;
constexpr uint32_t aubFileAndTbx = 2;
} // namespace mode

using MMIOPair = std::pair<uint32_t, uint32_t>;
using MMIOList = std::vector<MMIOPair>;

extern "C" void injectMMIOList(MMIOList mmioList);
extern "C" void setTbxServerPort(uint16_t port);
extern "C" void setTbxServerIp(std::string server);

} // namespace aub_stream
