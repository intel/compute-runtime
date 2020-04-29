/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"

#include "sysman/pci/os_pci.h"
#include "sysman/pci/pci_imp.h"
#include "sysman/sysman.h"
#include "sysman/sysman_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct Mock<OsPci> : public OsPci {

    Mock<OsPci>() = default;

    MOCK_METHOD1(getPciBdf, ze_result_t(std::string &bdf));
    MOCK_METHOD1(getMaxLinkSpeed, ze_result_t(double &maxLinkSpeed));
    MOCK_METHOD1(getMaxLinkWidth, ze_result_t(uint32_t &maxLinkWidth));
    MOCK_METHOD1(getLinkGen, ze_result_t(uint32_t &linkGen));
    MOCK_METHOD1(initializeBarProperties, ze_result_t(std::vector<zet_pci_bar_properties_t *> &pBarProperties));
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
