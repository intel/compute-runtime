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
#include <queue>
#include <string>

#if defined(_WIN32)
#define ELF_CALL __stdcall
#else
#define ELF_CALL
#endif

using namespace std;

namespace CLElfLib {
static const unsigned int g_scElfHeaderAlignment = 16; // allocation alignment restriction
static const unsigned int g_scInitialElfSize = 2048;   // initial elf size (in bytes)
static const unsigned int g_scInitNumSectionHeaders = 8;

struct SSectionNode {
    E_SH_TYPE Type;
    unsigned int Flags;
    string Name;
    char *pData;
    unsigned int DataSize;

    SSectionNode() {
        Type = SH_TYPE_NULL;
        Flags = 0;
        pData = NULL;
        DataSize = 0;
    }

    ~SSectionNode() {
    }
};

/******************************************************************************\

 Class:         CElfWriter

 Description:   Class to provide simpler interaction with the ELF standard
                binary object.  SElf64Header defines the ELF header type and 
                SElf64SectionHeader defines the section header type.

\******************************************************************************/
class CElfWriter {
  public:
    static CElfWriter *ELF_CALL create(
        E_EH_TYPE type,
        E_EH_MACHINE machine,
        Elf64_Xword flags);

    static void ELF_CALL destroy(CElfWriter *&pElfWriter);

    bool ELF_CALL addSection(
        SSectionNode *pSectionNode);

    bool ELF_CALL resolveBinary(
        char *const pBinary,
        size_t &dataSize);

    bool ELF_CALL initialize();
    bool ELF_CALL patchElfHeader(char *const pBinary);

  protected:
    ELF_CALL CElfWriter(
        E_EH_TYPE type,
        E_EH_MACHINE machine,
        Elf64_Xword flags);

    ELF_CALL ~CElfWriter();

    E_EH_TYPE m_type = EH_TYPE_NONE;
    E_EH_MACHINE m_machine = EH_MACHINE_NONE;
    Elf64_Xword m_flags = 0U;

    std::queue<SSectionNode *> m_nodeQueue;

    unsigned int m_dataSize = 0U;
    unsigned int m_numSections = 0U;
    size_t m_stringTableSize = 0U;
    size_t m_totalBinarySize = 0U;
};
} // namespace CLElfLib
