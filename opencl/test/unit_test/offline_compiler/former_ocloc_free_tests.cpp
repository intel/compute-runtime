/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/offline_compiler/source/ocloc_interface.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/product_config_helper_former.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_os_library.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include "gtest/gtest.h"

#include <memory>
#include <set>

using namespace Ocloc;
using namespace NEO;

namespace Ocloc {
extern std::string oclocFormerLibName;
}

namespace FormerOclocTestMocks {
std::set<void *> allocatedMemory;
bool freeWasCalled = false;

void resetTracking() {
    allocatedMemory.clear();
    freeWasCalled = false;
}

int mockFormerOclocInvoke(unsigned int numArgs, const char *argv[],
                          const uint32_t numSources, const uint8_t **dataSources, const uint64_t *lenSources, const char **nameSources,
                          const uint32_t numInputHeaders, const uint8_t **dataInputHeaders, const uint64_t *lenInputHeaders, const char **nameInputHeaders,
                          uint32_t *numOutputs, uint8_t ***dataOutputs, uint64_t **lenOutputs, char ***nameOutputs) {

    if (numOutputs && dataOutputs && lenOutputs && nameOutputs) {
        *numOutputs = 2;

        // Allocate output arrays
        *dataOutputs = new uint8_t *[2];
        allocatedMemory.insert(*dataOutputs);

        (*dataOutputs)[0] = new uint8_t[100];
        allocatedMemory.insert((*dataOutputs)[0]);

        (*dataOutputs)[1] = new uint8_t[50];
        allocatedMemory.insert((*dataOutputs)[1]);

        *lenOutputs = new uint64_t[2];
        allocatedMemory.insert(*lenOutputs);
        (*lenOutputs)[0] = 100;
        (*lenOutputs)[1] = 50;

        *nameOutputs = new char *[2];
        allocatedMemory.insert(*nameOutputs);

        constexpr char name0[] = "out.bin";
        constexpr char name1[] = "out.gen";
        (*nameOutputs)[0] = new char[sizeof(name0)];
        allocatedMemory.insert((*nameOutputs)[0]);
        memcpy_s((*nameOutputs)[0], sizeof(name0), name0, sizeof(name0));

        (*nameOutputs)[1] = new char[sizeof(name1)];
        allocatedMemory.insert((*nameOutputs)[1]);
        memcpy_s((*nameOutputs)[1], sizeof(name1), name1, sizeof(name1));
    }

    return OCLOC_SUCCESS;
}

int mockFormerOclocFree(uint32_t *numOutputs, uint8_t ***dataOutputs,
                        uint64_t **lenOutputs, char ***nameOutputs) {
    freeWasCalled = true;

    if (numOutputs && dataOutputs && lenOutputs && nameOutputs) {
        for (uint32_t i = 0; i < *numOutputs; ++i) {
            if ((*dataOutputs)[i]) {
                allocatedMemory.erase((*dataOutputs)[i]);
                delete[] (*dataOutputs)[i];
            }
            if ((*nameOutputs)[i]) {
                allocatedMemory.erase((*nameOutputs)[i]);
                delete[] (*nameOutputs)[i];
            }
        }
        allocatedMemory.erase(*dataOutputs);
        delete[] * dataOutputs;
        allocatedMemory.erase(*lenOutputs);
        delete[] * lenOutputs;
        allocatedMemory.erase(*nameOutputs);
        delete[] * nameOutputs;

        *dataOutputs = nullptr;
        *lenOutputs = nullptr;
        *nameOutputs = nullptr;
        *numOutputs = 0;
    }

    return OCLOC_SUCCESS;
}
} // namespace FormerOclocTestMocks

// Helper function to create mock former ocloc device data
static Ocloc::SupportedDevicesHelper::SupportedDevicesData createFormerOclocTestData() {
    return Ocloc::SupportedDevicesHelper::SupportedDevicesData{
        {0x02000000, 0x02400009, 0x02404009},                            // deviceIpVersions
        {{0x1616, 0x00, 0x02000000}, {0x1912, 0x00, 0x02400009}},        // deviceInfos
        {{"bdw", 0x02000000}, {"skl", 0x02400009}, {"kbl", 0x02404009}}, // acronyms
        {{"gen8", {0x02000000}}, {"gen9", {0x02400009, 0x02404009}}},    // familyGroups
        {{"gen8", {0x02000000}}, {"gen9", {0x02400009, 0x02404009}}}     // releaseGroups
    };
}

TEST(FormerOclocCompileMemoryTest, givenFormerDeviceWhenCompilingThenMemoryIsProperlyFreed) {
    FormerOclocTestMocks::resetTracking();

    Ocloc::oclocFormerLibName = "oclocFormer";

    static auto storedOriginalLoadFunc = NEO::OsLibrary::loadFunc;
    auto selectiveLoadFunc = [](const NEO::OsLibraryCreateProperties &properties) -> NEO::OsLibrary * {
        if (properties.libraryName == "oclocFormer") {
            // Create a new mock library for each load (invoke + free)
            auto mockLib = new MockOsLibraryCustom(nullptr, true);
            mockLib->procMap["oclocInvoke"] = reinterpret_cast<void *>(FormerOclocTestMocks::mockFormerOclocInvoke);
            mockLib->procMap["oclocFreeOutput"] = reinterpret_cast<void *>(FormerOclocTestMocks::mockFormerOclocFree);
            return mockLib;
        }
        return storedOriginalLoadFunc(properties);
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, +selectiveLoadFunc};

    std::string clFileName = "test_kernel.cl";
    std::string clSource = "__kernel void test(__global int *out) { out[0] = 42; }";
    MockOclocArgHelper::FilesMap mockFiles{{clFileName, clSource}};
    MockOclocArgHelper mockArgHelper{mockFiles};

    auto formerHelper = std::make_unique<FormerProductConfigHelper>();
    formerHelper->getData() = createFormerOclocTestData();
    mockArgHelper.setFormerProductConfigHelper(std::move(formerHelper));

    std::vector<std::string> args = {
        "ocloc", "-file", clFileName, "-device", "bdw", "-q"};

    StreamCapture capture;
    capture.captureStdout();
    Commands::compile(&mockArgHelper, args);
    capture.getCapturedStdout();

    EXPECT_TRUE(FormerOclocTestMocks::freeWasCalled);
    EXPECT_TRUE(FormerOclocTestMocks::allocatedMemory.empty());
}

TEST(FormerOclocFatbinaryMemoryTest, givenFormerDevicesInFatbinaryWhenCompilingThenMemoryIsProperlyFreed) {
    FormerOclocTestMocks::resetTracking();

    Ocloc::oclocFormerLibName = "oclocFormer";

    static auto storedOriginalLoadFunc = NEO::OsLibrary::loadFunc;
    auto selectiveLoadFunc = [](const NEO::OsLibraryCreateProperties &properties) -> NEO::OsLibrary * {
        if (properties.libraryName == "oclocFormer") {
            // Create a new mock library for each load (invoke + free)
            auto mockLib = new MockOsLibraryCustom(nullptr, true);
            mockLib->procMap["oclocInvoke"] = reinterpret_cast<void *>(FormerOclocTestMocks::mockFormerOclocInvoke);
            mockLib->procMap["oclocFreeOutput"] = reinterpret_cast<void *>(FormerOclocTestMocks::mockFormerOclocFree);
            return mockLib;
        }
        return storedOriginalLoadFunc(properties);
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, +selectiveLoadFunc};

    std::string clFileName = "test_kernel.cl";
    std::string clSource = "__kernel void test(__global int *out) { out[0] = 42; }";
    MockOclocArgHelper::FilesMap mockFiles{{clFileName, clSource}};
    MockOclocArgHelper mockArgHelper{mockFiles};

    auto formerHelper = std::make_unique<FormerProductConfigHelper>();
    formerHelper->getData() = createFormerOclocTestData();
    mockArgHelper.setFormerProductConfigHelper(std::move(formerHelper));

    std::vector<std::string> args = {
        "ocloc", "-file", clFileName, "-device", "bdw,skl,kbl", "-q"};

    StreamCapture capture;
    capture.captureStdout();
    Commands::compile(&mockArgHelper, args);
    capture.getCapturedStdout();

    EXPECT_TRUE(FormerOclocTestMocks::freeWasCalled);
    EXPECT_TRUE(FormerOclocTestMocks::allocatedMemory.empty());
}
