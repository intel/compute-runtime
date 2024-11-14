/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/sysman/source/api/vf_management/linux/sysman_os_vf_imp.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string fileForNumberOfVfs = "device/sriov_numvfs";
const std::string fileForLmemUsed = "iov/vf1/telemetry/lmem_alloc_size";
const std::string fileForLmemQuota = "iov/vf1/gt/lmem_quota";
const std::string pathForVfBdf = "device/virtfn0";
const std::string mockBdfdata = "pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:4d:00.1";
const std::string mockInvalidBdfdata = "pci0000:4a";
const std::string mockPathWithInvalidTokens = "pci0000:4a/0000:4a";
const uint64_t mockLmemUsed = 65536;
const uint64_t mockLmemQuota = 128000;
const uint32_t mockNumberOfVfs = 1;

struct MockVfSysfsAccessInterface : public L0::Sysman::SysFsAccessInterface {
    ze_result_t mockError = ZE_RESULT_SUCCESS;
    ze_result_t mockRealPathError = ZE_RESULT_SUCCESS;
    bool mockValidBdfData = true;
    bool mockInvalidTokens = true;
    bool mockLmemValue = true;
    bool mockLmemQuotaValue = true;

    ze_result_t read(const std::string file, uint32_t &val) override {
        if (mockError != ZE_RESULT_SUCCESS) {
            return mockError;
        }

        if (file.compare(fileForNumberOfVfs) == 0) {
            val = mockNumberOfVfs;
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {
        return getVal(file, val);
    }

    ze_result_t getVal(const std::string file, uint64_t &val) {
        if (mockError != ZE_RESULT_SUCCESS) {
            return mockError;
        }

        if (file.compare(fileForLmemUsed) == 0) {
            if (mockLmemValue) {
                val = mockLmemUsed;
            } else {
                val = 0;
            }
            return ZE_RESULT_SUCCESS;
        }

        if (file.compare(fileForLmemQuota) == 0) {
            if (mockLmemQuotaValue) {
                val = mockLmemQuota;
            } else {
                val = 0;
            }
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t getRealPath(const std::string path, std::string &buf) override {
        if (mockRealPathError != ZE_RESULT_SUCCESS) {
            return mockRealPathError;
        }
        if (path.compare(pathForVfBdf) == 0) {
            if (mockValidBdfData) {
                buf = mockBdfdata;
            } else if (mockInvalidTokens) {
                buf = mockPathWithInvalidTokens;
            } else {
                buf = mockInvalidBdfdata;
            }
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    MockVfSysfsAccessInterface() = default;
    ~MockVfSysfsAccessInterface() override = default;
};

class PublicLinuxVfImp : public L0::Sysman::LinuxVfImp {
  public:
    PublicLinuxVfImp(L0::Sysman::OsSysman *pOsSysman, uint32_t vfId) : L0::Sysman::LinuxVfImp(pOsSysman, vfId) {}
    using L0::Sysman::LinuxVfImp::pSysfsAccess;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
