/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/custom_event_listener.h"

#include "gtest/gtest.h"

int main(int argc, char **argv) {
    int retVal = 0;

    ::testing::InitGoogleTest(&argc, argv);

    ::testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
    ::testing::TestEventListener *default_listner = listeners.default_result_printer();

    CCustomEventListener *customEventListener = new CCustomEventListener(default_listner);

    listeners.Release(listeners.default_result_printer());
    listeners.Append(customEventListener);

    retVal = RUN_ALL_TESTS();

    return retVal;
}
