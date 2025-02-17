/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/linux/pmu/pmu.h"

#include <linux/perf_event.h>
#include <string>
#include <sys/sysinfo.h>

namespace L0 {

class PmuInterfaceImp : public PmuInterface, NEO::NonCopyableAndNonMovableClass {
  public:
    PmuInterfaceImp() = delete;
    PmuInterfaceImp(LinuxSysmanImp *pLinuxSysmanImp);
    ~PmuInterfaceImp() override = default;
    int64_t pmuInterfaceOpen(uint64_t config, int group, uint32_t format) override;
    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override;

  protected:
    MOCKABLE_VIRTUAL int getErrorNo();
    MOCKABLE_VIRTUAL int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags);
    decltype(&read) readFunction = read;
    decltype(&syscall) syscallFunction = syscall;

  private:
    uint32_t getEventType();
    FsAccess *pFsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    Device *pDevice = nullptr;
    static const std::string deviceDir;
    static const std::string sysDevicesDir;
};

} // namespace L0
