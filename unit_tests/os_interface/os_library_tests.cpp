/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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

TEST(OsLibrary, whenCallingIndexOperatorThenObjectConvertibleToFunctionOrVoidPointerIsReturned) {
    struct MockOsLibrary : OsLibrary {
        void *getProcAddress(const std::string &procName) override {
            lastRequestedProcName = procName;
            return ptrToReturn;
        }
        bool isLoaded() override { return true; }

        void *ptrToReturn = nullptr;
        std::string lastRequestedProcName;
    };

    MockOsLibrary lib;

    int varA;
    int varB;
    int varC;

    using FunctionTypeA = void (*)(int *, float);
    using FunctionTypeB = int (*)();

    lib.ptrToReturn = &varA;
    FunctionTypeA functionA = lib["funcA"];
    EXPECT_STREQ("funcA", lib.lastRequestedProcName.c_str());
    EXPECT_EQ(&varA, reinterpret_cast<void *>(functionA));

    lib.ptrToReturn = &varB;
    FunctionTypeB functionB = lib["funcB"];
    EXPECT_STREQ("funcB", lib.lastRequestedProcName.c_str());
    EXPECT_EQ(&varB, reinterpret_cast<void *>(functionB));

    lib.ptrToReturn = &varC;
    void *rawPtr = lib["funcC"];
    EXPECT_STREQ("funcC", lib.lastRequestedProcName.c_str());
    EXPECT_EQ(&varC, rawPtr);
}
