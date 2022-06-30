/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/string.h"
#include "shared/source/program/print_formatter.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <cmath>

using namespace NEO;
using namespace iOpenCL;

// -------------------- Base Fixture ------------------------
class PrintFormatterTest : public testing::Test {
  public:
    std::unique_ptr<PrintFormatter> printFormatter;
    static const size_t maxPrintfOutputLength = 4096;
    static const size_t printfBufferSize = 1024;

    std::string format;
    uint8_t buffer;

    MockGraphicsAllocation *data;
    MockKernel *kernel;
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<MockKernelInfo> kernelInfo;
    ClDevice *device;

    uint8_t underlyingBuffer[maxPrintfOutputLength];
    uint32_t offset;

    int maxStringIndex;

  protected:
    void SetUp() override {
        offset = 4;
        maxStringIndex = 0;
        data = new MockGraphicsAllocation(underlyingBuffer, maxPrintfOutputLength);

        kernelInfo = std::make_unique<MockKernelInfo>();
        device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
        program = std::make_unique<MockProgram>(toClDeviceVector(*device));
        kernel = new MockKernel(program.get(), *kernelInfo, *device);

        printFormatter = std::unique_ptr<PrintFormatter>(new PrintFormatter(static_cast<uint8_t *>(data->getUnderlyingBuffer()), printfBufferSize, is32bit, &kernelInfo->kernelDescriptor.kernelMetadata.printfStringsMap));

        underlyingBuffer[0] = 0;
        underlyingBuffer[1] = 0;
        underlyingBuffer[2] = 0;
        underlyingBuffer[3] = 0;
    }

    void TearDown() override {
        delete data;
        delete kernel;
        delete device;
    }

    enum class PRINTF_DATA_TYPE : int {
        INVALID,
        BYTE,
        SHORT,
        INT,
        FLOAT,
        STRING,
        LONG,
        POINTER,
        DOUBLE,
        VECTOR_BYTE,
        VECTOR_SHORT,
        VECTOR_INT,
        VECTOR_LONG,
        VECTOR_FLOAT,
        VECTOR_DOUBLE
    };

    PRINTF_DATA_TYPE getPrintfDataType(int8_t value) { return PRINTF_DATA_TYPE::BYTE; };
    PRINTF_DATA_TYPE getPrintfDataType(uint8_t value) { return PRINTF_DATA_TYPE::BYTE; };
    PRINTF_DATA_TYPE getPrintfDataType(int16_t value) { return PRINTF_DATA_TYPE::SHORT; };
    PRINTF_DATA_TYPE getPrintfDataType(uint16_t value) { return PRINTF_DATA_TYPE::SHORT; };
    PRINTF_DATA_TYPE getPrintfDataType(int32_t value) { return PRINTF_DATA_TYPE::INT; };
    PRINTF_DATA_TYPE getPrintfDataType(uint32_t value) { return PRINTF_DATA_TYPE::INT; };
    PRINTF_DATA_TYPE getPrintfDataType(int64_t value) { return PRINTF_DATA_TYPE::LONG; };
    PRINTF_DATA_TYPE getPrintfDataType(uint64_t value) { return PRINTF_DATA_TYPE::LONG; };
    PRINTF_DATA_TYPE getPrintfDataType(float value) { return PRINTF_DATA_TYPE::FLOAT; };
    PRINTF_DATA_TYPE getPrintfDataType(double value) { return PRINTF_DATA_TYPE::DOUBLE; };
    PRINTF_DATA_TYPE getPrintfDataType(char *value) { return PRINTF_DATA_TYPE::STRING; };

    template <class T>
    void injectValue(T value) {
        storeData(getPrintfDataType(value));
        storeData(value);
    }

    void injectStringValue(int value) {
        storeData(PRINTF_DATA_TYPE::STRING);
        storeData(value);
    }

    template <class T>
    void storeData(T value) {
        T *valuePointer = reinterpret_cast<T *>(underlyingBuffer + offset);

        if (isAligned(valuePointer))
            *valuePointer = value;
        else {
            memcpy_s(valuePointer, sizeof(underlyingBuffer) - offset, &value, sizeof(T));
        }

        offset += sizeof(T);

        // first four bytes always store the size
        uint32_t *pointer = reinterpret_cast<uint32_t *>(underlyingBuffer);
        *pointer = offset;
    }

    int injectFormatString(std::string str) {
        auto index = maxStringIndex++;
        kernelInfo->addToPrintfStringsMap(index, str);
        return index;
    }
};

// for tests printing a single value
template <class T>
struct SingleValueTestParam {
    std::string format;
    T value;
};

typedef SingleValueTestParam<int8_t> Int8Params;
typedef SingleValueTestParam<uint8_t> Uint8Params;
typedef SingleValueTestParam<int16_t> Int16Params;
typedef SingleValueTestParam<uint16_t> Uint16Params;
typedef SingleValueTestParam<int32_t> Int32Params;
typedef SingleValueTestParam<uint32_t> Uint32Params;
typedef SingleValueTestParam<int64_t> Int64Params;
typedef SingleValueTestParam<uint64_t> Uint64Params;
typedef SingleValueTestParam<float> FloatParams;
typedef SingleValueTestParam<double> DoubleParams;
typedef SingleValueTestParam<std::string> StringParams;

Int8Params byteValues[] = {
    {"%c", 'a'},
};

class PrintfInt8Test : public PrintFormatterTest,
                       public ::testing::WithParamInterface<Int8Params> {};

TEST_P(PrintfInt8Test, GivenFormatContainingIntWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto stringIndex = injectFormatString(input.format);
    storeData(stringIndex);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_CASE_P(PrintfInt8Test,
                        PrintfInt8Test,
                        ::testing::ValuesIn(byteValues));

Int32Params intValues[] = {
    {"%d", 0},
    {"%d", 1},
    {"%d", -1},
    {"%d", INT32_MAX},
    {"%d", INT32_MIN},
    {"%5d", 10},
    {"%-5d", 10},
    {"%05d", 10},
    {"%+5d", 10},
    {"%-+5d", 10},
    {"%.5i", 100},
    {"%6.5i", 100},
    {"%-06i", 100},
    {"%06.5i", 100}};

class PrintfInt32Test : public PrintFormatterTest,
                        public ::testing::WithParamInterface<Int32Params> {};

TEST_P(PrintfInt32Test, GivenFormatContainingIntWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto stringIndex = injectFormatString(input.format);
    storeData(stringIndex);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_CASE_P(PrintfInt32Test,
                        PrintfInt32Test,
                        ::testing::ValuesIn(intValues));

Uint32Params uintValues[] = {
    {"%u", 0},
    {"%u", 1},
    {"%u", UINT32_MAX},
    {"%.0u", 0},
    // octal
    {"%o", 10},
    {"%.5o", 10},
    {"%#o", 100000000},
    {"%04.5o", 10},
    // hexadecimal
    {"%#x", 0xABCDEF},
    {"%#X", 0xABCDEF},
    {"%#X", 0},
    {"%8x", 399},
    {"%04x", 399}};

class PrintfUint32Test : public PrintFormatterTest,
                         public ::testing::WithParamInterface<Uint32Params> {};

TEST_P(PrintfUint32Test, GivenFormatContainingUintWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto stringIndex = injectFormatString(input.format);
    storeData(stringIndex);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

TEST_P(PrintfUint32Test, GivenBufferSizeGreaterThanPrintBufferWhenPrintingThenBufferIsTrimmed) {
    auto input = GetParam();
    printFormatter = std::unique_ptr<PrintFormatter>(new PrintFormatter(static_cast<uint8_t *>(data->getUnderlyingBuffer()), 0, is32bit, &kernelInfo->kernelDescriptor.kernelMetadata.printfStringsMap));

    auto stringIndex = injectFormatString(input.format);
    storeData(stringIndex);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength] = "";
    char actualOutput[maxPrintfOutputLength] = "";
    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_CASE_P(PrintfUint32Test,
                        PrintfUint32Test,
                        ::testing::ValuesIn(uintValues));

FloatParams floatValues[] = {
    {"%f", 10.3456f},
    {"%.1f", 10.3456f},
    {"%.2f", 10.3456f},
    {"%8.3f", 10.3456f},
    {"%08.2f", 10.3456f},
    {"%-8.2f", 10.3456f},
    {"%+8.2f", -10.3456f},
    {"%.0f", 0.1f},
    {"%.0f", 0.6f},
    {"%0f", 0.6f},
};

class PrintfFloatTest : public PrintFormatterTest,
                        public ::testing::WithParamInterface<FloatParams> {};

TEST_P(PrintfFloatTest, GivenFormatContainingFloatWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto stringIndex = injectFormatString(input.format);
    storeData(stringIndex);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_CASE_P(PrintfFloatTest,
                        PrintfFloatTest,
                        ::testing::ValuesIn(floatValues));

class PrintfDoubleToFloatTest : public PrintFormatterTest,
                                public ::testing::WithParamInterface<DoubleParams> {};

DoubleParams doubleToFloatValues[] = {
    {"%f", 10.3456},
    {"%.1f", 10.3456},
    {"%.2f", 10.3456},
    {"%8.3f", 10.3456},
    {"%08.2f", 10.3456},
    {"%-8.2f", 10.3456},
    {"%+8.2f", -10.3456},
    {"%.0f", 0.1},
    {"%0f", 0.6},
    {"%4g", 12345.6789},
    {"%4.2g", 12345.6789},
    {"%4G", 0.0000023},
    {"%4G", 0.023},
    {"%-#20.15e", 19456120.0},
    {"%+#21.15E", 19456120.0},
    {"%.6a", 0.1},
    {"%10.2a", 9990.235}};

TEST_P(PrintfDoubleToFloatTest, GivenFormatContainingFloatAndDoubleWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto stringIndex = injectFormatString(input.format);
    storeData(stringIndex);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_CASE_P(PrintfDoubleToFloatTest,
                        PrintfDoubleToFloatTest,
                        ::testing::ValuesIn(doubleToFloatValues));

DoubleParams doubleValues[] = {
    {"%f", 302230.12156260},
    {"%+f", 2937289102.1562},
    {"% #F", (double)-1254},
    {"%6.2f", 0.1562},
    {"%06.2f", -0.1562},
    {"%e", 0.1562},
    {"%E", -1254.0001001},
    {"%+.10e", 0.1562000102241},
    {"% E", (double)1254},
    {"%10.2e", 100230.1562},
    {"%g", 74010.00001562},
    {"%G", -1254.0001001},
    {"%+g", 325001.00001562},
    {"%+#G", -1254.0001001},
    {"%8.2g", 19.844},
    {"%1.5G", -1.1},
    {"%.13a", 1890.00001562},
    {"%.13A", -1254.0001001},
};

class PrintfDoubleTest : public PrintFormatterTest,
                         public ::testing::WithParamInterface<DoubleParams> {};

TEST_P(PrintfDoubleTest, GivenFormatContainingDoubleWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto stringIndex = injectFormatString(input.format);
    storeData(stringIndex);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    if (input.format[input.format.length() - 1] == 'F')
        input.format[input.format.length() - 1] = 'f';

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_CASE_P(PrintfDoubleTest,
                        PrintfDoubleTest,
                        ::testing::ValuesIn(doubleValues));

std::pair<std::string, std::string> specialValues[] = {
    {"%%", "%"},
    {"nothing%", "nothing"},
};

class PrintfSpecialTest : public PrintFormatterTest,
                          public ::testing::WithParamInterface<std::pair<std::string, std::string>> {};

TEST_P(PrintfSpecialTest, GivenFormatContainingDoublePercentageWhenPrintingThenValueIsInsertedCorrectly) {
    auto input = GetParam();

    auto stringIndex = injectFormatString(input.first);
    storeData(stringIndex);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ(input.second.c_str(), actualOutput);
}

INSTANTIATE_TEST_CASE_P(PrintfSpecialTest,
                        PrintfSpecialTest,
                        ::testing::ValuesIn(specialValues));

// ------------------------- Testing Strings only with no Formatting ------------------------

class PrintfNoArgumentsTest : public PrintFormatterTest,
                              public ::testing::WithParamInterface<std::pair<std::string, std::string>> {};

// escape/non-escaped strings are specified manually to avoid converting them in code
// automatic code would have to do the same thing it is testing and therefore would be prone to mistakes
// this is needed because compiler doesn't escape the format strings and provides them exactly as they were typed in kernel source
std::pair<std::string, std::string> stringValues[] = {
    {R"(test)", "test"},
    {R"(test\n)", "test\n"},
};

TEST_P(PrintfNoArgumentsTest, GivenNoArgumentsWhenPrintingThenCharsAreEscaped) {
    auto input = GetParam();

    auto stringIndex = injectFormatString(input.first);
    storeData(stringIndex);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ(input.second.c_str(), actualOutput);
}

INSTANTIATE_TEST_CASE_P(PrintfNoArgumentsTest,
                        PrintfNoArgumentsTest,
                        ::testing::ValuesIn(stringValues));

StringParams stringValues2[] = {
    {"%s", "foo"},
};

class PrintfStringTest : public PrintFormatterTest,
                         public ::testing::WithParamInterface<StringParams> {};

TEST_P(PrintfStringTest, GivenFormatContainingStringWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto stringIndex = injectFormatString(input.format);
    storeData(stringIndex);

    auto inputIndex = injectFormatString(input.value);
    injectStringValue(inputIndex);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value.c_str());

    EXPECT_STREQ(input.value.c_str(), actualOutput);
}

INSTANTIATE_TEST_CASE_P(PrintfStringTest,
                        PrintfStringTest,
                        ::testing::ValuesIn(stringValues2));

TEST_F(PrintFormatterTest, GivenLongStringValueWhenPrintedThenFullStringIsPrinted) {
    char testedLongString[maxPrintfOutputLength];
    memset(testedLongString, 'a', maxPrintfOutputLength - 1);
    testedLongString[maxPrintfOutputLength - 1] = '\0';

    auto stringIndex = injectFormatString(testedLongString);
    storeData(stringIndex);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ(testedLongString, actualOutput);
}

TEST_F(PrintFormatterTest, GivenStringSpecifierWhenLongStringIsPassedAsValueThenFullStringIsPrinted) {
    char testedLongString[maxPrintfOutputLength];
    memset(testedLongString, 'a', maxPrintfOutputLength - 5);
    testedLongString[maxPrintfOutputLength - 5] = '\0';

    auto stringIndex = injectFormatString("%s");
    storeData(stringIndex);

    auto inputIndex = injectFormatString(testedLongString);
    injectStringValue(inputIndex);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ(testedLongString, actualOutput);
}

TEST_F(PrintFormatterTest, GivenTooLongStringWhenPrintedThenOutputIsTruncated) {
    std::unique_ptr<char[]> testedLongString(new char[PrintFormatter::maxSinglePrintStringLength + 1024]);
    memset(testedLongString.get(), 'a', PrintFormatter::maxSinglePrintStringLength + 1024 - 1);
    testedLongString[PrintFormatter::maxSinglePrintStringLength + 1024 - 1] = '\0';

    auto stringIndex = injectFormatString(testedLongString.get());
    storeData(stringIndex);

    std::unique_ptr<char[]> actualOutput(new char[PrintFormatter::maxSinglePrintStringLength + 1024]);

    printFormatter->printKernelOutput([&actualOutput](char *str) { 
        size_t length = strnlen_s(str, PrintFormatter::maxSinglePrintStringLength + 1024);
        strncpy_s(actualOutput.get(), PrintFormatter::maxSinglePrintStringLength + 1024, str, length); });

    auto testedLength = strnlen_s(testedLongString.get(), PrintFormatter::maxSinglePrintStringLength + 1024);
    auto actualLength = strnlen_s(actualOutput.get(), PrintFormatter::maxSinglePrintStringLength + 1024);
    EXPECT_GT(testedLength, actualLength);
}

TEST_F(PrintFormatterTest, GivenNullTokenWhenPrintingThenNullIsInserted) {
    auto stringIndex = injectFormatString("%s");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_INT);
    storeData(0);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("(null)", actualOutput);
}

// ----------------------- Vector channel count ---------------------------------
TEST_F(PrintFormatterTest, GivenVector2WhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto stringIndex = injectFormatString("%v2d");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_INT);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++)
        storeData(i + 1);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVector4WhenPrintingThenAllValuesAreInserted) {
    int channelCount = 4;

    auto stringIndex = injectFormatString("%v4d");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_INT);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++)
        storeData(i + 1);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2,3,4", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVector8WhenPrintingThenAllValuesAreInserted) {
    int channelCount = 8;

    auto stringIndex = injectFormatString("%v8d");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_INT);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++)
        storeData(i + 1);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2,3,4,5,6,7,8", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVector16WhenPrintingThenAllValuesAreInserted) {
    int channelCount = 16;

    auto stringIndex = injectFormatString("%v16d");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_INT);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++)
        storeData(i + 1);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16", actualOutput);
}

// ------------------- vector types ----------------------------
TEST_F(PrintFormatterTest, GivenVectorOfBytesWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto stringIndex = injectFormatString("%v2hhd");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_BYTE);
    // channel count
    storeData(channelCount);

    storeData<int8_t>(1);
    storeData<int8_t>(2);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorOfShortsWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto stringIndex = injectFormatString("%v2hd");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_SHORT);
    // channel count
    storeData(channelCount);

    storeData<int16_t>(1);
    storeData<int16_t>(2);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorOfIntsWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto stringIndex = injectFormatString("%v2d");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_INT);
    // channel count
    storeData(channelCount);

    storeData<int32_t>(1);
    storeData<int32_t>(2);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2", actualOutput);
}

TEST_F(PrintFormatterTest, GivenSpecialVectorWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto stringIndex = injectFormatString("%v2hld");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_INT);
    // channel count
    storeData(channelCount);

    storeData<int32_t>(1);
    storeData<int32_t>(2);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2", actualOutput);
}
TEST_F(PrintFormatterTest, GivenVectorOfLongsWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto stringIndex = injectFormatString("%v2lld");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_LONG);
    // channel count
    storeData(channelCount);

    storeData<int64_t>(1);
    storeData<int64_t>(2);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorOfFloatsWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto stringIndex = injectFormatString("%v2f");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_FLOAT);
    // channel count
    storeData(channelCount);

    storeData<float>(1.0);
    storeData<float>(2.0);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1.000000,2.000000", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorOfDoublesWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto stringIndex = injectFormatString("%v2f");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_DOUBLE);
    // channel count
    storeData(channelCount);

    storeData<double>(1.0);
    storeData<double>(2.0);

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1.000000,2.000000", actualOutput);
}

TEST_F(PrintFormatterTest, GivenPointerWhenPrintingThenValueIsInserted) {
    auto stringIndex = injectFormatString("%p");
    storeData(stringIndex);

    int temp;

    storeData(PRINTF_DATA_TYPE::POINTER);
    // channel count
    storeData(reinterpret_cast<void *>(&temp));

    // on 32bit configurations add extra 4 bytes when storing pointers, IGC always stores pointers on 8 bytes
    if (is32bit) {
        uint32_t padding = 0;
        storeData(padding);
    }

    char actualOutput[maxPrintfOutputLength];
    char referenceOutput[maxPrintfOutputLength];

    snprintf(referenceOutput, sizeof(referenceOutput), "%p", reinterpret_cast<void *>(&temp));

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ(referenceOutput, actualOutput);
}

TEST_F(PrintFormatterTest, GivenPointerWith32BitKernelWhenPrintingThen32BitPointerIsPrinted) {
    printFormatter.reset(new PrintFormatter(static_cast<uint8_t *>(data->getUnderlyingBuffer()), printfBufferSize, true, &kernelInfo->kernelDescriptor.kernelMetadata.printfStringsMap));
    auto stringIndex = injectFormatString("%p");
    storeData(stringIndex);
    kernelInfo->kernelDescriptor.kernelAttributes.gpuPointerSize = 4;

    storeData(PRINTF_DATA_TYPE::POINTER);

    // store pointer
    uint32_t addressValue = 0;
    storeData(addressValue);

    void *pointer = nullptr;

    // store non zero padding
    uint32_t padding = 0xdeadbeef;
    storeData(padding);

    char actualOutput[maxPrintfOutputLength];
    char referenceOutput[maxPrintfOutputLength];

    snprintf(referenceOutput, sizeof(referenceOutput), "%p", pointer);

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ(referenceOutput, actualOutput);
}

TEST_F(PrintFormatterTest, Given2ByteVectorsWhenPrintingThenDataBufferParsedProperly) {
    int channelCount = 4;

    auto stringIndex = injectFormatString("%v4hhd %v4hhd");
    storeData(stringIndex);

    storeData(PRINTF_DATA_TYPE::VECTOR_BYTE);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++)
        storeData(static_cast<int8_t>(i + 1));

    // filler, should not be printed
    for (int i = 0; i < 12; i++)
        storeData(static_cast<int8_t>(0));

    storeData(PRINTF_DATA_TYPE::VECTOR_BYTE);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++)
        storeData(static_cast<int8_t>(i + 1));

    // filler, should not be printed
    for (int i = 0; i < 12; i++)
        storeData(static_cast<int8_t>(0));

    char actualOutput[maxPrintfOutputLength];

    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });

    EXPECT_STREQ("1,2,3,4 1,2,3,4", actualOutput);
}

TEST_F(PrintFormatterTest, GivenEmptyBufferWhenPrintingThenFailSafely) {
    char actualOutput[maxPrintfOutputLength];
    actualOutput[0] = 0;
    printFormatter->printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength); });
    EXPECT_STREQ("", actualOutput);
}

TEST_F(PrintFormatterTest, GivenNoStringMapAndBufferWithFormatStringThenItIsPrintedProperly) {
    printFormatter.reset(new PrintFormatter(static_cast<uint8_t *>(data->getUnderlyingBuffer()), printfBufferSize, true));
    const char *formatString = "test string";
    storeData(formatString);

    char output[maxPrintfOutputLength];
    printFormatter->printKernelOutput([&output](char *str) { strncpy_s(output, maxPrintfOutputLength, str, maxPrintfOutputLength); });
    EXPECT_STREQ(formatString, output);
}

TEST_F(PrintFormatterTest, GivenNoStringMapAndBufferWithFormatStringAnd2StringsThenDataIsParsedAndPrintedProperly) {
    printFormatter.reset(new PrintFormatter(static_cast<uint8_t *>(data->getUnderlyingBuffer()), printfBufferSize, true));
    const char *formatString = "%s %s";
    storeData(formatString);

    const char *string1 = "str1";
    storeData(PRINTF_DATA_TYPE::POINTER);
    storeData(string1);
    const char *string2 = "str2";
    storeData(PRINTF_DATA_TYPE::POINTER);
    storeData(string2);

    const char *expectedOutput = "str1 str2";
    char output[maxPrintfOutputLength];
    printFormatter->printKernelOutput([&output](char *str) { strncpy_s(output, maxPrintfOutputLength, str, maxPrintfOutputLength); });
    EXPECT_STREQ(expectedOutput, output);
}

TEST(printToSTDOUTTest, GivenStringWhenPrintingToStdoutThenOutputOccurs) {
    testing::internal::CaptureStdout();
    printToSTDOUT("test");
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ("test", output.c_str());
}

TEST(simpleSprintf, GivenEmptyFormatStringWhenSimpleSprintfIsCalledThenBailOutWith0) {
    char out[1024] = {7, 0};
    auto ret = simpleSprintf<float>(out, sizeof(out), "", 3.0f);
    EXPECT_EQ(0U, ret);
    EXPECT_EQ(0, out[0]);
    EXPECT_EQ(0, out[1]);
}
