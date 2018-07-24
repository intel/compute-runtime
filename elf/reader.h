/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include "types.h"

#if defined(_WIN32)
#define ELF_CALL __stdcall
#else
#define ELF_CALL
#endif

namespace CLElfLib {
/******************************************************************************\

 Class:         CElfReader

 Description:   Class to provide simpler interaction with the ELF standard
                binary object.  SElf64Header defines the ELF header type and 
                SElf64SectionHeader defines the section header type.

\******************************************************************************/
class CElfReader {
  public:
    static CElfReader *ELF_CALL create(
        const char *pElfBinary,
        const size_t elfBinarySize);

    static void ELF_CALL destroy(
        CElfReader *&pElfObject);

    static bool ELF_CALL isValidElf64(
        const void *pBinary,
        const size_t binarySize);

    const SElf64Header *ELF_CALL getElfHeader();

    const SElf64SectionHeader *ELF_CALL getSectionHeader(
        unsigned int sectionIndex);

    const char *ELF_CALL getSectionName(
        unsigned int sectionIndex);

    bool ELF_CALL getSectionData(
        const unsigned int sectionIndex,
        char *&pData,
        size_t &dataSize);

    bool ELF_CALL getSectionData(
        const char *sectionName,
        char *&pData,
        size_t &dataSize);

  protected:
    ELF_CALL CElfReader(
        const char *pElfBinary,
        const size_t elfBinarySize);

    ELF_CALL ~CElfReader();

    const SElf64Header *m_pElfHeader; // pointer to the ELF header
    const char *m_pBinary;      // portable ELF binary
    char *m_pNameTable;         // pointer to the string table
    size_t m_nameTableSize;     // size of string table in bytes
};
} // namespace CLElfLib
