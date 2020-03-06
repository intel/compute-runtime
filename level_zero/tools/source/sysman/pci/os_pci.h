/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zet_api.h>

#include "sysman/os_sysman.h"

#include <string>
#include <vector>

namespace L0 {

class OsPci {
  public:
    virtual ze_result_t getPciBdf(std::string &bdf) = 0;
    virtual ze_result_t getMaxLinkSpeed(double &maxLinkSpeed) = 0;
    virtual ze_result_t getMaxLinkWidth(uint32_t &maxLinkWidth) = 0;
    virtual ze_result_t getLinkGen(uint32_t &linkGen) = 0;
    virtual ze_result_t initializeBarProperties(std::vector<zet_pci_bar_properties_t *> &pBarProperties) = 0;
    static OsPci *create(OsSysman *pOsSysman);
    virtual ~OsPci() {}
};

} // namespace L0
