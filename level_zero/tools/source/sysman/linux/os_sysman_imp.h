/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/firmware_util/firmware_util.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"
#include "level_zero/tools/source/sysman/linux/pmu/pmu_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include <map>

namespace L0 {
class PmuInterface;

class LinuxSysmanImp : public OsSysman, NEO::NonCopyableOrMovableClass {
  public:
    LinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp);
    ~LinuxSysmanImp() override;

    ze_result_t init() override;

    PmuInterface *getPmuInterface();
    FirmwareUtil *getFwUtilInterface();
    FsAccess &getFsAccess();
    ProcfsAccess &getProcfsAccess();
    SysfsAccess &getSysfsAccess();
    NEO::Drm &getDrm();
    PlatformMonitoringTech *getPlatformMonitoringTechAccess(uint32_t subDeviceId);
    Device *getDeviceHandle();
    SysmanDeviceImp *getSysmanDeviceImp();
    std::string getPciRootPortDirectoryPath(std::string realPciPath);
    std::string getPciRootPortDirectoryPathForReset(std::string realPciPath);
    std::string getPciCardBusDirectoryPath(std::string realPciPath);
    void releasePmtObject();
    ze_result_t createPmtHandles();
    void createFwUtilInterface();
    void releaseFwUtilInterface();
    void releaseLocalDrmHandle();
    PRODUCT_FAMILY getProductFamily();
    void releaseSysmanDeviceResources();
    void releaseDeviceResources();
    ze_result_t initDevice();
    void reInitSysmanDeviceResources();
    void getPidFdsForOpenDevice(ProcfsAccess *, SysfsAccess *, const ::pid_t, std::vector<int> &);
    ze_result_t osWarmReset();
    ze_result_t osColdReset();
    std::string getAddressFromPath(std::string &rootPortPath);
    decltype(&NEO::SysCalls::open) openFunction = NEO::SysCalls::open;
    decltype(&NEO::SysCalls::close) closeFunction = NEO::SysCalls::close;
    decltype(&NEO::SysCalls::pread) preadFunction = NEO::SysCalls::pread;
    decltype(&NEO::SysCalls::pwrite) pwriteFunction = NEO::SysCalls::pwrite;
    std::string devicePciBdf = "";
    uint32_t rootDeviceIndex = 0u;
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    bool diagnosticsReset = false;

  protected:
    FsAccess *pFsAccess = nullptr;
    ProcfsAccess *pProcfsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    Device *pDevice = nullptr;
    NEO::Drm *pDrm = nullptr;
    PmuInterface *pPmuInterface = nullptr;
    FirmwareUtil *pFwUtilInterface = nullptr;
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    ze_result_t initLocalDeviceAndDrmHandles();

  private:
    LinuxSysmanImp() = delete;
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
    static const std::string deviceDir;
};
class ExecutionEnvironmentRefCountRestore {
  public:
    ExecutionEnvironmentRefCountRestore() = delete;
    ExecutionEnvironmentRefCountRestore(NEO::ExecutionEnvironment *executionEnvironmentRecevied) {
        executionEnvironment = executionEnvironmentRecevied;
        executionEnvironment->incRefInternal();
    }
    ~ExecutionEnvironmentRefCountRestore() {
        executionEnvironment->decRefInternal();
    }
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
};
} // namespace L0
