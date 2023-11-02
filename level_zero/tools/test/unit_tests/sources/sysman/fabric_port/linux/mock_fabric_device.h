/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/tools/source/sysman/fabric_port/fabric_port.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {
namespace ult {

const uint32_t mockNumPorts = 2;
struct MockFabricDevice : public FabricDevice {
    uint32_t getNumPorts() override {
        return mockNumPorts;
    }

    ADDMETHOD_NOBASE(getOsFabricDevice, OsFabricDevice *, nullptr, ());
    MockFabricDevice() = default;
};

class MockFabricFsAccess : public FsAccess {
  public:
    ze_result_t mockListDirectory = ZE_RESULT_SUCCESS;
    ze_result_t canRead(const std::string file) override {
        if (accessibleNodes.find(file) != accessibleNodes.end()) {
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t listDirectory(const std::string path, std::vector<std::string> &list) override {
        list = accessibleDirectories;
        return mockListDirectory;
    }

    ~MockFabricFsAccess() override = default;

    ze_result_t read(const std::string file, uint64_t &val) override {
        if (canRead(file) == ZE_RESULT_SUCCESS) {
            val = accessibleNodes[file];
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    void setAccessibleNodes(std::map<std::string, uint64_t> &nodes) {
        accessibleNodes = nodes;
    }

    void setAccessibleDirectories(std::vector<std::string> &dirs) {
        accessibleDirectories = dirs;
    }

  private:
    std::map<std::string, uint64_t> accessibleNodes = {};
    std::vector<std::string> accessibleDirectories = {};
};

class MockFabricSysFsAccess : public SysfsAccess {
  public:
    ze_result_t getRealPath(const std::string path, std::string &buf) override {
        buf.append("/mockRealPath");
        return mockRealPathStatus;
    }

    ze_result_t mockRealPathStatus = ZE_RESULT_SUCCESS;
};

} // namespace ult
} // namespace L0
