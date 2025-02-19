/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "print_formatter.h"

#include "shared/source/helpers/string.h"

#include <iostream>

namespace NEO {

PrintFormatter::PrintFormatter(const uint8_t *printfOutputBuffer, uint32_t printfOutputBufferMaxSize,
                               bool using32BitPointers, const StringMap *stringLiteralMap)
    : printfOutputBuffer(printfOutputBuffer),
      printfOutputBufferSize(printfOutputBufferMaxSize),
      using32BitPointers(using32BitPointers),
      usesStringMap(stringLiteralMap != nullptr),
      stringLiteralMap(stringLiteralMap) {

    output.reset(new char[maxSinglePrintStringLength]);
}

void PrintFormatter::printKernelOutput(const std::function<void(char *)> &print) {
    currentOffset = initialOffset;

    // first 4 bytes of the buffer store the actual size of data that was written by printf from within EUs
    uint32_t printfOutputBufferSizeRead = 0;
    read(&printfOutputBufferSizeRead);
    printfOutputBufferSize = std::min(printfOutputBufferSizeRead, printfOutputBufferSize);

    if (usesStringMap) {
        uint32_t stringIndex = 0;
        while (currentOffset + 4 <= printfOutputBufferSize) {
            read(&stringIndex);
            const char *formatString = queryPrintfString(stringIndex);
            if (formatString != nullptr) {
                printString(formatString, print);
            }
        }
    } else {
        while (currentOffset + sizeof(char *) <= printfOutputBufferSize) {
            char *formatString = nullptr;
            read(&formatString);
            if (formatString == reinterpret_cast<char *>(static_cast<uintptr_t>(0xffffffff))) {
                break;
            }
            if (formatString != nullptr) {
                printString(formatString, print);
            }
        }
    }
}

void PrintFormatter::printString(const char *formatString, const std::function<void(char *)> &print) {
    size_t length = strnlen_s(formatString, maxSinglePrintStringLength - 1);

    size_t cursor = 0;
    std::unique_ptr<char[]> dataFormat(new char[length + 1]);

    for (size_t i = 0; i <= length; i++) {
        if (formatString[i] == '%') {
            size_t end = i;
            if (end + 1 <= length && formatString[end + 1] == '%') {
                output[cursor++] = '%';
                i++;
                continue;
            }

            while (isConversionSpecifier(formatString[end++]) == false && end < length)
                ;

            memcpy_s(dataFormat.get(), length, formatString + i, end - i);
            dataFormat[end - i] = '\0';

            if (formatString[end - 1] == 's')
                cursor += printStringToken(output.get() + cursor, maxSinglePrintStringLength - cursor, dataFormat.get());
            else
                cursor += printToken(output.get() + cursor, maxSinglePrintStringLength - cursor, dataFormat.get());

            i = end - 1;
        } else {
            output[cursor++] = formatString[i];
        }
    }
    output[maxSinglePrintStringLength - 1] = '\0';
    print(output.get());
}

void PrintFormatter::stripVectorFormat(const char *format, char *stripped) {
    while (*format != '\0') { // //NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)
        if (*format != 'v') {
            *stripped = *format;
        } else if (*(format + 1) != '1') {
            format += 2;
            continue;

        } else {
            format += 3;
            continue;
        }
        stripped++;
        format++;
    }
    *stripped = '\0';
}

void PrintFormatter::stripVectorTypeConversion(char *format) {
    size_t len = strlen(format);
    if (len > 3 && format[len - 3] == 'h' && format[len - 2] == 'l') {
        format[len - 3] = format[len - 1];
        format[len - 2] = '\0';
    }
}

template <>
void PrintFormatter::adjustFormatString<int64_t>(std::string &formatString) {
    auto longPosition = formatString.find('l');

    if (longPosition == std::string::npos) {
        return;
    }
    UNRECOVERABLE_IF(formatString.size() - 1 == longPosition);

    if (formatString.at(longPosition + 1) != 'l') {
        formatString.append(1, '\0');
        std::move_backward(formatString.begin() + longPosition, formatString.end() - 1, formatString.end());
        formatString[longPosition] = 'l';
    }
}

size_t PrintFormatter::printToken(char *output, size_t size, const char *formatString) {
    PrintfDataType type(PrintfDataType::invalidType);
    read(&type);

    switch (type) {
    case PrintfDataType::byteType:
        return typedPrintToken<int8_t>(output, size, formatString);
    case PrintfDataType::shortType:
        return typedPrintToken<int16_t>(output, size, formatString);
    case PrintfDataType::intType:
        return typedPrintToken<int>(output, size, formatString);
    case PrintfDataType::floatType:
        return typedPrintToken<float>(output, size, formatString);
    case PrintfDataType::longType:
        return typedPrintToken<int64_t>(output, size, formatString);
    case PrintfDataType::pointerType:
        return printPointerToken(output, size, formatString);
    case PrintfDataType::doubleType:
        return typedPrintToken<double>(output, size, formatString);
    case PrintfDataType::vectorByteType:
        return typedPrintVectorToken<int8_t>(output, size, formatString);
    case PrintfDataType::vectorShortType:
        return typedPrintVectorToken<int16_t>(output, size, formatString);
    case PrintfDataType::vectorIntType:
        return typedPrintVectorToken<int>(output, size, formatString);
    case PrintfDataType::vectorLongType:
        return typedPrintVectorToken<int64_t>(output, size, formatString);
    case PrintfDataType::vectorFloatType:
        return typedPrintVectorToken<float>(output, size, formatString);
    case PrintfDataType::vectorDoubleType:
        return typedPrintVectorToken<double>(output, size, formatString);
    default:
        return 0;
    }
}

size_t PrintFormatter::printStringToken(char *output, size_t size, const char *formatString) {
    PrintfDataType type = PrintfDataType::invalidType;
    read(&type);

    const char *string = nullptr;
    if (usesStringMap) {
        int index = 0;
        read(&index);
        string = queryPrintfString(index);
    } else {
        read(&string);
    }

    switch (type) {
    default:
        return simpleSprintf(output, size, formatString, 0);
    case PrintfDataType::stringType:
    case PrintfDataType::pointerType:
        return simpleSprintf(output, size, formatString, string);
    }
}

size_t PrintFormatter::printPointerToken(char *output, size_t size, const char *formatString) {
    uint64_t value = {0};
    read(&value);

    if (using32BitPointers) {
        value &= 0x00000000FFFFFFFF;
    }

    return simpleSprintf(output, size, formatString, value);
}

const char *PrintFormatter::queryPrintfString(uint32_t index) const {
    auto stringEntry = stringLiteralMap->find(index);
    return stringEntry == stringLiteralMap->end() ? nullptr : stringEntry->second.c_str();
}

char PrintFormatter::escapeChar(char escape) {
    switch (escape) {
    case 'n':
        return '\n';
    default:
        return escape;
    }
}

bool PrintFormatter::isConversionSpecifier(char c) {
    switch (c) {
    case 'd':
    case 'i':
    case 'o':
    case 'u':
    case 'x':
    case 'X':
    case 'a':
    case 'A':
    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case 's':
    case 'c':
    case 'p':
        return true;
    default:
        return false;
    }
}

} // namespace NEO
