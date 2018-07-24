/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "elf/reader.h"
#include "elf/writer.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "gtest/gtest.h"

using namespace CLElfLib;

struct ElfTests : public MemoryManagementFixture,
                  public ::testing::Test {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
    }

    void TearDown() override {
        MemoryManagementFixture::TearDown();
    }
};

TEST_F(ElfTests, Create_Delete_Writer_Simple) {
    CElfWriter *pWriter = CElfWriter::create(
        E_EH_TYPE::EH_TYPE_EXECUTABLE,
        E_EH_MACHINE::EH_MACHINE_NONE,
        0);
    EXPECT_NE((CElfWriter *)NULL, pWriter);

    CElfWriter::destroy(pWriter);
    EXPECT_EQ((CElfWriter *)NULL, pWriter);
}

TEST_F(ElfTests, Create_Reader_NULL_Binary) {
    char *pBinary = NULL;
    CElfReader *pReader = CElfReader::create(pBinary, 1);
    EXPECT_EQ(nullptr, pReader);
}

TEST_F(ElfTests, Create_Reader_Garbage_Binary) {
    char *pBinary = new char[16];
    if (pBinary) {
        memset(pBinary, 0x00, 16);
    }

    CElfReader *pReader = CElfReader::create(pBinary, 16);
    EXPECT_EQ((CElfReader *)NULL, pReader);

    delete[] pBinary;
}

TEST_F(ElfTests, Create_Delete_Reader_Writer_NoCheck) {
    CElfWriter *pWriter = CElfWriter::create(
        E_EH_TYPE::EH_TYPE_EXECUTABLE,
        E_EH_MACHINE::EH_MACHINE_NONE,
        0);
    EXPECT_NE((CElfWriter *)NULL, pWriter);

    size_t dataSize;
    pWriter->resolveBinary(NULL, dataSize);

    char *pBinary = new char[dataSize];
    if (pBinary) {
        pWriter->resolveBinary(pBinary, dataSize);
    }

    CElfReader *pReader = CElfReader::create(pBinary, dataSize);
    ASSERT_NE(nullptr, pReader);
    EXPECT_NE(nullptr, pReader->getElfHeader());

    CElfReader::destroy(pReader);
    EXPECT_EQ((CElfReader *)NULL, pReader);

    CElfWriter::destroy(pWriter);
    EXPECT_EQ((CElfWriter *)NULL, pWriter);

    delete[] pBinary;
}

TEST_F(ElfTests, givenNullptrWriterWhenDestroyThenDontCrash) {
    CElfWriter *ptr = nullptr;
    CElfWriter::destroy(ptr);
}

TEST_F(ElfTests, givenElfWriterWhenNullptrThenFalseIsReturned) {
    CElfWriter *pWriter = CElfWriter::create(
        E_EH_TYPE::EH_TYPE_EXECUTABLE,
        E_EH_MACHINE::EH_MACHINE_NONE,
        0);
    ASSERT_NE(nullptr, pWriter);

    auto ret = pWriter->addSection(nullptr);
    EXPECT_FALSE(ret);

    CElfWriter::destroy(pWriter);
}

TEST_F(ElfTests, givenElfWriterWhenSectionAndFailuresThenFalseIsReturned) {
    InjectedFunction method = [](size_t failureIndex) {
        CElfWriter *pWriter = CElfWriter::create(
            E_EH_TYPE::EH_TYPE_EXECUTABLE,
            E_EH_MACHINE::EH_MACHINE_NONE,
            0);
        if (nonfailingAllocation == failureIndex) {
            ASSERT_NE(nullptr, pWriter);
        }

        if (pWriter != nullptr) {
            char sectionData[16];
            memset(sectionData, 0xdeadbeef, 4);

            SSectionNode sectionNode;
            sectionNode.DataSize = 16;
            sectionNode.pData = sectionData;
            sectionNode.Flags = E_SH_FLAG::SH_FLAG_WRITE;
            sectionNode.Name = "Steve";
            sectionNode.Type = E_SH_TYPE::SH_TYPE_OPENCL_SOURCE;

            auto ret = pWriter->addSection(&sectionNode);
            if (nonfailingAllocation == failureIndex) {
                EXPECT_TRUE(ret);
            } else {
                EXPECT_FALSE(ret);
            }

            CElfWriter::destroy(pWriter);
        }
    };
    injectFailures(method);
}

TEST_F(ElfTests, givenElfWriterWhenPatchNullptrThenFalseIsReturned) {
    CElfWriter *pWriter = CElfWriter::create(
        E_EH_TYPE::EH_TYPE_EXECUTABLE,
        E_EH_MACHINE::EH_MACHINE_NONE,
        0);
    ASSERT_NE(nullptr, pWriter);

    auto ret = pWriter->patchElfHeader(nullptr);
    EXPECT_FALSE(ret);

    CElfWriter::destroy(pWriter);
}

TEST_F(ElfTests, Write_Read_Section_Data_By_Name) {
    CElfWriter *pWriter = CElfWriter::create(
        E_EH_TYPE::EH_TYPE_EXECUTABLE,
        E_EH_MACHINE::EH_MACHINE_NONE,
        0);
    EXPECT_NE((CElfWriter *)NULL, pWriter);

    char sectionData[16];
    memset(sectionData, 0xdeadbeef, 4);

    SSectionNode sectionNode;
    sectionNode.DataSize = 16;
    sectionNode.pData = sectionData;
    sectionNode.Flags = E_SH_FLAG::SH_FLAG_WRITE;
    sectionNode.Name = "Steve";
    sectionNode.Type = E_SH_TYPE::SH_TYPE_OPENCL_SOURCE;

    pWriter->addSection(&sectionNode);

    size_t binarySize;
    pWriter->resolveBinary(NULL, binarySize);

    char *pBinary = new char[binarySize];
    if (pBinary) {
        pWriter->resolveBinary(pBinary, binarySize);
    }

    CElfReader *pReader = CElfReader::create(pBinary, binarySize);
    EXPECT_NE((CElfReader *)NULL, pReader);

    char *pData = NULL;
    size_t dataSize = 0;
    auto retVal = pReader->getSectionData("Steve", pData, dataSize);

    EXPECT_TRUE(retVal);
    EXPECT_EQ(16u, dataSize);
    for (unsigned int i = 0; i < dataSize; i++) {
        EXPECT_EQ(sectionData[i], pData[i]);
    }

    CElfReader::destroy(pReader);
    EXPECT_EQ((CElfReader *)NULL, pReader);

    CElfWriter::destroy(pWriter);
    EXPECT_EQ((CElfWriter *)NULL, pWriter);

    delete[] pBinary;
}

TEST_F(ElfTests, Write_Read_Section_Data_By_Index) {
    CElfWriter *pWriter = CElfWriter::create(
        E_EH_TYPE::EH_TYPE_EXECUTABLE,
        E_EH_MACHINE::EH_MACHINE_NONE,
        0);
    EXPECT_NE((CElfWriter *)NULL, pWriter);

    char sectionData[16];
    memset(sectionData, 0xdeadbeef, 4);

    SSectionNode sectionNode;
    sectionNode.DataSize = 16;
    sectionNode.pData = sectionData;
    sectionNode.Flags = E_SH_FLAG::SH_FLAG_WRITE;
    sectionNode.Name = "";
    sectionNode.Type = E_SH_TYPE::SH_TYPE_OPENCL_SOURCE;

    pWriter->addSection(&sectionNode);

    size_t binarySize;
    pWriter->resolveBinary(NULL, binarySize);

    char *pBinary = new char[binarySize];
    if (pBinary) {
        pWriter->resolveBinary(pBinary, binarySize);
    }

    CElfReader *pReader = CElfReader::create(pBinary, binarySize);
    EXPECT_NE((CElfReader *)NULL, pReader);

    char *pData = NULL;
    size_t dataSize = 0;
    auto retVal = pReader->getSectionData(1, pData, dataSize);

    EXPECT_TRUE(retVal);
    EXPECT_EQ(16u, dataSize);
    for (unsigned int i = 0; i < dataSize; i++) {
        EXPECT_EQ(sectionData[i], pData[i]);
    }

    CElfReader::destroy(pReader);
    EXPECT_EQ((CElfReader *)NULL, pReader);

    CElfWriter::destroy(pWriter);
    EXPECT_EQ((CElfWriter *)NULL, pWriter);

    delete[] pBinary;
}
