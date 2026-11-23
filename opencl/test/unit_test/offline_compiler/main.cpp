/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/utilities/logger.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/helpers/ult_main_helpers.h"
#include "shared/test/common/libult/signal_utils.h"

#include "environment.h"
#include "limits.h"

#include <igfxfmid.h>
#include <regex>

Environment *gEnvironment;
extern PRODUCT_FAMILY productFamily;
extern GFXCORE_FAMILY renderCoreFamily;

std::string getRunPath() {
    char *cwd;
#if defined(__linux__)
    cwd = getcwd(nullptr, 0);
#else
    cwd = _getcwd(nullptr, 0);
#endif
    std::string res{cwd};
    free(cwd);

    return res;
}

void applyWorkarounds() {
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
        std::string res("abc");
        res = res.substr(0, 2);
        res += "abc";
    }
    // Create FileLogger to prevent false memory leaks
    {
        NEO::fileLoggerInstance();
    }
    {
        [[maybe_unused]] auto ret = std::regex_search("str", std::regex(std::string(".")));
    }
    // initialize rand
    srand(static_cast<unsigned int>(time(nullptr)));
}

int main(int argc, char **argv) {
    int retVal = 0;
    NEO::UltCommandLineOptions ultOptions;
    bool enableAbrt = true;
    bool enableAlarm = true;
    bool enableSegv = true;

    std::string devicePrefix(DEFAULT_TEST_PLATFORM_NAME);
    std::string productConfig("");

    applyWorkarounds();

#if defined(__linux__)
    if (getenv("CLOC_SELFTEST") == nullptr) {
        setenv("CLOC_SELFTEST", "YES", 1);

        char *ldLibraryPath = getenv("LD_LIBRARY_PATH");

        if (ldLibraryPath == nullptr) {
            setenv("LD_LIBRARY_PATH", getRunPath().c_str(), 1);
        } else {
            std::string ldLibraryPathConcat = getRunPath() + ":" + std::string(ldLibraryPath);
            setenv("LD_LIBRARY_PATH", ldLibraryPathConcat.c_str(), 1);
        }

        execv(argv[0], argv);
        // execv failed, we return with error
        printf("FATAL ERROR: cannot self-exec test!\n");
        return -1;
    }
#endif

    ::testing::InitGoogleTest(&argc, argv);
    for (int i = 1; i < argc; i++) {
        if (NEO::parseCommonUltOption(argc, argv, i, ultOptions, enableAlarm)) {
            continue;
        }
        if (strcmp("--device", argv[i]) == 0 && i + 1 < argc) {
            devicePrefix = argv[++i];
        }
    }

    uint16_t revision = 0;
    for (unsigned int productId = 0; productId < NEO::maxProductEnumValue; ++productId) {
        if (NEO::hardwarePrefix[productId] && (0 == strcmp(devicePrefix.c_str(), NEO::hardwarePrefix[productId]))) {
            if (NEO::hardwareInfoTable[productId]) {
                renderCoreFamily = NEO::hardwareInfoTable[productId]->platform.eRenderCoreFamily;
                productFamily = NEO::hardwareInfoTable[productId]->platform.eProductFamily;
                revision = NEO::hardwareInfoTable[productId]->platform.usRevId;
                break;
            }
        }
    }

    auto productConfigHelper = new ProductConfigHelper();
    auto allEnabledDeviceConfigs = productConfigHelper->getDeviceAotInfo();

    for (const auto &device : allEnabledDeviceConfigs) {
        if (device.hwInfo->platform.eProductFamily == productFamily) {
            productConfig = ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
            break;
        }
    }

    // we look for test files always relative to binary location
    // this simplifies multi-process execution and using different
    // working directories
    std::string nTestFiles = getRunPath();
    nTestFiles.append("/");
    nTestFiles.append(devicePrefix);
    nTestFiles.append("/");
    nTestFiles.append(std::to_string(revision));
    nTestFiles.append("/");
    nTestFiles.append(testFiles);
    testFiles = nTestFiles;
    binaryNameSuffix.append(devicePrefix);

#if defined(_WIN32)
#include <direct.h>
    if (_chdir(devicePrefix.c_str())) {
        std::cout << "chdir into " << devicePrefix << " directory failed.\nThis might cause test failures." << std::endl;
    }
#elif defined(__linux__)
#include <unistd.h>
    if (chdir(devicePrefix.c_str()) != 0) {
        std::cout << "chdir into " << devicePrefix << " directory failed.\nThis might cause test failures." << std::endl;
    }
#endif

    auto &listeners = ::testing::UnitTest::GetInstance()->listeners();
    if (!ultOptions.useDefaultListener) {
        NEO::replaceDefaultResultPrinter(listeners);
    }
    NEO::appendCommonUltListeners(listeners);

    gEnvironment = reinterpret_cast<Environment *>(::testing::AddGlobalTestEnvironment(new Environment(devicePrefix, productConfig)));

    int sigOut = setSegv(enableSegv);
    if (sigOut != 0) {
        return sigOut;
    }

    sigOut = setAbrt(enableAbrt);
    if (sigOut != 0) {
        return sigOut;
    }

    sigOut = setAlarm(enableAlarm);
    if (sigOut != 0) {
        return sigOut;
    }

    retVal = RUN_ALL_TESTS();
    cleanupSignals();

    NEO::reportTestStats(ultOptions);

    return retVal;
}
