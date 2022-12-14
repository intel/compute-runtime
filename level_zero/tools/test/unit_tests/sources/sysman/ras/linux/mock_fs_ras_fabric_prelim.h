/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/tools/source/sysman/linux/pmu/pmu_imp.h"
#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp_prelim.h"

#include "sysman/linux/fs_access.h"
#include "sysman/linux/os_sysman_imp.h"
#include "sysman/ras/ras.h"
#include "sysman/ras/ras_imp.h"

#include <map>

namespace L0 {
namespace ult {

class MockRasFabricFsAccess : public FsAccess {
  public:
    ze_result_t canRead(const std::string file) override {
        if (accessibleNodes.find(file) != accessibleNodes.end()) {
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ~MockRasFabricFsAccess() override = default;

    bool isRootUser() override {
        return true;
    }

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

    bool directoryExists(const std::string path) override {
        if (std::find(accessibleDirectories.begin(), accessibleDirectories.end(), path) != accessibleDirectories.end()) {
            return true;
        } else {
            return false;
        }
    }

  private:
    std::map<std::string, uint64_t> accessibleNodes = {};
    std::vector<std::string> accessibleDirectories = {};
};

class MockRasFabricSysFsAccess : public SysfsAccess {
  public:
    ze_result_t readSymLink(const std::string path, std::string &buf) override {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t getRealPath(const std::string path, std::string &buf) override {
        buf.append("/mockRealPath");
        return mockRealPathStatus;
    }

    ze_result_t mockRealPathStatus = ZE_RESULT_SUCCESS;
};

struct MockMemoryManagerInRasSysman : public MemoryManagerMock {
    MockMemoryManagerInRasSysman(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

} // namespace ult
} // namespace L0
