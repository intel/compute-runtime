/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/required_libs_helpers_tests.h"

#include "shared/source/helpers/required_libs_helpers.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace NEO {

namespace {

struct MockModuleQueries {
    MockModuleQueries()
        : instanceBackup(&instance, this),
          handleBackup(&SysCalls::sysCallsGetModuleHandleExA, getModuleHandleExA),
          fileNameBackup(&SysCalls::sysCallsGetModuleFileNameA, getModuleFileNameA) {}

    static BOOL getModuleHandleExA(DWORD flags, LPCSTR moduleName, HMODULE *module) {
        ++instance->getModuleHandleCalls;
        EXPECT_EQ(static_cast<DWORD>(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT), flags);
        EXPECT_EQ(instance->expectedIgcName, moduleName);
        if (not instance->igcLoaded) {
            return FALSE;
        }
        *module = instance->igcModule;
        return TRUE;
    }

    static DWORD getModuleFileNameA(HMODULE module, LPSTR filename, DWORD size) {
        ++instance->getModuleFileNameCalls;
        if (instance->fileNameQueryFails) {
            return 0;
        }
        const auto &path = (module == instance->igcModule) ? instance->igcPath : instance->driverPath;
        if (instance->forceTruncation || (path.size() >= size)) {
            return size;
        }
        std::copy(path.begin(), path.end(), filename);
        filename[path.size()] = '\0';
        return static_cast<DWORD>(path.size());
    }

    std::string expectedIgcName = Os::igcDllName;
    bool igcLoaded = true;
    bool fileNameQueryFails = false;
    bool forceTruncation = false;
    std::string igcPath = R"(C:\Mock IGC\igc.dll)";
    std::string driverPath = R"(C:\Mock driver\driver.dll)";
    HMODULE igcModule = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1234));
    uint32_t getModuleHandleCalls = 0;
    uint32_t getModuleFileNameCalls = 0;

  private:
    inline static MockModuleQueries *instance = nullptr;
    VariableBackup<MockModuleQueries *> instanceBackup;
    VariableBackup<decltype(SysCalls::sysCallsGetModuleHandleExA)> handleBackup;
    VariableBackup<decltype(SysCalls::sysCallsGetModuleFileNameA)> fileNameBackup;
};

constexpr auto igcDirectory = R"(C:\Mock IGC\)";
constexpr auto driverDirectory = R"(C:\Mock driver\)";

} // namespace

struct RequiredLibsHelpersWindowsTest : RequiredLibsHelpersTest {
    MockModuleQueries modules;
    RequiredLibsHelpers::LockableSearchPaths cache;
};

TEST_F(RequiredLibsHelpersWindowsTest, givenNoFixedLocationsOnWindowsThenDefaultSearchPathsAreEmpty) {
    EXPECT_TRUE(RequiredLibsHelpers::getDefaultBinarySearchPaths().empty());
}

TEST_F(RequiredLibsHelpersWindowsTest, givenLoadedIgcWhenSearchingThenOnlyItsDirectoryIsUsed) {
    auto paths = RequiredLibsHelpers::getOptionalBinarySearchPaths(cache);
    ASSERT_EQ(1u, paths.size());
    EXPECT_EQ(igcDirectory, paths[0]);
    EXPECT_EQ(1u, modules.getModuleHandleCalls);
}

TEST_F(RequiredLibsHelpersWindowsTest, givenCustomIgcLibraryNameWhenSearchingThenThatModuleIsQueried) {
    debugManager.flags.IgcLibraryName.set(R"(C:\Custom IGC\igc-compute.dll)");
    modules.expectedIgcName = R"(C:\Custom IGC\igc-compute.dll)";

    auto paths = RequiredLibsHelpers::getOptionalBinarySearchPaths(cache);
    ASSERT_EQ(1u, paths.size());
    EXPECT_EQ(igcDirectory, paths[0]);
    EXPECT_EQ(1u, modules.getModuleHandleCalls);
}

TEST_F(RequiredLibsHelpersWindowsTest, givenIgcNotLoadedWhenSearchingThenTheDriverDirectoryIsUsed) {
    modules.igcLoaded = false;

    auto paths = RequiredLibsHelpers::getOptionalBinarySearchPaths(cache);
    ASSERT_EQ(1u, paths.size());
    EXPECT_EQ(driverDirectory, paths[0]);
}

TEST_F(RequiredLibsHelpersWindowsTest, givenAlreadyResolvedDirectoryWhenSearchingAgainThenCachedValueIsReused) {
    EXPECT_EQ(1u, RequiredLibsHelpers::getOptionalBinarySearchPaths(cache).size());
    EXPECT_EQ(1u, modules.getModuleHandleCalls);

    auto paths = RequiredLibsHelpers::getOptionalBinarySearchPaths(cache);
    ASSERT_EQ(1u, paths.size());
    EXPECT_EQ(igcDirectory, paths[0]);
    EXPECT_EQ(1u, modules.getModuleHandleCalls);
    EXPECT_EQ(1u, modules.getModuleFileNameCalls);
}

TEST_F(RequiredLibsHelpersWindowsTest, givenFailedFileNameQueryWhenSearchingThenNoPathIsPublishedAndFailureIsNotCached) {
    modules.fileNameQueryFails = true;
    EXPECT_TRUE(RequiredLibsHelpers::getOptionalBinarySearchPaths(cache).empty());

    modules.fileNameQueryFails = false;
    auto paths = RequiredLibsHelpers::getOptionalBinarySearchPaths(cache);
    ASSERT_EQ(1u, paths.size());
    EXPECT_EQ(igcDirectory, paths[0]);
}

TEST_F(RequiredLibsHelpersWindowsTest, givenTruncatedModulePathWhenSearchingThenNoPathIsPublished) {
    modules.forceTruncation = true;
    EXPECT_TRUE(RequiredLibsHelpers::getOptionalBinarySearchPaths(cache).empty());
}

TEST_F(RequiredLibsHelpersWindowsTest, givenModulePathWithoutSeparatorWhenSearchingThenNoPathIsPublished) {
    modules.igcPath = "igc.dll";
    EXPECT_TRUE(RequiredLibsHelpers::getOptionalBinarySearchPaths(cache).empty());
}

TEST_F(RequiredLibsHelpersWindowsTest, givenNonAsciiModulePathWhenSearchingThenItsBytesArePreserved) {
    const auto nonAsciiDirectory = std::string(R"(C:\Mock )") + '\xCF' + R"( IGC\)";
    modules.igcPath = nonAsciiDirectory + "igc.dll";

    auto paths = RequiredLibsHelpers::getOptionalBinarySearchPaths(cache);
    ASSERT_EQ(1u, paths.size());
    EXPECT_EQ(nonAsciiDirectory, paths[0]);
}

TEST_F(RequiredLibsHelpersWindowsTest, givenLibNextToIgcWhenGettingRequiredLibDirPathThenItIsFound) {
    VariableBackup<FILE *> mockFopenReturnedBkp(&IoFunctions::mockFopenReturned, static_cast<FILE *>(nullptr));

    constexpr auto libName = "libigc-ifp.zebin.0.1.3";
    const auto libPath = NEO::joinPath(igcDirectory, libName);
    writeDataToFile(libPath.c_str(), "fakeData", false);

    std::string directory;
    EXPECT_TRUE(RequiredLibsHelpers::getRequiredLibDirPath(libName, cache, directory));
    EXPECT_EQ(igcDirectory, directory);

    removeVirtualFile(libPath);
}

TEST_F(RequiredLibsHelpersWindowsTest, givenLibOnlyInAnUnrelatedDirectoryWhenGettingRequiredLibDirPathThenItIsNotFound) {
    VariableBackup<FILE *> mockFopenReturnedBkp(&IoFunctions::mockFopenReturned, static_cast<FILE *>(nullptr));

    constexpr auto libName = "libigc-ifp.zebin.0.1.3";
    const auto libPath = NEO::joinPath(R"(C:\Somewhere else\)", libName);
    writeDataToFile(libPath.c_str(), "fakeData", false);

    std::string directory;
    EXPECT_FALSE(RequiredLibsHelpers::getRequiredLibDirPath(libName, cache, directory));

    removeVirtualFile(libPath);
}

} // namespace NEO
