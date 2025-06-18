/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/diagnostics/linux/sysman_os_diagnostics_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string mockQuiescentGpuFile("quiesce_gpu");
const std::string mockinvalidateLmemFile("invalidate_lmem_mmaps");
const std::vector<std::string> mockSupportedDiagTypes = {"MOCKSUITE1", "MOCKSUITE2"};
const std::string deviceDirDiag("device");
const std::string mockRealPathConfig("/sys/devices/pci0000:89/0000:89:02.0/config");
const std::string mockdeviceDirDiag("/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0");
const std::string mockdeviceDirConfig("/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/config");
const std::string mockDeviceName("/MOCK_DEVICE_NAME");
const std::string mockRemove("remove");
const std::string mockRescan("rescan");
const std::string mockSlotPath("/sys/bus/pci/slots/");
const std::string mockSlotPath1("/sys/bus/pci/slots/1/");
const std::string mockCorrectRootAddress("0000:8a:00.0");
const std::string mockWrongRootAddress("0000:7a:00.0");

struct MockDiagnosticsFwInterface : public L0::Sysman::FirmwareUtil {
    zes_diag_result_t mockDiagResult = ZES_DIAG_RESULT_NO_ERRORS;
    ze_result_t mockFwInitResult = ZE_RESULT_SUCCESS;
    ze_result_t mockFwRunDiagTestsResult = ZE_RESULT_SUCCESS;
    ze_result_t fwDeviceInit(void) override {
        return mockFwInitResult;
    }
    void setFwInitRetVal(ze_result_t val) {
        mockFwInitResult = val;
    }
    ze_result_t fwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) override {
        supportedDiagTests.push_back(mockSupportedDiagTypes[0]);
        supportedDiagTests.push_back(mockSupportedDiagTypes[1]);
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t fwRunDiagTests(std::string &osDiagType, zes_diag_result_t *pResult) override {
        *pResult = mockDiagResult;
        return mockFwRunDiagTestsResult;
    }

    void setDiagResult(zes_diag_result_t result) {
        mockDiagResult = result;
    }

    MockDiagnosticsFwInterface() = default;

    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
    ADDMETHOD_NOBASE(getFlashFirmwareProgress, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCompletionPercent));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwIfrApplied, ze_result_t, ZE_RESULT_SUCCESS, (bool &ifrStatus));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE(fwGetEccAvailable, ze_result_t, ZE_RESULT_SUCCESS, (ze_bool_t * pAvailable));
    ADDMETHOD_NOBASE(fwGetEccConfigurable, ze_result_t, ZE_RESULT_SUCCESS, (ze_bool_t * pConfigurable));
    ADDMETHOD_NOBASE(fwGetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t * currentState, uint8_t *pendingState, uint8_t *defaultState));
    ADDMETHOD_NOBASE(fwSetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t newState, uint8_t *currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
    ADDMETHOD_NOBASE_VOIDRETURN(fwGetMemoryHealthIndicator, (zes_mem_health_t * health));
    ADDMETHOD_NOBASE_VOIDRETURN(getLateBindingSupportedFwTypes, (std::vector<std::string> & fwTypes));
};
struct MockGlobalOperationsEngineHandleContext : public L0::Sysman::EngineHandleContext {
    MockGlobalOperationsEngineHandleContext(L0::Sysman::OsSysman *pOsSysman) : EngineHandleContext(pOsSysman) {}
    ADDMETHOD_NOBASE_VOIDRETURN(init, (uint32_t subDeviceCount));
};

struct MockDiagFsAccess : public L0::Sysman::FsAccessInterface {
    ze_result_t mockReadError = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteError = ZE_RESULT_SUCCESS;
    ze_result_t mockListDirError = ZE_RESULT_SUCCESS;
    int checkErrorAfterCount = 0;
    std::string mockRootAddress = mockCorrectRootAddress;

    ze_result_t write(const std::string file, std::string val) override {
        if (checkErrorAfterCount) {
            checkErrorAfterCount--;
        } else {
            if (mockWriteError != ZE_RESULT_SUCCESS) {
                return mockWriteError;
            }
        }
        if (!file.compare(mockSlotPath1 + "power")) {
            return ZE_RESULT_SUCCESS;
        }
        if (std::string::npos != file.find(mockRemove)) {
            return ZE_RESULT_SUCCESS;
        } else if (std::string::npos != file.find(mockRescan)) {
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    ze_result_t listDirectory(const std::string directory, std::vector<std::string> &listOfslots) override {
        if (mockListDirError != ZE_RESULT_SUCCESS) {
            return mockListDirError;
        }
        if (directory.compare(mockSlotPath) == 0) {
            listOfslots.push_back("1");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    ze_result_t read(const std::string file, std::string &val) override {
        if (mockReadError != ZE_RESULT_SUCCESS) {
            return mockReadError;
        }
        if (file.compare(mockSlotPath1)) {
            val = mockRootAddress;
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }
    void setWrongMockAddress() {
        mockRootAddress = mockWrongRootAddress;
    }

    MockDiagFsAccess() = default;
};

struct MockDiagSysfsAccess : public L0::Sysman::SysFsAccessInterface {
    ze_result_t mockError = ZE_RESULT_SUCCESS;
    int checkErrorAfterCount = 0;
    ze_result_t getRealPath(const std::string file, std::string &val) override {
        if (mockError != ZE_RESULT_SUCCESS) {
            return mockError;
        }
        if (file.compare(deviceDirDiag) == 0) {
            val = mockdeviceDirDiag;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t write(const std::string file, const int val) override {
        if (checkErrorAfterCount) {
            checkErrorAfterCount--;
        } else {
            if (mockError != ZE_RESULT_SUCCESS) {
                return mockError;
            }
        }
        if (std::string::npos != file.find(mockQuiescentGpuFile)) {
            if (checkErrorAfterCount) {
                return mockError;
            }
            return ZE_RESULT_SUCCESS;
        } else if (std::string::npos != file.find(mockinvalidateLmemFile)) {
            if (checkErrorAfterCount) {
                return mockError;
            }
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    bool isMyDeviceFile(const std::string dev) override {
        if (dev.compare(mockDeviceName) == 0) {
            return true;
        }
        return false;
    }

    void setMockError(ze_result_t result) {
        mockError = result;
    }
    void setErrorAfterCount(int count, ze_result_t result) {
        checkErrorAfterCount = count;
        setMockError(result);
    }
    MockDiagSysfsAccess() = default;
};

struct MockDiagProcfsAccess : public L0::Sysman::ProcFsAccessInterface {
    std::vector<::pid_t> pidList = {1, 2, 3};
    ::pid_t ourDevicePid = 0;
    ze_result_t mockError = ZE_RESULT_SUCCESS;

    ze_result_t listProcesses(std::vector<::pid_t> &list) override {
        if (mockError != ZE_RESULT_SUCCESS) {
            return mockError;
        }
        list = pidList;
        if (ourDevicePid) {
            list.push_back(ourDevicePid);
        }
        return ZE_RESULT_SUCCESS;
    }

    ::pid_t myProcessId() override {
        return ::getpid();
    }

    void kill(const ::pid_t pid) override {
        ourDevicePid = 0;
    }

    void setMockError(ze_result_t result) {
        mockError = result;
    }

    MockDiagProcfsAccess() = default;
};

struct MockDiagLinuxSysmanImp : public L0::Sysman::LinuxSysmanImp {
    using L0::Sysman::LinuxSysmanImp::pProcfsAccess;
    MockDiagLinuxSysmanImp(L0::Sysman::SysmanDeviceImp *pParentSysmanDeviceImp) : L0::Sysman::LinuxSysmanImp(pParentSysmanDeviceImp) {}
    std::vector<int> fdList = {0, 1, 2};
    ::pid_t ourDevicePid = 0;
    int ourDeviceFd = 0;
    ze_result_t mockError = ZE_RESULT_SUCCESS;
    ze_result_t mockReInitSysmanDeviceError = ZE_RESULT_SUCCESS;
    void getPidFdsForOpenDevice(const ::pid_t pid, std::vector<int> &deviceFds) override {
        if (ourDevicePid) {
            deviceFds.push_back(ourDeviceFd);
        }
    }
    void releaseSysmanDeviceResources(void) override {}

    ze_result_t reInitSysmanDeviceResources() override {
        if (mockReInitSysmanDeviceError != ZE_RESULT_SUCCESS) {
            return mockReInitSysmanDeviceError;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t osWarmReset() override {
        if (mockError != ZE_RESULT_SUCCESS) {
            return mockError;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t osColdReset() override {
        if (mockError != ZE_RESULT_SUCCESS) {
            return mockError;
        }
        return ZE_RESULT_SUCCESS;
    }
    void setMockError(ze_result_t result) {
        mockError = result;
    }
    void setMockReInitSysmanDeviceError(ze_result_t result) {
        mockReInitSysmanDeviceError = result;
    }
};
class PublicLinuxDiagnosticsImp : public L0::Sysman::LinuxDiagnosticsImp {
  public:
    using L0::Sysman::LinuxDiagnosticsImp::pFwInterface;
    using L0::Sysman::LinuxDiagnosticsImp::pLinuxSysmanImp;
    using L0::Sysman::LinuxDiagnosticsImp::pSysfsAccess;
    using L0::Sysman::LinuxDiagnosticsImp::waitForQuiescentCompletion;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
