/*
 * Copyright (c) 2017, Intel Corporation
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

#include "config.h"
#include "environment.h"
#include "runtime/os_interface/os_library.h"
#include "unit_tests/custom_event_listener.h"
#include "unit_tests/helpers/test_files.h"

#include "limits.h"

#ifdef WIN32
const char *fSeparator = "\\";
#elif defined(__linux__)
const char *fSeparator = "/";
#endif

Environment *gEnvironment;

extern bool printMemoryOpCallStack;

std::string getRunPath(char *argv0) {
    std::string res(argv0);

    auto pos = res.rfind(fSeparator);
    if (pos != std::string::npos)
        return res.substr(0, pos);

    return res;
}

int main(int argc, char **argv) {
    int retVal = 0;
    bool useDefaultListener = false;
    std::string devicePrefix("skl");

#if defined(__linux__)
    if (getenv("CLOC_SELFTEST") == nullptr) {
        setenv("CLOC_SELFTEST", "YES", 1);

        char *ldLibraryPath = getenv("LD_LIBRARY_PATH");

        if (ldLibraryPath == nullptr) {
            setenv("LD_LIBRARY_PATH", getRunPath(argv[0]).c_str(), 1);
        } else {
            std::string ldLibraryPathConcat = getRunPath(argv[0]) + ":" + std::string(ldLibraryPath);
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
            } else if (strcmp("--device", argv[i]) == 0) {
                ++i;
                devicePrefix = argv[i];
            }
        }
    }

    // we look for test files always relative to binary location
    // this simplifies multi-process execution and using different
    // working directories
    std::string nTestFiles = getRunPath(argv[0]);
    nTestFiles.append("/");
    nTestFiles.append(devicePrefix);
    nTestFiles.append("/");
    nTestFiles.append(testFiles);
    testFiles = nTestFiles;

    if (useDefaultListener == false) {
        ::testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
        ::testing::TestEventListener *defaultListener = listeners.default_result_printer();

        auto customEventListener = new CCustomEventListener(defaultListener);

        listeners.Release(listeners.default_result_printer());
        listeners.Append(customEventListener);
    }

    gEnvironment = reinterpret_cast<Environment *>(::testing::AddGlobalTestEnvironment(new Environment(devicePrefix)));

    retVal = RUN_ALL_TESTS();

    return retVal;
}
