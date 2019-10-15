/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/platform/platform.h"
#include "runtime/sku_info/operations/sku_info_transfer.h"
#include "unit_tests/helpers/variable_backup.h"

#include "gtest/gtest.h"

#include <array>

#ifdef _WIN32
#ifdef _WIN64
const char *mockGmmInitFuncName = "initMockGmm";
const char *mockGmmDestroyFuncName = "destroyMockGmm";
#else
const char *mockGmmInitFuncName = "_initMockGmm@8";
const char *mockGmmDestroyFuncName = "_destroyMockGmm@4";
#endif
#define EXPORT_KEYWORD __declspec(dllexport)
#else
const char *mockGmmInitFuncName = "initMockGmm";
const char *mockGmmDestroyFuncName = "destroyMockGmm";
#define EXPORT_KEYWORD
#endif

GMM_STATUS openGmmReturnValue = GMM_SUCCESS;
GMM_INIT_IN_ARGS passedInputArgs = {};
SKU_FEATURE_TABLE passedFtrTable = {};
WA_TABLE passedWaTable = {};
bool copyInputArgs = false;
extern "C" {
EXPORT_KEYWORD GMM_STATUS GMM_STDCALL initMockGmm(GMM_INIT_IN_ARGS *pInArgs, GMM_INIT_OUT_ARGS *pOutArgs) {
    if (copyInputArgs && pInArgs) {
        passedInputArgs = *pInArgs;
        passedFtrTable = *reinterpret_cast<SKU_FEATURE_TABLE *>(pInArgs->pSkuTable);
        passedWaTable = *reinterpret_cast<WA_TABLE *>(pInArgs->pWaTable);
    }
    pOutArgs->pGmmClientContext = reinterpret_cast<GMM_CLIENT_CONTEXT *>(0x01);
    return openGmmReturnValue;
}

EXPORT_KEYWORD void GMM_STDCALL destroyMockGmm(GMM_INIT_OUT_ARGS *pInArgs) {
}
}

namespace Os {
extern const char *gmmDllName;
extern const char *gmmInitFuncName;
extern const char *gmmDestroyFuncName;
} // namespace Os
namespace NEO {
extern const HardwareInfo **platformDevices;
}
using namespace NEO;
struct GmmInterfaceTest : public ::testing::Test {
    const char *empty = "";

    VariableBackup<const char *> gmmDllNameBackup = {&Os::gmmDllName, empty};
    VariableBackup<const char *> gmmInitNameBackup = {&Os::gmmInitFuncName, mockGmmInitFuncName};
    VariableBackup<const char *> gmmDestroyNameBackup = {&Os::gmmDestroyFuncName, mockGmmDestroyFuncName};
};

TEST_F(GmmInterfaceTest, givenValidGmmLibWhenCreateGmmHelperThenEverythingWorksFine) {
    std::unique_ptr<GmmHelper> gmmHelper;
    EXPECT_NO_THROW(gmmHelper.reset(new GmmHelper(*platformDevices)));
}

TEST_F(GmmInterfaceTest, givenInvalidGmmLibNameWhenCreateGmmHelperThenThrowException) {
    std::unique_ptr<GmmHelper> gmmHelper;
    gmmDllNameBackup = "invalidName";
    EXPECT_THROW(gmmHelper.reset(new GmmHelper(*platformDevices)), std::exception);
}

TEST_F(GmmInterfaceTest, givenGmmLibWhenOpenGmmFunctionFailsThenThrowException) {
    std::unique_ptr<GmmHelper> gmmHelper;
    VariableBackup<GMM_STATUS> openGmmReturnValueBackup(&openGmmReturnValue, GMM_ERROR);
    EXPECT_THROW(gmmHelper.reset(new GmmHelper(*platformDevices)), std::exception);
}

TEST_F(GmmInterfaceTest, givenInvalidGmmInitFunctionNameWhenCreateGmmHelperThenThrowException) {
    std::unique_ptr<GmmHelper> gmmHelper;
    gmmInitNameBackup = "invalidName";
    EXPECT_THROW(gmmHelper.reset(new GmmHelper(*platformDevices)), std::exception);
}

TEST_F(GmmInterfaceTest, givenInvalidGmmDestroyFunctionNameWhenCreateGmmHelperThenThrowException) {
    std::unique_ptr<GmmHelper> gmmHelper;
    gmmDestroyNameBackup = "invalidName";
    EXPECT_THROW(gmmHelper.reset(new GmmHelper(*platformDevices)), std::exception);
}

TEST_F(GmmInterfaceTest, givenValidGmmFunctionsWhenCreateGmmHelperWithInitializedOsInterfaceThenProperParametersArePassed) {
    std::unique_ptr<GmmHelper> gmmHelper;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    size_t numDevices;
    DeviceFactory::getDevices(numDevices, *executionEnvironment);
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    auto hwInfo = platformDevices[0];
    SKU_FEATURE_TABLE expectedFtrTable = {};
    WA_TABLE expectedWaTable = {};
    SkuInfoTransfer::transferFtrTableForGmm(&expectedFtrTable, &hwInfo->featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&expectedWaTable, &hwInfo->workaroundTable);

    gmmHelper.reset(new GmmHelper(hwInfo));
    EXPECT_EQ(0, memcmp(&hwInfo->platform, &passedInputArgs.Platform, sizeof(PLATFORM)));
    EXPECT_EQ(&hwInfo->gtSystemInfo, passedInputArgs.pGtSysInfo);
    EXPECT_EQ(0, memcmp(&expectedFtrTable, &passedFtrTable, sizeof(SKU_FEATURE_TABLE)));
    EXPECT_EQ(0, memcmp(&expectedWaTable, &passedWaTable, sizeof(WA_TABLE)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
}

TEST_F(GmmInterfaceTest, givenValidGmmFunctionsWhenCreateGmmHelperWithoutOsInterfaceThenInitializationDoesntCrashAndProperParametersArePassed) {
    std::unique_ptr<GmmHelper> gmmHelper;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->osInterface.reset();
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    auto hwInfo = platformDevices[0];
    SKU_FEATURE_TABLE expectedFtrTable = {};
    WA_TABLE expectedWaTable = {};
    SkuInfoTransfer::transferFtrTableForGmm(&expectedFtrTable, &hwInfo->featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&expectedWaTable, &hwInfo->workaroundTable);

    gmmHelper.reset(new GmmHelper(hwInfo));
    EXPECT_EQ(0, memcmp(&hwInfo->platform, &passedInputArgs.Platform, sizeof(PLATFORM)));
    EXPECT_EQ(&hwInfo->gtSystemInfo, passedInputArgs.pGtSysInfo);
    EXPECT_EQ(0, memcmp(&expectedFtrTable, &passedFtrTable, sizeof(SKU_FEATURE_TABLE)));
    EXPECT_EQ(0, memcmp(&expectedWaTable, &passedWaTable, sizeof(WA_TABLE)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
}