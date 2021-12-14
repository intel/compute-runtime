/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <map>
#include <poll.h>
#include <string>
#include <string_view>

namespace NEO {

class PmtUtil {
  public:
    static constexpr uint32_t guidStringSize = 16u;
    static void getTelemNodesInPciPath(std::string_view rootPciPath, std::map<uint32_t, std::string> &telemPciPath);
    static bool readGuid(std::string_view telemDir, std::array<char, PmtUtil::guidStringSize> &guidString);
    static bool readOffset(std::string_view telemDir, uint64_t &offset);
    static ssize_t readTelem(std::string_view telemDir, const std::size_t size, const uint64_t offset, void *data);
};

} // namespace NEO