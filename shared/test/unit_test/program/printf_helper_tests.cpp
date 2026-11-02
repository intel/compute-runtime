/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/string.h"
#include "shared/source/program/print_formatter.h"
#include "shared/test/common/helpers/stream_capture.h"

#include "gtest/gtest.h"

#include <cmath>
#include <deque>

using namespace NEO;

// -------------------- Base Fixture ------------------------
class PrintFormatterTest : public testing::Test {
  public:
    static constexpr size_t maxPrintfOutputLength = 4096;
    static constexpr size_t printfBufferSize = 1024;

    uint8_t underlyingBuffer[maxPrintfOutputLength] = {};
    uint32_t offset = 4;
    PrintFormatter printFormatter{underlyingBuffer, printfBufferSize};

    std::deque<std::string> formatStrings;

  protected:
    PrintfDataType getPrintfDataType(char value) { return PrintfDataType::byteType; };
    PrintfDataType getPrintfDataType(int8_t value) { return PrintfDataType::byteType; };
    PrintfDataType getPrintfDataType(uint8_t value) { return PrintfDataType::byteType; };
    PrintfDataType getPrintfDataType(int16_t value) { return PrintfDataType::shortType; };
    PrintfDataType getPrintfDataType(uint16_t value) { return PrintfDataType::shortType; };
    PrintfDataType getPrintfDataType(int32_t value) { return PrintfDataType::intType; };
    PrintfDataType getPrintfDataType(uint32_t value) { return PrintfDataType::intType; };
    PrintfDataType getPrintfDataType(int64_t value) { return PrintfDataType::longType; };
    PrintfDataType getPrintfDataType(uint64_t value) { return PrintfDataType::longType; };
    PrintfDataType getPrintfDataType(float value) { return PrintfDataType::floatType; };
    PrintfDataType getPrintfDataType(double value) { return PrintfDataType::doubleType; };
    PrintfDataType getPrintfDataType(char *value) { return PrintfDataType::stringType; };

    template <class T>
    void injectValue(T value) {
        auto dataType = getPrintfDataType(value);
        storeData(dataType);
        if (dataType == PrintfDataType::byteType ||
            dataType == PrintfDataType::shortType) {
            storeData(static_cast<int>(value));
        } else {
            storeData(value);
        }
    }

    void injectStringValue(const char *value) {
        storeData(PrintfDataType::stringType);
        storeData(value);
    }

    template <class T>
    void storeData(T value) {
        T *valuePointer = reinterpret_cast<T *>(underlyingBuffer + offset);

        if (isAligned(valuePointer)) {
            *valuePointer = value;
        } else {
            memcpy_s(valuePointer, sizeof(underlyingBuffer) - offset, &value, sizeof(T));
        }

        offset += sizeof(T);

        // first four bytes always store the size
        uint32_t *pointer = reinterpret_cast<uint32_t *>(underlyingBuffer);
        *pointer = offset;
    }

    const char *injectFormatString(std::string str) {
        formatStrings.push_back(std::move(str));
        return formatStrings.back().c_str();
    }
};

TEST_F(PrintFormatterTest, GivenInlineFormatStringWhenPrintingThenBytesFromBufferAreUsed) {
    if (is32bit) {
        GTEST_SKIP();
    }

    std::string fmt = "answer %d\n";
    uint32_t byteLength = static_cast<uint32_t>(fmt.size() + 1);

    storeData<uint64_t>(PrintFormatter::inlineStringFormatFlag | byteLength);
    memcpy_s(underlyingBuffer + offset, sizeof(underlyingBuffer) - offset, fmt.c_str(), byteLength);
    offset += static_cast<uint32_t>(alignUp(byteLength, sizeof(uint32_t)));
    *reinterpret_cast<uint32_t *>(underlyingBuffer) = offset;

    injectValue(static_cast<int32_t>(42));

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    snprintf(referenceOutput, sizeof(referenceOutput), fmt.c_str(), 42);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

// for tests printing a single value
template <class T>
struct SingleValueTestParam {
    std::string format;
    T value;
};

template <class T>
struct SingleValueTestParamWithHostFormat {
    std::string format;
    std::string hostFormat;
    T value;
};

typedef SingleValueTestParam<int8_t> Int8Params;
typedef SingleValueTestParam<uint8_t> Uint8Params;
typedef SingleValueTestParam<int16_t> Int16Params;
typedef SingleValueTestParam<uint16_t> Uint16Params;
typedef SingleValueTestParam<int32_t> Int32Params;
typedef SingleValueTestParam<uint32_t> Uint32Params;
typedef SingleValueTestParamWithHostFormat<int64_t> Int64Params;
typedef SingleValueTestParamWithHostFormat<uint64_t> Uint64Params;
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

    auto formatString = injectFormatString(input.format);
    storeData(formatString);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfInt8Test,
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

    auto formatString = injectFormatString(input.format);
    storeData(formatString);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfInt32Test,
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

    auto formatString = injectFormatString(input.format);
    storeData(formatString);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

TEST_P(PrintfUint32Test, GivenBufferSizeGreaterThanPrintBufferWhenPrintingThenBufferIsTrimmed) {
    auto input = GetParam();
    PrintFormatter zeroSizeFormatter{underlyingBuffer, 0u};

    auto formatString = injectFormatString(input.format);
    storeData(formatString);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength] = "";
    char actualOutput[maxPrintfOutputLength] = "";
    zeroSizeFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfUint32Test,
                         PrintfUint32Test,
                         ::testing::ValuesIn(uintValues));

Int64Params longValues[] = {
    {"%lld", "%lld", INT64_MAX},
    {"%ld", "%lld", INT64_MAX},
    {"%ld", "%lld", INT64_MIN},
    {"%lld", "%lld", INT64_MIN},
    {"%zd", "%lld", INT64_MIN},
    {"%td", "%lld", INT64_MIN},
    {"%llx", "%llx", INT64_MAX},
    {"%lx", "%llx", INT64_MAX},
    {"%lx", "%llx", INT64_MIN},
    {"%llx", "%llx", INT64_MIN},
    {"%tx", "%llx", INT64_MIN},
    {"%d", "%d", INT32_MAX},
    {"%x", "%x", INT32_MAX}};

class PrintfInt64Test : public PrintFormatterTest,
                        public ::testing::WithParamInterface<Int64Params> {};

TEST_P(PrintfInt64Test, GivenFormatContainingLongWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto formatString = injectFormatString(input.format);
    storeData(formatString);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.hostFormat.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfInt64Test,
                         PrintfInt64Test,
                         ::testing::ValuesIn(longValues));

Uint64Params ulongValues[] = {
    {"%llu", "%llu", UINT64_MAX},
    {"%lu", "%llu", UINT64_MAX},
    {"%zu", "%llu", UINT64_MAX},
    {"%llux", "%llux", UINT64_MAX},
    {"%lux", "%llux", UINT64_MAX}};

class PrintfUint64Test : public PrintFormatterTest,
                         public ::testing::WithParamInterface<Uint64Params> {};

TEST_P(PrintfUint64Test, GivenFormatContainingUlongWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto formatString = injectFormatString(input.format);
    storeData(formatString);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.hostFormat.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfUint64Test,
                         PrintfUint64Test,
                         ::testing::ValuesIn(ulongValues));

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

    auto formatString = injectFormatString(input.format);
    storeData(formatString);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfFloatTest,
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

    auto formatString = injectFormatString(input.format);
    storeData(formatString);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfDoubleToFloatTest,
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

    auto formatString = injectFormatString(input.format);
    storeData(formatString);
    injectValue(input.value);

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    if (input.format[input.format.length() - 1] == 'F') {
        input.format[input.format.length() - 1] = 'f';
    }

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value);

    EXPECT_STREQ(referenceOutput, actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfDoubleTest,
                         PrintfDoubleTest,
                         ::testing::ValuesIn(doubleValues));

std::pair<std::string, std::string> specialValues[] = {
    {"%%", "%"},
    {"%% ", "% "},
    {"nothing%", "nothing"},
};

class PrintfSpecialTest : public PrintFormatterTest,
                          public ::testing::WithParamInterface<std::pair<std::string, std::string>> {};

TEST_P(PrintfSpecialTest, GivenFormatContainingDoublePercentageWhenPrintingThenValueIsInsertedCorrectly) {
    auto input = GetParam();

    auto formatString = injectFormatString(input.first);
    storeData(formatString);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ(input.second.c_str(), actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfSpecialTest,
                         PrintfSpecialTest,
                         ::testing::ValuesIn(specialValues));

// ------------------------- Testing Strings only with no Formatting ------------------------

class PrintfNoArgumentsTest : public PrintFormatterTest,
                              public ::testing::WithParamInterface<std::pair<std::string, std::string>> {};

// escape/non-escaped strings are specified manually to avoid converting them in code
// automatic code would have to do the same thing it is testing and therefore would be prone to mistakes
// this is needed because compiler doesn't escape the format strings and provides them exactly as they were typed in kernel source
std::pair<std::string, std::string> stringValues[] = {
    {R"(test)", R"(test)"},
    {R"(test\n)", R"(test\n)"},
    {R"(test\\n)", R"(test\\n)"},
    {R"(test\\\n)", R"(test\\\n)"},
    {R"(test\t)", R"(test\t)"},
    {R"(test\\t)", R"(test\\t)"},
    {R"(test\\\t)", R"(test\\\t)"},
};

TEST_P(PrintfNoArgumentsTest, GivenNoArgumentsWhenPrintingThenCharsAreEscaped) {
    auto input = GetParam();

    auto formatString = injectFormatString(input.first);
    storeData(formatString);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });
    EXPECT_STREQ(input.second.c_str(), actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfNoArgumentsTest,
                         PrintfNoArgumentsTest,
                         ::testing::ValuesIn(stringValues));

StringParams stringValues2[] = {
    {"%s", "foo"},
};

class PrintfStringTest : public PrintFormatterTest,
                         public ::testing::WithParamInterface<StringParams> {};

TEST_P(PrintfStringTest, GivenFormatContainingStringWhenPrintingThenValueIsInserted) {
    auto input = GetParam();

    auto formatString = injectFormatString(input.format);
    storeData(formatString);

    injectStringValue(injectFormatString(input.value));

    char referenceOutput[maxPrintfOutputLength];
    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    snprintf(referenceOutput, sizeof(referenceOutput), input.format.c_str(), input.value.c_str());

    EXPECT_STREQ(input.value.c_str(), actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfStringTest,
                         PrintfStringTest,
                         ::testing::ValuesIn(stringValues2));

TEST_F(PrintFormatterTest, GivenLongStringValueWhenPrintedThenFullStringIsPrinted) {
    char testedLongString[maxPrintfOutputLength];
    memset(testedLongString, 'a', maxPrintfOutputLength - 1);
    testedLongString[maxPrintfOutputLength - 1] = '\0';

    auto formatString = injectFormatString(testedLongString);
    storeData(formatString);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ(testedLongString, actualOutput);
}

TEST_F(PrintFormatterTest, GivenStringSpecifierWhenLongStringIsPassedAsValueThenFullStringIsPrinted) {
    char testedLongString[maxPrintfOutputLength];
    memset(testedLongString, 'a', maxPrintfOutputLength - 5);
    testedLongString[maxPrintfOutputLength - 5] = '\0';

    auto formatString = injectFormatString("%s");
    storeData(formatString);

    injectStringValue(injectFormatString(testedLongString));

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ(testedLongString, actualOutput);
}

TEST_F(PrintFormatterTest, GivenTooLongStringWhenPrintedThenOutputIsTruncated) {
    std::unique_ptr<char[]> testedLongString(new char[PrintFormatter::maxSinglePrintStringLength + 1024]);
    memset(testedLongString.get(), 'a', PrintFormatter::maxSinglePrintStringLength + 1024 - 1);
    testedLongString[PrintFormatter::maxSinglePrintStringLength + 1024 - 1] = '\0';

    auto formatString = injectFormatString(testedLongString.get());
    storeData(formatString);

    std::unique_ptr<char[]> actualOutput(new char[PrintFormatter::maxSinglePrintStringLength + 1024]);

    printFormatter.printKernelOutput([&actualOutput](char *str) { 
        size_t length = strnlen_s(str, PrintFormatter::maxSinglePrintStringLength + 1023);
        strncpy_s(actualOutput.get(), PrintFormatter::maxSinglePrintStringLength + 1024, str, length); });

    auto testedLength = strnlen_s(testedLongString.get(), PrintFormatter::maxSinglePrintStringLength + 1024);
    auto actualLength = strnlen_s(actualOutput.get(), PrintFormatter::maxSinglePrintStringLength + 1024);
    EXPECT_GT(testedLength, actualLength);
}

TEST_F(PrintFormatterTest, GivenNullTokenWhenPrintingThenNullIsInserted) {
    auto formatString = injectFormatString("%s");
    storeData(formatString);

    storeData(PrintfDataType::vectorIntType);
    storeData(0);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("(null)", actualOutput);
}

std::pair<std::string, std::string> stringLengthModifierValues[] = {
    {"<%ls>|%d", "<wide>|7"},
    {"<%Ls>|%d", "<wide>|7"},
    {"<%ws>|%d", "<wide>|7"},
    {"<%lls>|%d", "<wide>|7"},
    {"<%-6.3ls>|%d", "<wid   >|7"},
};

class PrintfStringLengthModifierTest : public PrintFormatterTest,
                                       public ::testing::WithParamInterface<std::pair<std::string, std::string>> {};

TEST_P(PrintfStringLengthModifierTest, GivenNarrowStringForStringConversionWithLengthModifierWhenPrintingThenStringAndNextArgumentArePrinted) {
    auto input = GetParam();

    auto formatString = injectFormatString(input.first);
    storeData(formatString);
    injectStringValue(injectFormatString("wide"));
    injectValue(static_cast<int32_t>(7));

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ(input.second.c_str(), actualOutput);
}

INSTANTIATE_TEST_SUITE_P(PrintfStringLengthModifierTest,
                         PrintfStringLengthModifierTest,
                         ::testing::ValuesIn(stringLengthModifierValues));

TEST_F(PrintFormatterTest, GivenUnknownConversionFollowedByDirectiveWhenPrintingThenEachDirectiveConsumesItsOwnArgument) {
    auto formatString = injectFormatString("%y%d|");
    storeData(formatString);
    injectValue(static_cast<int32_t>(1));
    injectValue(static_cast<int32_t>(7));

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("7|", actualOutput);
}

TEST_F(PrintFormatterTest, GivenStringArgumentForNonStringConversionWhenPrintingThenNothingIsPrintedForItAndNextArgumentIsPrinted) {
    auto formatString = injectFormatString("%d%d|");
    storeData(formatString);
    injectStringValue(injectFormatString("abc"));
    injectValue(static_cast<int32_t>(7));

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("7|", actualOutput);
}

TEST_F(PrintFormatterTest, GivenArgumentsOfEveryTypeForUnknownConversionsWhenPrintingThenAllAreSkippedAndNextArgumentIsPrinted) {
    auto formatString = injectFormatString("%y%y%y%y%y%y%y%y%y%y%y%y%y%y%d|");
    storeData(formatString);
    injectValue(static_cast<int8_t>(1));
    injectValue(static_cast<int16_t>(1));
    injectValue(static_cast<int32_t>(1));
    injectValue(1.0f);
    injectValue(static_cast<int64_t>(1));
    injectStringValue(injectFormatString("abc"));
    storeData(PrintfDataType::pointerType);
    storeData<uint64_t>(1);
    injectValue(1.0);

    storeData(PrintfDataType::vectorByteType);
    storeData(4);
    for (int i = 0; i < 4; i++) {
        storeData<int8_t>(1);
    }
    for (int i = 0; i < 3; i++) {
        storeData<int32_t>(0);
    }

    storeData(PrintfDataType::vectorShortType);
    storeData(2);
    storeData<int16_t>(1);
    storeData<int16_t>(1);
    storeData<int32_t>(0);

    storeData(PrintfDataType::vectorIntType);
    storeData(2);
    storeData<int32_t>(1);
    storeData<int32_t>(1);

    storeData(PrintfDataType::vectorLongType);
    storeData(2);
    storeData<int64_t>(1);
    storeData<int64_t>(1);

    storeData(PrintfDataType::vectorFloatType);
    storeData(2);
    storeData(1.0f);
    storeData(1.0f);

    storeData(PrintfDataType::vectorDoubleType);
    storeData(2);
    storeData(1.0);
    storeData(1.0);

    injectValue(static_cast<int32_t>(7));

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("7|", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorArgumentForStringConversionWhenPrintingThenNullIsInsertedAndNextArgumentIsPrinted) {
    auto formatString = injectFormatString("%s|%d");
    storeData(formatString);
    storeData(PrintfDataType::vectorIntType);
    storeData(2);
    storeData(1);
    storeData(2);
    injectValue(static_cast<int32_t>(7));

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("(null)|7", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorArgumentForStringConversionAtEndOfOutputWhenPrintingThenOnlyNullIsInserted) {
    std::string literal(PrintFormatter::maxSinglePrintStringLength - 7, 'a');
    auto formatString = injectFormatString(literal + "%s");
    storeData(formatString);
    storeData(PrintfDataType::vectorIntType);
    storeData(16);
    for (int i = 0; i < 16; i++) {
        storeData(i);
    }

    std::string actualOutput;

    printFormatter.printKernelOutput([&actualOutput](char *str) { actualOutput = str; });

    EXPECT_EQ(literal + "(null)", actualOutput);
}

TEST_F(PrintFormatterTest, GivenLongLengthModifierWithoutConversionWhenPrintingThenNextArgumentIsPrinted) {
    auto formatString = injectFormatString("%l%d|");
    storeData(formatString);
    injectValue(static_cast<int64_t>(1));
    injectValue(static_cast<int32_t>(7));

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("7|", actualOutput);
}

// ----------------------- Vector channel count ---------------------------------
TEST_F(PrintFormatterTest, GivenVector2WhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto formatString = injectFormatString("%v2d");
    storeData(formatString);

    storeData(PrintfDataType::vectorIntType);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++) {
        storeData(i + 1);
    }

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1,2", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVector4WhenPrintingThenAllValuesAreInserted) {
    int channelCount = 4;

    auto formatString = injectFormatString("%v4d");
    storeData(formatString);

    storeData(PrintfDataType::vectorIntType);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++) {
        storeData(i + 1);
    }

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1,2,3,4", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVector8WhenPrintingThenAllValuesAreInserted) {
    int channelCount = 8;

    auto formatString = injectFormatString("%v8d");
    storeData(formatString);

    storeData(PrintfDataType::vectorIntType);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++) {
        storeData(i + 1);
    }

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1,2,3,4,5,6,7,8", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVector16WhenPrintingThenAllValuesAreInserted) {
    int channelCount = 16;

    auto formatString = injectFormatString("%v16d");
    storeData(formatString);

    storeData(PrintfDataType::vectorIntType);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++) {
        storeData(i + 1);
    }

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16", actualOutput);
}

// ------------------- vector types ----------------------------
TEST_F(PrintFormatterTest, GivenVectorOfBytesWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto formatString = injectFormatString("%v2hhd");
    storeData(formatString);

    storeData(PrintfDataType::vectorByteType);
    // channel count
    storeData(channelCount);

    storeData<int8_t>(1);
    storeData<int8_t>(2);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1,2", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorOfShortsWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto formatString = injectFormatString("%v2hd");
    storeData(formatString);

    storeData(PrintfDataType::vectorShortType);
    // channel count
    storeData(channelCount);

    storeData<int16_t>(1);
    storeData<int16_t>(2);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1,2", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorOfIntsWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto formatString = injectFormatString("%v2d");
    storeData(formatString);

    storeData(PrintfDataType::vectorIntType);
    // channel count
    storeData(channelCount);

    storeData<int32_t>(1);
    storeData<int32_t>(2);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1,2", actualOutput);
}

TEST_F(PrintFormatterTest, GivenSpecialVectorWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto formatString = injectFormatString("%v2hld");
    storeData(formatString);

    storeData(PrintfDataType::vectorIntType);
    // channel count
    storeData(channelCount);

    storeData<int32_t>(1);
    storeData<int32_t>(2);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1,2", actualOutput);
}
TEST_F(PrintFormatterTest, GivenVectorOfLongsWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto formatString = injectFormatString("%v2ld");
    storeData(formatString);

    storeData(PrintfDataType::vectorLongType);
    // channel count
    storeData(channelCount);

    storeData<int64_t>(100000000000000);
    storeData<int64_t>(200000000000000);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("100000000000000,200000000000000", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorOfFloatsWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto formatString = injectFormatString("%v2f");
    storeData(formatString);

    storeData(PrintfDataType::vectorFloatType);
    // channel count
    storeData(channelCount);

    storeData<float>(1.0);
    storeData<float>(2.0);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1.000000,2.000000", actualOutput);
}

TEST_F(PrintFormatterTest, GivenVectorOfDoublesWhenPrintingThenAllValuesAreInserted) {
    int channelCount = 2;

    auto formatString = injectFormatString("%v2f");
    storeData(formatString);

    storeData(PrintfDataType::vectorDoubleType);
    // channel count
    storeData(channelCount);

    storeData<double>(1.0);
    storeData<double>(2.0);

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1.000000,2.000000", actualOutput);
}

TEST_F(PrintFormatterTest, GivenPointerWhenPrintingThenValueIsInserted) {
    auto formatString = injectFormatString("%p");
    storeData(formatString);

    int temp;

    storeData(PrintfDataType::pointerType);
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

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ(referenceOutput, actualOutput);
}

TEST_F(PrintFormatterTest, Given2ByteVectorsWhenPrintingThenDataBufferParsedProperly) {
    int channelCount = 4;

    auto formatString = injectFormatString("%v4hhd %v4hhd");
    storeData(formatString);

    storeData(PrintfDataType::vectorByteType);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++) {
        storeData(static_cast<int8_t>(i + 1));
    }

    // filler, should not be printed
    for (int i = 0; i < 12; i++) {
        storeData(static_cast<int8_t>(0));
    }

    storeData(PrintfDataType::vectorByteType);
    // channel count
    storeData(channelCount);

    // channel values
    for (int i = 0; i < channelCount; i++) {
        storeData(static_cast<int8_t>(i + 1));
    }

    // filler, should not be printed
    for (int i = 0; i < 12; i++) {
        storeData(static_cast<int8_t>(0));
    }

    char actualOutput[maxPrintfOutputLength];

    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });

    EXPECT_STREQ("1,2,3,4 1,2,3,4", actualOutput);
}

TEST_F(PrintFormatterTest, GivenEmptyBufferWhenPrintingThenFailSafely) {
    char actualOutput[maxPrintfOutputLength];
    actualOutput[0] = 0;
    printFormatter.printKernelOutput([&actualOutput](char *str) { strncpy_s(actualOutput, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });
    EXPECT_STREQ("", actualOutput);
}

TEST_F(PrintFormatterTest, GivenBufferWithFormatStringThenItIsPrintedProperly) {
    const char *formatString = "test string";
    storeData(formatString);

    char output[maxPrintfOutputLength];
    printFormatter.printKernelOutput([&output](char *str) { strncpy_s(output, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });
    EXPECT_STREQ(formatString, output);
}

TEST_F(PrintFormatterTest, GivenBufferWithEmptyFormatThenNothingIsPrinted) {
    const char *formatString = nullptr;
    storeData(formatString);

    char output[maxPrintfOutputLength]{};
    printFormatter.printKernelOutput([&output](char *str) { strncpy_s(output, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });
    EXPECT_STREQ("", output);
}

TEST_F(PrintFormatterTest, GivenBufferWithFormatStringAnd2StringsThenDataIsParsedAndPrintedProperly) {
    const char *formatString = "%s %s";
    storeData(formatString);

    const char *string1 = "str1";
    storeData(PrintfDataType::pointerType);
    storeData(string1);
    const char *string2 = "str2";
    storeData(PrintfDataType::pointerType);
    storeData(string2);

    const char *expectedOutput = "str1 str2";
    char output[maxPrintfOutputLength];
    printFormatter.printKernelOutput([&output](char *str) { strncpy_s(output, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });
    EXPECT_STREQ(expectedOutput, output);
}

TEST_F(PrintFormatterTest, GivenEndMarkerStoredWhenPrintingOutputThenBufferParsingStops) {
    const char *string1 = "str1";
    storeData(string1);
    storeData(PrintFormatter::endOfDataMarker);

    const char *string2 = "str2";
    storeData(string2);

    const char *expectedOutput = "str1";
    char output[maxPrintfOutputLength];
    printFormatter.printKernelOutput([&output](char *str) { strncpy_s(output, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });
    EXPECT_STREQ(expectedOutput, output);
}

TEST_F(PrintFormatterTest, GivenTypeSmallerThan4BThenItIsReadAs4BValue) {
    const char *formatString = "%c %hd %d";
    storeData(formatString);

    char byteValue = 'a';
    injectValue(byteValue);
    short shortValue = 123;
    injectValue(shortValue);
    int intValue = 456;
    injectValue(intValue);

    const char *expectedOutput = "a 123 456";
    char output[maxPrintfOutputLength];
    printFormatter.printKernelOutput([&output](char *str) { strncpy_s(output, maxPrintfOutputLength, str, maxPrintfOutputLength - 1); });
    EXPECT_STREQ(expectedOutput, output);
}

TEST(printToStdoutTest, GivenStringWhenPrintingToStdoutThenOutputOccurs) {
    StreamCapture capture;
    capture.captureStdout();
    printToStdout("test");
    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ("test", output.c_str());
}

TEST(simpleSprintf, GivenFormatRejectedByCrtWhenSimpleSprintfIsCalledThenZeroIsReturned) {
    if (is32bit) {
        GTEST_SKIP();
    }
    const char narrowString[8] = "wide";
    char out[1024] = {};

    EXPECT_EQ(0U, simpleSprintf(out, sizeof(out), "%ls", narrowString));
    EXPECT_EQ(0U, simpleSprintf<uint64_t>(out, sizeof(out), "%ls", reinterpret_cast<uint64_t>(narrowString)));
}

TEST(simpleSprintf, GivenEmptyFormatStringWhenSimpleSprintfIsCalledThenBailOutWith0) {
    char out[1024] = {7, 0};
    auto ret = simpleSprintf<float>(out, sizeof(out), "", 3.0f);
    EXPECT_EQ(0U, ret);
    EXPECT_EQ(0, out[0]);
    EXPECT_EQ(0, out[1]);
}
