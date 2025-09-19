/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_interface.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/source/utilities/logger.h"
#include "shared/test/common/helpers/custom_event_listener.h"
#include "shared/test/common/helpers/default_hw_info.inl"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/memory_leak_listener.h"
#include "shared/test/common/helpers/mock_sip_listener.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/helpers/ult_hw_config.inl"
#include "shared/test/common/helpers/virtual_file_system_listener.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/libult/signal_utils.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/common/test_stats.h"
#include "shared/test/common/tests_configuration.h"

#include "gtest/gtest.h"
#include "hw_cmds_default.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#if !defined(__linux__)
#include <regex>
#endif

namespace NEO {
namespace PagaFaultManagerTestConfig {
bool disabled = false;
}
extern const char *executionDirectorySuffix;
extern bool useMockGmm;

#ifdef WIN32
const char *fSeparator = "\\";
#else
const char *fSeparator = "/";
#endif

bool checkAubTestsExecutionPathValidity() {
    bool valid = true;
    if ((testMode == TestMode::aubTests || testMode == TestMode::aubTestsWithTbx)) {
        std::ofstream testFile;
        std::string aubPath = folderAUB;
        aubPath += fSeparator;
        aubPath += "testAubFolder";
        testFile.open(aubPath, std::ofstream::app);
        if (testFile.is_open()) {
            testFile.close();
        } else {
            valid = false;
            std::cout << "ERROR: Aub tests must be run in directory containing \" " << folderAUB << "\" folder!\n";
        }
    }
    return valid;
}

} // namespace NEO

using namespace NEO;

extern PRODUCT_FAMILY productFamily;
extern GFXCORE_FAMILY renderCoreFamily;

void applyWorkarounds();
void setupTestFiles(std::string testBinaryFiles, int32_t revId);
std::string getBaseExecutionDir();
bool isChangeDirectoryRequired();

void addUltListener(::testing::TestEventListeners &listener);
void cleanTestHelpers();

bool generateRandomInput = false;
std::optional<uint32_t> blitterMaskOverride;

std::string getRunPath(char *argv0) {
    std::string res(argv0);

    auto pos = res.rfind(fSeparator);
    if (pos != std::string::npos)
        res = res.substr(0, pos);

    if (res == "." || pos == std::string::npos) {
        char *cwd;
#if defined(__linux__)
        cwd = getcwd(nullptr, 0);
#else
        cwd = _getcwd(nullptr, 0);
#endif
        if (cwd) {
            res = cwd;
            free(cwd);
        }
    }

    return res;
}
std::thread::id tempThreadID;
void applyCommonWorkarounds() {
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

    // initialize rand
    srand(static_cast<unsigned int>(time(nullptr)));

    // Create at least on thread to prevent false memory leaks in tests using threads
    std::thread t([&]() {
    });
    tempThreadID = t.get_id();
    t.join();

    // Create FileLogger to prevent false memory leaks
    {
        NEO::fileLoggerInstance();
        NEO::usmReusePerfLoggerInstance();
    }
}

bool enableAlarm = ENABLE_ALARM_DEFAULT;
int main(int argc, char **argv) {
#if !defined(__linux__)
    std::regex dummyRegex{"dummyRegex"};   // these dummy objects are neededed to prevent false-positive
    std::wstringstream dummyWstringstream; // leaks when using instances of std::regex and std::wstringstream in tests
    dummyWstringstream << std::setw(4)     // in Windows Release builds
                       << std::setfill(L'0')
                       << std::hex << 5;
#endif
    int retVal = 0;
    bool useDefaultListener = false;
    bool enableAbrt = true;
    bool enableSegv = true;
    bool showTestStats = false;
    bool dumpTestStats = false;
    std::string dumpTestStatsFileName = "";
    applyWorkarounds();
    applyCommonWorkarounds();
    CpuInfo::cpuidexFunc = [](int *, int, int) -> void {};
    CpuInfo::cpuidFunc = [](int[4], int) -> void {};

#if defined(__linux__)
    if (getenv("IGDRCL_TEST_SELF_EXEC") == nullptr) {
        std::string wd = getRunPath(argv[0]);
        char *ldLibraryPath = getenv("LD_LIBRARY_PATH");

        if (ldLibraryPath == nullptr) {
            setenv("LD_LIBRARY_PATH", wd.c_str(), 1);
        } else {
            std::string ldLibraryPathConcat = wd + ":" + std::string(ldLibraryPath);
            setenv("LD_LIBRARY_PATH", ldLibraryPathConcat.c_str(), 1);
        }

        setenv("IGDRCL_TEST_SELF_EXEC", wd.c_str(), 1);
        execv(argv[0], argv);
        printf("FATAL ERROR: cannot self-exec test: %s!, errno: %d\n", argv[0], errno);
        return -1;
    }
#endif

    ::testing::InitGoogleTest(&argc, argv);
    HardwareInfo hwInfoForTests = DEFAULT_TEST_PLATFORM::hwInfo;

    uint32_t euPerSubSlice = 0;
    uint32_t sliceCount = 0;
    uint32_t subSlicePerSliceCount = 0;
    int32_t revId = -1;
    int32_t devId = -1;
    int dieRecovery = 0;
    bool productOptionSelected = false;

    std::vector<PRODUCT_FAMILY> selectedTestProducts;

    auto baseTestFiles = testFiles;
    auto baseTestFilesApiSpecific = testFilesApiSpecific;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp("--disable_default_listener", argv[i])) {
            useDefaultListener = false;
        } else if (!strcmp("--enable_default_listener", argv[i])) {
            useDefaultListener = true;
        } else if (!strcmp("--disable_alarm", argv[i])) {
            enableAlarm = false;
        } else if (!strcmp("--show_test_stats", argv[i])) {
            showTestStats = true;
        } else if (!strcmp("--dump_test_stats", argv[i])) {
            dumpTestStats = true;
            ++i;
            dumpTestStatsFileName = std::string(argv[i]);
        } else if (!strcmp("--disable_pagefaulting_tests", argv[i])) { // disable tests which raise page fault signal during execution
            NEO::PagaFaultManagerTestConfig::disabled = true;
        } else if (!strcmp("--tbx", argv[i])) {
            if (isAubTestMode(testMode)) {
                testMode = TestMode::aubTestsWithTbx;
            }
            initialHardwareTag = 0;
        } else if (!strcmp("--rev_id", argv[i])) {
            ++i;
            if (i < argc) {
                revId = atoi(argv[i]);
            }
        } else if (!strcmp("--dev_id", argv[i])) {
            ++i;
            if (i < argc) {
                devId = static_cast<int32_t>(strtol(argv[i], nullptr, 16));
            }
        } else if (!strcmp("--product", argv[i])) {
            ++i;
            if (i < argc) {
                if (::isdigit(argv[i][0])) {
                    int productValue = atoi(argv[i]);
                    if (productValue > 0 && productValue < IGFX_MAX_PRODUCT && hardwarePrefix[productValue] != nullptr) {
                        productFamily = static_cast<PRODUCT_FAMILY>(productValue);
                    } else {
                        productFamily = IGFX_UNKNOWN;
                    }
                } else {
                    bool selectAllProducts = (strcmp("*", argv[i]) == 0);
                    productFamily = IGFX_UNKNOWN;
                    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
                        if (hardwarePrefix[j] == nullptr)
                            continue;
                        if ((strcmp(hardwarePrefix[j], argv[i]) == 0) || selectAllProducts) {
                            productFamily = static_cast<PRODUCT_FAMILY>(j);
                            selectedTestProducts.push_back(productFamily);

                            if (!selectAllProducts) {
                                break;
                            }
                        }
                    }
                }
                if (productFamily == IGFX_UNKNOWN) {
                    std::cout << "unknown or unsupported product family has been set: " << argv[i] << std::endl;
                    return -1;
                } else {
                    std::cout << "product family: " << hardwarePrefix[productFamily] << " (" << productFamily << ")" << std::endl;
                }
                hwInfoForTests = *hardwareInfoTable[productFamily];
                productOptionSelected = true;
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
        } else if (!strcmp("--read-config", argv[i]) && isAubTestMode(testMode)) {
            if (debugManager.registryReadAvailable()) {
                ApiSpecificConfig::initPrefixes();
                debugManager.setReaderImpl(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
                debugManager.injectSettingsFromReader();
            }
        } else if (!strcmp("--dump_buffer_format", argv[i]) && isAubTestMode(testMode)) {
            ++i;
            std::string dumpBufferFormat(argv[i]);
            std::transform(dumpBufferFormat.begin(), dumpBufferFormat.end(), dumpBufferFormat.begin(), ::toupper);
            debugManager.flags.AUBDumpBufferFormat.set(dumpBufferFormat);
        } else if (!strcmp("--dump_image_format", argv[i]) && isAubTestMode(testMode)) {
            ++i;
            std::string dumpImageFormat(argv[i]);
            std::transform(dumpImageFormat.begin(), dumpImageFormat.end(), dumpImageFormat.begin(), ::toupper);
            debugManager.flags.AUBDumpImageFormat.set(dumpImageFormat);
        } else if (!strcmp("--null_aubstream", argv[i])) {
            if (isAubTestMode(testMode)) {
                testMode = TestMode::aubTestsWithoutOutputFiles;
            }
            initialHardwareTag = 0;
        } else if (!strcmp("--blitterMask", argv[i])) {
            ++i;
            if (i < argc) {
                blitterMaskOverride = static_cast<uint32_t>(std::stoi(argv[i]));
            }
        }
    }

    if (!productOptionSelected) {
        selectedTestProducts.push_back(hwInfoForTests.platform.eProductFamily);
    }

    auto &listeners = ::testing::UnitTest::GetInstance()->listeners();
    auto defaultListener = listeners.default_result_printer();

    defaultHwInfo = std::make_unique<HardwareInfo>();

    CCustomEventListener *customEventListener = nullptr;
    bool ultListenersInitialized = false;
    bool gmmInitialized = false;
    bool sipInitialized = false;

    adjustCsrType(testMode);
    for (auto &selectedProduct : selectedTestProducts) {
        hwInfoForTests = *hardwareInfoTable[selectedProduct];

        productFamily = hwInfoForTests.platform.eProductFamily;
        renderCoreFamily = hwInfoForTests.platform.eRenderCoreFamily;
        PLATFORM &platform = hwInfoForTests.platform;

        auto testRevId = revId;
        auto testDevId = devId;

        if (testRevId != -1) {
            platform.usRevId = testRevId;
        } else {
            testRevId = platform.usRevId;
        }

        if (testDevId != -1) {
            platform.usDeviceID = testDevId;
        }

        adjustHwInfoForTests(hwInfoForTests, euPerSubSlice, sliceCount, subSlicePerSliceCount, dieRecovery);

        binaryNameSuffix = hardwarePrefix[hwInfoForTests.platform.eProductFamily];

        testFiles = baseTestFiles;
        testFilesApiSpecific = baseTestFilesApiSpecific;

        setupTestFiles(getRunPath(argv[0]), testRevId);

        if (isChangeDirectoryRequired()) {
            auto executionDirectory = getBaseExecutionDir();
            executionDirectory += hardwarePrefix[productFamily];
            executionDirectory += NEO::executionDirectorySuffix; // _aub for aub_tests, empty otherwise
            executionDirectory += "/";
            executionDirectory += std::to_string(testRevId);

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
        }

        if (!checkAubTestsExecutionPathValidity()) {
            return -1;
        }

        *defaultHwInfo = hwInfoForTests;

        if (!useDefaultListener) {
            if (!customEventListener) {
                customEventListener = new CCustomEventListener(defaultListener);
            }
            customEventListener->setHwPrefix(hardwarePrefix[productFamily]);
        }

        if (!ultListenersInitialized) {
            if (!useDefaultListener) {
                listeners.Release(defaultListener);

                listeners.Append(customEventListener);
            }

            listeners.Append(new MockSipListener);
            listeners.Append(new MemoryLeakListener);
            listeners.Append(new NEO::VirtualFileSystemListener);

            addUltListener(listeners);
            ultListenersInitialized = true;
        }

        if (!gEnvironment) {
            gEnvironment = reinterpret_cast<TestEnvironment *>(::testing::AddGlobalTestEnvironment(new TestEnvironment));
        }

        MockCompilerDebugVars fclDebugVars;
        MockCompilerDebugVars igcDebugVars;

        std::string builtInsFileName;
        if (TestChecks::supportsImages(defaultHwInfo)) {
            builtInsFileName = KernelBinaryHelper::BUILT_INS_WITH_IMAGES;
        } else {
            builtInsFileName = KernelBinaryHelper::BUILT_INS;
        }
        std::string options = "";
        if (defaultHwInfo->featureTable.flags.ftrHeaplessMode) {
            options = "-heapless";
        }
        retrieveBinaryKernelFilename(fclDebugVars.fileName, builtInsFileName + "_", ".spv", options);
        retrieveBinaryKernelFilename(igcDebugVars.fileName, builtInsFileName + "_", ".bin", options);

        gEnvironment->setMockFileNames(fclDebugVars.fileName, igcDebugVars.fileName);
        gEnvironment->setDefaultDebugVars(fclDebugVars, igcDebugVars, hwInfoForTests);

        int sigOut = setSegv(enableSegv);
        if (sigOut != 0) {
            return sigOut;
        }

        sigOut = setAbrt(enableAbrt);
        if (sigOut != 0) {
            return sigOut;
        }

        if (!gmmInitialized) {
            if (useMockGmm) {
                GmmHelper::createGmmContextWrapperFunc = GmmClientContext::create<MockGmmClientContext>;
            } else {
                GmmInterface::initialize(nullptr, nullptr);
            }
            gmmInitialized = true;
        }

        if (!sipInitialized) {
            MockSipData::mockSipKernel.reset(new MockSipKernel());
            if (testMode == TestMode::aubTests || testMode == TestMode::aubTestsWithTbx) {
                MockSipData::useMockSip = false;
                debugManager.flags.OverrideCsrAllocationSize.set(1);
            } else {
                MockSipData::useMockSip = true;
            }
            sipInitialized = true;
        }

        Device::createPerformanceCountersFunc = [](Device *) -> std::unique_ptr<NEO::PerformanceCounters> { return {}; };

        sigOut = setAlarm(enableAlarm);
        if (sigOut != 0) {
            return sigOut;
        }

        retVal = RUN_ALL_TESTS();
        cleanupSignals();

        if (showTestStats) {
            std::cout << getTestStats() << std::endl;
        }

        if (dumpTestStats) {
            std::ofstream dumpTestStatsFile;
            dumpTestStatsFile.open(dumpTestStatsFileName);
            dumpTestStatsFile << getTestStatsJson();
            dumpTestStatsFile.close();
        }
        cleanTestHelpers();

        if (retVal != 0) {
            break;
        }
    }

    return retVal;
}