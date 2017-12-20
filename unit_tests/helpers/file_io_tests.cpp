/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/file_io.h"
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
