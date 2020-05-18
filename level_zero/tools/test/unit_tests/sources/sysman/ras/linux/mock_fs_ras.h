/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp.h"

#include "sysman/linux/fs_access.h"
#include "sysman/ras/ras.h"
#include "sysman/ras/ras_imp.h"

namespace L0 {
namespace ult {

constexpr uint64_t mockResetCount = 42;
constexpr uint64_t mockComputeErrorCount = 142;
constexpr uint64_t mockNonComputeErrorCount = 242;
constexpr uint64_t mockCacheErrorCount = 342;
constexpr uint64_t mockProgrammingErrorCount = 442;
constexpr uint64_t mockDisplayErrorCount = 542;
constexpr uint64_t mockDriverErrorCount = 642;

class RasFsAccess : public FsAccess {};
template <>
struct Mock<RasFsAccess> : public RasFsAccess {
    std::string mockrasCounterDir = "/var/lib/libze_intel_gpu/";
    std::string mockresetCounter = "ras_reset_count";
    std::string mockComputeErrorCounter = "ras_compute_error_count";
    std::string mockNonComputeErrorCounter = "ras_non_compute_error_count";
    std::string mockCacheErrorCounter = "ras_cache_error_count";
    std::string mockProgrammingErrorCounter = "ras_programming_error_count";
    std::string mockDisplayErrorCounter = "ras_display_error_count";
    std::string mockDriverErrorCounter = "ras_driver_error_count";
    MOCK_METHOD(ze_result_t, read, (const std::string file, uint64_t &val), (override));
    MOCK_METHOD(bool, fileExists, (const std::string file), (override));

    ze_result_t setResetCounterFileName(const std::string file) {
        mockresetCounter.assign(file);
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t setRasDirName(const std::string dirName) {
        mockrasCounterDir.assign(dirName);
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t getVal(const std::string file, uint64_t &val) {
        if (file.compare(mockrasCounterDir + mockresetCounter) == 0) {
            val = mockResetCount;
            return ZE_RESULT_SUCCESS;
        }
        if (file.compare(mockrasCounterDir + mockComputeErrorCounter) == 0) {
            val = mockComputeErrorCount;
            return ZE_RESULT_SUCCESS;
        }
        if (file.compare(mockrasCounterDir + mockNonComputeErrorCounter) == 0) {
            val = mockNonComputeErrorCount;
            return ZE_RESULT_SUCCESS;
        }
        if (file.compare(mockrasCounterDir + mockCacheErrorCounter) == 0) {
            val = mockCacheErrorCount;
            return ZE_RESULT_SUCCESS;
        }
        if (file.compare(mockrasCounterDir + mockProgrammingErrorCounter) == 0) {
            val = mockProgrammingErrorCount;
            return ZE_RESULT_SUCCESS;
        }
        if (file.compare(mockrasCounterDir + mockDisplayErrorCounter) == 0) {
            val = mockDisplayErrorCount;
            return ZE_RESULT_SUCCESS;
        }
        if (file.compare(mockrasCounterDir + mockDriverErrorCounter) == 0) {
            val = mockDriverErrorCount;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    bool checkFileExists(const std::string file) {
        if ((file.compare(mockrasCounterDir + mockresetCounter) == 0) ||
            (file.compare(mockrasCounterDir + mockComputeErrorCounter) == 0) ||
            (file.compare(mockrasCounterDir + mockNonComputeErrorCounter) == 0) ||
            (file.compare(mockrasCounterDir + mockCacheErrorCounter) == 0) ||
            (file.compare(mockrasCounterDir + mockProgrammingErrorCounter) == 0) ||
            (file.compare(mockrasCounterDir + mockDisplayErrorCounter) == 0) ||
            (file.compare(mockrasCounterDir + mockDriverErrorCounter) == 0) ||
            (file.compare(mockrasCounterDir) == 0)) {
            return true;
        }
        return false;
    }
    Mock() = default;
    ~Mock() override = default;
};

class PublicLinuxRasImp : public L0::LinuxRasImp {
  public:
    using LinuxRasImp::pFsAccess;
};
} // namespace ult
} // namespace L0
