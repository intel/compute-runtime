/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "global_environment.h"
#include "hw_cmds.h"
#include "runtime/helpers/options.h"
#include "unit_tests/custom_event_listener.h"
#include "helpers/test_files.h"
#include "unit_tests/memory_leak_listener.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_sip.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "gmock/gmock.h"
#include <algorithm>
#include <mutex>
#include <fstream>
#include <sstream>
#include <thread>

#include <limits.h>

#ifdef WIN32
const char *fSeparator = "\\";
#else
const char *fSeparator = "/";
#endif

namespace OCLRT {
extern const char *hardwarePrefix[];
extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];

extern const unsigned int ultIterationMaxTime;

std::thread::id tempThreadID;
} // namespace OCLRT

using namespace OCLRT;
TestEnvironment *gEnvironment;

PRODUCT_FAMILY productFamily = IGFX_SKYLAKE;
GFXCORE_FAMILY renderCoreFamily = IGFX_GEN9_CORE;
PRODUCT_FAMILY defaultProductFamily = productFamily;

extern bool printMemoryOpCallStack;
extern std::string lastTest;

void applyWorkarounds() {
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
    const HardwareInfo *hwinfo = *platformDevices;
    auto initialized = Gmm::initContext(hwinfo->pPlatform, hwinfo->pSkuTable,
                                        hwinfo->pWaTable, hwinfo->pSysInfo);
    ASSERT_TRUE(initialized);
    GlobalMockSipProgram::initSipProgram();
}

void cleanTestHelpers() {
    GlobalMockSipProgram::shutDownSipProgram();
    Gmm::destroyContext();
}

std::string getHardwarePrefix() {
    std::string s = hardwarePrefix[platformDevices[0]->pPlatform->eProductFamily];
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
    bool enable_segv = true;
    bool enable_abrt = true;

    applyWorkarounds();

#if defined(__linux__)
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

    auto numDevices = numPlatformDevices;
    HardwareInfo device = DEFAULT_TEST_PLATFORM::hwInfo;
    GT_SYSTEM_INFO gtSystemInfo = *device.pSysInfo;
    hardwareInfoSetupGt[device.pPlatform->eProductFamily](&gtSystemInfo);
    size_t revisionId = device.pPlatform->usRevId;
    uint32_t euPerSubSlice = 0;
    uint32_t sliceCount = 0;
    uint32_t subSliceCount = 0;
    int dieRecovery = 1;
    ::productFamily = device.pPlatform->eProductFamily;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp("--disable_default_listener", argv[i])) {
            useDefaultListener = false;
        } else if (!strcmp("--enable_default_listener", argv[i])) {
            useDefaultListener = true;
        } else if (!strcmp("--disable_alarm", argv[i])) {
            enable_alarm = false;
        } else if (!strcmp("--print_memory_op_cs", argv[i])) {
            printMemoryOpCallStack = true;
        } else if (!strcmp("--devices", argv[i])) {
            ++i;
            if (i < argc) {
                numDevices = atoi(argv[i]);
            }
        } else if (!strcmp("--rev_id", argv[i])) {
            ++i;
            if (i < argc) {
                revisionId = atoi(argv[i]);
            }
        } else if (!strcmp("--product", argv[i])) {
            ++i;
            if (i < argc) {
                if (::isdigit(argv[i][0])) {
                    ::productFamily = (PRODUCT_FAMILY)atoi(argv[i]);
                } else {
                    ::productFamily = IGFX_UNKNOWN;
                    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
                        if (hardwarePrefix[j] == nullptr)
                            continue;
                        if (strcmp(hardwarePrefix[j], argv[i]) == 0) {
                            ::productFamily = (PRODUCT_FAMILY)j;
                            break;
                        }
                    }
                }
                std::cout << "product family: " << hardwarePrefix[::productFamily] << " (" << ::productFamily << ")" << std::endl;
            }
        } else if (!strcmp("--slices", argv[i])) {
            ++i;
            if (i < argc) {
                sliceCount = atoi(argv[i]);
            }
        } else if (!strcmp("--subslices", argv[i])) {
            ++i;
            if (i < argc) {
                subSliceCount = atoi(argv[i]);
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
        }
    }

    if (numDevices < 1) {
        return -1;
    }

    uint32_t threadsPerEu = 7;
    PLATFORM platform;
    auto hardwareInfo = hardwareInfoTable[productFamily];
    if (!hardwareInfo) {
        return -1;
    }
    platform = *hardwareInfo->pPlatform;

    platform.usRevId = (uint16_t)revisionId;

    // set Gt to initial state
    hardwareInfoSetupGt[productFamily](&gtSystemInfo);
    // and adjust dynamic values if not secified
    sliceCount = sliceCount > 0 ? sliceCount : gtSystemInfo.SliceCount;
    subSliceCount = subSliceCount > 0 ? subSliceCount : gtSystemInfo.SubSliceCount;
    euPerSubSlice = euPerSubSlice > 0 ? euPerSubSlice : gtSystemInfo.MaxEuPerSubSlice;
    // clang-format off
    gtSystemInfo.SliceCount             = sliceCount;
    gtSystemInfo.SubSliceCount          = subSliceCount;
    gtSystemInfo.EUCount                = gtSystemInfo.SubSliceCount * euPerSubSlice - dieRecovery;
    gtSystemInfo.ThreadCount            = gtSystemInfo.EUCount * threadsPerEu;
    gtSystemInfo.MaxEuPerSubSlice       = std::max(gtSystemInfo.MaxEuPerSubSlice, euPerSubSlice);
    gtSystemInfo.MaxSlicesSupported     = std::max(gtSystemInfo.MaxSlicesSupported, gtSystemInfo.SliceCount);
    gtSystemInfo.MaxSubSlicesSupported  = std::max(gtSystemInfo.MaxSubSlicesSupported, gtSystemInfo.SubSliceCount);
    gtSystemInfo.IsDynamicallyPopulated = false;
    // clang-format on

    ::productFamily = platform.eProductFamily;
    ::renderCoreFamily = platform.eRenderCoreFamily;

    device.pPlatform = &platform;
    device.pSysInfo = &gtSystemInfo;
    device.capabilityTable = hardwareInfo->capabilityTable;

    binaryNameSuffix.append(familyName[device.pPlatform->eRenderCoreFamily]);
    binaryNameSuffix.append(getPlatformType(device));

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

#ifdef WIN32
#include <direct.h>
    if (_chdir(hardwarePrefix[productFamily])) {
        std::cout << "chdir into " << hardwarePrefix[productFamily] << " directory failed.\nThis might cause test failures." << std::endl;
    }
#elif defined(__linux__)
#include <unistd.h>
    if (chdir(hardwarePrefix[productFamily]) != 0) {
        std::cout << "chdir into " << hardwarePrefix[productFamily] << " directory failed.\nThis might cause test failures." << std::endl;
    }
#endif

    auto pDevices = new const HardwareInfo *[numDevices];
    for (decltype(numDevices) i = 0; i < numDevices; ++i) {
        pDevices[i] = &device;
    }

    numPlatformDevices = numDevices;
    platformDevices = pDevices;

    auto &listeners = ::testing::UnitTest::GetInstance()->listeners();
    if (useDefaultListener == false) {
        auto defaultListener = listeners.default_result_printer();

        auto customEventListener = new CCustomEventListener(defaultListener);

        listeners.Release(defaultListener);
        listeners.Append(customEventListener);
    }

    listeners.Append(new MemoryLeakListener);

    gEnvironment = reinterpret_cast<TestEnvironment *>(::testing::AddGlobalTestEnvironment(new TestEnvironment));

    MockCompilerDebugVars fclDebugVars;
    MockCompilerDebugVars igcDebugVars;

    retrieveBinaryKernelFilename(fclDebugVars.fileName, "15895692906525787409_", ".bc");
    retrieveBinaryKernelFilename(igcDebugVars.fileName, "15895692906525787409_", ".gen");

    gEnvironment->setMockFileNames(fclDebugVars.fileName, igcDebugVars.fileName);
    gEnvironment->setDefaultDebugVars(fclDebugVars, igcDebugVars, device);

#if defined(__linux__)
    //ULTs timeout
    if (enable_alarm) {
        unsigned int alarmTime = OCLRT::ultIterationMaxTime * ::testing::GTEST_FLAG(repeat);

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
    initializeTestHelpers();

    retVal = RUN_ALL_TESTS();

    cleanTestHelpers();

    delete[] pDevices;

    return retVal;
}
