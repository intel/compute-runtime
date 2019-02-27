/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/print.h"

#include "runtime/os_interface/windows/windows_wrapper.h"

#include <cctype>
#include <cstdint>
#include <fcntl.h>
#include <io.h>
#include <iostream>

void printToSTDOUT(const char *str) {
    int fd = 0;
    HANDLE stdoutDuplicate = 0;
    FILE *pFile = nullptr;

    if ((DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_OUTPUT_HANDLE),
                         GetCurrentProcess(), &stdoutDuplicate, 0L, TRUE, DUPLICATE_SAME_ACCESS))) {
        if ((fd = _open_osfhandle((DWORD_PTR)stdoutDuplicate, _O_TEXT)) &&
            (pFile = _fdopen(fd, "w"))) {
            fprintf_s(pFile, "%s", str);

            fflush(pFile);
            fclose(pFile);
        }
    }
}

template <class T>
size_t simple_sprintf(char *output, size_t outputSize, const char *format, T value) {
#if (_MSC_VER == 1800)
    _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
    size_t len = strlen(format);
    if (len == 0) {
        output[0] = '\0';
        return 0;
    }

    if (len > 3 && *(format + len - 2) == 'h' && *(format + len - 3) == 'h') {
        if (*(format + len - 1) == 'i' || *(format + len - 1) == 'd') {
            int32_t fixedValue = (char)value;
            return sprintf_s(output, outputSize, format, fixedValue);
        } else {
            uint32_t fixedValue = (unsigned char)value;
            return sprintf_s(output, outputSize, format, fixedValue);
        }
    } else if (format[len - 1] == 'F') {
        std::string formatCopy = format;
        *formatCopy.rbegin() = 'f';

        size_t returnValue = sprintf_s(output, outputSize, formatCopy.c_str(), value);
        for (size_t i = 0; i < returnValue; i++)
            output[i] = std::toupper(output[i]);
        return returnValue;
    } else {
        return sprintf_s(output, outputSize, format, value);
    }
}

size_t simple_sprintf(char *output, size_t outputSize, const char *format, const char *value) {
    return sprintf_s(output, outputSize, format, value);
}

template size_t simple_sprintf<float>(char *output, size_t output_size, const char *format, float value);
template size_t simple_sprintf<double>(char *output, size_t output_size, const char *format, double value);
template size_t simple_sprintf<char>(char *output, size_t output_size, const char *format, char value);
template size_t simple_sprintf<int8_t>(char *output, size_t output_size, const char *format, int8_t value);
template size_t simple_sprintf<int16_t>(char *output, size_t output_size, const char *format, int16_t value);
template size_t simple_sprintf<int32_t>(char *output, size_t output_size, const char *format, int32_t value);
template size_t simple_sprintf<int64_t>(char *output, size_t output_size, const char *format, int64_t value);
template size_t simple_sprintf<uint8_t>(char *output, size_t output_size, const char *format, uint8_t value);
template size_t simple_sprintf<uint16_t>(char *output, size_t output_size, const char *format, uint16_t value);
template size_t simple_sprintf<uint32_t>(char *output, size_t output_size, const char *format, uint32_t value);
template size_t simple_sprintf<uint64_t>(char *output, size_t output_size, const char *format, uint64_t value);