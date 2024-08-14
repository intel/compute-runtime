/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"

#include "gtest/gtest.h"

#include <string>

namespace NEO {
using namespace Ocloc;

class MockSupportedDevicesHelper : public SupportedDevicesHelper {
  public:
    using SupportedDevicesHelper::extractOclocName;
    using SupportedDevicesHelper::getCurrentOclocName;
    using SupportedDevicesHelper::getFormerOclocName;
    using SupportedDevicesHelper::mergeOclocData;

  public:
    MockSupportedDevicesHelper(SupportedDevicesMode mode, ProductConfigHelper *productConfigHelper)
        : SupportedDevicesHelper(mode, productConfigHelper) {}

    std::string getDataFromFormerOcloc() const override {
        if (getDataFromFormerOclocVersionEmptyResult) {
            return "";
        }

        return R"(ocloc-former:
  device_ip_versions:
    - 0x3000001
    - 0x3000002
  ip_to_dev_rev_id:
    - ip: 0x3000001
      revision_id: 1
      device_id: 0x3333
    - ip: 0x3000002
      revision_id: 2
      device_id: 0x4444
  acronym:
    eee: 0x3000001
    fff: 0x3000002
  family_groups:
    FAMILY_FORMER: [0x3000001, 0x3000002]
  release_groups:
    RELEASE_FORMER: [0x3000001, 0x3000002]
)";
    }

    std::string getCurrentOclocName() const override {
        if (!getCurrentOclocNameMockResult.empty()) {
            return getCurrentOclocNameMockResult;
        }
        return SupportedDevicesHelper::getCurrentOclocName();
    }

  public:
    bool getDataFromFormerOclocVersionEmptyResult = false;
    std::string getCurrentOclocNameMockResult = "ocloc-current";
};

} // namespace NEO
