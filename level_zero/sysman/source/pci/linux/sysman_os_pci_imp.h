/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/sysman/source/pci/sysman_os_pci.h"

#include <fcntl.h>
#include <memory>

namespace L0 {
namespace Sysman {
class SysfsAccess;
class FsAccess;
class LinuxSysmanImp;
struct OsSysman;

class LinuxPciImp : public OsPci, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getPciBdf(zes_pci_properties_t &pciProperties) override;
    void getMaxLinkCaps(double &maxLinkSpeed, int32_t &maxLinkWidth) override;
    ze_result_t getState(zes_pci_state_t *state) override;
    ze_result_t getProperties(zes_pci_properties_t *properties) override;
    bool resizableBarSupported() override;
    bool resizableBarEnabled(uint32_t barIndex) override;
    ze_result_t initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) override;
    static uint32_t getRebarCapabilityPos(uint8_t *configMemory, bool isVfBar);
    LinuxPciImp() = default;
    LinuxPciImp(OsSysman *pOsSysman);
    ~LinuxPciImp() override = default;

  protected:
    L0::Sysman::SysfsAccess *pSysfsAccess = nullptr;
    L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    std::unique_ptr<uint8_t[]> configMemory;
    std::unique_ptr<uint8_t[]> uspConfigMemory;
    void pciExtendedConfigRead();
    void pciCardBusConfigRead();
    decltype(&NEO::SysCalls::open) openFunction = NEO::SysCalls::open;
    decltype(&NEO::SysCalls::close) closeFunction = NEO::SysCalls::close;
    decltype(&NEO::SysCalls::pread) preadFunction = NEO::SysCalls::pread;

  private:
    static const std::string deviceDir;
    static const std::string resourceFile;
    static const std::string maxLinkSpeedFile;
    static const std::string maxLinkWidthFile;
    bool isIntegratedDevice = false;
    static inline uint32_t getDwordFromConfig(uint32_t pos, uint8_t *configMemory) {
        return configMemory[pos] | (configMemory[pos + 1] << 8) |
               (configMemory[pos + 2] << 16) | (configMemory[pos + 3] << 24);
    }
    uint16_t getWordFromConfig(uint32_t pos, uint8_t *configMem) {
        return configMem[pos] | (configMem[pos + 1] << 8);
    }
    uint8_t getByteFromConfig(uint32_t pos, uint8_t *configMem) {
        return configMem[pos];
    }
    uint32_t getRebarCapabilityPos();
    uint16_t getLinkCapabilityPos();
};

} // namespace Sysman
} // namespace L0
