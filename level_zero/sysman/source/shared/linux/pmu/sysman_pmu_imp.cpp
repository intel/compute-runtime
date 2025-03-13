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

int32_t PmuInterfaceImp::getConfigFromEventFile(const std::string_view &eventFile, uint64_t &config) {

    // The event file from the events directory has following contents
    // event=0x02 --> for /sys/devices/xe_<bdf>/events/engine-active-ticks
    // event=0x03 --> for /sys/devices/xe_<bdf>/events/engine-total-ticks
    // The config values fetched are in hexadecimal format

    auto pFsAccess = pSysmanKmdInterface->getFsAccess();
    std::string readVal = "";
    ze_result_t result = pFsAccess->read(std::string(eventFile), readVal);
    if (result != ZE_RESULT_SUCCESS) {
        return -1;
    }

    size_t pos = readVal.rfind("=");
    if (pos == std::string::npos) {
        return -1;
    }
    readVal = readVal.substr(pos + 1, std::string::npos);
    config = std::strtoul(readVal.c_str(), nullptr, 16);
    return 0;
}

static ze_result_t getShiftValue(const std::string_view &readFile, FsAccessInterface *pFsAccess, uint32_t &shiftValue) {

    // The contents of the file passed as an argument is of the form 'config:<start_val>-<end_val>'
    // The start_val is the shift value. It is in decimal format.
    // Eg. if the contents are config:60-63, the shift value in this case is 60

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::string readVal = "";

    result = pFsAccess->read(std::string(readFile), readVal);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    size_t pos = readVal.rfind(":");
    if (pos == std::string::npos) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    readVal = readVal.substr(pos + 1, std::string::npos);
    pos = readVal.rfind("-");
    if (pos == std::string::npos) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    readVal = readVal.substr(0, pos);
    shiftValue = static_cast<uint32_t>(std::strtoul(readVal.c_str(), nullptr, 10));
    return result;
}

int32_t PmuInterfaceImp::getConfigAfterFormat(const std::string_view &formatDir, uint64_t &config, uint32_t engineClass, uint32_t engineInstance, uint32_t gt) {

    // The final config is computed by the bitwise OR operation of the config fetched from the event file and value obtained by shifting the parameters gt,
    // engineClass and engineInstance with the shift value fetched from the corresponding file in /sys/devices/xe_<bdf>/format/ directory.
    // The contents of these files in the form of 'config:<start_val>-<end_val>'. For eg.
    // config:60-63 --> for /sys/devices/xe_<bdf>/format/gt
    // config:20-27 --> for /sys/devices/xe_<bdf>/format/engine_class
    // config:12-19 --> for /sys/devices/xe_<bdf>/format/engine_instance
    // The start_val is the shift value by which the parameter should be shifted. The final config is computed as follows.
    // config |= gt << gt_shift_value
    // config |= engineClass << engineClass_shift_value
    // config |= engineInstance << engineInstance_shift_value

    auto pFsAccess = pSysmanKmdInterface->getFsAccess();
    std::string readVal = "";
    uint32_t shiftValue = 0;
    std::string readFile = std::string(formatDir) + "gt";
    ze_result_t result = getShiftValue(readFile, pFsAccess, shiftValue);
    if (result != ZE_RESULT_SUCCESS) {
        return -1;
    }
    config |= gt << shiftValue;

    shiftValue = 0;
    readFile = std::string(formatDir) + "engine_class";
    result = getShiftValue(readFile, pFsAccess, shiftValue);
    if (result != ZE_RESULT_SUCCESS) {
        return -1;
    }
    config |= engineClass << shiftValue;

    shiftValue = 0;
    readFile = std::string(formatDir) + "engine_instance";
    result = getShiftValue(readFile, pFsAccess, shiftValue);
    if (result != ZE_RESULT_SUCCESS) {
        return -1;
    }
    config |= engineInstance << shiftValue;

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
