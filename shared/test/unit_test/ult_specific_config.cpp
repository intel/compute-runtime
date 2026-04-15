/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"
#include "shared/test/common/base_ult_config_listener.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/tests_configuration.h"
#include "shared/test/unit_test/mocks/mock_cpuid_functions.h"

#include "test_files_setup.h"

namespace NEO {
namespace ImplicitScaling {
bool apiSupport = false;
} // namespace ImplicitScaling

const char *neoMockSettingsFileName = "neo_mock.config";

bool CompressionSelector::preferCompressedAllocation(const AllocationProperties &properties) {
    return false;
}
void CpuPageFaultManager::transferToCpu(void *ptr, size_t size, void *cmdQ) {
}
void CpuPageFaultManager::transferToGpu(void *ptr, void *cmdQ) {
}
void CpuPageFaultManager::allowCPUMemoryEviction(bool evict, void *ptr, PageFaultData &pageFaultData) {
}

void RootDeviceEnvironment::initApiGfxCoreHelper() {
}
const char *apiName = "SHARED";

} // namespace NEO

using namespace NEO;
void cleanTestHelpers() {}

void applyWorkarounds() {
    CpuInfo::cpuidexFunc = [](int *, int, int) -> void {};
    CpuInfo::cpuidFunc = [](int[4], int) -> void {};
    auto *mockCpuInfo = getMockCpuInfo(CpuInfo::getInstance());
    mockCpuInfo->features = CpuInfo::featureNone;
    mockCpuInfo->featuresDetected = true;
    mockCpuInfo->virtualAddressSize = is32bit ? 32u : 48u;
    mockCpuInfo->cpuFlags.clear();
    const std::array<ConstStringRef, 12> builtinBaseNames{"copy_buffer_to_buffer",
                                                          "copy_buffer_rect",
                                                          "fill_buffer",
                                                          "copy_buffer_to_image3d",
                                                          "copy_image3d_to_buffer",
                                                          "copy_image_to_image1d",
                                                          "copy_image_to_image2d",
                                                          "copy_image_to_image3d",
                                                          "fill_image1d",
                                                          "fill_image2d",
                                                          "fill_image3d",
                                                          "fill_image1d_buffer"};
    const BuiltIn::AddressingMode modes[] = {
        BuiltIn::bindfulImageBindfulBuffer,
        BuiltIn::bindfulImageStatelessBuffer,
        BuiltIn::bindfulImageStatelessBufferWide,
        BuiltIn::bindlessImageBindlessBuffer,
        BuiltIn::bindlessImageStatelessBuffer,
        BuiltIn::bindlessImageStatelessBufferWide};
    auto &storageRegistry = BuiltIn::EmbeddedStorageRegistry::getInstance();
    std::string resource = "__mock_spirv_resource";
    for (const auto &baseName : builtinBaseNames) {
        for (const auto &mode : modes) {
            std::string fullName = mode.toString() + baseName.str() + ".builtin_kernel.spv";
            storageRegistry.store(fullName, BuiltIn::createResource(resource.data(), resource.size() + 1));
        }
    }
}

void setupTestFiles(std::string testBinaryFiles, int32_t revId) {
    testBinaryFiles.append("/");
    testBinaryFiles.append(binaryNameSuffix);
    testBinaryFiles.append("/");
    testBinaryFiles.append(std::to_string(revId));
    testBinaryFiles.append("/");
    testBinaryFiles.append(testFiles);
    testFiles = testBinaryFiles;

    std::string nClFiles = NEO_SHARED_TEST_FILES_DIR;
    nClFiles.append("/");
    clFiles = nClFiles;
}

std::string getBaseExecutionDir() {
    if (!isAubTestMode(testMode)) {
        return "shared/";
    }
    return "";
}

bool isChangeDirectoryRequired() {
    return true;
}

void addUltListener(::testing::TestEventListeners &listeners) {
    listeners.Append(new BaseUltConfigListener);
}

void populateApiSpecificVirtualFileList(const NEO::HardwareInfo &hwInfo) {
}
