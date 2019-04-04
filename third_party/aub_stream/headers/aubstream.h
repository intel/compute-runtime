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

struct SurfaceInfo {
    uint64_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t format;
    uint32_t surftype;
    uint32_t tilingType;
    bool compressed;
    uint32_t dumpType;
};

namespace surftype {
constexpr uint32_t image1D = 0;
constexpr uint32_t image2D = 1;
constexpr uint32_t image3D = 2;
constexpr uint32_t buffer = 4;
} // namespace surftype

namespace tilingType {
constexpr uint32_t linear = 0;
constexpr uint32_t xmajor = 2;
constexpr uint32_t ymajor = 3;
} // namespace tilingType

namespace dumpType {
constexpr uint32_t bmp = 0;
constexpr uint32_t bin = 1;
constexpr uint32_t tre = 3;
} // namespace dumpType

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
