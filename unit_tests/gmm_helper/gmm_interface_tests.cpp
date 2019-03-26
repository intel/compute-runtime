/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"
#include "unit_tests/helpers/variable_backup.h"

#include "gtest/gtest.h"

#include <array>

GMM_CLIENT_CONTEXT *GMM_STDCALL createClientContext(GMM_CLIENT clientType) {
    return reinterpret_cast<GMM_CLIENT_CONTEXT *>(0x1);
}
void GMM_STDCALL deleteClientContext(GMM_CLIENT_CONTEXT *pGmmClientContext) {
}
void GMM_STDCALL destroySingletonContext(void) {
}
#ifdef _WIN32
#ifdef _WIN64
const char *mockGmmEntryName = "openMockGmm";
#else
const char *mockGmmEntryName = "_openMockGmm@4";
#endif
#define EXPORT_KEYWORD __declspec(dllexport)
GMM_STATUS GMM_STDCALL createSingletonContext(const PLATFORM platform,
                                              const SKU_FEATURE_TABLE *pSkuTable,
                                              const WA_TABLE *pWaTable,
                                              const GT_SYSTEM_INFO *pGtSysInfo) {
    return GMM_SUCCESS;
}
#else
const char *mockGmmEntryName = "openMockGmm";
#define EXPORT_KEYWORD
GMM_STATUS GMM_STDCALL createSingletonContext(const PLATFORM platform,
                                              const void *pSkuTable,
                                              const void *pWaTable,
                                              const void *pGtSysInfo) {
    return GMM_SUCCESS;
}
#endif

bool setCreateContextFunction = true;
bool setCreateContextSingletonFunction = true;
bool setDeleteContextFunction = true;
bool setDestroyContextSingletonFunction = true;
GMM_STATUS openGmmReturnValue = GMM_SUCCESS;

extern "C" EXPORT_KEYWORD GMM_STATUS GMM_STDCALL openMockGmm(GmmExportEntries *pGmmFuncs) {
    if (setCreateContextFunction) {
        pGmmFuncs->pfnCreateClientContext = &createClientContext;
    }
    if (setCreateContextSingletonFunction) {
        pGmmFuncs->pfnCreateSingletonContext = &createSingletonContext;
    }
    if (setDeleteContextFunction) {
        pGmmFuncs->pfnDeleteClientContext = &deleteClientContext;
    }
    if (setDestroyContextSingletonFunction) {
        pGmmFuncs->pfnDestroySingletonContext = &destroySingletonContext;
    }
    return openGmmReturnValue;
}

namespace Os {
extern const char *gmmDllName;
extern const char *gmmEntryName;
} // namespace Os
namespace NEO {
extern const HardwareInfo **platformDevices;
}
using namespace NEO;
struct GmmInterfaceTest : public ::testing::Test {

    void SetUp() override {
        gmmDllNameBackup = std::make_unique<VariableBackup<const char *>>(&Os::gmmDllName, "");
        gmmEntryNameBackup = std::make_unique<VariableBackup<const char *>>(&Os::gmmEntryName, mockGmmEntryName);
    }

    std::unique_ptr<VariableBackup<const char *>> gmmDllNameBackup;
    std::unique_ptr<VariableBackup<const char *>> gmmEntryNameBackup;
};

TEST_F(GmmInterfaceTest, givenValidGmmLibWhenCreateGmmHelperThenEverythingWorksFine) {
    std::unique_ptr<GmmHelper> gmmHelper;
    EXPECT_NO_THROW(gmmHelper.reset(new GmmHelper(*platformDevices)));
}
TEST_F(GmmInterfaceTest, givenInvalidGmmLibNameWhenCreateGmmHelperThenThrowException) {
    std::unique_ptr<GmmHelper> gmmHelper;
    *gmmDllNameBackup = "invalidName";
    EXPECT_THROW(gmmHelper.reset(new GmmHelper(*platformDevices)), std::exception);
}
TEST_F(GmmInterfaceTest, givenGmmLibWhenOpenGmmFunctionFailsThenThrowException) {
    std::unique_ptr<GmmHelper> gmmHelper;
    VariableBackup<GMM_STATUS> openGmmReturnValueBackup(&openGmmReturnValue, GMM_ERROR);
    EXPECT_THROW(gmmHelper.reset(new GmmHelper(*platformDevices)), std::exception);
}
TEST_F(GmmInterfaceTest, givenGmmLibWhenAnyFunctionIsNotLoadedThenThrowExceptionDuringGmmHelperCreation) {
    std::unique_ptr<GmmHelper> gmmHelper;
    std::array<bool *, 4> flags = {{&setCreateContextFunction, &setCreateContextSingletonFunction, &setDeleteContextFunction, &setDestroyContextSingletonFunction}};
    for (auto &flag : flags) {
        VariableBackup<bool> flagBackup(flag, false);
        EXPECT_THROW(gmmHelper.reset(new GmmHelper(*platformDevices)), std::exception);
    }
}
