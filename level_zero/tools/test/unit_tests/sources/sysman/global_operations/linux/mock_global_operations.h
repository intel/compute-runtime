/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

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
const std::string mockDeviceDir("devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0");
const std::string mockFunctionResetPath(mockDeviceDir + "/reset");
const std::string mockDeviceName("/MOCK_DEVICE_NAME");

enum MockEnumListProcessCall {
    DEVICE_IN_USE = 0,
    DEVICE_UNUSED = 1,
    RETURN_ERROR = 2
};

struct MockGlobalOperationsEngineHandleContext : public EngineHandleContext {
    MockGlobalOperationsEngineHandleContext(OsSysman *pOsSysman) : EngineHandleContext(pOsSysman) {}
    ADDMETHOD_NOBASE_VOIDRETURN(init, (std::vector<ze_device_handle_t> & deviceHandles));
};

struct MockGlobalOperationsRasHandleContext : public RasHandleContext {
    MockGlobalOperationsRasHandleContext(OsSysman *pOsSysman) : RasHandleContext(pOsSysman) {}
    ADDMETHOD_NOBASE_VOIDRETURN(init, (std::vector<ze_device_handle_t> & deviceHandles));
};

struct MockGlobalOperationsDiagnosticsHandleContext : public DiagnosticsHandleContext {
    MockGlobalOperationsDiagnosticsHandleContext(OsSysman *pOsSysman) : DiagnosticsHandleContext(pOsSysman) {}
    ADDMETHOD_NOBASE_VOIDRETURN(init, ());
};

struct MockGlobalOperationsFirmwareHandleContext : public FirmwareHandleContext {
    MockGlobalOperationsFirmwareHandleContext(OsSysman *pOsSysman) : FirmwareHandleContext(pOsSysman) {}
    ADDMETHOD_NOBASE_VOIDRETURN(init, ());
};

struct MockGlobalOperationsSysfsAccess : public SysfsAccess {

    ze_result_t mockScanDirEntriesError = ZE_RESULT_SUCCESS;
    ze_result_t mockReadError = ZE_RESULT_SUCCESS;
    ze_result_t mockBindDeviceError = ZE_RESULT_SUCCESS;
    ze_result_t mockUnbindDeviceError = ZE_RESULT_SUCCESS;
    uint32_t mockCount = 0;
    bool isRootSet = true;
    bool mockGetScannedDir4EntriesStatus = false;
    bool mockGetScannedDirPidEntriesStatus = false;
    bool mockGetScannedDirPidEntriesForClientsStatus = false;
    bool mockReadStatus = false;
    bool mockGetValUnsignedLongStatus = false;
    bool mockDeviceUnbound = false;

    ze_result_t getRealPath(const std::string file, std::string &val) override {
        if (file.compare(deviceDir) == 0) {
            val = mockDeviceDir;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return mockDeviceUnbound ? ZE_RESULT_ERROR_NOT_AVAILABLE : ZE_RESULT_SUCCESS;
    }

    ze_result_t readResult = ZE_RESULT_SUCCESS;
    std::string mockReadVal = "";
    ze_result_t read(const std::string file, std::string &val) override {
        if (mockReadError != ZE_RESULT_SUCCESS) {
            return mockReadError;
        }

        if (file.compare(subsystemVendorFile) == 0) {
            val = mockReadVal;
        } else if (file.compare("clients/8/pid") == 0) {
            val = bPid4;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return readResult;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {
        if (mockReadStatus == true) {
            if (mockCount == 0) {
                mockCount++;
                return getValUnsignedLongCreatedBytesSuccess(file, val);
            }

            else {
                return ZE_RESULT_ERROR_UNKNOWN;
            }
        }

        if (mockReadError != ZE_RESULT_SUCCESS) {
            return mockReadError;
        }

        if (mockGetValUnsignedLongStatus == true) {
            return getValUnsignedLongCreatedBytesSuccess(file, val);
        }

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

    ze_result_t scanDirEntries(const std::string path, std::vector<std::string> &list) override {
        if (mockScanDirEntriesError != ZE_RESULT_SUCCESS) {
            return mockScanDirEntriesError;
        }

        if (mockGetScannedDir4EntriesStatus == true) {
            return getScannedDir4Entries(path, list);
        }

        if (mockGetScannedDirPidEntriesStatus == true) {
            return getScannedDirPidEntries(path, list);
        }

        if (mockGetScannedDirPidEntriesForClientsStatus == true) {
            return getScannedDirPidEntiresForClients(path, list);
        }

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

    ze_result_t getScannedDirPidEntries(const std::string path, std::vector<std::string> &list) {
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

    bool isMyDeviceFile(const std::string dev) override {
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

    ze_result_t unbindDevice(const std::string device) override {
        if (mockUnbindDeviceError != ZE_RESULT_SUCCESS) {
            return mockUnbindDeviceError;
        }

        mockDeviceUnbound = true;

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t bindDevice(const std::string device) override {
        if (mockBindDeviceError != ZE_RESULT_SUCCESS) {
            return mockBindDeviceError;
        }

        mockDeviceUnbound = false;

        return ZE_RESULT_SUCCESS;
    }

    MockGlobalOperationsSysfsAccess() = default;

    ADDMETHOD_NOBASE(fileExists, bool, true, (const std::string file));
};

struct MockGlobalOperationsProcfsAccess : public ProcfsAccess {

    const ::pid_t extraPid = 4;
    const int extraFd = 5;
    std::vector<::pid_t> pidList = {1, 2, 3};
    std::vector<int> fdList = {0, 1, 2};
    ::pid_t ourDevicePid = 0;
    int ourDeviceFd = 0;

    std::vector<MockEnumListProcessCall> mockListProcessCall{};
    std::vector<bool> isRepeated{};
    ze_result_t listProcessesResult = ZE_RESULT_SUCCESS;
    uint32_t listProcessCalled = 0u;
    ze_result_t listProcesses(std::vector<::pid_t> &list) override {
        listProcessCalled++;
        list = pidList;

        if (!mockListProcessCall.empty()) {
            MockEnumListProcessCall mockListProcessCallValue = mockListProcessCall.front();
            if (mockListProcessCallValue == MockEnumListProcessCall::DEVICE_IN_USE) {
                if (ourDevicePid) {
                    list.push_back(ourDevicePid);
                }
            }

            else if (mockListProcessCallValue == MockEnumListProcessCall::DEVICE_UNUSED) {
            }

            else if (mockListProcessCallValue == MockEnumListProcessCall::RETURN_ERROR) {
                listProcessesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
            }

            if (!isRepeated.empty()) {
                if (isRepeated.front() == false) {
                    mockListProcessCall.erase(mockListProcessCall.begin());
                    isRepeated.erase(isRepeated.begin());
                }
            }
        }
        return listProcessesResult;
    }

    ::pid_t myProcessId() override {
        return ::getpid();
    }

    ze_result_t mockGetFileDescriptorsError = ZE_RESULT_SUCCESS;
    ze_result_t getFileDescriptorsResult = ZE_RESULT_SUCCESS;
    ze_result_t getFileDescriptors(const ::pid_t pid, std::vector<int> &list) override {
        list.clear();

        if (mockGetFileDescriptorsError != ZE_RESULT_SUCCESS) {
            getFileDescriptorsResult = mockGetFileDescriptorsError;
            mockGetFileDescriptorsError = ZE_RESULT_SUCCESS;
            return getFileDescriptorsResult;
        }

        list = fdList;
        if (ourDevicePid == pid) {
            list.push_back(ourDeviceFd);
        }
        return getFileDescriptorsResult;
    }

    ze_result_t mockGetFileNameError = ZE_RESULT_SUCCESS;
    ze_result_t getFileNameResult = ZE_RESULT_SUCCESS;
    ze_result_t getFileName(const ::pid_t pid, const int fd, std::string &val) override {
        if (mockGetFileNameError != ZE_RESULT_SUCCESS) {
            return mockGetFileNameError;
        }

        if (pid == ourDevicePid && fd == ourDeviceFd) {
            val = mockDeviceName;
        } else {
            // return fake filenames for other file descriptors
            val = std::string("/FILENAME") + std::to_string(fd);
        }
        return getFileNameResult;
    }

    bool isAlive(const ::pid_t pid) override {
        if (pid == ourDevicePid) {
            return true;
        }
        return false;
    }

    bool mockNoKill = false;
    void kill(const ::pid_t pid) override {
        if (mockNoKill == true) {
            return;
        }
        ourDevicePid = 0;
    }

    MockGlobalOperationsProcfsAccess() = default;
};

struct MockGlobalOperationsFsAccess : public FsAccess {
    ze_result_t mockReadError = ZE_RESULT_SUCCESS;
    ze_result_t readResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    std::string mockReadVal = "";
    ze_result_t read(const std::string file, std::string &val) override {
        if (mockReadError != ZE_RESULT_SUCCESS) {
            return mockReadError;
        }

        if (mockReadVal == srcVersion) {
            if (file.compare(srcVersionFile) == 0) {
                val = mockReadVal;
                readResult = ZE_RESULT_SUCCESS;
                return readResult;
            }
        } else if (mockReadVal == driverVersion) {
            if (file.compare(agamaVersionFile) == 0) {
                val = mockReadVal;
                readResult = ZE_RESULT_SUCCESS;
                return readResult;
            }
        } else {
            readResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return readResult;
    }

    ze_result_t mockWriteError = ZE_RESULT_SUCCESS;
    ze_result_t writeResult = ZE_RESULT_SUCCESS;
    ze_result_t write(const std::string file, const std::string val) override {
        if (mockWriteError != ZE_RESULT_SUCCESS) {
            return mockWriteError;
        }

        return writeResult;
    }

    ADDMETHOD_NOBASE(canWrite, ze_result_t, ZE_RESULT_SUCCESS, (const std::string file));
    MockGlobalOperationsFsAccess() = default;
};

struct MockGlobalOpsFwInterface : public FirmwareUtil {
    ze_result_t mockIfrError = ZE_RESULT_SUCCESS;
    ze_result_t mockIfrResult = ZE_RESULT_SUCCESS;
    bool mockIfrStatus = true;
    ze_result_t fwIfrApplied(bool &ifrStatus) override {
        if (mockIfrError != ZE_RESULT_SUCCESS) {
            return mockIfrError;
        }

        ifrStatus = mockIfrStatus;
        return mockIfrResult;
    }
    MockGlobalOpsFwInterface() = default;

    ADDMETHOD_NOBASE(fwDeviceInit, ze_result_t, ZE_RESULT_SUCCESS, (void));
    ADDMETHOD_NOBASE(getFirstDevice, ze_result_t, ZE_RESULT_SUCCESS, (IgscDeviceInfo * info));
    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE(fwGetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t * currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE(fwSetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t newState, uint8_t *currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
    ADDMETHOD_NOBASE_VOIDRETURN(fwGetMemoryHealthIndicator, (zes_mem_health_t * health));
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
    ADDMETHOD_NOBASE(gpuProcessCleanup, ze_result_t, ZE_RESULT_SUCCESS, (ze_bool_t force));
};

class DrmGlobalOpsMock : public DrmMock {
  public:
    using DrmMock::DrmMock;
    int ioctlRetVal = 0;
    int ioctlErrno = 0;
    int ioctl(DrmIoctl request, void *arg) override {
        return ioctlRetVal;
    }
    int getErrno() override { return ioctlErrno; }
};

class PublicLinuxGlobalOperationsImp : public L0::LinuxGlobalOperationsImp {
  public:
    using LinuxGlobalOperationsImp::pLinuxSysmanImp;
    using LinuxGlobalOperationsImp::resetTimeout;
};

} // namespace ult
} // namespace L0
