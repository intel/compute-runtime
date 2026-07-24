/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/driver_info.h"

#include "level_zero/sysman/source/driver/os_sysman_driver.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_drm.h"

#include <memory>
#include <string>

namespace L0 {
namespace Sysman {
namespace ult {

inline const std::string mockRescanPciUuid = "01234567-89ab-cdef-0123-456789abcdef";

// Drm mock that lets tests force queryAdapterBDF() to fail, exercising updateHwDeviceId's error path.
class MockRescanDrm : public SysmanMockDrm {
  public:
    using SysmanMockDrm::SysmanMockDrm;
    int mockQueryAdapterBdfResult = 0;
    int queryAdapterBDF() override {
        return mockQueryAdapterBdfResult;
    }
};

class MockRescanSysfsAccess : public L0::Sysman::SysFsAccessInterface {
  public:
    ze_result_t mockReadResult = ZE_RESULT_SUCCESS;
    std::string mockUuid = mockRescanPciUuid;
    std::string mockBdf = "0000:03:00.0";
    std::string mockRealPath = "/sys/devices/pci0000:03/0000:03:00.0";
    uint32_t getRealPathCalled = 0;

    ze_result_t read(const std::string file, std::string &val) override {
        if (mockReadResult != ZE_RESULT_SUCCESS) {
            return mockReadResult;
        }
        if (file.find("device_uuid") != std::string::npos) {
            val = mockUuid;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    ze_result_t readSymLink(const std::string path, std::string &val) override {
        val = mockBdf;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t getRealPath(const std::string &path, std::string &val) override {
        getRealPathCalled++;
        val = mockRealPath + "/" + mockBdf;
        return ZE_RESULT_SUCCESS;
    }
};

// Procfs access mock returning a canned device node name for updateBdfDependentData().
class MockRescanProcfsAccess : public L0::Sysman::ProcFsAccessInterface {
  public:
    ze_result_t mockGetFileNameResult = ZE_RESULT_SUCCESS;
    ze_result_t getFileName(const ::pid_t pid, const int fd, std::string &val) override {
        val = "/dev/dri/renderD128";
        return mockGetFileNameResult;
    }
    ::pid_t myProcessId() override {
        return ::getpid();
    }
};

class MockRescanOsSysmanDriver : public L0::Sysman::LinuxSysmanDriverImp {
  public:
    ze_result_t mockGetPciBdfAndUuidResult = ZE_RESULT_SUCCESS;
    ze_result_t mockUpdateHwDeviceIdResult = ZE_RESULT_SUCCESS;
    std::string mockPciBdf = "0000:03:00.0";
    std::string mockPciUuid = "";
    uint32_t getPciBdfAndUuidCalled = 0;
    uint32_t updateHwDeviceIdCalled = 0;

    ze_result_t getPciBdfAndUuidForHwDevice(NEO::HwDeviceId *hwDeviceId, std::string &pciBdf, std::string &pciUuid) override {
        getPciBdfAndUuidCalled++;
        pciBdf = mockPciBdf;
        pciUuid = mockPciUuid;
        return mockGetPciBdfAndUuidResult;
    }
    ze_result_t updateHwDeviceId(SysmanDevice *sysmanDevice, const std::string &newBdf) override {
        updateHwDeviceIdCalled++;
        return mockUpdateHwDeviceIdResult;
    }
};

// OsSysman mock allowing control over getPciUuid()/updateBdfDependentData() without real sysfs.
class MockRescanOsSysman : public L0::Sysman::LinuxSysmanImp {
  public:
    using L0::Sysman::LinuxSysmanImp::LinuxSysmanImp;
    std::string mockPciUuid = "";
    ze_result_t mockUpdateBdfResult = ZE_RESULT_SUCCESS;
    uint32_t updateBdfDependentDataCalled = 0;
    bool useMockPciBdfInfo = false;
    NEO::PhysicalDevicePciBusInfo mockPciBdfInfo{};
    bool useMockDeviceUuids = false;
    std::vector<std::string> mockDeviceUuids = {};

    std::string getPciUuid() override {
        return mockPciUuid;
    }
    void getDeviceUuids(std::vector<std::string> &deviceUuids) override {
        if (useMockDeviceUuids) {
            deviceUuids = mockDeviceUuids;
            return;
        }
        L0::Sysman::LinuxSysmanImp::getDeviceUuids(deviceUuids);
    }
    ze_result_t updateBdfDependentData() override {
        updateBdfDependentDataCalled++;
        return mockUpdateBdfResult;
    }
    std::unique_ptr<NEO::PhysicalDevicePciBusInfo> getPciBdfInfo() const override {
        if (useMockPciBdfInfo) {
            return std::make_unique<NEO::PhysicalDevicePciBusInfo>(mockPciBdfInfo);
        }
        return L0::Sysman::LinuxSysmanImp::getPciBdfInfo();
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
