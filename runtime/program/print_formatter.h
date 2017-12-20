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

#pragma once

#include "runtime/helpers/aligned_memory.h"
#include "runtime/kernel/kernel.h"
#include "runtime/os_interface/print.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>

extern int memcpy_s(void *dst, size_t destSize, const void *src, size_t count);

namespace OCLRT {

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
    PrintFormatter(Kernel &kernelArg, GraphicsAllocation &dataArg);
    void printKernelOutput(const std::function<void(char *)> &print = [](char *str) { printToSTDOUT(str); });

    static const size_t maxPrintfOutputLength = 1024;

  protected:
    void printString(const char *formatString, const std::function<void(char *)> &print);
    size_t printToken(char *output, size_t size, const char *formatString);

    char escapeChar(char escape);
    bool isConversionSpecifier(char c);
    void stripVectorFormat(const char *format, char *stripped);
    void stripVectorTypeConversion(char *format);

    template <class T>
    bool read(T *value) {
        if (offset + sizeof(T) <= bufferSize) {
            auto srcPtr = reinterpret_cast<T *>(buffer + offset);

            if (isAligned(srcPtr)) {
                *value = *srcPtr;
            } else {
                memcpy_s(value, bufferSize - offset, srcPtr, sizeof(T));
            }
            offset += sizeof(T);
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
            if (i < valueCount - 1)
                charactersPrinted += simple_sprintf(output + charactersPrinted, size - charactersPrinted, "%c", ',');
        }

        if (sizeof(T) < 4) {
            offset += (4 - sizeof(T)) * valueCount;
        }

        return charactersPrinted;
    }

    size_t printStringToken(char *output, size_t size, const char *formatString) {
        int index = 0;
        int type = 0;
        // additional read to discard the data type
        read(&type);
        read(&index);
        if (type == static_cast<int>(PRINTF_DATA_TYPE::STRING))
            return simple_sprintf(output, size, formatString, kernel.getKernelInfo().queryPrintfString(index));
        else
            return simple_sprintf(output, size, formatString, 0);
    }

    Kernel &kernel;
    GraphicsAllocation &data;

    uint8_t *buffer;     // buffer extracted from the kernel, contains values to be printed
    uint32_t bufferSize; // size of the data contained in the buffer
    uint32_t offset;     // current position in currently parsed buffer
};
};
