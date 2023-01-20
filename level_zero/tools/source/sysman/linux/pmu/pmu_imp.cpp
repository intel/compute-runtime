/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/pmu/pmu_imp.h"

#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/driver/driver_handle.h"

namespace L0 {

const std::string PmuInterfaceImp::deviceDir("device");
const std::string PmuInterfaceImp::sysDevicesDir("/sys/devices/");
static constexpr int64_t perfEventOpenSyscallNumber = 298;
// Get event id
uint32_t PmuInterfaceImp::getEventType() {
    std::string i915DirName("i915");
    bool isLmemSupported = pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
    if (isLmemSupported) {
        std::string bdfDir;
        // ID or type of Pmu driver for discrete graphics is obtained by reading sysfs node as explained below:
        // For instance DG1 in PCI slot 0000:03:00.0:
        // $ cat /sys/devices/i915_0000_03_00.0/type
        // 23
        ze_result_t result = pSysfsAccess->readSymLink(deviceDir, bdfDir);
        if (ZE_RESULT_SUCCESS != result) {
            return 0;
        }
        const auto loc = bdfDir.find_last_of('/');
        auto bdf = bdfDir.substr(loc + 1);
        std::replace(bdf.begin(), bdf.end(), ':', '_');
        i915DirName = "i915_" + bdf;
    }
    // For integrated graphics type of PMU driver is obtained by reading /sys/devices/i915/type node
    // # cat /sys/devices/i915/type
    // 18
    const std::string eventTypeSysfsNode = sysDevicesDir + i915DirName + "/" + "type";
    auto eventTypeVal = 0u;
    if (ZE_RESULT_SUCCESS != pFsAccess->read(eventTypeSysfsNode, eventTypeVal)) {
        return 0;
    }
    return eventTypeVal;
}

int PmuInterfaceImp::getErrorNo() {
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

    attr.type = getEventType();
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

int PmuInterfaceImp::pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) {
    ssize_t len;
    len = this->readFunction(fd, data, sizeOfdata);
    if (len != sizeOfdata) {
        return -1;
    }
    return 0;
}

PmuInterfaceImp::PmuInterfaceImp(LinuxSysmanImp *pLinuxSysmanImp) {
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
}

PmuInterface *PmuInterface::create(LinuxSysmanImp *pLinuxSysmanImp) {
    PmuInterfaceImp *pPmuInterfaceImp = new PmuInterfaceImp(pLinuxSysmanImp);
    UNRECOVERABLE_IF(nullptr == pPmuInterfaceImp);
    return pPmuInterfaceImp;
}

} // namespace L0
