/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/print.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

extern int memcpy_s(void *dst, size_t destSize, const void *src, size_t count);

namespace NEO {

using StringMap = std::unordered_map<uint32_t, std::string>;

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

class PrintFormatter {
  public:
    PrintFormatter(const uint8_t *printfOutputBuffer, uint32_t printfOutputBufferMaxSize,
                   bool using32BitPointers, const StringMap &stringLiteralMap);
    void printKernelOutput(const std::function<void(char *)> &print = [](char *str) { printToSTDOUT(str); });

    static const size_t maxPrintfOutputLength = 1024;

  protected:
    const char *queryPrintfString(uint32_t index) const;
    void printString(const char *formatString, const std::function<void(char *)> &print);
    size_t printToken(char *output, size_t size, const char *formatString);
    size_t printStringToken(char *output, size_t size, const char *formatString);
    size_t printPointerToken(char *output, size_t size, const char *formatString);

    char escapeChar(char escape);
    bool isConversionSpecifier(char c);
    void stripVectorFormat(const char *format, char *stripped);
    void stripVectorTypeConversion(char *format);

    template <class T>
    bool read(T *value) {
        if (currentOffset + sizeof(T) <= printfOutputBufferSize) {
            auto srcPtr = reinterpret_cast<const T *>(printfOutputBuffer + currentOffset);

            if (isAligned(srcPtr)) {
                *value = *srcPtr;
            } else {
                memcpy_s(value, printfOutputBufferSize - currentOffset, srcPtr, sizeof(T));
            }
            currentOffset += sizeof(T);
            return true;
        } else {
            return false;
        }
    }

    template <class T>
    size_t typedPrintToken(char *output, size_t size, const char *formatString) {
        T value = {0};
        read(&value);
        return simple_sprintf(output, size, formatString, value);
    }

    template <class T>
    size_t typedPrintVectorToken(char *output, size_t size, const char *formatString) {
        T value = {0};
        int valueCount = 0;
        read(&valueCount);

        size_t charactersPrinted = 0;
        char strippedFormat[1024];

        stripVectorFormat(formatString, strippedFormat);
        stripVectorTypeConversion(strippedFormat);

        for (int i = 0; i < valueCount; i++) {
            read(&value);
            charactersPrinted += simple_sprintf(output + charactersPrinted, size - charactersPrinted, strippedFormat, value);
            if (i < valueCount - 1) {
                charactersPrinted += simple_sprintf(output + charactersPrinted, size - charactersPrinted, "%c", ',');
            }
        }

        if (sizeof(T) < 4) {
            currentOffset += (4 - sizeof(T)) * valueCount;
        }

        return charactersPrinted;
    }

    const uint8_t *printfOutputBuffer = nullptr; // buffer extracted from the kernel, contains values to be printed
    uint32_t printfOutputBufferSize = 0;         // size of the data contained in the buffer

    const StringMap &stringLiteralMap;
    bool using32BitPointers = false;

    uint32_t currentOffset = 0; // current position in currently parsed buffer
};
}; // namespace NEO
