/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/pci/sysman_os_pci.h"
#include <level_zero/zes_intel_gpu_sysman.h>

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
    ze_result_t pciLinkSpeedUpdate(ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) override;
    bool resizableBarSupported() override;
    bool resizableBarEnabled(uint32_t barIndex) override;
    ze_result_t initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) override;
    ze_result_t getExtensionProperties(void *pNext) override;
    static uint32_t getRebarCapabilityPos(uint8_t *configMemory, bool isVfBar);
    static uint16_t getLinkRegisterPos(uint8_t *configMem, uint16_t linkRegisterOffset);
    static uint16_t getPcieCapabilityPos(uint8_t *configMem);
    LinuxPciImp() = default;
    LinuxPciImp(OsSysman *pOsSysman);
    ~LinuxPciImp() override = default;

  protected:
    L0::Sysman::SysFsAccessInterface *pSysfsAccess = nullptr;
    L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    ze_result_t getPciConfigMemory(const std::string &pciPath, std::vector<uint8_t> &configMem);

  private:
    static const std::string deviceDir;
    static const std::string resourceFile;
    static const std::string maxLinkSpeedFile;
    static const std::string maxLinkWidthFile;
    void getPciLinkSpeed(zes_pci_speed_t &linkSpeed);
    void getPciDowngradeProperties(zes_pci_link_speed_downgrade_ext_properties_t *pDowngradeProperties);
    zes_pci_link_speed_downgrade_ext_properties_t pciDowngradeProperties = {};
    zes_intel_pci_config_exp_properties_t pciConfigProperties = {};
    ze_result_t pciConfigPropertiesResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    static void getPcieLinkCapabilities(std::vector<uint8_t> &configMemory, uint32_t &capabilityVersion, uint32_t &supportedLinkSpeeds);
    ze_result_t getPciConfigProperties(zes_intel_pci_config_exp_properties_t *pConfigProperties) override;
};

} // namespace Sysman
} // namespace L0
