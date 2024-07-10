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
    using SupportedDevicesHelper::extractOclocVersion;
    using SupportedDevicesHelper::getOclocCurrentLibName;
    using SupportedDevicesHelper::getOclocCurrentVersion;
    using SupportedDevicesHelper::getOclocFormerLibName;
    using SupportedDevicesHelper::getOclocFormerVersion;
    using SupportedDevicesHelper::mergeOclocVersionData;

  public:
    MockSupportedDevicesHelper(SupportedDevicesMode mode, ProductConfigHelper *productConfigHelper)
        : SupportedDevicesHelper(mode, productConfigHelper) {}

    std::string getOclocCurrentLibName() const override {
        if (!getOclocCurrentLibNameMockResult.empty()) {
            return getOclocCurrentLibNameMockResult;
        }
        return SupportedDevicesHelper::getOclocCurrentLibName();
    }

    std::string getOclocFormerLibName() const override {
        if (!getOclocFormerLibNameMockResult.empty()) {
            return getOclocFormerLibNameMockResult;
        }
        return SupportedDevicesHelper::getOclocFormerLibName();
    }

    std::string getDataFromFormerOclocVersion() const override {
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

    std::string getOclocCurrentVersion() const override {
        if (!getOclocCurrentVersionMockResult.empty()) {
            return getOclocCurrentVersionMockResult;
        }
        return SupportedDevicesHelper::getOclocCurrentVersion();
    }

  public:
    std::string getOclocCurrentLibNameMockResult = "libocloc-current.so";
    std::string getOclocFormerLibNameMockResult = "libocloc-former.so";
    bool getDataFromFormerOclocVersionEmptyResult = false;
    std::string getOclocCurrentVersionMockResult = "ocloc-current";
};

} // namespace NEO
