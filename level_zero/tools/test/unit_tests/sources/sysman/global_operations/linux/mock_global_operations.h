/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/global_operations/global_operations_imp.h"
#include "level_zero/tools/source/sysman/global_operations/linux/os_global_operations_imp.h"

namespace L0 {
namespace ult {

const std::string vendorIntel("Intel(R) Corporation");
const std::string unknown("unknown");
const std::string intelPciId("0x8086");
const std::string deviceDir("device");
const std::string subsystemVendorFile("device/subsystem_vendor");
const std::string driverFile("device/driver");
const std::string agamaVersionFile("/sys/module/i915/agama_version");
const std::string srcVersionFile("/sys/module/i915/srcversion");
const std::string functionLevelReset("device/reset");
const std::string clientsDir("clients");
constexpr uint64_t pid1 = 1711u;
constexpr uint64_t pid2 = 1722u;
constexpr uint64_t pid3 = 1723u;
constexpr uint64_t pid4 = 1733u;
constexpr uint64_t pid6 = 1744u;
constexpr uint64_t pid7 = 1755u;
const std::string bPid4 = "<1733>";
constexpr uint64_t engineTimeSpent = 123456u;
const std::string clientId1("4");
const std::string clientId2("5");
const std::string clientId3("6");
const std::string clientId4("7");
const std::string clientId5("8");
const std::string clientId6("10");
const std::string clientId7("11");
const std::string clientId8("12");
const std::string clientId9("13");
const std::string engine0("0");
const std::string engine1("1");
const std::string engine2("2");
const std::string engine3("3");
const std::string engine6("6");
const std::string driverVersion("5.0.0-37-generic SMP mod_unload");
const std::string srcVersion("5.0.0-37");
const std::string ueventWedgedFile("/var/lib/libze_intel_gpu/wedged_file");
const std::string mockFunctionResetPath("/MOCK_FUNCTION_LEVEL_RESET_PATH");
const std::string mockDeviceDir("/MOCK_DEVICE_DIR");
const std::string mockDeviceName("/MOCK_DEVICE_NAME");

struct GlobalOperationsEngineHandleContext : public EngineHandleContext {
    GlobalOperationsEngineHandleContext(OsSysman *pOsSysman) : EngineHandleContext(pOsSysman) {}
};
template <>
struct Mock<GlobalOperationsEngineHandleContext> : public GlobalOperationsEngineHandleContext {
    void initMock() {}
    Mock<GlobalOperationsEngineHandleContext>(OsSysman *pOsSysman) : GlobalOperationsEngineHandleContext(pOsSysman) {}
    MOCK_METHOD(void, init, (), (override));
};

struct GlobalOperationsRasHandleContext : public RasHandleContext {
    GlobalOperationsRasHandleContext(OsSysman *pOsSysman) : RasHandleContext(pOsSysman) {}
};
template <>
struct Mock<GlobalOperationsRasHandleContext> : public GlobalOperationsRasHandleContext {
    void initMock(std::vector<ze_device_handle_t> &deviceHandles) {}
    Mock<GlobalOperationsRasHandleContext>(OsSysman *pOsSysman) : GlobalOperationsRasHandleContext(pOsSysman) {}
    MOCK_METHOD(void, init, (std::vector<ze_device_handle_t> & deviceHandles), (override));
};

struct GlobalOperationsDiagnosticsHandleContext : public DiagnosticsHandleContext {
    GlobalOperationsDiagnosticsHandleContext(OsSysman *pOsSysman) : DiagnosticsHandleContext(pOsSysman) {}
};
template <>
struct Mock<GlobalOperationsDiagnosticsHandleContext> : public GlobalOperationsDiagnosticsHandleContext {
    Mock<GlobalOperationsDiagnosticsHandleContext>(OsSysman *pOsSysman) : GlobalOperationsDiagnosticsHandleContext(pOsSysman) {}
    ADDMETHOD_NOBASE_VOIDRETURN(init, ());
};

struct GlobalOperationsFirmwareHandleContext : public FirmwareHandleContext {
    GlobalOperationsFirmwareHandleContext(OsSysman *pOsSysman) : FirmwareHandleContext(pOsSysman) {}
};
template <>
struct Mock<GlobalOperationsFirmwareHandleContext> : public GlobalOperationsFirmwareHandleContext {
    Mock<GlobalOperationsFirmwareHandleContext>(OsSysman *pOsSysman) : GlobalOperationsFirmwareHandleContext(pOsSysman) {}
    ADDMETHOD_NOBASE_VOIDRETURN(init, ());
};

class GlobalOperationsSysfsAccess : public SysfsAccess {};

template <>
struct Mock<GlobalOperationsSysfsAccess> : public GlobalOperationsSysfsAccess {
    bool isRootSet = true;
    ze_result_t getRealPathVal(const std::string file, std::string &val) {
        if (file.compare(functionLevelReset) == 0) {
            val = mockFunctionResetPath;
        } else if (file.compare(deviceDir) == 0) {
            val = mockDeviceDir;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValString(const std::string file, std::string &val) {
        if (file.compare(subsystemVendorFile) == 0) {
            val = "0x8086";
        } else if (file.compare("clients/8/pid") == 0) {
            val = bPid4;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getFalseValString(const std::string file, std::string &val) {
        if (file.compare(subsystemVendorFile) == 0) {
            val = "0xa086";
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t getValUnsignedLong(const std::string file, uint64_t &val) {
        if ((file.compare("clients/4/pid") == 0) || (file.compare("clients/5/pid") == 0)) {
            val = pid1;
        } else if (file.compare("clients/6/pid") == 0) {
            val = pid2;
        } else if (file.compare("clients/7/pid") == 0) {
            val = pid3;
        } else if (file.compare("clients/10/pid") == 0) {
            val = pid6;
        } else if (file.compare("clients/11/pid") == 0) {
            val = pid7;
        } else if (file.compare("clients/12/pid") == 0) {
            val = pid7;
        } else if (file.compare("clients/13/pid") == 0) {
            val = pid7;
        } else if ((file.compare("clients/4/busy/0") == 0) || (file.compare("clients/4/busy/3") == 0) ||
                   (file.compare("clients/5/busy/1") == 0) || (file.compare("clients/6/busy/0") == 0) ||
                   (file.compare("clients/8/busy/1") == 0) || (file.compare("clients/8/busy/0") == 0) ||
                   (file.compare("clients/13/busy/6") == 0)) {
            val = engineTimeSpent;
        } else if ((file.compare("clients/4/busy/1") == 0) || (file.compare("clients/4/busy/2") == 0) ||
                   (file.compare("clients/5/busy/0") == 0) || (file.compare("clients/5/busy/2") == 0) ||
                   (file.compare("clients/7/busy/0") == 0) || (file.compare("clients/7/busy/2") == 0) ||
                   (file.compare("clients/5/busy/3") == 0) || (file.compare("clients/6/busy/1") == 0) ||
                   (file.compare("clients/6/busy/2") == 0) || (file.compare("clients/6/busy/3") == 0) ||
                   (file.compare("clients/8/busy/2") == 0) || (file.compare("clients/8/busy/3") == 0)) {
            val = 0;
        } else if ((file.compare("clients/4/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/5/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/6/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/8/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/10/total_device_memory_buffer_objects/created_bytes") == 0)) {
            val = 1024;
        } else if ((file.compare("clients/4/total_device_memory_buffer_objects/imported_bytes") == 0) ||
                   (file.compare("clients/5/total_device_memory_buffer_objects/imported_bytes") == 0) ||
                   (file.compare("clients/6/total_device_memory_buffer_objects/imported_bytes") == 0) ||
                   (file.compare("clients/8/total_device_memory_buffer_objects/imported_bytes") == 0) ||
                   (file.compare("clients/10/total_device_memory_buffer_objects/imported_bytes") == 0)) {
            val = 512;
        } else if (file.compare("clients/7/total_device_memory_buffer_objects/created_bytes") == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        } else if (file.compare("clients/7/total_device_memory_buffer_objects/imported_bytes") == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        } else if (file.compare("clients/13/total_device_memory_buffer_objects/imported_bytes") == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLongCreatedBytesSuccess(const std::string file, uint64_t &val) {
        if ((file.compare("clients/4/pid") == 0) || (file.compare("clients/5/pid") == 0)) {
            val = pid1;
        } else if (file.compare("clients/6/pid") == 0) {
            val = pid2;
        } else if ((file.compare("clients/4/busy/0") == 0) || (file.compare("clients/4/busy/3") == 0) ||
                   (file.compare("clients/5/busy/1") == 0) || (file.compare("clients/6/busy/0") == 0) ||
                   (file.compare("clients/8/busy/1") == 0) || (file.compare("clients/8/busy/0") == 0)) {
            val = engineTimeSpent;
        } else if ((file.compare("clients/4/busy/1") == 0) || (file.compare("clients/4/busy/2") == 0) ||
                   (file.compare("clients/5/busy/0") == 0) || (file.compare("clients/5/busy/2") == 0) ||
                   (file.compare("clients/7/busy/0") == 0) || (file.compare("clients/7/busy/2") == 0) ||
                   (file.compare("clients/5/busy/3") == 0) || (file.compare("clients/6/busy/1") == 0) ||
                   (file.compare("clients/6/busy/2") == 0) || (file.compare("clients/6/busy/3") == 0) ||
                   (file.compare("clients/8/busy/2") == 0) || (file.compare("clients/8/busy/3") == 0)) {
            val = 0;
        } else if ((file.compare("clients/4/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/5/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/6/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/8/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/7/total_device_memory_buffer_objects/created_bytes") == 0)) {
            val = 1024;
        } else if ((file.compare("clients/4/total_device_memory_buffer_objects/imported_bytes") == 0) ||
                   (file.compare("clients/5/total_device_memory_buffer_objects/imported_bytes") == 0) ||
                   (file.compare("clients/6/total_device_memory_buffer_objects/imported_bytes") == 0) ||
                   (file.compare("clients/8/total_device_memory_buffer_objects/imported_bytes") == 0)) {
            val = 512;
        } else if (file.compare("clients/7/total_device_memory_buffer_objects/imported_bytes") == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getScannedDir4Entries(const std::string path, std::vector<std::string> &list) {
        if (path.compare(clientsDir) == 0) {
            list.push_back(clientId1);
            list.push_back(clientId2);
            list.push_back(clientId3);
            list.push_back(clientId4);
            list.push_back(clientId5);
            list.push_back(clientId6);
        } else if ((path.compare("clients/4/busy") == 0) || (path.compare("clients/5/busy") == 0) ||
                   (path.compare("clients/6/busy") == 0) || (path.compare("clients/7/busy") == 0) ||
                   (path.compare("clients/8/busy") == 0)) {
            list.push_back(engine0);
            list.push_back(engine1);
            list.push_back(engine2);
            list.push_back(engine3);
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getScannedDirEntries(const std::string path, std::vector<std::string> &list) {
        if (path.compare(clientsDir) == 0) {
            list.push_back(clientId1);
            list.push_back(clientId2);
            list.push_back(clientId3);
            list.push_back(clientId5);
            list.push_back(clientId6);
            list.push_back(clientId7);
        } else if ((path.compare("clients/4/busy") == 0) || (path.compare("clients/5/busy") == 0) ||
                   (path.compare("clients/6/busy") == 0) || (path.compare("clients/8/busy") == 0)) {
            list.push_back(engine0);
            list.push_back(engine1);
            list.push_back(engine2);
            list.push_back(engine3);
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getScannedDirPidEntires(const std::string path, std::vector<std::string> &list) {
        if (path.compare(clientsDir) == 0) {
            list.push_back(clientId8);
        } else if (path.compare("clients/12/busy") == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getScannedDirPidEntiresForClients(const std::string path, std::vector<std::string> &list) {
        if (path.compare(clientsDir) == 0) {
            list.push_back(clientId9);
        } else if (path.compare("clients/13/busy") == 0) {
            list.push_back(engine6);
        }
        return ZE_RESULT_SUCCESS;
    }

    bool mockIsMyDeviceFile(const std::string dev) {
        if (dev.compare(mockDeviceName) == 0) {
            return true;
        }
        return false;
    }

    bool isRootUser() override {
        if (isRootSet == true) {
            return true;
        } else {
            return false;
        }
    }
    Mock<GlobalOperationsSysfsAccess>() = default;

    MOCK_METHOD(ze_result_t, read, (const std::string file, std::string &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, uint64_t &val), (override));
    MOCK_METHOD(ze_result_t, scanDirEntries, (const std::string path, std::vector<std::string> &list), (override));
    MOCK_METHOD(ze_result_t, getRealPath, (const std::string path, std::string &val), (override));
    MOCK_METHOD(ze_result_t, bindDevice, (const std::string device), (override));
    MOCK_METHOD(ze_result_t, unbindDevice, (const std::string device), (override));
    MOCK_METHOD(bool, fileExists, (const std::string file), (override));
    MOCK_METHOD(bool, isMyDeviceFile, (const std::string dev), (override));
};

class GlobalOperationsProcfsAccess : public ProcfsAccess {};

template <>
struct Mock<GlobalOperationsProcfsAccess> : public GlobalOperationsProcfsAccess {

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

    Mock<GlobalOperationsProcfsAccess>() = default;

    MOCK_METHOD(ze_result_t, listProcesses, (std::vector<::pid_t> & list), (override));
    MOCK_METHOD(::pid_t, myProcessId, (), (override));
    MOCK_METHOD(ze_result_t, getFileDescriptors, (const ::pid_t pid, std::vector<int> &list), (override));
    MOCK_METHOD(ze_result_t, getFileName, (const ::pid_t pid, const int fd, std::string &val), (override));
    MOCK_METHOD(bool, isAlive, (const ::pid_t pid), (override));
    MOCK_METHOD(void, kill, (const ::pid_t pid), (override));
};

class GlobalOperationsFsAccess : public FsAccess {};

template <>
struct Mock<GlobalOperationsFsAccess> : public GlobalOperationsFsAccess {
    ze_result_t getValAgamaFile(const std::string file, std::string &val) {
        if (file.compare(agamaVersionFile) == 0) {
            val = driverVersion;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValSrcFile(const std::string file, std::string &val) {
        if (file.compare(srcVersionFile) == 0) {
            val = srcVersion;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValWedgedFileTrue(const std::string file, uint32_t &val) {
        if (file.compare(ueventWedgedFile) == 0) {
            val = 1;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValWedgedFileFalse(const std::string file, uint32_t &val) {
        if (file.compare(ueventWedgedFile) == 0) {
            val = 0;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    Mock<GlobalOperationsFsAccess>() = default;

    MOCK_METHOD(ze_result_t, read, (const std::string file, std::string &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, uint32_t &val), (override));
    MOCK_METHOD(ze_result_t, write, (const std::string file, const std::string val), (override));
    MOCK_METHOD(ze_result_t, canWrite, (const std::string file), (override));
};

class GlobalOpsFwInterface : public FirmwareUtil {};
template <>
struct Mock<GlobalOpsFwInterface> : public GlobalOpsFwInterface {

    ze_result_t mockFwDeviceInit(void) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwDeviceInitFail(void) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    ze_result_t mockIfrReturnTrue(bool &ifrStatus) {
        ifrStatus = true;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockIfrReturnFail(bool &ifrStatus) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    ze_result_t mockIfrReturnFalse(bool &ifrStatus) {
        ifrStatus = false;
        return ZE_RESULT_SUCCESS;
    }
    Mock<GlobalOpsFwInterface>() = default;

    MOCK_METHOD(ze_result_t, fwDeviceInit, (), (override));
    MOCK_METHOD(ze_result_t, getFirstDevice, (igsc_device_info * info), (override));
    MOCK_METHOD(ze_result_t, getFwVersion, (std::string fwType, std::string &firmwareVersion), (override));
    MOCK_METHOD(ze_result_t, flashFirmware, (std::string fwType, void *pImage, uint32_t size), (override));
    MOCK_METHOD(ze_result_t, fwIfrApplied, (bool &ifrStatus), (override));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE(fwGetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t * currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE(fwSetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t newState, uint8_t *currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
};

struct MockGlobalOpsLinuxSysmanImp : public LinuxSysmanImp {
    MockGlobalOpsLinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp) : LinuxSysmanImp(pParentSysmanDeviceImp) {}
    std::vector<int> fdList = {0, 1, 2};
    ::pid_t ourDevicePid = 0;
    int ourDeviceFd = 0;
    ze_result_t mockError = ZE_RESULT_SUCCESS;
    ze_result_t mockInitDeviceError = ZE_RESULT_SUCCESS;
    void getPidFdsForOpenDevice(ProcfsAccess *pProcfsAccess, SysfsAccess *pSysfsAccess, const ::pid_t pid, std::vector<int> &deviceFds) override {
        if (ourDevicePid) {
            deviceFds.push_back(ourDeviceFd);
        }
    }
    void releaseDeviceResources(void) override {}
    ze_result_t initDevice() override {
        if (mockInitDeviceError != ZE_RESULT_SUCCESS) {
            return mockInitDeviceError;
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
    void setMockInitDeviceError(ze_result_t result) {
        mockInitDeviceError = result;
    }
};

class PublicLinuxGlobalOperationsImp : public L0::LinuxGlobalOperationsImp {
  public:
    using LinuxGlobalOperationsImp::pLinuxSysmanImp;
    using LinuxGlobalOperationsImp::resetTimeout;
};

} // namespace ult
} // namespace L0
