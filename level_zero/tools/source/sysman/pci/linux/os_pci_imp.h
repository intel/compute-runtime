/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/pci/os_pci.h"

namespace L0 {

class SysfsAccess;
class FsAccess;

class LinuxPciImp : public OsPci, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getPciBdf(std::string &bdf) override;
    ze_result_t getMaxLinkSpeed(double &maxLinkSpeed) override;
    ze_result_t getMaxLinkWidth(int32_t &maxLinkwidth) override;
    ze_result_t getLinkGen(int32_t &linkGen) override;
    ze_result_t initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) override;
    LinuxPciImp() = default;
    LinuxPciImp(OsSysman *pOsSysman);
    ~LinuxPciImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;
    FsAccess *pfsAccess = nullptr;

  private:
    static const std::string deviceDir;
    static const std::string resourceFile;
    static const std::string maxLinkSpeedFile;
    static const std::string maxLinkWidthFile;
    bool isLmemSupported = false;
};

} // namespace L0
