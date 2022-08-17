/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/custom_event_listener.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/signal_utils.h"
#include "shared/test/common/test_stats.h"

#include "environment.h"
#include "limits.h"
#include "test_files_setup.h"

#ifdef WIN32
const char *fSeparator = "\\";
#elif defined(__linux__)
const char *fSeparator = "/";
#endif

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

int main(int argc, char **argv) {
    int retVal = 0;
    bool useDefaultListener = false;
    bool enableAbrt = true;
    bool enableAlarm = true;
    bool enableSegv = true;
    bool showTestStats = false;
    bool dumpTestStats = false;
    std::string dumpTestStatsFileName = "";

    std::string devicePrefix("skl");
    std::string familyNameWithType("Gen9core");
    std::string revId("0");

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
        //execv failed, we return with error
        printf("FATAL ERROR: cannot self-exec test!\n");
        return -1;
    }
#endif

    ::testing::InitGoogleTest(&argc, argv);
    if (argc > 0) {
        // parse remaining args assuming they're mine
        for (int i = 0; i < argc; i++) {
            if (strcmp("--use_default_listener", argv[i]) == 0) {
                useDefaultListener = true;
            } else if (!strcmp("--disable_alarm", argv[i])) {
                enableAlarm = false;
            } else if (strcmp("--device", argv[i]) == 0) {
                ++i;
                devicePrefix = argv[i];
            } else if (strcmp("--family_type", argv[i]) == 0) {
                ++i;
                familyNameWithType = argv[i];
            } else if (strcmp("--rev_id", argv[i]) == 0) {
                ++i;
                revId = argv[i];
            } else if (!strcmp("--show_test_stats", argv[i])) {
                showTestStats = true;
            } else if (!strcmp("--dump_test_stats", argv[i])) {
                dumpTestStats = true;
                ++i;
                dumpTestStatsFileName = std::string(argv[i]);
            }
        }
    }

    for (unsigned int productId = 0; productId < IGFX_MAX_PRODUCT; ++productId) {
        if (NEO::hardwarePrefix[productId] && (0 == strcmp(devicePrefix.c_str(), NEO::hardwarePrefix[productId]))) {
            if (NEO::hardwareInfoTable[productId]) {
                renderCoreFamily = NEO::hardwareInfoTable[productId]->platform.eRenderCoreFamily;
                productFamily = NEO::hardwareInfoTable[productId]->platform.eProductFamily;
                break;
            }
        }
    }

    // we look for test files always relative to binary location
    // this simplifies multi-process execution and using different
    // working directories
    std::string nTestFiles = getRunPath();
    nTestFiles.append("/");
    nTestFiles.append(familyNameWithType);
    nTestFiles.append("/");
    nTestFiles.append(revId);
    nTestFiles.append("/");
    nTestFiles.append(testFiles);
    testFiles = nTestFiles;
    binaryNameSuffix.append(familyNameWithType);

    std::string nClFiles = NEO_OPENCL_TEST_FILES_DIR;
    nClFiles.append("/");
    clFiles = nClFiles;

#ifdef WIN32
#include <direct.h>
    if (_chdir(familyNameWithType.c_str())) {
        std::cout << "chdir into " << familyNameWithType << " directory failed.\nThis might cause test failures." << std::endl;
    }
#elif defined(__linux__)
#include <unistd.h>
    if (chdir(familyNameWithType.c_str()) != 0) {
        std::cout << "chdir into " << familyNameWithType << " directory failed.\nThis might cause test failures." << std::endl;
    }
#endif

    if (useDefaultListener == false) {
        ::testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
        ::testing::TestEventListener *defaultListener = listeners.default_result_printer();

        auto customEventListener = new CCustomEventListener(defaultListener);

        listeners.Release(listeners.default_result_printer());
        listeners.Append(customEventListener);
    }

    gEnvironment = reinterpret_cast<Environment *>(::testing::AddGlobalTestEnvironment(new Environment(devicePrefix, familyNameWithType)));

    int sigOut = setAlarm(enableAlarm);
    if (sigOut != 0) {
        return sigOut;
    }

    sigOut = setSegv(enableSegv);
    if (sigOut != 0) {
        return sigOut;
    }

    sigOut = setAbrt(enableAbrt);
    if (sigOut != 0) {
        return sigOut;
    }

    retVal = RUN_ALL_TESTS();

    if (showTestStats) {
        std::cout << getTestStats() << std::endl;
    }

    if (dumpTestStats) {
        std::ofstream dumpTestStatsFile;
        dumpTestStatsFile.open(dumpTestStatsFileName);
        dumpTestStatsFile << getTestStatsJson();
        dumpTestStatsFile.close();
    }

    return retVal;
}
