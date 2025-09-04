/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <linux/perf_event.h>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace L0 {
namespace Sysman {

class SysmanKmdInterface;

class PmuInterfaceImp : public PmuInterface, NEO::NonCopyableAndNonMovableClass {
  public:
    PmuInterfaceImp() = delete;
    PmuInterfaceImp(LinuxSysmanImp *pLinuxSysmanImp);
    ~PmuInterfaceImp() override = default;
    int64_t pmuInterfaceOpen(uint64_t config, int group, uint32_t format) override;
    int32_t pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override;
    int32_t getPmuConfigs(std::string_view sysmanDeviceDir, uint64_t engineClass, uint64_t engineInstance, uint64_t gtId, uint64_t &activeTicksConfig, uint64_t &totalTicksConfig) override;
    int32_t getPmuConfigsForVf(std::string_view sysmanDeviceDir, uint64_t fnNumber, uint64_t &activeTicksConfig, uint64_t &totalTicksConfig) override;

  protected:
    MOCKABLE_VIRTUAL int32_t getErrorNo();
    MOCKABLE_VIRTUAL int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags);
    MOCKABLE_VIRTUAL int32_t getConfigFromEventFile(std::string_view eventFile, uint64_t &config);
    MOCKABLE_VIRTUAL int32_t getConfigAfterFormat(std::string_view formatDir, uint64_t &config, uint64_t engineClass, uint64_t engineInstance, uint64_t gt);
    decltype(&read) readFunction = read;
    decltype(&syscall) syscallFunction = syscall;
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;

  private:
    static const std::string deviceDir;
    static const std::string sysDevicesDir;
};

} // namespace Sysman
} // namespace L0
