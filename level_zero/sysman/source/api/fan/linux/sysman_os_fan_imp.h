/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/fan/sysman_os_fan.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

#include <array>
#include <string>

namespace L0 {
namespace Sysman {

class LinuxSysmanImp;
class SysmanKmdInterface;
struct OsSysman;

// Maximum number of fan curve control points supported by the KMD
static constexpr uint32_t maxFanControlPoints = 10;

class LinuxFanImp : public OsFan, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_fan_properties_t *pProperties) override;
    ze_result_t getConfig(zes_fan_config_t *pConfig) override;
    ze_result_t setDefaultMode() override;
    ze_result_t setFixedSpeedMode(const zes_fan_speed_t *pSpeed) override;
    ze_result_t setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) override;
    ze_result_t getState(zes_fan_speed_units_t units, int32_t *pSpeed) override;
    LinuxFanImp(OsSysman *pOsSysman, uint32_t fanIndex, bool multipleFansSupported);
    LinuxFanImp() = default;
    ~LinuxFanImp() override = default;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;
    SysFsAccessInterface *pSysfsAccess = nullptr;
    uint32_t fanIndex = 0;
    std::string hwmonDir;
    std::string fanInputNode;
    std::string fanMaxNode;
    std::string pwmNode;
    std::string pwmEnableNode;
    std::array<std::string, maxFanControlPoints> autoPointTempNodes{};
    std::array<std::string, maxFanControlPoints> autoPointPwmNodes{};

  private:
    // fanIndex holds the actual 1-based hwmon channel number (e.g. 1 for fan1_input).
    uint32_t channel() const { return fanIndex; }

    void init();
    ze_result_t readCurvePoints(zes_fan_config_t *pConfig, uint32_t numPoints);
};

} // namespace Sysman
} // namespace L0
