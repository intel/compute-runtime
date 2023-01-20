/*
 * Copyright (C) 2022-2023 Intel Corporation
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

using namespace NEO;
namespace L0 {
namespace ult {

const std::string deviceDir("device");
const std::string eventsDir("/sys/devices/i915_0000_03_00.0/events");
constexpr int64_t mockPmuFd = 10;
constexpr uint64_t correctableGrfErrorCount = 100u;
constexpr uint64_t correctableEuErrorCount = 75u;
constexpr uint64_t fatalEuErrorCount = 50u;
constexpr uint64_t fatalTlb = 3u;
constexpr uint64_t socFatalPsfCsc0Count = 5u;
constexpr uint64_t fatalEngineResetCount = 45u;
constexpr uint64_t correctableGrfErrorCountTile0 = 90u;
constexpr uint64_t correctableEuErrorCountTile0 = 70u;
constexpr uint64_t fatalEuErrorCountTile0 = 55u;
constexpr uint64_t fatalEngineResetCountTile0 = 72u;
constexpr uint64_t correctableSamplerErrorCountTile1 = 30u;
constexpr uint64_t fatalGucErrorCountTile1 = 40u;
constexpr uint64_t fatalIdiParityErrorCountTile1 = 60u;
constexpr uint64_t correctableGucErrorCountTile1 = 25u;
constexpr uint64_t fatalEngineResetCountTile1 = 85u;
constexpr uint64_t socFatalMdfiEastCount = 3u;
constexpr uint64_t socFatalMdfiWestCountTile1 = 0u;
constexpr uint64_t socFatalPunitTile1 = 3u;
constexpr uint64_t fatalFpuTile0 = 1u;
constexpr uint64_t FatalL3FabricTile0 = 4u;
constexpr uint64_t euAttention = 10u;
constexpr uint64_t euAttentionTile0 = 5u;
constexpr uint64_t euAttentionTile1 = 2u;
constexpr uint64_t driverMigration = 2u;
constexpr uint64_t driverGgtt = 1u;
constexpr uint64_t driverRps = 2u;
constexpr uint64_t driverEngineOther = 3u;
constexpr uint64_t initialUncorrectableCacheErrors = 2u;
constexpr uint64_t initialEngineReset = 2u;
constexpr uint64_t initialProgrammingErrors = 7u;
constexpr uint64_t initialUncorrectableNonComputeErrors = 8u;
constexpr uint64_t initialUncorrectableComputeErrors = 10u;
constexpr uint64_t initialCorrectableComputeErrors = 6u;
constexpr uint64_t initialUncorrectableDriverErrors = 5u;
constexpr uint64_t initialUncorrectableCacheErrorsTile1 = 1u;
constexpr uint64_t initialEngineResetTile1 = 4u;
constexpr uint64_t initialProgrammingErrorsTile1 = 5u;
constexpr uint64_t initialCorrectableComputeErrorsTile1 = 5u;
constexpr uint64_t initialUncorrectableNonComputeErrorsTile1 = 5u;
constexpr uint64_t initialUncorrectableComputeErrorsTile1 = 6u;
constexpr uint64_t initialUncorrectableDriverErrorsTile1 = 4u;
constexpr uint64_t timeStamp = 1000u;
constexpr uint32_t pmuDriverType = 16u;
constexpr uint64_t hbmCorrectableErrorCount = 2;
constexpr uint64_t hbmUncorrectableErrorCount = 3;

struct MockMemoryManagerInRasSysman : public MemoryManagerMock {
    MockMemoryManagerInRasSysman(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

struct MockRasPmuInterfaceImp : public PmuInterfaceImp {
    using PmuInterfaceImp::perfEventOpen;
    MockRasPmuInterfaceImp(LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

    int32_t mockPmuReadCount = 0;
    int32_t mockPmuReadCountAfterClear = 0;
    int32_t mockPmuReadTileCount = 0;

    bool mockPmuReadCorrectable = false;
    bool mockPmuReadAfterClear = false;
    bool mockPmuReadResult = false;
    bool mockPerfEvent = false;
    bool mockPmuReadTile = false;

    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {

        if (mockPerfEvent == true) {
            return mockedPerfEventOpenAndFailureReturn(attr, pid, cpu, groupFd, flags);
        }

        return mockPmuFd;
    }

    int64_t mockedPerfEventOpenAndFailureReturn(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) {
        return -1;
    }

    int mockedPmuReadForCorrectableAndSuccessReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        memset(data, 0, sizeOfdata);
        data[1] = timeStamp;
        data[2] = correctableGrfErrorCount;
        data[3] = correctableEuErrorCount;
        return 0;
    }

    int mockedPmuReadForUncorrectableAndSuccessReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        memset(data, 0, sizeOfdata);
        data[1] = timeStamp;
        data[2] = fatalEngineResetCount;
        data[3] = euAttention;
        data[4] = driverMigration;
        data[5] = driverGgtt;
        data[6] = driverRps;
        data[7] = 0;
        data[8] = 0;
        data[9] = fatalEuErrorCount;
        data[10] = socFatalMdfiEastCount;
        data[11] = socFatalPsfCsc0Count;
        data[12] = fatalTlb;
        return 0;
    }

    int mockedPmuReadForCorrectableTile0AndSuccessReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        memset(data, 0, sizeOfdata);
        data[1] = timeStamp;
        data[2] = correctableGrfErrorCount;
        data[3] = correctableEuErrorCount;
        return 0;
    }

    int mockedPmuReadForUncorrectableTile0AndSuccessReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        memset(data, 0, sizeOfdata);
        data[1] = timeStamp;
        data[2] = fatalEngineResetCount;
        data[3] = euAttention;
        data[4] = driverMigration;
        data[5] = driverGgtt;
        data[6] = driverRps;
        data[7] = 0;
        data[8] = 0;
        data[9] = fatalEuErrorCount;
        data[10] = socFatalMdfiEastCount;
        data[11] = socFatalPsfCsc0Count;
        data[12] = fatalTlb;
        return 0;
    }

    int mockedPmuReadForCorrectableTile1AndSuccessReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        memset(data, 0, sizeOfdata);
        data[1] = timeStamp;
        data[2] = correctableGucErrorCountTile1;
        data[3] = correctableSamplerErrorCountTile1;
        return 0;
    }

    int mockedPmuReadForUncorrectableTile1AndSuccessReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        memset(data, 0, sizeOfdata);
        data[1] = timeStamp;
        data[2] = fatalEngineResetCountTile1;
        data[3] = euAttentionTile1;
        data[4] = driverMigration;
        data[5] = driverEngineOther;
        data[6] = fatalGucErrorCountTile1;
        data[7] = socFatalMdfiWestCountTile1;
        data[8] = socFatalPunitTile1;
        data[9] = fatalIdiParityErrorCountTile1;
        return 0;
    }

    int mockedPmuReadAfterClearAndSuccessReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        memset(data, 0, sizeOfdata);
        return 0;
    }

    int mockedPmuReadAndFailureReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        return -1;
    }

    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override {

        if (mockPmuReadResult == true) {
            return mockedPmuReadAndFailureReturn(fd, data, sizeOfdata);
        }

        if (mockPmuReadCorrectable == true) {
            if (mockPmuReadCount == 0) {
                mockPmuReadCount++;
                return mockedPmuReadForCorrectableAndSuccessReturn(fd, data, sizeOfdata);
            }

            else if (mockPmuReadCount == 1) {
                mockPmuReadCount++;
                return mockedPmuReadForUncorrectableAndSuccessReturn(fd, data, sizeOfdata);
            }
        }

        if (mockPmuReadAfterClear == true) {
            if (mockPmuReadCountAfterClear == 0) {
                mockPmuReadCountAfterClear++;
                return mockedPmuReadForCorrectableAndSuccessReturn(fd, data, sizeOfdata);
            }

            else if (mockPmuReadCountAfterClear == 1) {
                mockPmuReadCountAfterClear++;
                return mockedPmuReadForUncorrectableAndSuccessReturn(fd, data, sizeOfdata);
            }

            else {
                mockPmuReadCountAfterClear++;
                return mockedPmuReadAfterClearAndSuccessReturn(fd, data, sizeOfdata);
            }
        }

        if (mockPmuReadTile == true) {
            if (mockPmuReadTileCount == 0) {
                mockPmuReadTileCount++;
                return mockedPmuReadForCorrectableTile0AndSuccessReturn(fd, data, sizeOfdata);
            }

            else if (mockPmuReadTileCount == 1) {
                mockPmuReadTileCount++;
                return mockedPmuReadForUncorrectableTile0AndSuccessReturn(fd, data, sizeOfdata);
            }

            else if (mockPmuReadTileCount == 2) {
                mockPmuReadTileCount++;
                return mockedPmuReadForCorrectableTile1AndSuccessReturn(fd, data, sizeOfdata);
            }

            else if (mockPmuReadTileCount == 3) {
                mockPmuReadTileCount++;
                return mockedPmuReadForUncorrectableTile1AndSuccessReturn(fd, data, sizeOfdata);
            }
        }
        return 0;
    }
};

struct MockRasSysfsAccess : public SysfsAccess {

    ze_result_t mockReadSymLinkStatus = ZE_RESULT_SUCCESS;
    bool mockReadSymLinkResult = false;

    ze_result_t readSymLink(const std::string file, std::string &val) override {

        if (mockReadSymLinkStatus != ZE_RESULT_SUCCESS) {
            return mockReadSymLinkStatus;
        }

        if (mockReadSymLinkResult == true) {
            return getValStringSymLinkFailure(file, val);
        }

        if (file.compare(deviceDir) == 0) {
            val = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0";
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValStringSymLinkFailure(const std::string file, std::string &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {
        if (file.compare("gt/gt0/error_counter/correctable_eu_grf") == 0) {
            val = 5u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/correctable_eu_ic") == 0) {
            val = 1u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/fatal_eu_ic") == 0) {
            val = 5u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/fatal_tlb") == 0) {
            val = 2u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/engine_reset") == 0) {
            val = 2u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt1/error_counter/correctable_sampler") == 0) {
            val = 2u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt1/error_counter/fatal_guc") == 0) {
            val = 6u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt1/error_counter/fatal_idi_parity") == 0) {
            val = 1u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt1/error_counter/correctable_guc") == 0) {
            val = 3u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt1/error_counter/engine_reset") == 0) {
            val = 4u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/eu_attention") == 0) {
            val = 7u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt1/error_counter/eu_attention") == 0) {
            val = 5u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/soc_fatal_mdfi_east") == 0) {
            val = 5u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/soc_fatal_psf_csc_0") == 0) {
            val = 3u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt1/error_counter/soc_fatal_punit") == 0) {
            val = 3u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt1/error_counter/soc_fatal_mdfi_west") == 0) {
            val = 2u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/fatal_fpu") == 0) {
            val = 2u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/fatal_l3_fabric") == 0) {
            val = 3u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/driver_ggtt") == 0) {
            val = 2u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/driver_rps") == 0) {
            val = 2u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("error_counter/driver_object_migration") == 0) {
            val = 1u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt1/error_counter/driver_engine_other") == 0) {
            val = 3u;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
};

struct MockRasFsAccess : public FsAccess {

    ze_result_t mockListDirectoryStatus = ZE_RESULT_SUCCESS;
    bool mockReadDirectoryFailure = false;
    bool mockReadFileFailure = false;
    bool mockReadDirectoryWithoutRasEvents = false;
    bool mockRootUser = false;
    bool mockReadVal = false;
    bool mockReadDirectoryForMultiDevice = false;

    bool directoryExists(const std::string path) override {
        // disables fabric errors
        return false;
    }

    ze_result_t listDirectory(const std::string directory, std::vector<std::string> &events) override {

        if (mockListDirectoryStatus != ZE_RESULT_SUCCESS) {
            return mockListDirectoryStatus;
        }

        if (mockReadDirectoryFailure == true) {
            return readDirectoryFailure(directory, events);
        }

        if (mockReadDirectoryWithoutRasEvents == true) {
            return readDirectoryWithoutRasEvents(directory, events);
        }

        if (mockReadDirectoryForMultiDevice == true) {
            return readDirectorySuccessForMultiDevice(directory, events);
        }

        if (directory.compare(eventsDir) == 0) {
            events.push_back("bcs0-busy");
            events.push_back("error--correctable-eu-grf");
            events.push_back("error--correctable-eu-ic");
            events.push_back("error--soc-fatal-mdfi-east");
            events.push_back("error--soc-fatal-psf-csc-0");
            events.push_back("error--fatal-eu-ic");
            events.push_back("error--fatal-tlb");
            events.push_back("error--engine-reset");
            events.push_back("error--eu-attention");
            events.push_back("error--driver-object-migration");
            events.push_back("error--driver-ggtt");
            events.push_back("error--driver-rps");
            events.push_back("error--fatal-fpu");
            events.push_back("error--fatal-l3-fabric");
            events.push_back("ccs0-busy");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t readDirectoryWithoutRasEvents(const std::string directory, std::vector<std::string> &events) {
        if (directory.compare(eventsDir) == 0) {
            events.push_back("bcs0-busy");
            events.push_back("ccs0-busy");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t readDirectorySuccessForMultiDevice(const std::string directory, std::vector<std::string> &events) {
        if (directory.compare(eventsDir) == 0) {
            events.push_back("bcs0-busy");
            events.push_back("error-gt0--correctable-eu-grf");
            events.push_back("error-gt0--correctable-eu-grf");
            events.push_back("error-gt0--correctable-eu-ic");
            events.push_back("error-gt0--soc-fatal-mdfi-east");
            events.push_back("error-gt0--soc-fatal-psf-csc-0");
            events.push_back("error-gt0--fatal-eu-ic");
            events.push_back("error-gt0--fatal-tlb");
            events.push_back("error-gt0--engine-reset");
            events.push_back("error-gt0--eu-attention");
            events.push_back("error-gt0--fatal-fpu");
            events.push_back("error-gt0--fatal-l3-fabric");
            events.push_back("error--driver-object-migration");
            events.push_back("error-gt0--driver-ggtt");
            events.push_back("error-gt0--driver-rps");
            events.push_back("error-gt1--correctable-sampler");
            events.push_back("error-gt1--soc-fatal-mdfi-west");
            events.push_back("error-gt1--soc-fatal-punit");
            events.push_back("error-gt1--fatal-guc");
            events.push_back("error-gt1--fatal-idi-parity");
            events.push_back("error-gt1--correctable-guc");
            events.push_back("error-gt1--engine-reset");
            events.push_back("error-gt1--eu-attention");
            events.push_back("error-gt1--driver-engine-other");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t readDirectoryFailure(const std::string directory, std::vector<std::string> &events) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, std::string &config) override {

        if (mockReadFileFailure == true) {
            return readFileFailure(file, config);
        }

        config = "config=0x0000000000000001";
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t readFileFailure(const std::string, std::string &config) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint32_t &val) override {

        if (mockReadVal == true) {
            return readValFailure(file, val);
        }

        val = pmuDriverType;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t readValFailure(const std::string file, uint32_t &val) {
        val = 0;
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    bool isRootUser() override {

        if (mockRootUser == true) {
            return userIsNotRoot();
        }

        return true;
    }

    bool userIsNotRoot() {
        return false;
    }

    MockRasFsAccess() = default;
};

struct MockRasFwInterface : public FirmwareUtil {

    bool mockMemorySuccess = false;

    ze_result_t mockGetMemoryErrorSuccess(zes_ras_error_type_t category, uint64_t subDeviceCount, uint64_t subDeviceId, uint64_t &count) {
        if (category == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            count = hbmCorrectableErrorCount;
        }
        if (category == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            count = hbmUncorrectableErrorCount;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t fwGetMemoryErrorCount(zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count) override {

        if (mockMemorySuccess == true) {
            return mockGetMemoryErrorSuccess(category, subDeviceCount, subDeviceId, count);
        }

        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    MockRasFwInterface() = default;

    ADDMETHOD_NOBASE(fwDeviceInit, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getFirstDevice, ze_result_t, ZE_RESULT_SUCCESS, (igsc_device_info * info));
    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwIfrApplied, ze_result_t, ZE_RESULT_SUCCESS, (bool &ifrStatus));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
    ADDMETHOD_NOBASE(fwGetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t * currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE(fwSetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t newState, uint8_t *currentState, uint8_t *pendingState));
};

class PublicLinuxRasImp : public L0::LinuxRasImp {
  public:
    PublicLinuxRasImp(OsSysman *pOsSysman, zes_ras_error_type_t type, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxRasImp(pOsSysman, type, onSubdevice, subdeviceId) {}
    using LinuxRasImp::pFsAccess;
};

} // namespace ult
} // namespace L0
