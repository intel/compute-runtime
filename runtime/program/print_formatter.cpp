/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "print_formatter.h"

#include "runtime/helpers/string.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include <iostream>

namespace OCLRT {

PrintFormatter::PrintFormatter(Kernel &kernelArg, GraphicsAllocation &dataArg) : kernel(kernelArg),
                                                                                 data(dataArg),
                                                                                 buffer(nullptr),
                                                                                 bufferSize(0),
                                                                                 offset(0) {
}

void PrintFormatter::printKernelOutput(const std::function<void(char *)> &print) {
    offset = 0;
    buffer = reinterpret_cast<uint8_t *>(data.getUnderlyingBuffer());

    // first 4 bytes of the buffer store it's own size
    // before reading it size needs to be set to 4 because read() checks bounds and would fail if bufferSize was 0
    bufferSize = 4;
    read(&bufferSize);

    uint32_t stringIndex = 0;

    while (offset + 4 <= bufferSize) {
        read(&stringIndex);
        const char *formatString = kernel.getKernelInfo().queryPrintfString(stringIndex);
        if (formatString != nullptr) {
            printString(formatString, print);
        }
    }
}

void PrintFormatter::printString(const char *formatString, const std::function<void(char *)> &print) {
    size_t length = strnlen_s(formatString, maxPrintfOutputLength);
    char output[maxPrintfOutputLength];

    size_t cursor = 0;
    for (size_t i = 0; i <= length; i++) {
        if (formatString[i] == '\\')
            output[cursor++] = escapeChar(formatString[++i]);
        else if (formatString[i] == '%') {
            size_t end = i;
            if (end + 1 <= length && formatString[end + 1] == '%') {
                output[cursor++] = '%';
                continue;
            }

            while (isConversionSpecifier(formatString[end++]) == false && end < length)
                ;
            char dataFormat[maxPrintfOutputLength];

            memcpy_s(dataFormat, maxPrintfOutputLength, formatString + i, end - i);
            dataFormat[end - i] = '\0';

            if (formatString[end - 1] == 's')
                cursor += printStringToken(output + cursor, maxPrintfOutputLength - cursor, dataFormat);
            else
                cursor += printToken(output + cursor, maxPrintfOutputLength - cursor, dataFormat);

            i = end - 1;
        } else {
            output[cursor++] = formatString[i];
        }
    }

    print(output);
}

void PrintFormatter::stripVectorFormat(const char *format, char *stripped) {
    while (*format != '\0') {
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

size_t PrintFormatter::printToken(char *output, size_t size, const char *formatString) {
    PRINTF_DATA_TYPE type(PRINTF_DATA_TYPE::INVALID);
    read(&type);

    switch (type) {
    case PRINTF_DATA_TYPE::BYTE:
        return typedPrintToken<int8_t>(output, size, formatString);
    case PRINTF_DATA_TYPE::SHORT:
        return typedPrintToken<int16_t>(output, size, formatString);
    case PRINTF_DATA_TYPE::INT:
        return typedPrintToken<int>(output, size, formatString);
    case PRINTF_DATA_TYPE::FLOAT:
        return typedPrintToken<float>(output, size, formatString);
    case PRINTF_DATA_TYPE::LONG:
        return typedPrintToken<int64_t>(output, size, formatString);
    case PRINTF_DATA_TYPE::POINTER:
        return printPointerToken(output, size, formatString);
    case PRINTF_DATA_TYPE::DOUBLE:
        return typedPrintToken<double>(output, size, formatString);
    case PRINTF_DATA_TYPE::VECTOR_BYTE:
        return typedPrintVectorToken<int8_t>(output, size, formatString);
    case PRINTF_DATA_TYPE::VECTOR_SHORT:
        return typedPrintVectorToken<int16_t>(output, size, formatString);
    case PRINTF_DATA_TYPE::VECTOR_INT:
        return typedPrintVectorToken<int>(output, size, formatString);
    case PRINTF_DATA_TYPE::VECTOR_LONG:
        return typedPrintVectorToken<int64_t>(output, size, formatString);
    case PRINTF_DATA_TYPE::VECTOR_FLOAT:
        return typedPrintVectorToken<float>(output, size, formatString);
    case PRINTF_DATA_TYPE::VECTOR_DOUBLE:
        return typedPrintVectorToken<double>(output, size, formatString);
    default:
        return 0;
    }
}

size_t PrintFormatter::printStringToken(char *output, size_t size, const char *formatString) {
    int index = 0;
    int type = 0;
    // additional read to discard the token
    read(&type);
    read(&index);
    if (type == static_cast<int>(PRINTF_DATA_TYPE::STRING)) {
        return simple_sprintf(output, size, formatString, kernel.getKernelInfo().queryPrintfString(index));
    } else {
        return simple_sprintf(output, size, formatString, 0);
    }
}

size_t PrintFormatter::printPointerToken(char *output, size_t size, const char *formatString) {
    uint64_t value = {0};
    read(&value);

    if (kernel.is32Bit()) {
        value &= 0x00000000FFFFFFFF;
    }

    return simple_sprintf(output, size, formatString, value);
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
} // namespace OCLRT
