/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "sysman/linux/os_sysman_imp.h"
#include "sysman/pci/os_pci.h"

namespace L0 {

class SysfsAccess;
class FsAccess;

class LinuxPciImp : public OsPci, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getPciBdf(zes_pci_properties_t &pciProperties) override;
    ze_result_t getMaxLinkSpeed(double &maxLinkSpeed) override;
    ze_result_t getMaxLinkWidth(int32_t &maxLinkwidth) override;
    ze_result_t getState(zes_pci_state_t *state) override;
    ze_result_t getProperties(zes_pci_properties_t *properties) override;
    bool resizableBarSupported() override;
    bool resizableBarEnabled(uint32_t barIndex) override;
    ze_result_t initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) override;
    LinuxPciImp() = default;
    LinuxPciImp(OsSysman *pOsSysman);
    ~LinuxPciImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;
    FsAccess *pfsAccess = nullptr;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    std::unique_ptr<uint8_t[]> configMemory;
    void pciExtendedConfigRead();
    decltype(&NEO::SysCalls::open) openFunction = NEO::SysCalls::open;
    decltype(&NEO::SysCalls::close) closeFunction = NEO::SysCalls::close;
    decltype(&NEO::SysCalls::pread) preadFunction = NEO::SysCalls::pread;

  private:
    static const std::string deviceDir;
    static const std::string resourceFile;
    static const std::string maxLinkSpeedFile;
    static const std::string maxLinkWidthFile;
    bool isLmemSupported = false;
    uint32_t getDwordFromConfig(uint32_t pos) {
        return configMemory[pos] | (configMemory[pos + 1] << 8) |
               (configMemory[pos + 2] << 16) | (configMemory[pos + 3] << 24);
    }
    uint32_t getRebarCapabilityPos();
};

} // namespace L0
