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

#if defined(_WIN32)
#include "runtime/os_interface/windows/os_library.h"
#elif defined(__linux__)
#include "runtime/os_interface/linux/os_library.h"
#endif
#include "runtime/os_interface/os_library.h"
#include "test.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "gtest/gtest.h"
#include <memory>

namespace Os {
extern const char *frontEndDllName;
extern const char *igcDllName;
extern const char *testDllName;
} // namespace Os
const std::string fakeLibName = "_fake_library_name_";
const std::string fnName = "testDynamicLibraryFunc";

using namespace OCLRT;

class OSLibraryFixture : public MemoryManagementFixture

{
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
    }

    void TearDown() override {
        MemoryManagementFixture::TearDown();
    }
};

typedef Test<OSLibraryFixture> OSLibraryTest;

TEST_F(OSLibraryTest, whenLibraryNameIsEmptyThenCurrentProcesIsUsedAsLibrary) {
    std::unique_ptr<OsLibrary> library{OsLibrary::load("")};
    EXPECT_NE(nullptr, library);
    void *ptr = library->getProcAddress("selfDynamicLibraryFunc");
    EXPECT_NE(nullptr, ptr);
}

TEST_F(OSLibraryTest, CreateFake) {
    OsLibrary *library = OsLibrary::load(fakeLibName);
    EXPECT_EQ(nullptr, library);
}

TEST_F(OSLibraryTest, whenLibraryNameIsValidThenLibraryIsLoadedCorrectly) {
    std::unique_ptr<OsLibrary> library(OsLibrary::load(Os::testDllName));
    EXPECT_NE(nullptr, library);
}

TEST_F(OSLibraryTest, whenSymbolNameIsValidThenGetProcAddressReturnsNonNullPointer) {
    std::unique_ptr<OsLibrary> library(OsLibrary::load(Os::testDllName));
    EXPECT_NE(nullptr, library);
    void *ptr = library->getProcAddress(fnName);
    EXPECT_NE(nullptr, ptr);
}

TEST_F(OSLibraryTest, whenSymbolNameIsInvalidThenGetProcAddressReturnsNullPointer) {
    std::unique_ptr<OsLibrary> library(OsLibrary::load(Os::testDllName));
    EXPECT_NE(nullptr, library);
    void *ptr = library->getProcAddress(fnName + "invalid");
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(OSLibraryTest, testFailNew) {
    InjectedFunction method = [](size_t failureIndex) {
        std::string libName(Os::testDllName);

        // System under test
        OsLibrary *library = OsLibrary::load(libName);

        if (nonfailingAllocation == failureIndex) {
            EXPECT_NE(nullptr, library);
        } else {
            EXPECT_EQ(nullptr, library);
        }

        // Make sure that we only have 1 buffer allocated at a time
        delete library;
    };
    injectFailures(method);
}
