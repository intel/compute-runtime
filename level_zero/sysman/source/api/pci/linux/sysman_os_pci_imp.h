/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/sysman/source/api/pci/sysman_os_pci.h"

#include <fcntl.h>
#include <memory>

namespace L0 {
namespace Sysman {
class SysFsAccessInterface;
class LinuxSysmanImp;
struct OsSysman;

class LinuxPciImp : public OsPci, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getPciBdf(zes_pci_properties_t &pciProperties) override;
    void getMaxLinkCaps(double &maxLinkSpeed, int32_t &maxLinkWidth) override;
    ze_result_t getState(zes_pci_state_t *state) override;
    ze_result_t getStats(zes_pci_stats_t *stats) override;
    ze_result_t getProperties(zes_pci_properties_t *properties) override;
    bool resizableBarSupported() override;
    bool resizableBarEnabled(uint32_t barIndex) override;
    ze_result_t initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) override;
    static uint32_t getRebarCapabilityPos(uint8_t *configMemory, bool isVfBar);
    static uint16_t getLinkCapabilityPos(uint8_t *configMem);
    LinuxPciImp() = default;
    LinuxPciImp(OsSysman *pOsSysman);
    ~LinuxPciImp() override = default;

  protected:
    L0::Sysman::SysFsAccessInterface *pSysfsAccess = nullptr;
    L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    bool getPciConfigMemory(std::string pciPath, std::vector<uint8_t> &configMem);
    decltype(&NEO::SysCalls::pread) preadFunction = NEO::SysCalls::pread;

  private:
    static const std::string deviceDir;
    static const std::string resourceFile;
    static const std::string maxLinkSpeedFile;
    static const std::string maxLinkWidthFile;
};

} // namespace Sysman
} // namespace L0
