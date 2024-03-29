/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/print.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <cctype>
#include <cstdint>
#include <fcntl.h>
#include <io.h>
#include <iostream>

void printToStreamHandle(const char *str, DWORD handle) {
    int fd = 0;
    HANDLE stdoutDuplicate = 0;
    FILE *pFile = nullptr;

    if ((DuplicateHandle(GetCurrentProcess(), GetStdHandle(handle),
                         GetCurrentProcess(), &stdoutDuplicate, 0L, TRUE, DUPLICATE_SAME_ACCESS))) {
        if ((fd = _open_osfhandle((DWORD_PTR)stdoutDuplicate, _O_TEXT)) &&
            (pFile = _fdopen(fd, "w"))) {
            fprintf_s(pFile, "%s", str);

            fflush(pFile);
            fclose(pFile);
        }
    }
}

void printToStdout(const char *str) {
    printToStreamHandle(str, STD_OUTPUT_HANDLE);
}

void printToStderr(const char *str) {
    printToStreamHandle(str, STD_ERROR_HANDLE);
}

template <class T>
size_t simpleSprintf(char *output, size_t outputSize, const char *format, T value) {
#if (_MSC_VER == 1800)
    _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
    size_t len = strlen(format);
    if (len == 0) {
        output[0] = '\0';
        return 0;
    }

    int retVal = 0;
    if (len > 3 && *(format + len - 2) == 'h' && *(format + len - 3) == 'h') {
        if (*(format + len - 1) == 'i' || *(format + len - 1) == 'd') {
            int32_t fixedValue = (char)value;
            retVal = sprintf_s(output, outputSize, format, fixedValue);
        } else {
            uint32_t fixedValue = (unsigned char)value;
            retVal = sprintf_s(output, outputSize, format, fixedValue);
        }
    } else if (format[len - 1] == 'F') {
        std::string formatCopy = format;
        *formatCopy.rbegin() = 'f';

        retVal = sprintf_s(output, outputSize, formatCopy.c_str(), value);
        for (int i = 0; i < retVal; i++)
            output[i] = std::toupper(output[i]);
    } else {
        retVal = sprintf_s(output, outputSize, format, value);
    }
    UNRECOVERABLE_IF(retVal < 0);
    return static_cast<size_t>(retVal);
}

size_t simpleSprintf(char *output, size_t outputSize, const char *format, const char *value) {
    auto retVal = sprintf_s(output, outputSize, format, value);
    UNRECOVERABLE_IF(retVal < 0);
    return static_cast<size_t>(retVal);
}

template size_t simpleSprintf<float>(char *output, size_t output_size, const char *format, float value);
template size_t simpleSprintf<double>(char *output, size_t output_size, const char *format, double value);
template size_t simpleSprintf<char>(char *output, size_t output_size, const char *format, char value);
template size_t simpleSprintf<int8_t>(char *output, size_t output_size, const char *format, int8_t value);
template size_t simpleSprintf<int16_t>(char *output, size_t output_size, const char *format, int16_t value);
template size_t simpleSprintf<int32_t>(char *output, size_t output_size, const char *format, int32_t value);
template size_t simpleSprintf<int64_t>(char *output, size_t output_size, const char *format, int64_t value);
template size_t simpleSprintf<uint8_t>(char *output, size_t output_size, const char *format, uint8_t value);
template size_t simpleSprintf<uint16_t>(char *output, size_t output_size, const char *format, uint16_t value);
template size_t simpleSprintf<uint32_t>(char *output, size_t output_size, const char *format, uint32_t value);
template size_t simpleSprintf<uint64_t>(char *output, size_t output_size, const char *format, uint64_t value);
