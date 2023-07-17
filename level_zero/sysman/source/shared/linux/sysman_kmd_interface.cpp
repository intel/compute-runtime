/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"

#include "shared/source/os_interface/linux/drm_neo.h"

namespace L0 {
namespace Sysman {

std::unique_ptr<SysmanKmdInterface> SysmanKmdInterface::create(const NEO::Drm &drm) {
    std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterface;
    auto drmVersion = drm.getDrmVersion(drm.getFileDescriptor());
    if ("xe" == drmVersion) {
        pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>();
    } else {
        pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915>();
    }
    return pSysmanKmdInterface;
}

std::string SysmanKmdInterfaceI915::getBasePath(int subDeviceId) const {
    return "gt/gt" + std::to_string(subDeviceId) + "/";
}

std::string SysmanKmdInterfaceXe::getBasePath(int subDeviceId) const {
    return "device/gt" + std::to_string(subDeviceId) + "/";
}

void SysmanKmdInterfaceI915::initSysfsNameToFileMap() {
    sysfsNameToFileMap[SysfsName::sysfsNameMinFrequency] = std::make_pair("rps_min_freq_mhz", "gt_min_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxFrequency] = std::make_pair("rps_max_freq_mhz", "gt_max_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameMinDefaultFrequency] = std::make_pair(".defaults/rps_min_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxDefaultFrequency] = std::make_pair(".defaults/rps_max_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameBoostFrequency] = std::make_pair("rps_boost_freq_mhz", "gt_boost_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameCurrentFrequency] = std::make_pair("punit_req_freq_mhz", "gt_cur_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameTdpFrequency] = std::make_pair("rapl_PL1_freq_mhz", "rapl_PL1_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameActualFrequency] = std::make_pair("rps_act_freq_mhz", "gt_act_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameEfficientFrequency] = std::make_pair("rps_RP1_freq_mhz", "gt_RP1_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxValueFrequency] = std::make_pair("rps_RP0_freq_mhz", "gt_RP0_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameMinValueFrequency] = std::make_pair("rps_RPn_freq_mhz", "gt_RPn_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonStatus] = std::make_pair("throttle_reason_status", "gt_throttle_reason_status");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL1] = std::make_pair("throttle_reason_pl1", "gt_throttle_reason_status_pl1");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL2] = std::make_pair("throttle_reason_pl2", "gt_throttle_reason_status_pl2");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL4] = std::make_pair("throttle_reason_pl4", "gt_throttle_reason_status_pl4");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonThermal] = std::make_pair("throttle_reason_thermal", "gt_throttle_reason_status_thermal");
}

void SysmanKmdInterfaceXe::initSysfsNameToFileMap() {
    sysfsNameToFileMap[SysfsName::sysfsNameMinFrequency] = std::make_pair("rps_min_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxFrequency] = std::make_pair("rps_max_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMinDefaultFrequency] = std::make_pair(".defaults/rps_min_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxDefaultFrequency] = std::make_pair(".defaults/rps_max_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameBoostFrequency] = std::make_pair("rps_boost_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameCurrentFrequency] = std::make_pair("punit_req_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameTdpFrequency] = std::make_pair("rapl_PL1_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameActualFrequency] = std::make_pair("rps_act_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameEfficientFrequency] = std::make_pair("rps_RP1_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxValueFrequency] = std::make_pair("rps_RP0_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMinValueFrequency] = std::make_pair("rps_RPn_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonStatus] = std::make_pair("throttle_reason_status", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL1] = std::make_pair("throttle_reason_pl1", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL2] = std::make_pair("throttle_reason_pl2", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL4] = std::make_pair("throttle_reason_pl4", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonThermal] = std::make_pair("throttle_reason_thermal", "");
}

std::string SysmanKmdInterfaceI915::getSysfsFilePath(SysfsName sysfsName, int subDeviceId, bool baseDirectoryExists) {
    std::string filePath = baseDirectoryExists ? getBasePath(subDeviceId) + sysfsNameToFileMap[sysfsName].first : sysfsNameToFileMap[sysfsName].second;
    return filePath;
}

std::string SysmanKmdInterfaceXe::getSysfsFilePath(SysfsName sysfsName, int subDeviceId, bool baseDirectoryExists) {
    std::string filePath = baseDirectoryExists ? getBasePath(subDeviceId) + sysfsNameToFileMap[sysfsName].first : sysfsNameToFileMap[sysfsName].second;
    return filePath;
}

} // namespace Sysman
} // namespace L0