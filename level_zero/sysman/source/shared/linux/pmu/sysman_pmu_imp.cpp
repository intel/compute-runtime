/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"

#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

namespace L0 {
namespace Sysman {

const std::string PmuInterfaceImp::deviceDir("device");
const std::string PmuInterfaceImp::sysDevicesDir("/sys/devices/");
static constexpr int64_t perfEventOpenSyscallNumber = 298;

int32_t PmuInterfaceImp::getErrorNo() {
    return errno;
}

inline int64_t PmuInterfaceImp::perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) {
    attr->size = sizeof(*attr);
    return this->syscallFunction(perfEventOpenSyscallNumber, attr, pid, cpu, groupFd, flags);
}

int64_t PmuInterfaceImp::pmuInterfaceOpen(uint64_t config, int group, uint32_t format) {
    struct perf_event_attr attr = {};
    int nrCpus = get_nprocs_conf();
    int cpu = 0;
    int64_t ret = 0;

    attr.type = pSysmanKmdInterface->getEventType();
    if (attr.type == 0) {
        return -ENOENT;
    }

    if (group >= 0) {
        format &= ~PERF_FORMAT_GROUP;
    }

    attr.read_format = static_cast<uint64_t>(format);
    attr.config = config;

    do {
        ret = perfEventOpen(&attr, -1, cpu++, group, 0);
    } while ((ret < 0 && getErrorNo() == EINVAL) && (cpu < nrCpus));

    return ret;
}

int32_t PmuInterfaceImp::pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) {
    ssize_t len;
    len = this->readFunction(fd, data, sizeOfdata);
    if (len != sizeOfdata) {
        return -1;
    }
    return 0;
}

PmuInterfaceImp::PmuInterfaceImp(LinuxSysmanImp *pLinuxSysmanImp) {
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
}

PmuInterface *PmuInterface::create(LinuxSysmanImp *pLinuxSysmanImp) {
    PmuInterfaceImp *pPmuInterfaceImp = new PmuInterfaceImp(pLinuxSysmanImp);
    UNRECOVERABLE_IF(nullptr == pPmuInterfaceImp);
    return pPmuInterfaceImp;
}

} // namespace Sysman
} // namespace L0
