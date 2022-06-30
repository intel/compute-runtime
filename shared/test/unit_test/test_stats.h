/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <sstream>
#include <string>

std::string getTestStats() {
    std::stringstream stream;

    stream << "Total test count:               " << ::testing::UnitTest::GetInstance()->total_test_count() << std::endl;
    stream << "Test to run count:              " << ::testing::UnitTest::GetInstance()->test_to_run_count() << std::endl;
    stream << "Successful test count:          " << ::testing::UnitTest::GetInstance()->successful_test_count() << std::endl;
    stream << "Skipped test count:             " << ::testing::UnitTest::GetInstance()->skipped_test_count() << std::endl;
    stream << "Failed test count:              " << ::testing::UnitTest::GetInstance()->failed_test_count() << std::endl;
    stream << "Disabled test count:            " << ::testing::UnitTest::GetInstance()->disabled_test_count() << std::endl;
    stream << "Elapsed time:                   " << ::testing::UnitTest::GetInstance()->elapsed_time() << std::endl;

    return stream.str();
}

std::string getTestStatsJson() {
    std::stringstream stream;

    stream << "{" << std::endl;

    stream << "  \"total_test_count\": " << ::testing::UnitTest::GetInstance()->total_test_count() << "," << std::endl;
    stream << "  \"test_to_run_count\": " << ::testing::UnitTest::GetInstance()->test_to_run_count() << "," << std::endl;
    stream << "  \"successful_test_count\": " << ::testing::UnitTest::GetInstance()->successful_test_count() << "," << std::endl;
    stream << "  \"skipped_test_count\": " << ::testing::UnitTest::GetInstance()->skipped_test_count() << "," << std::endl;
    stream << "  \"failed_test_count\": " << ::testing::UnitTest::GetInstance()->failed_test_count() << "," << std::endl;
    stream << "  \"disabled_test_count\": " << ::testing::UnitTest::GetInstance()->disabled_test_count() << "," << std::endl;
    stream << "  \"elapsed_time\": " << ::testing::UnitTest::GetInstance()->elapsed_time() << std::endl;

    stream << "}" << std::endl;

    return stream.str();
}
