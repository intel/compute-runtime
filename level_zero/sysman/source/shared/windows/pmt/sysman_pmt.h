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
    PlatformMonitoringTech(SysmanProductHelper *pSysmanProductHelper, uint32_t bus, uint32_t device, uint32_t function) : pSysmanProductHelper(pSysmanProductHelper), pciBus(bus), pciDevice(device), pciFunction(function) {}
    virtual ~PlatformMonitoringTech();

    virtual ze_result_t readValue(const std::string &key, uint32_t &value);
    virtual ze_result_t readValue(const std::string &key, uint64_t &value);
    ze_result_t getKeyOffsetMap(std::map<std::string, std::pair<uint32_t, uint32_t>> &keyOffsetMap);
    static std::unique_ptr<PlatformMonitoringTech> create(SysmanProductHelper *pSysmanProductHelper, uint32_t bus, uint32_t device, uint32_t function);

  protected:
    std::wstring deviceInterface;
    std::map<std::string, std::pair<uint32_t, uint32_t>> keyOffsetMap;
    unsigned long guidToIndexList[PmtSysman::PmtMaxInterfaces] = {0};
    ze_result_t ioctlReadWriteData(std::wstring &path, uint32_t ioctl, void *bufferIn, uint32_t inSize, void *bufferOut, uint32_t outSize, uint32_t *sizeReturned);
    ze_result_t getDeviceInterface();
    ze_result_t getDeviceRegistryProperty(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA *pDeviceInfoData, DWORD devicePropertyType, std::vector<uint8_t> &dataBuffer);
    bool isInstancePciBdfMatchingWithWddmDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA *pDeviceInfoData);
    ze_result_t getDeviceInstanceId(DEVINST deviceInstance, std::wstring &deviceInstanceId);
    void getAllChildDeviceInterfaces(HDEVINFO hDevInfo, DEVINST deviceInstance, uint32_t level, std::vector<std::wstring> &interfacePathList);
    virtual ze_result_t init();
    ze_result_t getGuid();
    decltype(&NEO::SysCalls::deviceIoControl) pdeviceIoControl = NEO::SysCalls::deviceIoControl;
    decltype(&NEO::SysCalls::createFile) pcreateFile = NEO::SysCalls::createFile;
    decltype(&NEO::SysCalls::closeHandle) pcloseHandle = NEO::SysCalls::closeHandle;
    decltype(&NEO::SysCalls::heapAlloc) heapAllocFunction = NEO::SysCalls::heapAlloc;
    decltype(&NEO::SysCalls::heapFree) heapFreeFunction = NEO::SysCalls::heapFree;

  private:
    SysmanProductHelper *pSysmanProductHelper = nullptr;
    uint32_t baseOffset = 0;
    uint32_t pciBus;
    uint32_t pciDevice;
    uint32_t pciFunction;
};

} // namespace Sysman
} // namespace L0