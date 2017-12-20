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

#include "file_io.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/stdio.h"
#include <cstring>
#include <new>

size_t loadDataFromFile(
    const char *filename,
    void *&pData) {
    FILE *fp = nullptr;
    size_t nsize = 0;

    DEBUG_BREAK_IF(nullptr == filename);
    // Open the file
    fopen_s(&fp, filename, "rb");
    if (fp) {
        // Allocate a buffer for the file contents
        fseek(fp, 0, SEEK_END);
        nsize = (size_t)ftell(fp);

        fseek(fp, 0, SEEK_SET);

        pData = new (std::nothrow) char[nsize + 1];

        if (pData) {
            // we initialize to all zeroes before reading in data
            memset(pData, 0x00, nsize + 1);
            auto read = fread(pData, sizeof(unsigned char), nsize, fp);
            DEBUG_BREAK_IF(read != nsize);
            ((void)(read));
        } else {
            nsize = 0;
        }

        fclose(fp);
    }

    return nsize;
}

void deleteDataReadFromFile(void *&pData) {
    delete[](char *) pData;
    pData = nullptr;
}

size_t writeDataToFile(
    const char *filename,
    const void *pData,
    size_t dataSize) {
    FILE *fp = nullptr;
    size_t nsize = 0;

    DEBUG_BREAK_IF(nullptr == pData);
    DEBUG_BREAK_IF(nullptr == filename);

    fopen_s(&fp, filename, "wb");
    if (fp) {
        if (fwrite(pData, sizeof(unsigned char), dataSize, fp) == 0) {
            fclose(fp);
        } else {
            fclose(fp);
        }
        nsize = dataSize;
    }

    return nsize;
}

bool fileExists(const std::string &fileName) {
    FILE *pFile = nullptr;

    DEBUG_BREAK_IF(fileName.empty());
    DEBUG_BREAK_IF(fileName == "");

    fopen_s(&pFile, fileName.c_str(), "rb");
    if (pFile) {
        fclose(pFile);
    }
    return pFile != nullptr;
}

bool fileExistsHasSize(const std::string &fileName) {
    FILE *pFile = nullptr;
    size_t nsize = 0;

    DEBUG_BREAK_IF(fileName.empty());
    DEBUG_BREAK_IF(fileName == "");

    fopen_s(&pFile, fileName.c_str(), "rb");
    if (pFile) {
        fseek(pFile, 0, SEEK_END);
        nsize = (size_t)ftell(pFile);
        fclose(pFile);
    }
    return pFile != nullptr && nsize > 0;
}
