/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/directory.h"
#include "test.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <fstream>

using namespace OCLRT;
using namespace std;

TEST(Directory, GetFiles) {
    ofstream tempfile("temp_file_that_does_not_exist.tmp");
    tempfile << " ";
    tempfile.flush();
    tempfile.close();

    string path = ".";
    vector<string> files = Directory::getFiles(path);

    EXPECT_LT(0u, files.size());
    remove("temp_file_that_does_not_exist.tmp");
}
