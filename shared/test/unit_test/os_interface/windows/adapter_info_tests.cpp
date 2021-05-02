/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/adapter_info_tests.h"

#include "test.h"

TEST(AdapterInfoTest, whenQueryingForDriverStorePathThenProvidesEnoughMemoryForReturnString) {
    NEO::Gdi gdi;
    auto handle = validHandle;
    gdi.queryAdapterInfo.mFunc = QueryAdapterInfoMock::queryadapterinfo;

    auto ret = NEO::queryAdapterDriverStorePath(gdi, handle);

    EXPECT_EQ(std::wstring(driverStorePathStr), ret) << "Expected " << driverStorePathStr << ", got : " << ret;
}
