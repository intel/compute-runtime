/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/file_io.h"
#include "core/helpers/stdio.h"

#include "gtest/gtest.h"

#include <cstdio>

TEST(FileIO, existsHasSize) {
    std::string fileName("fileIO.bin");
    std::remove(fileName.c_str());
    ASSERT_FALSE(fileExists(fileName.c_str()));

    FILE *fp = nullptr;
    fopen_s(&fp, fileName.c_str(), "wb");
    ASSERT_NE(nullptr, fp);
    fprintf(fp, "TEST");
    fclose(fp);

    EXPECT_TRUE(fileExists(fileName.c_str()));
    EXPECT_TRUE(fileExistsHasSize(fileName.c_str()));
}

TEST(FileIO, existsSizeZero) {
    std::string fileName("fileIO.bin");
    std::remove(fileName.c_str());
    ASSERT_FALSE(fileExists(fileName.c_str()));

    FILE *fp = nullptr;
    fopen_s(&fp, fileName.c_str(), "wb");
    ASSERT_NE(nullptr, fp);
    fclose(fp);

    EXPECT_TRUE(fileExists(fileName.c_str()));
    EXPECT_FALSE(fileExistsHasSize(fileName.c_str()));
}
