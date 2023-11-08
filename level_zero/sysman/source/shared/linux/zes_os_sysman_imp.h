/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/sysman/source/device/os_sysman.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_hw_device_id_linux.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <map>
#include <mutex>

namespace NEO {
class Drm;
} // namespace NEO

namespace L0 {
namespace Sysman {

class SysmanProductHelper;
class PlatformMonitoringTech;
class PmuInterface;
class FsAccess;
class ProcfsAccess;
class SysfsAccess;
class FirmwareUtil;
class SysmanKmdInterface;

class LinuxSysmanImp : public OsSysman, NEO::NonCopyableOrMovableClass {
  public:
    LinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp);
    ~LinuxSysmanImp() override;

    ze_result_t init() override;

    FirmwareUtil *getFwUtilInterface();
    PmuInterface *getPmuInterface() { return pPmuInterface; }
    FsAccess &getFsAccess();
    ProcfsAccess &getProcfsAccess();
    SysfsAccess &getSysfsAccess();
    SysmanDeviceImp *getSysmanDeviceImp();
    SysmanProductHelper *getSysmanProductHelper();
    uint32_t getSubDeviceCount() override;
    const NEO::HardwareInfo &getHardwareInfo() const override { return pParentSysmanDeviceImp->getHardwareInfo(); }
    std::string getPciCardBusDirectoryPath(std::string realPciPath);
    uint32_t getMemoryType();
    static std::string getPciRootPortDirectoryPath(std::string realPciPath);
    PlatformMonitoringTech *getPlatformMonitoringTechAccess(uint32_t subDeviceId);
    PRODUCT_FAMILY getProductFamily() const { return pParentSysmanDeviceImp->getProductFamily(); }
    SysmanHwDeviceIdDrm::SingleInstance getSysmanHwDeviceIdInstance();
    NEO::Drm *getDrm();
    void releasePmtObject();
    MOCKABLE_VIRTUAL void releaseSysmanDeviceResources();
    MOCKABLE_VIRTUAL ze_result_t reInitSysmanDeviceResources();
    MOCKABLE_VIRTUAL void getPidFdsForOpenDevice(const ::pid_t, std::vector<int> &);
    MOCKABLE_VIRTUAL ze_result_t osWarmReset();
    MOCKABLE_VIRTUAL ze_result_t osColdReset();
    ze_result_t gpuProcessCleanup(ze_bool_t force);
    std::string getAddressFromPath(std::string &rootPortPath);
    decltype(&NEO::SysCalls::pread) preadFunction = NEO::SysCalls::pread;
    decltype(&NEO::SysCalls::pwrite) pwriteFunction = NEO::SysCalls::pwrite;
    ze_result_t createPmtHandles();
    SysmanDeviceImp *getParentSysmanDeviceImp() { return pParentSysmanDeviceImp; }
    std::string &getPciRootPath();
    std::string devicePciBdf = "";
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    uint32_t rootDeviceIndex;
    bool diagnosticsReset = false;
    bool isMemoryDiagnostics = false;
    std::string gtDevicePath;
    SysmanKmdInterface *getSysmanKmdInterface() { return pSysmanKmdInterface.get(); }

  protected:
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper;
    std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterface;
    FsAccess *pFsAccess = nullptr;
    ProcfsAccess *pProcfsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    uint32_t subDeviceCount = 0;
    FirmwareUtil *pFwUtilInterface = nullptr;
    PmuInterface *pPmuInterface = nullptr;
    std::string rootPath;
    void releaseFwUtilInterface();
    uint32_t memType = unknownMemoryType;

  private:
    LinuxSysmanImp() = delete;
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
    static const std::string deviceDir;
    void createFwUtilInterface();
    void clearHPIE(int fd);
    ze_result_t resizeVfBar(uint8_t size);
    std::mutex fwLock;
};

} // namespace Sysman
} // namespace L0
