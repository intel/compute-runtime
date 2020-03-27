/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/directory.h"

#include "test.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <fstream>

using namespace NEO;

TEST(Directory, GetFiles) {
    std::ofstream tempfile("temp_file_that_does_not_exist.tmp");
    tempfile << " ";
    tempfile.flush();
    tempfile.close();

    std::string path = ".";
    std::vector<std::string> files = Directory::getFiles(path);

    EXPECT_LT(0u, files.size());
    std::remove("temp_file_that_does_not_exist.tmp");
}
