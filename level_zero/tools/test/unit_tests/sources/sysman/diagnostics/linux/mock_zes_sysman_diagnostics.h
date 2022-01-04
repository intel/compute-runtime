/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/diagnostics/linux/os_diagnostics_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

uint32_t mockDiagHandleCount = 2;
const std::string mockQuiescentGpuFile("quiesce_gpu");
const std::string mockinvalidateLmemFile("invalidate_lmem_mmaps");
const std::vector<std::string> mockSupportedDiagTypes = {"MOCKSUITE1", "MOCKSUITE2"};
const std::string deviceDirDiag("device");
const std::string mockdeviceDirDiag("/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0");
const std::string mockRemove("remove");
const std::string mockRescan("rescan");
const std::string mockRealPathConfig("/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/config");
const std::string mockRootAddress("0000:8a:00.0");
const std::string mockRootWrongAddress("0000:7a:00.0");
const std::string mockSlotPath1("/sys/bus/pci/slots/1/");
const std::string mockDeviceName("/MOCK_DEVICE_NAME");
const std::string mockSlotPath("/sys/bus/pci/slots/");

class DiagnosticsFwInterface : public FirmwareUtil {};

template <>
struct Mock<DiagnosticsFwInterface> : public FirmwareUtil {

    ze_result_t mockFwDeviceInit(void) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwDeviceInitFail(void) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    ze_result_t mockGetFirstDevice(igsc_device_info *info) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) {
        supportedDiagTests.push_back(mockSupportedDiagTypes[0]);
        supportedDiagTests.push_back(mockSupportedDiagTypes[1]);
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwRunDiagTestsReturnSuccess(std::string &osDiagType, zes_diag_result_t *pResult) {
        *pResult = ZES_DIAG_RESULT_NO_ERRORS;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwRunDiagTestsReturnSuccessWithResultRepair(std::string &osDiagType, zes_diag_result_t *pResult) {
        *pResult = ZES_DIAG_RESULT_REBOOT_FOR_REPAIR;
        return ZE_RESULT_SUCCESS;
    }

    Mock<DiagnosticsFwInterface>() = default;

    MOCK_METHOD(ze_result_t, fwDeviceInit, (), (override));
    MOCK_METHOD(ze_result_t, getFirstDevice, (igsc_device_info * info), (override));
    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwIfrApplied, ze_result_t, ZE_RESULT_SUCCESS, (bool &ifrStatus));
    MOCK_METHOD(ze_result_t, fwSupportedDiagTests, (std::vector<std::string> & supportedDiagTests), (override));
    MOCK_METHOD(ze_result_t, fwRunDiagTests, (std::string & osDiagType, zes_diag_result_t *pResult), (override));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
};
struct MockGlobalOperationsEngineHandleContext : public EngineHandleContext {
    MockGlobalOperationsEngineHandleContext(OsSysman *pOsSysman) : EngineHandleContext(pOsSysman) {}
    void init() override {}
};

class DiagSysfsAccess : public SysfsAccess {};
template <>
struct Mock<DiagSysfsAccess> : public DiagSysfsAccess {

    ze_result_t getRealPathVal(const std::string file, std::string &val) {
        if (file.compare(deviceDirDiag) == 0) {
            val = mockdeviceDirDiag;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t mockwrite(const std::string file, const int val) {
        if (std::string::npos != file.find(mockQuiescentGpuFile)) {
            return ZE_RESULT_SUCCESS;
        } else if (std::string::npos != file.find(mockinvalidateLmemFile)) {
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    ze_result_t mockwriteFails(const std::string file, const int val) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    bool mockIsMyDeviceFile(const std::string dev) {
        if (dev.compare(mockDeviceName) == 0) {
            return true;
        }
        return false;
    }

    Mock<DiagSysfsAccess>() = default;

    MOCK_METHOD(ze_result_t, write, (const std::string file, const int val), (override));
    MOCK_METHOD(ze_result_t, getRealPath, (const std::string path, std::string &val), (override));
    MOCK_METHOD(bool, isMyDeviceFile, (const std::string dev), (override));
};

class DiagFsAccess : public FsAccess {};
template <>
struct Mock<DiagFsAccess> : public DiagFsAccess {

    ze_result_t mockFsWrite(const std::string file, std::string val) {
        if (std::string::npos != file.find(mockRemove)) {
            return ZE_RESULT_SUCCESS;
        } else if (std::string::npos != file.find(mockRescan)) {
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    ze_result_t mockFsReadAddress(const std::string file, std::string &val) {
        if (file.compare(mockSlotPath1)) {
            val = mockRootAddress;
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    ze_result_t mockFsReadWrongAddress(const std::string file, std::string &val) {
        if (file.compare(mockSlotPath1)) {
            val = mockRootWrongAddress;
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    ze_result_t listDirectorySuccess(const std::string directory, std::vector<std::string> &listOfslots) {
        if (directory.compare(mockSlotPath) == 0) {
            listOfslots.push_back("1");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t listDirectoryFailure(const std::string directory, std::vector<std::string> &events) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getRealPathVal(const std::string file, std::string &val) {
        if (file.compare(deviceDirDiag) == 0) {
            val = mockdeviceDirDiag;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    Mock<DiagFsAccess>() = default;

    MOCK_METHOD(ze_result_t, write, (const std::string file, const std::string val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, std::string &val), (override));
    MOCK_METHOD(ze_result_t, getRealPath, (const std::string path, std::string &val), (override));
    MOCK_METHOD(ze_result_t, listDirectory, (const std::string path, std::vector<std::string> &list), (override));
};

class DiagProcfsAccess : public ProcfsAccess {};

template <>
struct Mock<DiagProcfsAccess> : public DiagProcfsAccess {

    const ::pid_t extraPid = 4;
    const int extraFd = 5;
    std::vector<::pid_t> pidList = {1, 2, 3};
    std::vector<int> fdList = {0, 1, 2};
    ::pid_t ourDevicePid = 0;
    int ourDeviceFd = 0;

    ze_result_t mockProcessListDeviceUnused(std::vector<::pid_t> &list) {
        list = pidList;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t mockProcessListDeviceInUse(std::vector<::pid_t> &list) {
        list = pidList;
        if (ourDevicePid) {
            list.push_back(ourDevicePid);
        }
        return ZE_RESULT_SUCCESS;
    }

    ::pid_t getMockMyProcessId() {
        return ::getpid();
    }

    ze_result_t getMockFileDescriptors(const ::pid_t pid, std::vector<int> &list) {
        // Give every process 3 file descriptors
        // Except the device that MOCK has the device open. Give it one extra.
        list.clear();
        list = fdList;
        if (ourDevicePid == pid) {
            list.push_back(ourDeviceFd);
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getMockFileDescriptorsFailure(const ::pid_t pid, std::vector<int> &list) {
        //return failure to verify the error condition check
        list.clear();
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t getMockFileName(const ::pid_t pid, const int fd, std::string &val) {
        if (pid == ourDevicePid && fd == ourDeviceFd) {
            val = mockDeviceName;
        } else {
            // return fake filenames for other file descriptors
            val = std::string("/FILENAME") + std::to_string(fd);
        }
        return ZE_RESULT_SUCCESS;
    }

    bool mockIsAlive(const ::pid_t pid) {
        if (pid == ourDevicePid) {
            return true;
        }
        return false;
    }

    void mockKill(const ::pid_t pid) {
        ourDevicePid = 0;
    }

    ze_result_t mockFsWrite(const std::string file, std::string val) {
        if (std::string::npos != file.find(mockRemove)) {
            return ZE_RESULT_SUCCESS;
        } else if (std::string::npos != file.find(mockRescan)) {
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    Mock<DiagProcfsAccess>() = default;

    MOCK_METHOD(ze_result_t, listProcesses, (std::vector<::pid_t> & list), (override));
    MOCK_METHOD(::pid_t, myProcessId, (), (override));
    MOCK_METHOD(ze_result_t, getFileDescriptors, (const ::pid_t pid, std::vector<int> &list), (override));
    MOCK_METHOD(ze_result_t, getFileName, (const ::pid_t pid, const int fd, std::string &val), (override));
    MOCK_METHOD(bool, isAlive, (const ::pid_t pid), (override));
    MOCK_METHOD(void, kill, (const ::pid_t pid), (override));
    MOCK_METHOD(ze_result_t, listDirectory, (const std::string path, std::vector<std::string> &list), (override));
    MOCK_METHOD(ze_result_t, write, (const std::string file, const std::string val), (override));
};

class PublicLinuxDiagnosticsImp : public L0::LinuxDiagnosticsImp {
  public:
    using LinuxDiagnosticsImp::closeFunction;
    using LinuxDiagnosticsImp::openFunction;
    using LinuxDiagnosticsImp::pDevice;
    using LinuxDiagnosticsImp::pFsAccess;
    using LinuxDiagnosticsImp::pFwInterface;
    using LinuxDiagnosticsImp::pLinuxSysmanImp;
    using LinuxDiagnosticsImp::pProcfsAccess;
    using LinuxDiagnosticsImp::preadFunction;
    using LinuxDiagnosticsImp::pSysfsAccess;
    using LinuxDiagnosticsImp::pwriteFunction;
};
} // namespace ult
} // namespace L0
