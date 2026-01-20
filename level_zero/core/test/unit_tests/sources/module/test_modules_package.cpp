/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module_build_log.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/source/module/modules_package_binary.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include <string>
#include <string_view>

using namespace std::literals;

namespace L0 {
namespace ult {

namespace {
template <typename ModuleUnitT = MockModule>
struct MockModulesPackage : L0::ult::ModulesPackage {
    MockModulesPackage(L0::Device *device, ModuleBuildLog *moduleBuildLog, ModuleType type) : L0::ult::ModulesPackage(device, moduleBuildLog, type) {
        moduleUnitFactory = [](L0::Device *device, ModuleBuildLog *buildLog, ModuleType type) {
            return std::make_unique<ModuleUnitT>(device, buildLog, type);
        };
    }

    MockModulesPackage(L0::Device *device) : MockModulesPackage(device, nullptr, ModuleType::user) {
    }

    std::unique_ptr<Module> createModuleUnit(L0::Device *device, ModuleBuildLog *buildLog, ModuleType type) override {
        return moduleUnitFactory(device, buildLog, type);
    }

    std::function<std::unique_ptr<Module>(L0::Device *device, ModuleBuildLog *buildLog, ModuleType type)> moduleUnitFactory;
};
} // namespace

TEST(ModulesPackageInit, WhenDestroyIsCalledThenDeletesThePackageAndReturnsSuccess) {
    MockDevice device;
    ModulesPackage *mp = new ModulesPackage(&device);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp->destroy());
}

TEST(ModulesPackageInit, WhenSpecConstantsArePresentThenDetectAsIncompatibleAndFailToInitialize) {
    uint64_t specConstantsData = {};
    const void *specConstantsDataArray[1] = {&specConstantsData};
    uint32_t specConstantsIds[1] = {0};
    ze_module_constants_t specConstants = {};
    specConstants.numConstants = 1;
    specConstants.pConstantIds = specConstantsIds;
    specConstants.pConstantValues = specConstantsDataArray;
    MockDevice device;
    ModulesPackage mp{&device};
    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;
    desc.pConstants = &specConstants;

    ze_module_program_exp_desc_t moduleProgramDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &moduleProgramDesc;

    auto res = mp.initialize(&desc, device.getNEODevice());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);

    desc.pConstants = nullptr;
    const ze_module_constants_t *modulesPackageConstants[1] = {&specConstants};
    moduleProgramDesc.pConstants = modulesPackageConstants;

    res = mp.initialize(&desc, device.getNEODevice());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

TEST(ModulesPackageInit, GivenEmptyInputThenCreatesEmptyModulesPackage) {
    MockDevice device;
    ModulesPackage mp{&device};
    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    ze_module_program_exp_desc_t moduleProgramDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &moduleProgramDesc;

    auto res = mp.initialize(&desc, device.getNEODevice());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST(IsModulesPackageInput, WhenFormatNotNativeThenReturnsFalse) {
    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    EXPECT_FALSE(ModulesPackage::isModulesPackageInput(&desc));
}

TEST(IsModulesPackageInput, WhenFormatIsNativeAndModuleProgramDescriptorPresentThenReturnsTrue) {
    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    ze_module_program_exp_desc_t modProgDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &modProgDesc;

    EXPECT_TRUE(ModulesPackage::isModulesPackageInput(&desc));
}

TEST(IsModulesPackageInput, WhenFormatIsNativeButModuleProgramDescriptorIsNotPresentThenChecksForNativeBinaryType) {
    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    uint8_t notPackage[64] = {2, 3, 5, 7};
    desc.pInputModule = notPackage;
    desc.inputSize = sizeof(notPackage);
    EXPECT_FALSE(ModulesPackage::isModulesPackageInput(&desc));

    ModulesPackageBinary package;
    auto packageBinary = package.encode();
    desc.pInputModule = packageBinary.data();
    desc.inputSize = packageBinary.size();
    EXPECT_TRUE(ModulesPackage::isModulesPackageInput(&desc));
}

TEST(ModuleCreateModulesPackage, GivenNativeBinariesInModuleProgramDescThenCreatesAModulesPackage) {
    MockDevice device;
    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    ze_module_program_exp_desc_t moduleProgramDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &moduleProgramDesc;

    ze_result_t res = {};
    auto module = L0::Module::create(&device, &desc, nullptr, L0::ModuleType::user, &res);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(module->isModulesPackage());
    module->destroy();
}

using ModulesPackageInitWithDevice = Test<L0::ult::DeviceFixture>;

TEST_F(ModulesPackageInitWithDevice, GivenAListOfBinaryModulesThenCreatesAPackageModuleThanEncompasesAllInputModules) {
    ZebinTestData::ValidEmptyProgram<> emptyZebin;

    MockModulesPackage<> mp{this->device};
    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    ze_module_program_exp_desc_t moduleProgramDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &moduleProgramDesc;

    moduleProgramDesc.count = 2;
    size_t inputSizes[2] = {emptyZebin.storage.size(), emptyZebin.storage.size()};
    moduleProgramDesc.inputSizes = inputSizes;
    const uint8_t *inputModules[2] = {emptyZebin.storage.data(), emptyZebin.storage.data()};
    moduleProgramDesc.pInputModules = inputModules;

    auto res = mp.initialize(&desc, device->getNEODevice());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(2U, mp.modules.size());
}

TEST_F(ModulesPackageInitWithDevice, GivenBaseModuleAndAListOfBinaryModulesThenCreatesAPackageModuleThanEncompasesAllInputModules) {
    ZebinTestData::ValidEmptyProgram<> emptyZebin;

    MockModulesPackage<> mp{this->device};
    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    ze_module_program_exp_desc_t moduleProgramDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &moduleProgramDesc;

    moduleProgramDesc.count = 2;
    size_t inputSizes[2] = {emptyZebin.storage.size(), emptyZebin.storage.size()};
    moduleProgramDesc.inputSizes = inputSizes;
    const uint8_t *inputModules[2] = {emptyZebin.storage.data(), emptyZebin.storage.data()};
    moduleProgramDesc.pInputModules = inputModules;

    desc.inputSize = emptyZebin.storage.size();
    desc.pInputModule = emptyZebin.storage.data();

    auto res = mp.initialize(&desc, device->getNEODevice());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(3U, mp.modules.size());
}

TEST_F(ModulesPackageInitWithDevice, GivenBuildLogThenAccumulatesLogsFromAllModulesAndLinkingStage) {
    ZebinTestData::ValidEmptyProgram<> emptyZebin;
    std::unique_ptr<ModuleBuildLog> buildLog{ModuleBuildLog::create()};

    struct LoggingModule : MockModule {
        using MockModule::MockModule;
        ze_result_t initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice) override {
            this->moduleBuildLog->appendString(dataToLog.c_str(), dataToLog.size());
            return ZE_RESULT_SUCCESS;
        }

        ze_result_t performDynamicLink(uint32_t numModules,
                                       ze_module_handle_t *phModules,
                                       ze_module_build_log_handle_t *phLinkLog) override {
            this->moduleBuildLog->appendString(dataToLog.c_str(), dataToLog.size());
            return ZE_RESULT_SUCCESS;
        }

        std::string dataToLog;
    };

    MockModulesPackage<LoggingModule> mp{this->device};
    int moduleId = 0;
    mp.moduleUnitFactory = [&](L0::Device *device, ModuleBuildLog *buildLog, ModuleType type) {
        auto ret = std::make_unique<LoggingModule>(device, buildLog, type);
        ret->dataToLog = std::to_string(++moduleId);
        return ret;
    };

    mp.packageBuildLog = buildLog.get();
    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    ze_module_program_exp_desc_t moduleProgramDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &moduleProgramDesc;

    moduleProgramDesc.count = 2;
    size_t inputSizes[2] = {emptyZebin.storage.size(), emptyZebin.storage.size()};
    moduleProgramDesc.inputSizes = inputSizes;
    const uint8_t *inputModules[2] = {emptyZebin.storage.data(), emptyZebin.storage.data()};
    moduleProgramDesc.pInputModules = inputModules;

    desc.inputSize = emptyZebin.storage.size();
    desc.pInputModule = emptyZebin.storage.data();

    auto res = mp.initialize(&desc, device->getNEODevice());
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_STREQ("1\n2\n3\n1", read(*buildLog).c_str()) << read(*buildLog);
}

TEST_F(ModulesPackageInitWithDevice, GivenAListOfBinaryModulesWhenOneOfTheUnitsFailsToInitializeThenPackageCreationFails) {
    ZebinTestData::ValidEmptyProgram<> emptyZebin;

    struct FailingModule : MockModule {
        using MockModule::MockModule;
        ze_result_t initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice) override {
            return fail ? ZE_RESULT_ERROR_INVALID_NATIVE_BINARY : ZE_RESULT_SUCCESS;
        }

        bool fail = false;
    };

    MockModulesPackage<FailingModule> mp{this->device};
    int moduleId = 0;
    mp.moduleUnitFactory = [&](L0::Device *device, ModuleBuildLog *buildLog, ModuleType type) {
        auto ret = std::make_unique<FailingModule>(device, buildLog, type);
        ret->fail = moduleId == 1;
        ++moduleId;
        return ret;
    };

    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    ze_module_program_exp_desc_t moduleProgramDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &moduleProgramDesc;

    moduleProgramDesc.count = 2;
    size_t inputSizes[2] = {emptyZebin.storage.size(), emptyZebin.storage.size()};
    moduleProgramDesc.inputSizes = inputSizes;
    const uint8_t *inputModules[2] = {emptyZebin.storage.data(), emptyZebin.storage.data()};
    moduleProgramDesc.pInputModules = inputModules;

    auto res = mp.initialize(&desc, device->getNEODevice());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NATIVE_BINARY, res);
}

namespace {
struct BuildOptionsLoggingModule : MockModule {
    using MockModule::MockModule;
    ze_result_t initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice) override {
        *log += desc->pBuildFlags;
        return ZE_RESULT_SUCCESS;
    }

    std::string *log = nullptr;
};
} // namespace

TEST_F(ModulesPackageInitWithDevice, GivenBuildOptionsThenPassThemToModuleUnits) {
    ZebinTestData::ValidEmptyProgram<> emptyZebin;

    std::string log;
    MockModulesPackage<BuildOptionsLoggingModule> mp{this->device};
    mp.moduleUnitFactory = [&](L0::Device *device, ModuleBuildLog *buildLog, ModuleType type) {
        auto ret = std::make_unique<BuildOptionsLoggingModule>(device, buildLog, type);
        ret->log = &log;
        return ret;
    };

    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    ze_module_program_exp_desc_t moduleProgramDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &moduleProgramDesc;

    moduleProgramDesc.count = 2;
    size_t inputSizes[2] = {emptyZebin.storage.size(), emptyZebin.storage.size()};
    moduleProgramDesc.inputSizes = inputSizes;
    const uint8_t *inputModules[2] = {emptyZebin.storage.data(), emptyZebin.storage.data()};
    moduleProgramDesc.pInputModules = inputModules;

    desc.inputSize = emptyZebin.storage.size();
    desc.pInputModule = emptyZebin.storage.data();

    desc.pBuildFlags = "0";
    const char *unitsBuildFlags[] = {"1", nullptr};
    moduleProgramDesc.pBuildFlags = unitsBuildFlags;

    auto res = mp.initialize(&desc, device->getNEODevice());
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_STREQ("010", log.c_str()) << log;
}

TEST_F(ModulesPackageInitWithDevice, GivenNoBuildOptionsForSpecificUnitsThenUseParentDescriptorsBuildOptions) {
    ZebinTestData::ValidEmptyProgram<> emptyZebin;

    std::string log;
    MockModulesPackage<BuildOptionsLoggingModule> mp{this->device};
    mp.moduleUnitFactory = [&](L0::Device *device, ModuleBuildLog *buildLog, ModuleType type) {
        auto ret = std::make_unique<BuildOptionsLoggingModule>(device, buildLog, type);
        ret->log = &log;
        return ret;
    };

    ze_module_desc_t desc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    desc.format = ZE_MODULE_FORMAT_NATIVE;

    ze_module_program_exp_desc_t moduleProgramDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    desc.pNext = &moduleProgramDesc;

    moduleProgramDesc.count = 2;
    size_t inputSizes[2] = {emptyZebin.storage.size(), emptyZebin.storage.size()};
    moduleProgramDesc.inputSizes = inputSizes;
    const uint8_t *inputModules[2] = {emptyZebin.storage.data(), emptyZebin.storage.data()};
    moduleProgramDesc.pInputModules = inputModules;

    desc.inputSize = emptyZebin.storage.size();
    desc.pInputModule = emptyZebin.storage.data();

    desc.pBuildFlags = "0";

    auto res = mp.initialize(&desc, device->getNEODevice());
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_STREQ("000", log.c_str()) << log;
}

using ModulesPackageForwarding = Test<L0::ult::DeviceFixture>;
TEST_F(ModulesPackageForwarding, WhenCreateKernelIsCalledThenForwardToAnyModuleThatCanSupportIt) {
    MockModulesPackage<> mp{this->device};

    ze_kernel_desc_t kernelDesc = {.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC, .pKernelName = "test"};
    ze_kernel_handle_t kernelHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, mp.createKernel(&kernelDesc, &kernelHandle));
    EXPECT_EQ(nullptr, kernelHandle);

    struct ModuleWithKernels : MockModule {
        using MockModule::MockModule;
        ze_result_t createKernel(const ze_kernel_desc_t *desc,
                                 ze_kernel_handle_t *kernelHandle) override {
            this->called = true;
            *kernelHandle = this->kern;
            return kern ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_INVALID_KERNEL_NAME;
        }

        ze_kernel_handle_t kern = nullptr;
        bool called = false;
    };

    mp.modules.push_back(std::make_unique<ModuleWithKernels>(this->device, nullptr, L0::ModuleType::user));
    mp.modules.push_back(std::make_unique<ModuleWithKernels>(this->device, nullptr, L0::ModuleType::user));
    mp.modules.push_back(std::make_unique<ModuleWithKernels>(this->device, nullptr, L0::ModuleType::user));

    L0::ult::Mock<::L0::KernelImp> kernToReturn;
    static_cast<ModuleWithKernels *>(mp.modules[1].get())->kern = &kernToReturn;

    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.createKernel(&kernelDesc, &kernelHandle));
    EXPECT_EQ(&kernToReturn, kernelHandle);
    EXPECT_TRUE(static_cast<ModuleWithKernels *>(mp.modules[0].get())->called);
    EXPECT_TRUE(static_cast<ModuleWithKernels *>(mp.modules[1].get())->called);
    EXPECT_FALSE(static_cast<ModuleWithKernels *>(mp.modules[2].get())->called);

    static_cast<ModuleWithKernels *>(mp.modules[1].get())->kern = nullptr;
    kernelHandle = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_KERNEL_NAME, mp.createKernel(&kernelDesc, &kernelHandle));
    EXPECT_EQ(nullptr, kernelHandle);
    EXPECT_TRUE(static_cast<ModuleWithKernels *>(mp.modules[0].get())->called);
    EXPECT_TRUE(static_cast<ModuleWithKernels *>(mp.modules[1].get())->called);
    EXPECT_TRUE(static_cast<ModuleWithKernels *>(mp.modules[2].get())->called);
}

TEST_F(ModulesPackageForwarding, WhenGetFunctionPointerIsCalledThenForwardToAnyModuleThatCanSupportIt) {
    MockModulesPackage<> mp{this->device};

    struct ModuleWithFunctionPointer : MockModule {
        using MockModule::MockModule;
        ze_result_t getFunctionPointer(const char *, void **pfnFunction) override {
            *pfnFunction = addressToReturn;
            return ZE_RESULT_SUCCESS;
        }

        void *addressToReturn = nullptr;
    };

    int var = 5;
    mp.modules.push_back(std::make_unique<ModuleWithFunctionPointer>(this->device, nullptr, L0::ModuleType::user));
    static_cast<ModuleWithFunctionPointer *>(mp.modules[0].get())->addressToReturn = &var;

    void *global = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getFunctionPointer("myGlobal", &global));
    EXPECT_EQ(&var, global);
}

TEST_F(ModulesPackageForwarding, WhenGetGlobalPointerIsCalledThenForwardToAnyModuleThatCanSupportIt) {
    MockModulesPackage<> mp{this->device};

    struct ModuleWithGlobalPointer : MockModule {
        using MockModule::MockModule;
        ze_result_t getGlobalPointer(const char *, size_t *pSize, void **pPtr) override {
            *pSize = size;
            *pPtr = addressToReturn;
            return ZE_RESULT_SUCCESS;
        }

        size_t size = 8;
        void *addressToReturn = nullptr;
    };

    int var = 5;
    mp.modules.push_back(std::make_unique<ModuleWithGlobalPointer>(this->device, nullptr, L0::ModuleType::user));
    static_cast<ModuleWithGlobalPointer *>(mp.modules[0].get())->addressToReturn = &var;

    void *global = nullptr;
    size_t size = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getGlobalPointer("myGlobal", &size, &global));
    EXPECT_EQ(8U, size);
    EXPECT_EQ(&var, global);
}

TEST_F(ModulesPackageForwarding, WhenGetKernelImmutableDataIsCalledThenForwardToAnyModuleThatCanSupportIt) {
    MockModulesPackage<> mp{this->device};

    EXPECT_EQ(nullptr, mp.getKernelImmutableData("myKern"));

    struct ModuleWithKernelImmutableData : MockModule {
        using MockModule::MockModule;
        const KernelImmutableData *getKernelImmutableData(const char *kernelName) const override {
            return kernImmDataToReturn;
        }

        const L0::KernelImmutableData *kernImmDataToReturn = nullptr;
    };

    L0::KernelImmutableData kernImmDataMock;
    mp.modules.push_back(std::make_unique<ModuleWithKernelImmutableData>(this->device, nullptr, L0::ModuleType::user));
    static_cast<ModuleWithKernelImmutableData *>(mp.modules[0].get())->kernImmDataToReturn = &kernImmDataMock;

    EXPECT_EQ(&kernImmDataMock, mp.getKernelImmutableData("myKern"));
}

TEST(ModulesPackageUnrechableInternalApis, WhenUnreachableApiIsCalledThenRaiseUnreachableError) {
    MockDevice device;
    ModulesPackage mp{&device};

    NEO::KernelDescriptor kernelDescriptorParam = {};
    EXPECT_ANY_THROW(mp.getKernelImmutableDataVector());
    EXPECT_ANY_THROW(mp.getMaxGroupSize(kernelDescriptorParam));
    EXPECT_ANY_THROW(mp.shouldAllocatePrivateMemoryPerDispatch());
    EXPECT_ANY_THROW(mp.getProfileFlags());
    EXPECT_ANY_THROW(mp.checkIfPrivateMemoryPerDispatchIsNeeded());
    EXPECT_ANY_THROW(mp.populateZebinExtendedArgsMetadata());
    EXPECT_ANY_THROW(mp.generateDefaultExtendedArgsMetadata());
}

TEST_F(ModulesPackageForwarding, WhenGetKernelsNamesIsCalledThenAccumulatesFromEachModule) {
    MockModulesPackage<> mp{this->device};

    uint32_t count = 0;
    const char *names[16] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getKernelNames(&count, nullptr));
    EXPECT_EQ(0U, count);

    count = 16;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getKernelNames(&count, nullptr));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(nullptr, names[0]);

    struct ModuleWithKernelNames : MockModule {
        using MockModule::MockModule;
        ze_result_t getKernelNames(uint32_t *pCount, const char **pNames) override {
            if (*pCount == 0) {
                *pCount = static_cast<uint32_t>(kernelNames.size());
            } else {
                *pCount = std::min(*pCount, static_cast<uint32_t>(kernelNames.size()));
                for (uint32_t i = 0; i < *pCount; ++i) {
                    pNames[i] = kernelNames[i].c_str();
                }
            }
            return ZE_RESULT_SUCCESS;
        }

        std::vector<std::string> kernelNames;
    };

    std::vector<std::string> kernelNames = {"1", "2", "3", "4", "5", "6", "7", "8"};

    std::unique_ptr<ModuleWithKernelNames> moduleUnits[3] = {};
    moduleUnits[0] = std::make_unique<ModuleWithKernelNames>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[1] = std::make_unique<ModuleWithKernelNames>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[2] = std::make_unique<ModuleWithKernelNames>(this->device, nullptr, L0::ModuleType::user);

    moduleUnits[0]->kernelNames.push_back(kernelNames[0]);
    moduleUnits[0]->kernelNames.push_back(kernelNames[1]);
    moduleUnits[0]->kernelNames.push_back(kernelNames[2]);
    moduleUnits[1]->kernelNames.push_back(kernelNames[3]);

    for (auto &m : moduleUnits) {
        mp.modules.push_back(std::move(m));
    }

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getKernelNames(&count, nullptr));
    EXPECT_EQ(4U, count);

    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getKernelNames(&count, names));
    EXPECT_EQ(4U, count);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_STREQ(kernelNames[i].c_str(), names[i]) << i << kernelNames[i] << " != " << names[i];
        names[i] = nullptr;
    }
    EXPECT_EQ(nullptr, names[count]);

    count = 16;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getKernelNames(&count, names));
    EXPECT_EQ(4U, count);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_STREQ(kernelNames[i].c_str(), names[i]) << i << kernelNames[i] << " != " << names[i];
        names[i] = nullptr;
    }
    EXPECT_EQ(nullptr, names[count]);

    count = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getKernelNames(&count, names));
    EXPECT_EQ(2U, count);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_STREQ(kernelNames[i].c_str(), names[i]) << i << kernelNames[i] << " != " << names[i];
        names[i] = nullptr;
    }
    EXPECT_EQ(nullptr, names[count]);
}

TEST_F(ModulesPackageForwarding, WhenGetPropertiesIsCalledThenAccumulatesFromEachModule) {
    MockModulesPackage<> mp{this->device};

    struct ModuleWithProperties : MockModule {
        using MockModule::MockModule;
        ze_result_t getProperties(ze_module_properties_t *pModuleProperties) override {
            pModuleProperties->flags = modulePropertiesToReturn.flags;
            return ZE_RESULT_SUCCESS;
        }

        ze_module_properties_t modulePropertiesToReturn = {};
    };

    std::unique_ptr<ModuleWithProperties> moduleUnits[3] = {};
    moduleUnits[0] = std::make_unique<ModuleWithProperties>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[1] = std::make_unique<ModuleWithProperties>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[2] = std::make_unique<ModuleWithProperties>(this->device, nullptr, L0::ModuleType::user);

    moduleUnits[0]->modulePropertiesToReturn.flags = 0;
    moduleUnits[1]->modulePropertiesToReturn.flags = ZE_MODULE_PROPERTY_FLAG_IMPORTS;
    moduleUnits[2]->modulePropertiesToReturn.flags = 0;

    ze_module_properties_t moduleProperties = {ZE_STRUCTURE_TYPE_MODULE_PROPERTIES};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getProperties(&moduleProperties));
    EXPECT_EQ(0U, moduleProperties.flags);

    moduleProperties = ze_module_properties_t{ZE_STRUCTURE_TYPE_MODULE_PROPERTIES};
    mp.modules.push_back(std::move(moduleUnits[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getProperties(&moduleProperties));
    EXPECT_EQ(0U, moduleProperties.flags);

    moduleProperties = ze_module_properties_t{ZE_STRUCTURE_TYPE_MODULE_PROPERTIES};
    mp.modules.push_back(std::move(moduleUnits[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getProperties(&moduleProperties));
    EXPECT_EQ(1U, moduleProperties.flags);

    moduleProperties = ze_module_properties_t{ZE_STRUCTURE_TYPE_MODULE_PROPERTIES};
    mp.modules.push_back(std::move(moduleUnits[2]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getProperties(&moduleProperties));
    EXPECT_EQ(1U, moduleProperties.flags);
}

TEST(ModulesPackageManifestLoad, GivenInvalidManifestThenFailsToLoad) {
    std::pair<std::string_view, std::string_view> invalidManifests[] = {
        {R"YML(
- : - ;
)YML"sv,
         "NEO::Yaml : Could not parse line : [1] : [- : - ;] <-- parser position on error. Reason : Invalid numeric literal\n"sv},
        {R"YML(
version:
    major : a
    minor : 0
)YML"sv,
         "NEO::ModulesPackage : Failed to read major version in manifest\n"sv},
        {R"YML(
version:
    major : 1
    minor : a
)YML"sv,
         "NEO::ModulesPackage : Failed to read minor version in manifest\n"sv},
        {R"YML(
version:
    major : 1
    minor : 0
units:
    - filename : ""
)YML"sv,
         "NEO::ModulesPackage : Failed to read unit's filename in manifest\n"sv},
        {R"YML(
units:
    - filename : "abc"
)YML"sv,
         "NEO::ModulesPackage : Missing version information\n"sv}};

    for (auto &[manifest, expectedError] : invalidManifests) {
        std::string error;
        std::string warning;
        auto loadedManifest = L0::ModulesPackageManifest::load(manifest, error, warning);
        EXPECT_FALSE(loadedManifest.has_value());
        EXPECT_TRUE(warning.empty()) << warning;
        if (error.empty()) {
            EXPECT_FALSE(error.empty()) << manifest;
        } else {
            EXPECT_STREQ(expectedError.data(), error.c_str());
        }
    }
}

TEST(ModulesPackageManifestLoad, GivenUnhandledManifestMajorVersionThenFailsToLoad) {
    std::string manifest = R"YML(
version:
    major : )YML" + std::to_string(L0::ModulesPackageManifest::compiledVersion.major + 1) +
                           R"YML(
    minor : 0
)YML";

    std::string error;
    std::string warning;
    auto loadedManifest = L0::ModulesPackageManifest::load(manifest, error, warning);
    EXPECT_FALSE(loadedManifest.has_value());
    EXPECT_TRUE(warning.empty()) << warning;
    ASSERT_FALSE(error.empty()) << manifest;
    EXPECT_STREQ("NEO::ModulesPackage : Unhandled package manifest major version\n", error.c_str());
}

TEST(ModulesPackageManifestLoad, GivenValidManifestThenLoadsItCorrectly) {
    std::string manifest = R"YML(
version:
    major : )YML" + std::to_string(L0::ModulesPackageManifest::compiledVersion.major) +
                           R"YML(
    minor : 13
units:
    - filename : unit.0
    - filename : unit.13
)YML";

    std::string error;
    std::string warning;
    auto loadedManifest = L0::ModulesPackageManifest::load(manifest, error, warning);
    ASSERT_TRUE(loadedManifest.has_value());
    EXPECT_TRUE(warning.empty()) << warning;
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_EQ(L0::ModulesPackageManifest::compiledVersion.major, loadedManifest->version.major);
    EXPECT_EQ(13, loadedManifest->version.minor);
    ASSERT_EQ(2U, loadedManifest->units.size());
    EXPECT_STREQ("unit.0", loadedManifest->units[0].fileName.c_str());
    EXPECT_STREQ("unit.13", loadedManifest->units[1].fileName.c_str());
}

TEST(ModulesPackageManifestDump, GivenNoUnitsThenSkipWholeSection) {
    L0::ModulesPackageManifest manifest;
    manifest.version.major = 7;
    manifest.version.major = 3;

    auto manifestStr = manifest.dump();
    EXPECT_STREQ("version : \n  major : 3\n  minor : 0\n", manifestStr.c_str());
}

TEST(ModulesPackageBinaryIsModulesPackageBinary, GivenNonArFileFormatThenReturnFalse) {
    std::string data = "deffinitely not ar";
    auto isModulesPackage = ModulesPackageBinary::isModulesPackageBinary(std::span(reinterpret_cast<const uint8_t *>(data.data()), data.size()));
    EXPECT_FALSE(isModulesPackage);
}

TEST(ModulesPackageBinaryIsModulesPackageBinary, GivenArThenChecksForPackageManifestFile) {
    NEO::Ar::ArEncoder arEncoder;
    auto ar = arEncoder.encode();
    auto isModulesPackage = ModulesPackageBinary::isModulesPackageBinary(std::span(ar));
    EXPECT_FALSE(isModulesPackage);

    L0::ModulesPackageManifest manifest;
    auto manifestStr = manifest.dump();
    arEncoder.appendFileEntry(ConstStringRef(L0::ModulesPackageManifest::packageManifestFileName), ArrayRef<const uint8_t>::fromAny(manifestStr.c_str(), manifestStr.size()));
    ar = arEncoder.encode();
    isModulesPackage = ModulesPackageBinary::isModulesPackageBinary(std::span(ar));
    EXPECT_TRUE(isModulesPackage);
}

TEST(ModulesPackageBinaryDecode, GivenArWithMissingManifestFileThenDecodeFails) {
    NEO::Ar::ArEncoder arEncoder;
    auto ar = arEncoder.encode();
    std::string error;
    std::string warning;
    auto package = ModulesPackageBinary::decode(ar, error, warning);
    EXPECT_FALSE(package.has_value());
    EXPECT_TRUE(warning.empty()) << warning;
    EXPECT_STREQ(error.c_str(), "NEO::ModulesPackage : Could not find manifest file modules.pkg.yml in packge\n");
}

TEST(ModulesPackageBinaryDecode, GivenArWithBrokenManifestFileThenDecodeFails) {
    NEO::Ar::ArEncoder arEncoder;
    uint8_t brokenManifest[] = "AA:BB\n";
    arEncoder.appendFileEntry(ConstStringRef(L0::ModulesPackageManifest::packageManifestFileName), ArrayRef<const uint8_t>{brokenManifest, sizeof(brokenManifest)});
    auto ar = arEncoder.encode();
    std::string error;
    std::string warning;
    auto package = ModulesPackageBinary::decode(ar, error, warning);
    EXPECT_FALSE(package.has_value());
    EXPECT_TRUE(warning.empty()) << warning;
    EXPECT_STREQ(error.c_str(), "NEO::ModulesPackage : Missing version information\nNEO::ModulesPackage : Could not load manifest file\n");
}

TEST(ModulesPackageBinaryDecode, WhenManifestContainsAMissingFileThenDecodeFails) {
    NEO::Ar::ArEncoder arEncoder;
    L0::ModulesPackageManifest manifest;
    manifest.units.push_back({"doesnt_exist.0"});
    auto manifestStr = manifest.dump();
    arEncoder.appendFileEntry(ConstStringRef(L0::ModulesPackageManifest::packageManifestFileName), ArrayRef<const uint8_t>{reinterpret_cast<const uint8_t *>(manifestStr.data()), manifestStr.size()});
    auto ar = arEncoder.encode();
    std::string error;
    std::string warning;
    auto package = ModulesPackageBinary::decode(ar, error, warning);
    EXPECT_FALSE(package.has_value());
    EXPECT_TRUE(warning.empty()) << warning;
    EXPECT_STREQ(error.c_str(), "NEO::ModulesPackage : Missing file doesnt_exist.0 in provided package\n");
}

TEST(ModulesPackageBinaryDecode, GivenValidBinaryThenDecodeSucceeds) {
    NEO::Ar::ArEncoder arEncoder;
    L0::ModulesPackageManifest manifest;
    manifest.units.push_back({"unit.0"});
    auto manifestStr = manifest.dump();
    uint8_t data[32] = {2, 3, 5, 7};
    arEncoder.appendFileEntry(ConstStringRef(L0::ModulesPackageManifest::packageManifestFileName), ArrayRef<const uint8_t>{reinterpret_cast<const uint8_t *>(manifestStr.data()), manifestStr.size()});
    arEncoder.appendFileEntry(ConstStringRef("unit.0"), ArrayRef<const uint8_t>{data, sizeof(data)});
    auto ar = arEncoder.encode();
    std::string error;
    std::string warning;
    auto package = ModulesPackageBinary::decode(ar, error, warning);
    EXPECT_TRUE(warning.empty()) << warning;
    EXPECT_TRUE(error.empty()) << error;
    ASSERT_TRUE(package.has_value());

    ASSERT_EQ(1U, package->units.size());
    ASSERT_EQ(sizeof(data), package->units[0].data.size());
    ASSERT_EQ(data[0], package->units[0].data[0]);
    ASSERT_EQ(data[1], package->units[0].data[1]);
    ASSERT_EQ(data[2], package->units[0].data[2]);
    ASSERT_EQ(data[3], package->units[0].data[3]);
}

TEST(ModulesPackageBinaryEncode, GivenValidBinaryThenEncodesItProperly) {
    L0::ModulesPackageBinary binary;
    uint8_t unit0[4] = {2, 3, 5, 7};
    uint8_t unit1[4] = {11, 13, 17, 19};
    binary.units.push_back(L0::ModulesPackageBinary::Unit(std::span(unit0)));
    binary.units.push_back(L0::ModulesPackageBinary::Unit(std::span(unit1)));
    auto encodedPackage = binary.encode();

    std::string error;
    std::string warning;
    auto decodedPackage = ModulesPackageBinary::decode(encodedPackage, error, warning);
    EXPECT_TRUE(warning.empty()) << warning;
    EXPECT_TRUE(error.empty()) << error;
    ASSERT_TRUE(decodedPackage.has_value());

    ASSERT_EQ(2U, decodedPackage->units.size());

    ASSERT_EQ(unit0[0], decodedPackage->units[0].data[0]);
    ASSERT_EQ(unit0[1], decodedPackage->units[0].data[1]);
    ASSERT_EQ(unit0[2], decodedPackage->units[0].data[2]);
    ASSERT_EQ(unit0[3], decodedPackage->units[0].data[3]);

    ASSERT_EQ(unit1[0], decodedPackage->units[1].data[0]);
    ASSERT_EQ(unit1[1], decodedPackage->units[1].data[1]);
    ASSERT_EQ(unit1[2], decodedPackage->units[1].data[2]);
    ASSERT_EQ(unit1[3], decodedPackage->units[1].data[3]);
}

using ModulesPackagePrepareNativeBinary = Test<L0::ult::DeviceFixture>;

TEST_F(ModulesPackagePrepareNativeBinary, WhenNativeBinaryIsAlreadyPreparedThenUsesIt) {
    MockModulesPackage<> mp{this->device};
    mp.setNativeBinary(std::vector<uint8_t>{2, 3, 5, 7});
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.prepareNativeBinary());
    ASSERT_EQ(4U, mp.nativeBinary.size());
    EXPECT_EQ(2U, mp.nativeBinary[0]);
    EXPECT_EQ(3U, mp.nativeBinary[1]);
    EXPECT_EQ(5U, mp.nativeBinary[2]);
    EXPECT_EQ(7U, mp.nativeBinary[3]);
}

namespace {
struct ModuleWithNativeBinary : MockModule {
    using MockModule::MockModule;
    ze_result_t getNativeBinary(size_t *pSize, uint8_t *pModuleNativeBinary) override {
        if (0 == sizeToReturn) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }
        if (nullptr == pModuleNativeBinary) {
            *pSize = sizeToReturn;
            return ZE_RESULT_SUCCESS;
        }
        if (*pSize == 0) {
            *pSize = sizeToReturn;
            return ZE_RESULT_SUCCESS;
        }

        if (nativeBinaryToReturn.size() != sizeToReturn) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }

        memcpy_s(pModuleNativeBinary, *pSize, nativeBinaryToReturn.data(), nativeBinaryToReturn.size());

        return ZE_RESULT_SUCCESS;
    }

    std::vector<uint8_t> nativeBinaryToReturn;
    size_t sizeToReturn = 0;
};
} // namespace

TEST_F(ModulesPackagePrepareNativeBinary, WhenFailedToGetAnyUnitsBinarySizeThenFailsToPrepareBinary) {
    MockModulesPackage<> mp{this->device};
    std::unique_ptr<ModuleWithNativeBinary> moduleUnits[3] = {};
    moduleUnits[0] = std::make_unique<ModuleWithNativeBinary>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[1] = std::make_unique<ModuleWithNativeBinary>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[2] = std::make_unique<ModuleWithNativeBinary>(this->device, nullptr, L0::ModuleType::user);

    moduleUnits[0]->sizeToReturn = 8;
    moduleUnits[1]->sizeToReturn = 0;
    moduleUnits[2]->sizeToReturn = 16;

    for (auto &m : moduleUnits) {
        mp.modules.push_back(std::move(m));
    }

    EXPECT_NE(ZE_RESULT_SUCCESS, mp.prepareNativeBinary());
}

TEST_F(ModulesPackagePrepareNativeBinary, WhenFailedToGetAnyUnitsBinaryThenFailsToPrepareBinary) {
    MockModulesPackage<> mp{this->device};
    std::unique_ptr<ModuleWithNativeBinary> moduleUnits[3] = {};
    moduleUnits[0] = std::make_unique<ModuleWithNativeBinary>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[1] = std::make_unique<ModuleWithNativeBinary>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[2] = std::make_unique<ModuleWithNativeBinary>(this->device, nullptr, L0::ModuleType::user);

    std::vector<uint8_t> unit0(8, 0);
    std::vector<uint8_t> unit2(16, 0);

    moduleUnits[0]->sizeToReturn = unit0.size();
    moduleUnits[0]->nativeBinaryToReturn = unit0;
    moduleUnits[1]->sizeToReturn = 24;
    moduleUnits[2]->sizeToReturn = unit2.size();
    moduleUnits[2]->nativeBinaryToReturn = unit2;

    for (auto &m : moduleUnits) {
        mp.modules.push_back(std::move(m));
    }

    EXPECT_NE(ZE_RESULT_SUCCESS, mp.prepareNativeBinary());
}

TEST_F(ModulesPackagePrepareNativeBinary, WhenAllUnitsReturnValidBinariesThenCreatesModulesPackageBinary) {
    MockModulesPackage<> mp{this->device};
    std::unique_ptr<ModuleWithNativeBinary> moduleUnits[3] = {};
    moduleUnits[0] = std::make_unique<ModuleWithNativeBinary>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[1] = std::make_unique<ModuleWithNativeBinary>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[2] = std::make_unique<ModuleWithNativeBinary>(this->device, nullptr, L0::ModuleType::user);

    std::vector<uint8_t> unit0(8, 2);
    std::vector<uint8_t> unit1(14, 3);
    std::vector<uint8_t> unit2(16, 5);

    moduleUnits[0]->sizeToReturn = unit0.size();
    moduleUnits[0]->nativeBinaryToReturn = unit0;
    moduleUnits[1]->sizeToReturn = unit1.size();
    moduleUnits[1]->nativeBinaryToReturn = unit1;
    moduleUnits[2]->sizeToReturn = unit2.size();
    moduleUnits[2]->nativeBinaryToReturn = unit2;

    for (auto &m : moduleUnits) {
        mp.modules.push_back(std::move(m));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.prepareNativeBinary());
    auto nativeBinary = mp.nativeBinary;
    ASSERT_TRUE(L0::ModulesPackageBinary::isModulesPackageBinary(nativeBinary));
    std::string warning;
    std::string error;
    auto package = ModulesPackageBinary::decode(nativeBinary, error, warning);
    EXPECT_TRUE(warning.empty()) << warning;
    EXPECT_TRUE(error.empty()) << error;
    ASSERT_TRUE(package.has_value());
    ASSERT_EQ(3U, package->units.size());
    ASSERT_EQ(unit0.size(), package->units[0].data.size());
    ASSERT_EQ(unit1.size(), package->units[1].data.size());
    ASSERT_EQ(unit2.size(), package->units[2].data.size());
    EXPECT_EQ(unit0[0], package->units[0].data[0]);
    EXPECT_EQ(unit1[0], package->units[1].data[0]);
    EXPECT_EQ(unit2[0], package->units[2].data[0]);
}

using ModulesPackageGetNativeBinary = Test<L0::ult::DeviceFixture>;
TEST_F(ModulesPackageGetNativeBinary, WhenPrepareNativeBinaryFailsThenReturnsError) {
    struct MockModulePackageFailPrepare : MockModulesPackage<> {
        using MockModulesPackage::MockModulesPackage;
        ze_result_t prepareNativeBinary() override {
            return ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET;
        }
    };
    MockModulePackageFailPrepare mp{this->device};
    size_t size = {};
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET, mp.getNativeBinary(&size, nullptr));
}

TEST_F(ModulesPackageGetNativeBinary, WhenSizeIs0OrNativeBinaryOutPointerIsNullThenReturnsSize) {
    MockModulesPackage<> mp{this->device};
    mp.nativeBinary.resize(256);
    size_t size = 0;
    uint8_t outBinary = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getNativeBinary(&size, &outBinary));
    EXPECT_EQ(256U, size);
    size = 8U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getNativeBinary(&size, nullptr));
    EXPECT_EQ(256U, size);
}

TEST_F(ModulesPackageGetNativeBinary, WhenSizeIsTooSmallThenFails) {
    MockModulesPackage<> mp{this->device};
    mp.nativeBinary.resize(256);
    uint8_t outBinary = 0;
    size_t size = sizeof(outBinary);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, mp.getNativeBinary(&size, &outBinary));
}

TEST_F(ModulesPackageGetNativeBinary, GivenValidArgumentsThenReturnsNativeBinary) {
    MockModulesPackage<> mp{this->device};
    mp.nativeBinary.resize(4);
    mp.nativeBinary[0] = 2;
    mp.nativeBinary[1] = 3;
    mp.nativeBinary[2] = 5;
    mp.nativeBinary[3] = 7;
    uint8_t outBinary[4] = {11, 13, 17, 19};
    size_t size = sizeof(outBinary);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getNativeBinary(&size, outBinary));
    EXPECT_EQ(2, outBinary[0]);
    EXPECT_EQ(3, outBinary[1]);
    EXPECT_EQ(5, outBinary[2]);
    EXPECT_EQ(7, outBinary[3]);
}

using ModulesPackageDynamicLink = Test<L0::ult::DeviceFixture>;

TEST_F(ModulesPackageDynamicLink, GivenEmptyModulesListTheReturnsSuccess) {
    MockModulesPackage<> mp{this->device};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.performDynamicLink(0, nullptr, nullptr));
}

TEST_F(ModulesPackageDynamicLink, GivenNestedModulePackagesThenProcessUnitsRecursively) {
    MockModulesPackage<> mp{this->device};
    mp.modules.push_back(std::make_unique<MockModulesPackage<>>(this->device));
    ze_module_handle_t moduleToLink = &mp;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.performDynamicLink(1, &moduleToLink, nullptr));
}

namespace {
struct ModuleWithLinking : MockModule {
    using ClbT = std::function<ze_result_t(uint32_t, ze_module_handle_t *, ze_module_build_log_handle_t *)>;

    ModuleWithLinking(L0::Device *device, const ClbT &callback) : MockModule(device, nullptr, ModuleType::user), callback(callback) {}

    ze_result_t performDynamicLink(uint32_t numModules,
                                   ze_module_handle_t *phModules,
                                   ze_module_build_log_handle_t *phLinkLog) override {

        return callback(numModules, phModules, phLinkLog);
    }
    ClbT callback = [](uint32_t, ze_module_handle_t *, ze_module_build_log_handle_t *) { return ZE_RESULT_SUCCESS; };
};
} // namespace

TEST_F(ModulesPackageDynamicLink, GivenModulePackagesThenUsesAllUnitModules) {
    std::vector<ze_module_handle_t> inputModules;
    auto clb = [&](uint32_t numModules, ze_module_handle_t *phModules, ze_module_build_log_handle_t *phLinkLog) {
        inputModules.insert(inputModules.end(), phModules, phModules + numModules);
        return ZE_RESULT_SUCCESS;
    };

    MockModulesPackage<> mp0{this->device};
    mp0.modules.push_back(std::make_unique<ModuleWithLinking>(this->device, clb));
    mp0.modules.push_back(std::make_unique<ModuleWithLinking>(this->device, clb));
    mp0.modules.push_back(std::make_unique<ModuleWithLinking>(this->device, clb));
    MockModulesPackage<> mp1{this->device};
    mp1.modules.push_back(std::make_unique<ModuleWithLinking>(this->device, clb));
    mp1.modules.push_back(std::make_unique<ModuleWithLinking>(this->device, clb));

    MockModule mp2{this->device, nullptr, L0::ModuleType::user};

    ze_module_handle_t mods[3] = {&mp0, &mp1, &mp2};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp0.performDynamicLink(3, mods, nullptr));
    EXPECT_EQ(6U, inputModules.size());

    ze_module_handle_t expectedModulesToLink[] = {mp0.modules[0]->toHandle(), mp0.modules[1]->toHandle(), mp0.modules[2]->toHandle(),
                                                  mp1.modules[0]->toHandle(), mp1.modules[1]->toHandle(), mp2.toHandle()};
    for (auto mod : expectedModulesToLink) {
        EXPECT_NE(inputModules.end(), std::ranges::find(inputModules, mod));
    }

    // also check that single module uses package's link routine
    inputModules.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp2.performDynamicLink(3, mods, nullptr));
    EXPECT_EQ(6U, inputModules.size());

    for (auto mod : expectedModulesToLink) {
        EXPECT_NE(inputModules.end(), std::ranges::find(inputModules, mod));
    }
}

namespace {
struct ModuleWithLinkageInformation : MockModule {
    using ClbT = std::function<ze_result_t(ze_linkage_inspection_ext_desc_t *, uint32_t, ze_module_handle_t *, ze_module_build_log_handle_t *)>;

    ModuleWithLinkageInformation(L0::Device *device, const ClbT &callback) : MockModule(device, nullptr, ModuleType::user), callback(callback) {}

    ze_result_t inspectLinkage(ze_linkage_inspection_ext_desc_t *pInspectDesc,
                               uint32_t numModules,
                               ze_module_handle_t *phModules,
                               ze_module_build_log_handle_t *phLog) override {
        return callback(pInspectDesc, numModules, phModules, phLog);
    }
    ClbT callback;
};
} // namespace

using ModulesPackageInspectLinkage = Test<L0::ult::DeviceFixture>;
TEST_F(ModulesPackageInspectLinkage, GivenModulePackagesThenUsesAllUnitModules) {
    std::vector<ze_module_handle_t> inputModules;
    auto clb = [&](ze_linkage_inspection_ext_desc_t *, uint32_t numModules, ze_module_handle_t *phModules, ze_module_build_log_handle_t *) {
        inputModules.insert(inputModules.end(), phModules, phModules + numModules);
        return ZE_RESULT_SUCCESS;
    };

    MockModulesPackage<> mp0{this->device};
    mp0.modules.push_back(std::make_unique<ModuleWithLinkageInformation>(this->device, clb));
    mp0.modules.push_back(std::make_unique<ModuleWithLinkageInformation>(this->device, clb));
    mp0.modules.push_back(std::make_unique<ModuleWithLinkageInformation>(this->device, clb));
    MockModulesPackage<> mp1{this->device};
    mp1.modules.push_back(std::make_unique<ModuleWithLinkageInformation>(this->device, clb));
    mp1.modules.push_back(std::make_unique<ModuleWithLinkageInformation>(this->device, clb));
    mp1.modules.push_back(std::make_unique<MockModulesPackage<>>(this->device));

    MockModule mp2{this->device, nullptr, L0::ModuleType::user};

    ze_module_handle_t mods[3] = {&mp0, &mp1, &mp2};
    ze_module_build_log_handle_t linkageLog = {};
    ze_linkage_inspection_ext_desc_t paramLinkeExtDesc = {ZE_STRUCTURE_TYPE_LINKAGE_INSPECTION_EXT_DESC};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp0.inspectLinkage(&paramLinkeExtDesc, 3, mods, &linkageLog));

    EXPECT_EQ(6U, inputModules.size());

    ze_module_handle_t expectedModulesToInspect[] = {mp0.modules[0]->toHandle(), mp0.modules[1]->toHandle(), mp0.modules[2]->toHandle(),
                                                     mp1.modules[0]->toHandle(), mp1.modules[1]->toHandle(), mp2.toHandle()};
    for (auto mod : expectedModulesToInspect) {
        EXPECT_NE(inputModules.end(), std::ranges::find(inputModules, mod));
    }

    // also check that single module uses package's inspection routine
    inputModules.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp2.inspectLinkage(&paramLinkeExtDesc, 3, mods, &linkageLog));
    EXPECT_EQ(6U, inputModules.size());

    for (auto mod : expectedModulesToInspect) {
        EXPECT_NE(inputModules.end(), std::ranges::find(inputModules, mod));
    }
}

using ModulesPackagePrepareDebugInfo = Test<L0::ult::DeviceFixture>;

TEST_F(ModulesPackagePrepareDebugInfo, WhenDebugInfoIsAlreadyPreparedThenUsesIt) {
    MockModulesPackage<> mp{this->device};
    mp.debugInfo = std::vector<uint8_t>{2, 3, 5, 7};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.prepareDebugInfo());
    ASSERT_EQ(4U, mp.debugInfo.size());
    EXPECT_EQ(2U, mp.debugInfo[0]);
    EXPECT_EQ(3U, mp.debugInfo[1]);
    EXPECT_EQ(5U, mp.debugInfo[2]);
    EXPECT_EQ(7U, mp.debugInfo[3]);
}

namespace {
struct ModuleWithDebugInfo : MockModule {
    using MockModule::MockModule;
    ze_result_t getDebugInfo(size_t *pDebugDataSize, uint8_t *pDebugData) override {
        if (0 == sizeToReturn) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }
        if (nullptr == pDebugData) {
            *pDebugDataSize = sizeToReturn;
            return ZE_RESULT_SUCCESS;
        }
        if (*pDebugDataSize == 0) {
            *pDebugDataSize = sizeToReturn;
            return ZE_RESULT_SUCCESS;
        }

        if (debugInfoToReturn.size() != sizeToReturn) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }

        memcpy_s(pDebugData, *pDebugDataSize, debugInfoToReturn.data(), debugInfoToReturn.size());

        return ZE_RESULT_SUCCESS;
    }

    std::vector<uint8_t> debugInfoToReturn;
    size_t sizeToReturn = 0;
};
} // namespace

TEST_F(ModulesPackagePrepareDebugInfo, WhenFailedToGetAnyUnitsDebugInfoSizesThenFailsToPrepareDebugInfo) {
    MockModulesPackage<> mp{this->device};
    std::unique_ptr<ModuleWithDebugInfo> moduleUnits[3] = {};
    moduleUnits[0] = std::make_unique<ModuleWithDebugInfo>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[1] = std::make_unique<ModuleWithDebugInfo>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[2] = std::make_unique<ModuleWithDebugInfo>(this->device, nullptr, L0::ModuleType::user);

    moduleUnits[0]->sizeToReturn = 8;
    moduleUnits[1]->sizeToReturn = 0;
    moduleUnits[2]->sizeToReturn = 16;

    for (auto &m : moduleUnits) {
        mp.modules.push_back(std::move(m));
    }

    EXPECT_NE(ZE_RESULT_SUCCESS, mp.prepareDebugInfo());
}

TEST_F(ModulesPackagePrepareDebugInfo, WhenFailedToGetAnyUnitsDebugInfoThenFailsToPrepareDebugInfo) {
    MockModulesPackage<> mp{this->device};
    std::unique_ptr<ModuleWithDebugInfo> moduleUnits[3] = {};
    moduleUnits[0] = std::make_unique<ModuleWithDebugInfo>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[1] = std::make_unique<ModuleWithDebugInfo>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[2] = std::make_unique<ModuleWithDebugInfo>(this->device, nullptr, L0::ModuleType::user);

    std::vector<uint8_t> unit0(8, 0);
    std::vector<uint8_t> unit2(16, 0);

    moduleUnits[0]->sizeToReturn = unit0.size();
    moduleUnits[0]->debugInfoToReturn = unit0;
    moduleUnits[1]->sizeToReturn = 24;
    moduleUnits[2]->sizeToReturn = unit2.size();
    moduleUnits[2]->debugInfoToReturn = unit2;

    for (auto &m : moduleUnits) {
        mp.modules.push_back(std::move(m));
    }

    EXPECT_NE(ZE_RESULT_SUCCESS, mp.prepareDebugInfo());
}

TEST_F(ModulesPackagePrepareDebugInfo, WhenAllUnitsReturnValidDebugInfoThenCreatesModulesPackageDebugInfo) {
    MockModulesPackage<> mp{this->device};
    std::unique_ptr<ModuleWithDebugInfo> moduleUnits[3] = {};
    moduleUnits[0] = std::make_unique<ModuleWithDebugInfo>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[1] = std::make_unique<ModuleWithDebugInfo>(this->device, nullptr, L0::ModuleType::user);
    moduleUnits[2] = std::make_unique<ModuleWithDebugInfo>(this->device, nullptr, L0::ModuleType::user);

    std::vector<uint8_t> unit0(8, 2);
    std::vector<uint8_t> unit1(14, 3);
    std::vector<uint8_t> unit2(16, 5);

    moduleUnits[0]->sizeToReturn = unit0.size();
    moduleUnits[0]->debugInfoToReturn = unit0;
    moduleUnits[1]->sizeToReturn = unit1.size();
    moduleUnits[1]->debugInfoToReturn = unit1;
    moduleUnits[2]->sizeToReturn = unit2.size();
    moduleUnits[2]->debugInfoToReturn = unit2;

    for (auto &m : moduleUnits) {
        mp.modules.push_back(std::move(m));
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.prepareDebugInfo());
}

using ModulesPackageGetDebugInfo = Test<L0::ult::DeviceFixture>;
TEST_F(ModulesPackageGetDebugInfo, WhenPrepareDebugInfoFailsThenReturnsError) {
    struct MockModulePackageFailPrepareDebugInfo : MockModulesPackage<> {
        using MockModulesPackage::MockModulesPackage;
        ze_result_t prepareDebugInfo() override {
            return ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET;
        }
    };
    MockModulePackageFailPrepareDebugInfo mp{this->device};
    size_t size = {};
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET, mp.getDebugInfo(&size, nullptr));
}

TEST_F(ModulesPackageGetDebugInfo, WhenSizeIs0OrDebugInfoOutPointerIsNullThenReturnsSize) {
    MockModulesPackage<> mp{this->device};
    mp.debugInfo.resize(256);
    size_t size = 0;
    uint8_t outBinary = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getDebugInfo(&size, &outBinary));
    EXPECT_EQ(256U, size);
    size = 8U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getDebugInfo(&size, nullptr));
    EXPECT_EQ(256U, size);
}

TEST_F(ModulesPackageGetDebugInfo, WhenSizeIsTooSmallThenFails) {
    MockModulesPackage<> mp{this->device};
    mp.debugInfo.resize(256);
    uint8_t outBinary = 0;
    size_t size = sizeof(outBinary);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, mp.getDebugInfo(&size, &outBinary));
}

TEST_F(ModulesPackageGetDebugInfo, GivenValidArgumentsThenReturnsDebugInfo) {
    MockModulesPackage<> mp{this->device};
    mp.debugInfo.resize(4);
    mp.debugInfo[0] = 2;
    mp.debugInfo[1] = 3;
    mp.debugInfo[2] = 5;
    mp.debugInfo[3] = 7;
    uint8_t outBinary[4] = {11, 13, 17, 19};
    size_t size = sizeof(outBinary);
    EXPECT_EQ(ZE_RESULT_SUCCESS, mp.getDebugInfo(&size, outBinary));
    EXPECT_EQ(2, outBinary[0]);
    EXPECT_EQ(3, outBinary[1]);
    EXPECT_EQ(5, outBinary[2]);
    EXPECT_EQ(7, outBinary[3]);
}

} // namespace ult
} // namespace L0
