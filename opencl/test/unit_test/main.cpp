/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_interface.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/unit_test/helpers/default_hw_info.inl"
#include "shared/test/unit_test/helpers/memory_leak_listener.h"
#include "shared/test/unit_test/helpers/ult_hw_config.inl"
#include "shared/source/utilities/debug_settings_reader.h"
#include "opencl/source/os_interface/ocl_reg_path.h"
#include "opencl/test/unit_test/custom_event_listener.h"
#include "opencl/test/unit_test/global_environment.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"
#include "opencl/test/unit_test/helpers/test_files.h"
#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_sip.h"
#include "opencl/test/unit_test/tests_configuration.h"
#include "opencl/test/unit_test/ult_config_listener.h"

#include "gmock/gmock.h"
#include "mock_gmm_client_context.h"

#include <algorithm>
#include <fstream>
#include <limits.h>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef WIN32
const char *fSeparator = "\\";
#else
const char *fSeparator = "/";
#endif

namespace NEO {
extern const char *hardwarePrefix[];
extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];

extern const unsigned int ultIterationMaxTime;
extern bool useMockGmm;
extern TestMode testMode;
extern const char *executionDirectorySuffix;

std::thread::id tempThreadID;

namespace MockSipData {
extern std::unique_ptr<MockSipKernel> mockSipKernel;
}
} // namespace NEO

using namespace NEO;
TestEnvironment *gEnvironment;

PRODUCT_FAMILY productFamily = IGFX_SKYLAKE;
GFXCORE_FAMILY renderCoreFamily = IGFX_GEN9_CORE;

extern std::string lastTest;
bool generateRandomInput = false;

void applyWorkarounds() {
    platformsImpl.reserve(1);
    {
        std::ofstream f;
        const std::string fileName("_tmp_");
        f.open(fileName, std::ofstream::binary);
        f.close();
    }
    {
        std::mutex mtx;
        std::unique_lock<std::mutex> stateLock(mtx);
    }
    {
        std::stringstream ss("1");
        int val;
        ss >> val;
    }
    {
        class BaseClass {
          public:
            int method(int param) { return 1; }
        };
        class MockClass : public BaseClass {
          public:
            MOCK_METHOD1(method, int(int param));
        };
        ::testing::NiceMock<MockClass> mockObj;
        EXPECT_CALL(mockObj, method(::testing::_))
            .Times(1);
        mockObj.method(2);
    }

    //intialize rand
    srand(static_cast<unsigned int>(time(nullptr)));

    //Create at least on thread to prevent false memory leaks in tests using threads
    std::thread t([&]() {
    });
    tempThreadID = t.get_id();
    t.join();
}
#ifdef __linux__
void handle_SIGALRM(int signal) {
    std::cout << "Tests timeout on: " << lastTest << std::endl;
    abort();
}
void handle_SIGSEGV(int signal) {
    std::cout << "SIGSEGV on: " << lastTest << std::endl;
    abort();
}
struct sigaction oldSigAbrt;
void handle_SIGABRT(int signal) {
    std::cout << "SIGABRT on: " << lastTest << std::endl;
    // restore signal handler to abort
    if (sigaction(SIGABRT, &oldSigAbrt, nullptr) == -1) {
        std::cout << "FATAL: cannot fatal SIGABRT handler" << std::endl;
        std::cout << "FATAL: try SEGV" << std::endl;
        uint8_t *ptr = nullptr;
        *ptr = 0;
        std::cout << "FATAL: still alive, call exit()" << std::endl;
        exit(-1);
    }
    raise(signal);
}
#else
LONG WINAPI UltExceptionFilter(
    _In_ struct _EXCEPTION_POINTERS *exceptionInfo) {
    std::cout << "UnhandledException: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << std::dec
              << " on test: " << lastTest
              << std::endl;
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void initializeTestHelpers() {
    GlobalMockSipProgram::initSipProgram();
    MockSipData::mockSipKernel.reset(new MockSipKernel());
}

void cleanTestHelpers() {
    GlobalMockSipProgram::shutDownSipProgram();
}

std::string getHardwarePrefix() {
    std::string s = hardwarePrefix[platformDevices[0]->platform.eProductFamily];
    return s;
}

std::string getRunPath(char *argv0) {
    std::string res(argv0);

    auto pos = res.rfind(fSeparator);
    if (pos != std::string::npos)
        res = res.substr(0, pos);

    if (res == "." || pos == std::string::npos) {
#if defined(__linux__)
        res = getcwd(nullptr, 0);
#else
        res = _getcwd(nullptr, 0);
#endif
    }

    return res;
}

int main(int argc, char **argv) {
    int retVal = 0;
    bool useDefaultListener = false;
    bool enable_alarm = true;
    bool setupFeatureTableAndWorkaroundTable = testMode == TestMode::AubTests ? true : false;

    applyWorkarounds();

#if defined(__linux__)
    bool enable_segv = true;
    bool enable_abrt = true;
    if (getenv("IGDRCL_TEST_SELF_EXEC") == nullptr) {
        std::string wd = getRunPath(argv[0]);
        setenv("LD_LIBRARY_PATH", wd.c_str(), 1);
        setenv("IGDRCL_TEST_SELF_EXEC", wd.c_str(), 1);
        execv(argv[0], argv);
        printf("FATAL ERROR: cannot self-exec test: %s!, errno: %d\n", argv[0], errno);
        return -1;
    } else {
    }
#endif

    ::testing::InitGoogleMock(&argc, argv);
    HardwareInfo device = DEFAULT_TEST_PLATFORM::hwInfo;
    ::productFamily = device.platform.eProductFamily;

    uint32_t euPerSubSlice = 0;
    uint32_t sliceCount = 0;
    uint32_t subSlicePerSliceCount = 0;
    int32_t revId = -1;
    int dieRecovery = 0;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp("--disable_default_listener", argv[i])) {
            useDefaultListener = false;
        } else if (!strcmp("--enable_default_listener", argv[i])) {
            useDefaultListener = true;
        } else if (!strcmp("--disable_alarm", argv[i])) {
            enable_alarm = false;
        } else if (!strcmp("--tbx", argv[i])) {
            if (testMode == TestMode::AubTests) {
                testMode = TestMode::AubTestsWithTbx;
            }
            initialHardwareTag = 0;
        } else if (!strcmp("--rev_id", argv[i])) {
            ++i;
            if (i < argc) {
                revId = atoi(argv[i]);
            }
        } else if (!strcmp("--product", argv[i])) {
            ++i;
            if (i < argc) {
                if (::isdigit(argv[i][0])) {
                    int productValue = atoi(argv[i]);
                    if (productValue > 0 && productValue < IGFX_MAX_PRODUCT && hardwarePrefix[productValue] != nullptr) {
                        ::productFamily = static_cast<PRODUCT_FAMILY>(productValue);
                    } else {
                        ::productFamily = IGFX_UNKNOWN;
                    }
                } else {
                    ::productFamily = IGFX_UNKNOWN;
                    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
                        if (hardwarePrefix[j] == nullptr)
                            continue;
                        if (strcmp(hardwarePrefix[j], argv[i]) == 0) {
                            ::productFamily = static_cast<PRODUCT_FAMILY>(j);
                            break;
                        }
                    }
                }
                if (::productFamily == IGFX_UNKNOWN) {
                    std::cout << "unknown or unsupported product family has been set: " << argv[i] << std::endl;
                    return -1;
                } else {
                    std::cout << "product family: " << hardwarePrefix[::productFamily] << " (" << ::productFamily << ")" << std::endl;
                }
            }
        } else if (!strcmp("--slices", argv[i])) {
            ++i;
            if (i < argc) {
                sliceCount = atoi(argv[i]);
            }
        } else if (!strcmp("--subslices", argv[i])) {
            ++i;
            if (i < argc) {
                subSlicePerSliceCount = atoi(argv[i]);
            }
        } else if (!strcmp("--eu_per_ss", argv[i])) {
            ++i;
            if (i < argc) {
                euPerSubSlice = atoi(argv[i]);
            }
        } else if (!strcmp("--die_recovery", argv[i])) {
            ++i;
            if (i < argc) {
                dieRecovery = atoi(argv[i]) ? 1 : 0;
            }
        } else if (!strcmp("--generate_random_inputs", argv[i])) {
            generateRandomInput = true;
        } else if (!strcmp("--read-config", argv[i]) && testMode == TestMode::AubTests) {
            if (DebugManager.registryReadAvailable()) {
                DebugManager.setReaderImpl(SettingsReader::create(oclRegPath));
                DebugManager.injectSettingsFromReader();
            }
        } else if (!strcmp("--dump_buffer_format", argv[i]) && testMode == TestMode::AubTests) {
            ++i;
            std::string dumpBufferFormat(argv[i]);
            std::transform(dumpBufferFormat.begin(), dumpBufferFormat.end(), dumpBufferFormat.begin(), ::toupper);
            DebugManager.flags.AUBDumpBufferFormat.set(dumpBufferFormat);
        } else if (!strcmp("--dump_image_format", argv[i]) && testMode == TestMode::AubTests) {
            ++i;
            std::string dumpImageFormat(argv[i]);
            std::transform(dumpImageFormat.begin(), dumpImageFormat.end(), dumpImageFormat.begin(), ::toupper);
            DebugManager.flags.AUBDumpImageFormat.set(dumpImageFormat);
        }
    }

    uint32_t threadsPerEu = hwInfoConfigFactory[productFamily]->threadsPerEu;
    PLATFORM platform;
    auto hardwareInfo = hardwareInfoTable[productFamily];
    if (!hardwareInfo) {
        return -1;
    }
    platform = hardwareInfo->platform;

    if (revId != -1) {
        platform.usRevId = revId;
    }

    HardwareInfo hwInfo = *hardwareInfo;

    uint64_t hwInfoConfig = defaultHardwareInfoConfigTable[productFamily];
    setHwInfoValuesFromConfig(hwInfoConfig, hwInfo);

    // set Gt and FeatureTable to initial state
    hardwareInfoSetup[productFamily](&hwInfo, setupFeatureTableAndWorkaroundTable, hwInfoConfig);
    FeatureTable featureTable = hwInfo.featureTable;
    GT_SYSTEM_INFO gtSystemInfo = hwInfo.gtSystemInfo;
    WorkaroundTable workaroundTable = hwInfo.workaroundTable;

    // and adjust dynamic values if not secified
    sliceCount = sliceCount > 0 ? sliceCount : gtSystemInfo.SliceCount;
    subSlicePerSliceCount = subSlicePerSliceCount > 0 ? subSlicePerSliceCount : (gtSystemInfo.SubSliceCount / sliceCount);
    euPerSubSlice = euPerSubSlice > 0 ? euPerSubSlice : gtSystemInfo.MaxEuPerSubSlice;
    // clang-format off
    gtSystemInfo.SliceCount             = sliceCount;
    gtSystemInfo.SubSliceCount          = gtSystemInfo.SliceCount * subSlicePerSliceCount;
    gtSystemInfo.EUCount                = gtSystemInfo.SubSliceCount * euPerSubSlice - dieRecovery;
    gtSystemInfo.ThreadCount            = gtSystemInfo.EUCount * threadsPerEu;
    gtSystemInfo.MaxEuPerSubSlice       = std::max(gtSystemInfo.MaxEuPerSubSlice, euPerSubSlice);
    gtSystemInfo.MaxSlicesSupported     = std::max(gtSystemInfo.MaxSlicesSupported, gtSystemInfo.SliceCount);
    gtSystemInfo.MaxSubSlicesSupported  = std::max(gtSystemInfo.MaxSubSlicesSupported, gtSystemInfo.SubSliceCount);
    gtSystemInfo.IsDynamicallyPopulated = false;
    // clang-format on

    ::productFamily = platform.eProductFamily;
    ::renderCoreFamily = platform.eRenderCoreFamily;

    device.platform = platform;
    device.gtSystemInfo = gtSystemInfo;
    device.featureTable = featureTable;
    device.workaroundTable = workaroundTable;
    device.capabilityTable = hardwareInfo->capabilityTable;

    binaryNameSuffix.append(familyName[device.platform.eRenderCoreFamily]);
    binaryNameSuffix.append(device.capabilityTable.platformType);

    std::string nBinaryKernelFiles = getRunPath(argv[0]);
    nBinaryKernelFiles.append("/");
    nBinaryKernelFiles.append(binaryNameSuffix);
    nBinaryKernelFiles.append("/");
    nBinaryKernelFiles.append(testFiles);
    testFiles = nBinaryKernelFiles;

    std::string nClFiles = getRunPath(argv[0]);
    nClFiles.append("/");
    nClFiles.append(hardwarePrefix[productFamily]);
    nClFiles.append("/");
    nClFiles.append(clFiles);
    clFiles = nClFiles;

    std::string executionDirectory(hardwarePrefix[productFamily]);
    executionDirectory += NEO::executionDirectorySuffix; // _aub for aub_tests, empty otherwise

#ifdef WIN32
#include <direct.h>
    if (_chdir(executionDirectory.c_str())) {
        std::cout << "chdir into " << executionDirectory << " directory failed.\nThis might cause test failures." << std::endl;
    }
#elif defined(__linux__)
#include <unistd.h>
    if (chdir(executionDirectory.c_str()) != 0) {
        std::cout << "chdir into " << executionDirectory << " directory failed.\nThis might cause test failures." << std::endl;
    }
#endif

    platformDevices = new const HardwareInfo *[1];
    platformDevices[0] = &device;

    auto &listeners = ::testing::UnitTest::GetInstance()->listeners();
    if (useDefaultListener == false) {
        auto defaultListener = listeners.default_result_printer();

        auto customEventListener = new CCustomEventListener(defaultListener, hardwarePrefix[productFamily]);

        listeners.Release(defaultListener);
        listeners.Append(customEventListener);
    }

    listeners.Append(new MemoryLeakListener);
    listeners.Append(new UltConfigListener);

    gEnvironment = reinterpret_cast<TestEnvironment *>(::testing::AddGlobalTestEnvironment(new TestEnvironment));

    MockCompilerDebugVars fclDebugVars;
    MockCompilerDebugVars igcDebugVars;

    retrieveBinaryKernelFilename(fclDebugVars.fileName, KernelBinaryHelper::BUILT_INS + "_", ".bc");
    retrieveBinaryKernelFilename(igcDebugVars.fileName, KernelBinaryHelper::BUILT_INS + "_", ".gen");

    gEnvironment->setMockFileNames(fclDebugVars.fileName, igcDebugVars.fileName);
    gEnvironment->setDefaultDebugVars(fclDebugVars, igcDebugVars, device);

#if defined(__linux__)
    //ULTs timeout
    if (enable_alarm) {
        unsigned int alarmTime = NEO::ultIterationMaxTime * ::testing::GTEST_FLAG(repeat);

        struct sigaction sa;
        sa.sa_handler = &handle_SIGALRM;
        sa.sa_flags = SA_RESTART;
        sigfillset(&sa.sa_mask);
        if (sigaction(SIGALRM, &sa, NULL) == -1) {
            printf("FATAL ERROR: cannot intercept SIGALRM\n");
            return -2;
        }
        alarm(alarmTime);
        std::cout << "set timeout to: " << alarmTime << std::endl;
    }

    if (enable_segv) {
        struct sigaction sa;
        sa.sa_handler = &handle_SIGSEGV;
        sa.sa_flags = SA_RESTART;
        sigfillset(&sa.sa_mask);
        if (sigaction(SIGSEGV, &sa, NULL) == -1) {
            printf("FATAL ERROR: cannot intercept SIGSEGV\n");
            return -2;
        }
    }

    if (enable_abrt) {
        struct sigaction sa;
        sa.sa_handler = &handle_SIGABRT;
        sa.sa_flags = SA_RESTART;
        sigfillset(&sa.sa_mask);
        if (sigaction(SIGABRT, &sa, &oldSigAbrt) == -1) {
            printf("FATAL ERROR: cannot intercept SIGABRT\n");
            return -2;
        }
    }
#else
    SetUnhandledExceptionFilter(&UltExceptionFilter);
#endif
    if (useMockGmm) {
        GmmHelper::createGmmContextWrapperFunc = GmmClientContextBase::create<MockGmmClientContext>;
    } else {
        GmmInterface::initialize(nullptr, nullptr);
    }
    initializeTestHelpers();

    retVal = RUN_ALL_TESTS();

    cleanTestHelpers();

    delete[] platformDevices;

    return retVal;
}
