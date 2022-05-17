/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/print.h"

#include <cctype>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

extern int memcpy_s(void *dst, size_t destSize, const void *src, size_t count); // NOLINT(readability-identifier-naming)

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
static_assert(sizeof(PRINTF_DATA_TYPE) == sizeof(int));

class PrintFormatter {
  public:
    PrintFormatter(const uint8_t *printfOutputBuffer, uint32_t printfOutputBufferMaxSize,
                   bool using32BitPointers, const StringMap *stringLiteralMap = nullptr);
    void printKernelOutput(const std::function<void(char *)> &print = [](char *str) { printToSTDOUT(str); });

    constexpr static size_t maxSinglePrintStringLength = 16 * MemoryConstants::kiloByte;

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
        return simpleSprintf(output, size, formatString, value);
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
            charactersPrinted += simpleSprintf(output + charactersPrinted, size - charactersPrinted, strippedFormat, value);
            if (i < valueCount - 1) {
                charactersPrinted += simpleSprintf(output + charactersPrinted, size - charactersPrinted, "%c", ',');
            }
        }

        if (sizeof(T) < 4) {
            currentOffset += (4 - sizeof(T)) * valueCount;
        }

        return charactersPrinted;
    }

    std::unique_ptr<char[]> output;

    const uint8_t *printfOutputBuffer = nullptr; // buffer extracted from the kernel, contains values to be printed
    uint32_t printfOutputBufferSize = 0;         // size of the data contained in the buffer

    bool using32BitPointers = false;
    const bool usesStringMap;
    const StringMap *stringLiteralMap;

    uint32_t currentOffset = 0; // current position in currently parsed buffer
};
}; // namespace NEO
