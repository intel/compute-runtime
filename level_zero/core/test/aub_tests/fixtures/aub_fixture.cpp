/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"

#include "shared/source/aub/aub_center.h"
#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/aub_memory_operations_handler.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/tests_configuration.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"
#include "test_mode.h"

namespace L0 {
AUBFixtureL0::AUBFixtureL0() = default;
AUBFixtureL0::~AUBFixtureL0() = default;

void AUBFixtureL0::setUp() {
    setUp(NEO::defaultHwInfo.get(), false);
}
void AUBFixtureL0::setUp(const NEO::HardwareInfo *hardwareInfo, bool debuggingEnabled) {
    ASSERT_NE(nullptr, hardwareInfo);
    const auto &hwInfo = *hardwareInfo;
    backupUltConfig = std::make_unique<VariableBackup<NEO::UltHwConfig>>(&NEO::ultHwConfig);

    executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1u);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    if (auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0]; !rootDeviceEnvironment.aubCenter) {
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
        auto hardwareInfo = rootDeviceEnvironment.getMutableHardwareInfo();
        auto localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(*hardwareInfo);
        rootDeviceEnvironment.initAubCenter(localMemoryEnabled, "", NEO::CommandStreamReceiverType::aub);
    }

    const auto aubCenter = executionEnvironment->rootDeviceEnvironments[0]->aubCenter.get();
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<NEO::AubMemoryOperationsHandler>(aubCenter->getAubManager());

    if (debuggingEnabled) {
        executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    }

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    auto engineType = getChosenEngineType(hwInfo);

    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream strfilename;

    strfilename << NEO::ApiSpecificConfig::getAubPrefixForSpecificApi();
    strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << gfxCoreHelper.getCsTraits(engineType).name;

    aubFileName = strfilename.str();
    NEO::ultHwConfig.aubTestName = aubFileName.c_str();

    neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, 0u);

    this->csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    if (!this->csr->getPreemptionAllocation()) {
        ASSERT_TRUE(this->csr->createPreemptionAllocation());
    }

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<ult::Mock<DriverHandleImp>>();

    driverHandle->enableProgramDebugging = debuggingEnabled ? NEO::DebuggingMode::online : NEO::DebuggingMode::disabled;

    driverHandle->initialize(std::move(devices));

    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_result_t returnValue;
    commandList.reset(ult::CommandList::whiteboxCast(CommandList::create(hwInfo.platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));

    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    ze_command_queue_desc_t queueDesc = {};
    pCmdq = CommandQueue::create(hwInfo.platform.eProductFamily, device, csr, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}
void AUBFixtureL0::tearDown() {
    context->destroy();
    pCmdq->destroy();
}

ze_module_handle_t AUBFixtureL0::createModuleFromFile(const std::string &fileName, ze_context_handle_t context, ze_device_handle_t device, const std::string &buildFlags, bool useSharedFile) {
    USE_REAL_FILE_SYSTEM();

    ze_module_handle_t moduleHandle;
    std::string testFile;
    if (useSharedFile) {
        retrieveBinaryKernelFilename(testFile, fileName + "_", ".bin");
    } else {
        retrieveBinaryKernelFilenameApiSpecific(testFile, fileName + "_", ".bin");
    }

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    EXPECT_NE(0u, size);
    EXPECT_NE(nullptr, src);

    if (!src || size == 0) {
        return nullptr;
    }

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;
    moduleDesc.pBuildFlags = buildFlags.data();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeModuleCreate(context, device, &moduleDesc, &moduleHandle, nullptr));
    return moduleHandle;
}

} // namespace L0