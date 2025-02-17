/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/zes_api.h"

#include <map>

namespace L0 {
namespace Sysman {
class LinuxSysmanImp;
class FsAccessInterface;
class SysmanProductHelper;

class PlatformMonitoringTech : NEO::NonCopyableAndNonMovableClass {
  public:
    struct TelemData {
        std::string telemDir;
        std::string guid;
        uint64_t offset;
    };

    static bool getKeyOffsetMap(SysmanProductHelper *pSysmanProductHelper, std::string guid, std::map<std::string, uint64_t> &keyOffsetMap);
    static bool getTelemOffsetAndTelemDir(LinuxSysmanImp *pLinuxSysmanImp, uint64_t &telemOffset, std::string &telemDir);
    static bool getTelemData(const std::map<uint32_t, std::string> telemNodesInPciPath, std::string &telemDir, std::string &guid, uint64_t &telemOffset);
    static bool getTelemDataForTileAggregator(const std::map<uint32_t, std::string> telemNodesInPciPath, uint32_t subDeviceId, std::string &telemDir, std::string &guid, uint64_t &telemOffset);
    static bool getTelemOffsetForContainer(SysmanProductHelper *pSysmanProductHelper, const std::string &telemDir, const std::string &key, uint64_t &telemOffset);
    static bool readValue(const std::map<std::string, uint64_t> keyOffsetMap, const std::string &telemDir, const std::string &key, const uint64_t &telemOffset, uint32_t &value);
    static bool readValue(const std::map<std::string, uint64_t> keyOffsetMap, const std::string &telemDir, const std::string &key, const uint64_t &telemOffset, uint64_t &value);
    static bool isTelemetrySupportAvailable(LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId);
};

} // namespace Sysman
} // namespace L0
