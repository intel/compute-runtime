/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <linux/perf_event.h>
#include <string>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace L0 {
namespace Sysman {

class SysmanKmdInterface;

class PmuInterfaceImp : public PmuInterface, NEO::NonCopyableOrMovableClass {
  public:
    PmuInterfaceImp() = delete;
    PmuInterfaceImp(LinuxSysmanImp *pLinuxSysmanImp);
    ~PmuInterfaceImp() override = default;
    int64_t pmuInterfaceOpen(uint64_t config, int group, uint32_t format) override;
    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override;

  protected:
    virtual int getErrorNo();
    virtual int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags);
    decltype(&read) readFunction = read;
    decltype(&syscall) syscallFunction = syscall;
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;

  private:
    SysmanDeviceImp *pDevice = nullptr;
    static const std::string deviceDir;
    static const std::string sysDevicesDir;
};

} // namespace Sysman
} // namespace L0
