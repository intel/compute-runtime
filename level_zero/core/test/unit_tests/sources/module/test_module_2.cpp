/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module_build_log.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
namespace L0 {
namespace ult {

class ModuleOnlineCompiled : public DeviceFixture, public testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::setUp();

        auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
        const auto &src = zebinData->storage;

        ze_module_desc_t modDesc = {};
        modDesc.format = ZE_MODULE_FORMAT_NATIVE;
        modDesc.inputSize = static_cast<uint32_t>(src.size());
        modDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());

        module.reset(whiteboxCast(Module::create(device, &modDesc, nullptr, ModuleType::User)));
        ASSERT_NE(nullptr, module);
    }

    void TearDown() override {
        module.reset(nullptr);
        DeviceFixture::tearDown();
    }
    std::unique_ptr<WhiteBox<L0::Module>> module;
};

using ModuleTests = Test<DeviceFixture>;

TEST_F(ModuleTests, WhenCreatingBuildOptionsThenOptionsParsedCorrectly) {
    auto module = new ModuleImp(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    std::string buildOptions;
    std::string internalBuildOptions;

    module->createBuildOptions("-ze-opt-disable -ze-opt-greater-than-4GB-buffer-required", buildOptions, internalBuildOptions);

    EXPECT_TRUE(NEO::CompilerOptions::contains(buildOptions, NEO::CompilerOptions::optDisable));
    EXPECT_TRUE(NEO::CompilerOptions::contains(internalBuildOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired));

    delete module;
}

TEST(ModuleBuildLog, WhenCreatingModuleBuildLogThenNonNullPointerReturned) {
    auto moduleBuildLog = ModuleBuildLog::create();
    ASSERT_NE(nullptr, moduleBuildLog);

    delete moduleBuildLog;
}

TEST(ModuleBuildLog, WhenDestroyingModuleBuildLogThenSuccessIsReturned) {
    auto moduleBuildLog = ModuleBuildLog::create();
    ASSERT_NE(nullptr, moduleBuildLog);

    auto result = moduleBuildLog->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(ModuleBuildLog, WhenGettingStringThenLogIsParsedCorrectly) {
    size_t buildLogSize = 0;
    char *buildLog = nullptr;
    const char *errorLog = "Error Log";
    const char *warnLog = "Warn Log";

    auto moduleBuildLog = ModuleBuildLog::create();
    ASSERT_NE(nullptr, moduleBuildLog);

    auto result = moduleBuildLog->getString(&buildLogSize, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, buildLogSize);

    buildLog = reinterpret_cast<char *>(malloc(buildLogSize));
    result = moduleBuildLog->getString(&buildLogSize, buildLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, buildLogSize);

    free(buildLog);

    moduleBuildLog->appendString(errorLog, strlen(errorLog));

    buildLogSize = 0;
    result = moduleBuildLog->getString(&buildLogSize, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ((strlen(errorLog) + 1), buildLogSize);

    buildLog = reinterpret_cast<char *>(malloc(buildLogSize));
    result = moduleBuildLog->getString(&buildLogSize, buildLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ((strlen(errorLog) + 1), buildLogSize);
    EXPECT_STREQ("Error Log", buildLog);

    free(buildLog);

    moduleBuildLog->appendString(warnLog, strlen(warnLog));

    buildLogSize = 0;
    result = moduleBuildLog->getString(&buildLogSize, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ((strlen(errorLog) + strlen("\n") + strlen(warnLog) + 1), buildLogSize);

    buildLog = reinterpret_cast<char *>(malloc(buildLogSize));
    result = moduleBuildLog->getString(&buildLogSize, buildLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ((strlen(errorLog) + strlen("\n") + strlen(warnLog) + 1), buildLogSize);
    EXPECT_STREQ("Error Log\nWarn Log", buildLog);

    free(buildLog);

    result = moduleBuildLog->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ModuleTests, WhenCreatingKernelThenSuccessIsReturned) {
    Mock<Module> module(device, nullptr);
    ze_kernel_desc_t desc = {};
    ze_kernel_handle_t kernel = {};

    auto result = zeKernelCreate(module.toHandle(), &desc, &kernel);
    EXPECT_EQ(1u, module.createKernelCalled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ModuleOnlineCompiled, WhenGettingNativeBinaryThenGenBinaryIsReturned) {
    size_t binarySize = 0;
    uint8_t *binary = nullptr;
    auto result = zeModuleGetNativeBinary(module->toHandle(), &binarySize, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, binarySize);

    auto storage = std::make_unique<uint8_t[]>(binarySize);
    binary = storage.get();
    result = zeModuleGetNativeBinary(module->toHandle(), &binarySize, binary);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0, memcmp(++binary, "ELF", 3));
}

TEST_F(ModuleOnlineCompiled, WhenCreatingFromNativeBinaryThenGenBinaryIsReturned) {
    size_t binarySize = 0;
    uint8_t *binary = nullptr;
    auto result = module->getNativeBinary(&binarySize, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, binarySize);

    auto storage = std::make_unique<uint8_t[]>(binarySize);
    binary = storage.get();
    result = module->getNativeBinary(&binarySize, binary);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0, memcmp(++binary, "ELF", 3));

    ze_module_desc_t modDesc = {};
    modDesc.format = ZE_MODULE_FORMAT_NATIVE;
    modDesc.inputSize = binarySize;
    modDesc.pInputModule = reinterpret_cast<const uint8_t *>(storage.get());

    L0::Module *moduleFromNativeBinary = Module::create(device, &modDesc, nullptr, ModuleType::User);
    EXPECT_NE(nullptr, moduleFromNativeBinary);

    delete moduleFromNativeBinary;
}

TEST_F(ModuleOnlineCompiled, GivenKernelThenThreadGroupParametersAreCorrect) {
    ze_kernel_desc_t kernelDesc = {};

    kernelDesc.pKernelName = "memcpy_bytes";

    ze_result_t res = ZE_RESULT_SUCCESS;
    auto kernel = std::unique_ptr<Kernel>(whiteboxCast(Kernel::create(
        neoDevice->getHardwareInfo().platform.eProductFamily,
        module.get(), &kernelDesc, &res)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, kernel);

    auto result = kernel->setGroupSize(5u, 3u, 13u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(kernel->getThreadExecutionMask(), 7u);
    EXPECT_EQ(kernel->getNumThreadsPerThreadGroup(), 7u);
}

TEST_F(ModuleOnlineCompiled, GivenKernelThenCorrectPropertiesAreReturned) {
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "memcpy_bytes";

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto kernel = std::unique_ptr<Kernel>(whiteboxCast(Kernel::create(
        neoDevice->getHardwareInfo().platform.eProductFamily,
        module.get(), &kernelDesc, &result)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, kernel);

    uint32_t groupSizeX = 5u;
    uint32_t groupSizeY = 3u;
    uint32_t groupSizeZ = 13u;
    result = kernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_kernel_properties_t properties = {};

    size_t kernelNameSize = 0;
    result = kernel->getKernelName(&kernelNameSize, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernelName = std::make_unique<char[]>(sizeof(char) * kernelNameSize);
    result = kernel->getKernelName(&kernelNameSize, kernelName.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    int compResult = memcmp(kernelDesc.pKernelName,
                            kernelName.get(), sizeof(*kernelDesc.pKernelName));
    EXPECT_EQ(0, compResult);

    result = kernel->getProperties(&properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0U, properties.requiredGroupSizeX);
    EXPECT_EQ(0U, properties.requiredGroupSizeY);
    EXPECT_EQ(0U, properties.requiredGroupSizeZ);

    EXPECT_EQ(2u, properties.numKernelArgs);
}

TEST_F(ModuleOnlineCompiled, GivenKernelThenCorrectAttributesAreReturned) {
    std::string testFile;
    retrieveBinaryKernelFilenameApiSpecific(testFile, "test_kernel_", ".bin");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;
    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(device, nullptr, ModuleType::User));
    module->initialize(&moduleDesc, device->getNEODevice());

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "memcpy_bytes_attr";

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto kernel = std::unique_ptr<Kernel>(whiteboxCast(Kernel::create(
        neoDevice->getHardwareInfo().platform.eProductFamily,
        module.get(), &kernelDesc, &result)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, kernel);

    ze_kernel_indirect_access_flags_t indirectFlags;
    result = kernel->getIndirectAccess(&indirectFlags);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, indirectFlags);

    const char attributeString[] = "work_group_size_hint(1,1,1)";
    uint32_t strSize = 0;

    result = kernel->getSourceAttributes(&strSize, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(strlen(attributeString) + 1, strSize);

    char *attributes = reinterpret_cast<char *>(malloc(sizeof(char) * strSize));
    result = kernel->getSourceAttributes(&strSize, &attributes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_STREQ(attributeString, attributes);

    free(attributes);
}

} // namespace ult
} // namespace L0
