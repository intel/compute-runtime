/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/windows/sys_calls.h"

#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt_helper.h"
#include "level_zero/zes_api.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace L0 {
namespace Sysman {

class SysmanProductHelper;
class PlatformMonitoringTech : NEO::NonCopyableAndNonMovableClass {
  public:
    PlatformMonitoringTech() = delete;
    PlatformMonitoringTech(std::vector<wchar_t> deviceInterface, SysmanProductHelper *pSysmanProductHelper) : deviceInterface(std::move(deviceInterface)), pSysmanProductHelper(pSysmanProductHelper) {}
    virtual ~PlatformMonitoringTech();

    virtual ze_result_t readValue(const std::string &key, uint32_t &value);
    virtual ze_result_t readValue(const std::string &key, uint64_t &value);
    ze_result_t getKeyOffsetMap(std::map<std::string, std::pair<uint32_t, uint32_t>> &keyOffsetMap);
    static std::unique_ptr<PlatformMonitoringTech> create(SysmanProductHelper *pSysmanProductHelper);
    static ze_result_t enumeratePMTInterface(const GUID *Guid, std::vector<wchar_t> &deviceInterface);

  protected:
    std::map<std::string, std::pair<uint32_t, uint32_t>> keyOffsetMap;
    unsigned long guidToIndexList[PmtSysman::PmtMaxInterfaces] = {0};
    ze_result_t ioctlReadWriteData(std::vector<wchar_t> &path, uint32_t ioctl, void *bufferIn, uint32_t inSize, void *bufferOut, uint32_t outSize, uint32_t *sizeReturned);
    virtual ze_result_t init();
    ze_result_t getGuid();
    decltype(&NEO::SysCalls::deviceIoControl) pdeviceIoControl = NEO::SysCalls::deviceIoControl;
    decltype(&NEO::SysCalls::createFile) pcreateFile = NEO::SysCalls::createFile;
    decltype(&NEO::SysCalls::closeHandle) pcloseHandle = NEO::SysCalls::closeHandle;
    decltype(&NEO::SysCalls::heapAlloc) heapAllocFunction = NEO::SysCalls::heapAlloc;
    decltype(&NEO::SysCalls::heapFree) heapFreeFunction = NEO::SysCalls::heapFree;

  private:
    std::vector<wchar_t> deviceInterface;
    SysmanProductHelper *pSysmanProductHelper = nullptr;
    uint32_t baseOffset = 0;
};

} // namespace Sysman
} // namespace L0