/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/zes_api.h"

#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>

namespace L0 {
namespace Sysman {
class LinuxSysmanImp;
class FsAccessInterface;
class SysmanProductHelper;

class PlatformMonitoringTech : NEO::NonCopyableOrMovableClass {
  public:
    struct TelemData {
        std::string telemDir;
        std::string guid;
        uint64_t offset;
    };

    PlatformMonitoringTech() = default;
    PlatformMonitoringTech(FsAccessInterface *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId);
    virtual ~PlatformMonitoringTech();

    virtual ze_result_t readValue(const std::string key, uint32_t &value);
    virtual ze_result_t readValue(const std::string key, uint64_t &value);
    static ze_result_t enumerateRootTelemIndex(FsAccessInterface *pFsAccess, std::string &gpuUpstreamPortPath);
    static void create(LinuxSysmanImp *pLinuxSysmanImp, std::string &gpuUpstreamPortPath,
                       std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject);
    static ze_result_t getKeyOffsetMap(SysmanProductHelper *pSysmanProductHelper, std::string guid, std::map<std::string, uint64_t> &keyOffsetMap);
    static bool getTelemOffsetAndTelemDir(LinuxSysmanImp *pLinuxSysmanImp, uint64_t &telemOffset, std::string &telemDir);
    static bool getTelemData(const std::map<uint32_t, std::string> telemNodesInPciPath, std::string &telemDir, std::string &guid, uint64_t &telemOffset);
    static bool getTelemDataForTileAggregator(const std::map<uint32_t, std::string> telemNodesInPciPath, uint32_t subDeviceId, std::string &telemDir, std::string &guid, uint64_t &telemOffset);
    static bool getTelemOffsetForContainer(SysmanProductHelper *pSysmanProductHelper, const std::string &telemDir, const std::string &key, uint64_t &telemOffset);
    static bool readValue(const std::map<std::string, uint64_t> keyOffsetMap, const std::string &telemDir, const std::string &key, const uint64_t &telemOffset, uint32_t &value);
    static bool readValue(const std::map<std::string, uint64_t> keyOffsetMap, const std::string &telemDir, const std::string &key, const uint64_t &telemOffset, uint64_t &value);
    static bool isTelemetrySupportAvailable(LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId);

  protected:
    static uint32_t rootDeviceTelemNodeIndex;
    std::string telemetryDeviceEntry{};
    std::map<std::string, uint64_t> keyOffsetMap;
    ze_result_t init(LinuxSysmanImp *pLinuxSysmanImp, const std::string &gpuUpstreamPortPath);
    static void doInitPmtObject(LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId, PlatformMonitoringTech *pPmt, const std::string &gpuUpstreamPortPath,
                                std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject);
    decltype(&NEO::SysCalls::pread) preadFunction = NEO::SysCalls::pread;

  private:
    static const std::string baseTelemSysFS;
    static const std::string telem;
    uint64_t baseOffset = 0;
    uint32_t subdeviceId = 0;
    ze_bool_t isSubdevice = 0;
};

} // namespace Sysman
} // namespace L0
