/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/experimental/source/tracing/tracing.h"
#include "level_zero/experimental/source/tracing/tracing_imp.h"
#include <level_zero/zet_api.h>

#include "gtest/gtest.h"

#include <array>
#include <iostream>

namespace L0 {

extern struct APITracerContextImp *pGlobalAPITracerContextImp;

namespace ult {
template <typename TFunctionPointer, typename... Args>
ze_result_t callHandleTracerRecursion(TFunctionPointer zeApiPtr, Args &&...args) {
    ZE_HANDLE_TRACER_RECURSION(zeApiPtr, args...);
    return ZE_RESULT_ERROR_UNKNOWN;
}

class ZeAPITracingCoreTestsFixture {
  public:
    ZeAPITracingCoreTestsFixture(){};

  protected:
    virtual void SetUp() { //NOLINT
        driver_ddiTable.enableTracing = true;
        myThreadPrivateTracerData.onList = false;
        myThreadPrivateTracerData.isInitialized = false;
        myThreadPrivateTracerData.testAndSetThreadTracerDataInitializedAndOnList();
    }

    virtual void TearDown() { //NOLINT
        myThreadPrivateTracerData.removeThreadTracerDataFromList();
        driver_ddiTable.enableTracing = false;
    }
};

class zeAPITracingCoreTests : public ZeAPITracingCoreTestsFixture, public ::testing::Test {

  protected:
    void SetUp() override { //NOLINT
        ZeAPITracingCoreTestsFixture::SetUp();
    }

    void TearDown() override { //NOLINT
        ZeAPITracingCoreTestsFixture::TearDown();
    }
};

class zeAPITracingRuntimeTests : public ZeAPITracingCoreTestsFixture, public ::testing::Test {

  protected:
    zet_core_callbacks_t prologCbs = {};
    zet_core_callbacks_t epilogCbs = {};
    zet_tracer_exp_handle_t apiTracerHandle;
    zet_tracer_exp_desc_t tracerDesc;
    int defaultUserData = 0;
    void *userData;

    void SetUp() override { //NOLINT
        ze_result_t result;

        ZeAPITracingCoreTestsFixture::SetUp();
        userData = &defaultUserData;
        tracerDesc.pUserData = userData;
        result = zetTracerExpCreate(nullptr, &tracerDesc, &apiTracerHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, apiTracerHandle);
    }

    void TearDown() override { //NOLINT
        ze_result_t result;

        result = zetTracerExpSetEnabled(apiTracerHandle, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpDestroy(apiTracerHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ZeAPITracingCoreTestsFixture::TearDown();
    }

    void setTracerCallbacksAndEnableTracer() {
        ze_result_t result;
        result = zetTracerExpSetPrologues(apiTracerHandle, &prologCbs);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        result = zetTracerExpSetEpilogues(apiTracerHandle, &epilogCbs);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        result = zetTracerExpSetEnabled(apiTracerHandle, true);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
};

class zeAPITracingRuntimeMultipleArgumentsTests : public ZeAPITracingCoreTestsFixture, public ::testing::Test {

  protected:
    zet_core_callbacks_t prologCbs0 = {};
    zet_core_callbacks_t epilogCbs0 = {};
    zet_core_callbacks_t prologCbs1 = {};
    zet_core_callbacks_t epilogCbs2 = {};
    zet_core_callbacks_t prologCbs3 = {};
    zet_core_callbacks_t epilogCbs3 = {};
    zet_tracer_exp_handle_t apiTracerHandle0;
    zet_tracer_exp_handle_t apiTracerHandle1;
    zet_tracer_exp_handle_t apiTracerHandle2;
    zet_tracer_exp_handle_t apiTracerHandle3;
    zet_tracer_exp_desc_t tracerDesc0;
    zet_tracer_exp_desc_t tracerDesc1;
    zet_tracer_exp_desc_t tracerDesc2;
    zet_tracer_exp_desc_t tracerDesc3;
    int defaultUserData0 = 1;
    void *pUserData0;
    int defaultUserData1 = 11;
    void *pUserData1;
    int defaultUserdata2 = 21;
    void *pUserData2;
    int defaultUserData3 = 31;
    void *pUserData3;

    void SetUp() override { //NOLINT
        ze_result_t result;

        ZeAPITracingCoreTestsFixture::SetUp();

        pUserData0 = &defaultUserData0;
        tracerDesc0.pUserData = pUserData0;
        result = zetTracerExpCreate(nullptr, &tracerDesc0, &apiTracerHandle0);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, apiTracerHandle0);

        pUserData1 = &defaultUserData1;
        tracerDesc1.pUserData = pUserData1;
        result = zetTracerExpCreate(nullptr, &tracerDesc1, &apiTracerHandle1);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, apiTracerHandle1);

        pUserData2 = &defaultUserdata2;
        tracerDesc2.pUserData = pUserData2;
        result = zetTracerExpCreate(nullptr, &tracerDesc2, &apiTracerHandle2);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, apiTracerHandle2);

        pUserData3 = &defaultUserData3;
        tracerDesc3.pUserData = pUserData3;
        result = zetTracerExpCreate(nullptr, &tracerDesc3, &apiTracerHandle3);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, apiTracerHandle3);
    }

    void TearDown() override { //NOLINT
        ze_result_t result;
        result = zetTracerExpSetEnabled(apiTracerHandle0, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpDestroy(apiTracerHandle0);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        result = zetTracerExpSetEnabled(apiTracerHandle1, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpDestroy(apiTracerHandle1);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        result = zetTracerExpSetEnabled(apiTracerHandle2, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpDestroy(apiTracerHandle2);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        result = zetTracerExpSetEnabled(apiTracerHandle3, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpDestroy(apiTracerHandle3);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        ZeAPITracingCoreTestsFixture::TearDown();
    }

    void setTracerCallbacksAndEnableTracer() {
        ze_result_t result;

        /* Both prolog and epilog, pass instance data from prolog to epilog */
        result = zetTracerExpSetPrologues(apiTracerHandle0, &prologCbs0);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpSetEpilogues(apiTracerHandle0, &epilogCbs0);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpSetEnabled(apiTracerHandle0, true);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        /* prolog only */
        result = zetTracerExpSetPrologues(apiTracerHandle1, &prologCbs1);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpSetEnabled(apiTracerHandle1, true);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        /* epilog only */
        result = zetTracerExpSetEpilogues(apiTracerHandle2, &epilogCbs2);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpSetEnabled(apiTracerHandle2, true);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        /* Both prolog and epilog, pass instance data from prolog to epilog */
        result = zetTracerExpSetPrologues(apiTracerHandle3, &prologCbs3);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpSetEpilogues(apiTracerHandle3, &epilogCbs3);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        result = zetTracerExpSetEnabled(apiTracerHandle3, true);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }

    void validateDefaultUserDataFinal() {
        EXPECT_EQ(defaultUserData0, 3);
        EXPECT_EQ(defaultUserData1, 22);
        EXPECT_EQ(defaultUserdata2, 42);
        EXPECT_EQ(defaultUserData3, 93);
    }
};

template <typename Tparams>
void genericPrologCallbackPtr(Tparams params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
    EXPECT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    *val += 1;
}

template <typename Tparams>
void genericEpilogCallbackPtr(Tparams params, ze_result_t result, void *pTracerUserData, void **ppTracerInstanceUserData) {
    EXPECT_NE(nullptr, pTracerUserData);
    int *val = static_cast<int *>(pTracerUserData);
    EXPECT_EQ(*val, 1);
}

template <typename THandleType>
THandleType generateRandomHandle() {
    return reinterpret_cast<THandleType>(static_cast<uint64_t>(rand() % (RAND_MAX - 1) + 1));
}

template <typename TSizeType>
TSizeType generateRandomSize() {
    return static_cast<TSizeType>(rand());
}

struct instanceDataStruct {
    void *instanceDataValue;
};

} // namespace ult
} // namespace L0
