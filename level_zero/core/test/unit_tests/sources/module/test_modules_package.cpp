/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module_build_log.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include <string>

namespace L0 {
namespace ult {

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

struct BuildOptionsLoggingModule : MockModule {
    using MockModule::MockModule;
    ze_result_t initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice) override {
        *log += desc->pBuildFlags;
        return ZE_RESULT_SUCCESS;
    }

    std::string *log = nullptr;
};

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

TEST(ModulesPackagePartialSupport, WhenCurrentlyUnsupportedApiIsCalledThenReturnAppropriateErrorCode) {
    MockDevice device;
    ModulesPackage mp{&device};

    size_t paramSizeT = 0;
    uint8_t paramUint8t = 0;
    uint32_t paramUint32t = 0;
    const char *paramConstCharPtr = nullptr;
    ze_module_properties_t paramModuleProperties = {ZE_STRUCTURE_TYPE_MODULE_PROPERTIES};
    ze_module_handle_t paramModuleHandleT = {};
    ze_linkage_inspection_ext_desc_t paramLinkeExtDesc = {ZE_STRUCTURE_TYPE_LINKAGE_INSPECTION_EXT_DESC};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, mp.getNativeBinary(&paramSizeT, &paramUint8t));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, mp.getDebugInfo(&paramSizeT, &paramUint8t));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, mp.getKernelNames(&paramUint32t, &paramConstCharPtr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, mp.getProperties(&paramModuleProperties));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, mp.performDynamicLink(1, &paramModuleHandleT, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, mp.inspectLinkage(&paramLinkeExtDesc, 1, &paramModuleHandleT, nullptr));
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
} // namespace ult
} // namespace L0
