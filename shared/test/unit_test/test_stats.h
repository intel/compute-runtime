/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gmock/gmock.h"

#include <sstream>
#include <string>

std::string getTestStats() {
    std::string output;
    std::stringstream stream;

    stream << "Total test count:               " << ::testing::UnitTest::GetInstance()->total_test_count() << std::endl;
    stream << "Test to run count:              " << ::testing::UnitTest::GetInstance()->test_to_run_count() << std::endl;
    stream << "Reportable test count:          " << ::testing::UnitTest::GetInstance()->reportable_test_count() << std::endl;
    stream << "Disabled test count:            " << ::testing::UnitTest::GetInstance()->disabled_test_count() << std::endl;
    stream << "Reportable disabled test count: " << ::testing::UnitTest::GetInstance()->reportable_disabled_test_count() << std::endl;

    return stream.str();
}
