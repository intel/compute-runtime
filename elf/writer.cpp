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
#include "writer.h"
#include "runtime/helpers/string.h"
#include <cstring>

namespace CLElfLib {
/******************************************************************************\
 Constructor: CElfWriter::CElfWriter
\******************************************************************************/
CElfWriter::CElfWriter(
    E_EH_TYPE type,
    E_EH_MACHINE machine,
    Elf64_Xword flags) {
    m_type = type;
    m_machine = machine;
    m_flags = flags;
}

/******************************************************************************\
 Destructor: CElfWriter::~CElfWriter
\******************************************************************************/
CElfWriter::~CElfWriter() {
    SSectionNode *pNode = NULL;

    // Walk through the section nodes
    while (m_nodeQueue.empty() == false) {
        pNode = m_nodeQueue.front();
        m_nodeQueue.pop();

        // delete the node and it's data
        if (pNode) {
            if (pNode->pData) {
                delete[] pNode->pData;
                pNode->pData = NULL;
            }

            delete pNode;
            pNode = nullptr;
        }
    }
}

/******************************************************************************\
 Member Function: CElfWriter::Create
\******************************************************************************/
CElfWriter *CElfWriter::create(
    E_EH_TYPE type,
    E_EH_MACHINE machine,
    Elf64_Xword flags) {
    CElfWriter *pWriter = new CElfWriter(type, machine, flags);

    if (!pWriter->initialize()) {
        destroy(pWriter);
    }

    return pWriter;
}

/******************************************************************************\
 Member Function: CElfWriter::Delete
\******************************************************************************/
void CElfWriter::destroy(
    CElfWriter *&pWriter) {
    if (pWriter) {
        delete pWriter;
        pWriter = NULL;
    }
}

/******************************************************************************\
 Member Function: CElfWriter::AddSection
\******************************************************************************/
bool CElfWriter::addSection(
    SSectionNode *pSectionNode) {
    bool retVal = true;
    SSectionNode *pNode = NULL;
    size_t nameSize = 0;
    unsigned int dataSize = 0;

    // The section header must be non-NULL
    if (pSectionNode) {
        pNode = new SSectionNode();
        if (!pNode)
            return false;
    } else {
        return false;
    }

    pNode->Flags = pSectionNode->Flags;
    pNode->Type = pSectionNode->Type;

    nameSize = pSectionNode->Name.size() + 1;
    dataSize = pSectionNode->DataSize;

    pNode->Name = pSectionNode->Name;

    // ok to have NULL data
    if (dataSize > 0) {
        pNode->pData = new char[dataSize];
        if (pNode->pData) {
            memcpy_s(pNode->pData, dataSize, pSectionNode->pData, dataSize);
            pNode->DataSize = dataSize;
        } else {
            retVal = false;
        }
    }

    if (retVal) {
        // push the node onto the queue
        m_nodeQueue.push(pNode);

        // increment the sizes for each section
        m_dataSize += dataSize;
        m_stringTableSize += nameSize;
        m_numSections++;
    } else {
        delete pNode;
        pNode = nullptr;
    }

    return retVal;
}

/******************************************************************************\
 Member Function: CElfWriter::ResolveBinary
\******************************************************************************/
bool CElfWriter::resolveBinary(
    char *const pBinary,
    size_t &binarySize) {
    bool retVal = true;
    SSectionNode *pNode = NULL;
    SElf64SectionHeader *pCurSectionHeader = NULL;
    char *pData = NULL;
    char *pStringTable = NULL;
    char *pCurString = NULL;

    m_totalBinarySize =
        sizeof(SElf64Header) +
        ((m_numSections + 1) * sizeof(SElf64SectionHeader)) + // +1 to account for string table entry
        m_dataSize +
        m_stringTableSize;

    if (pBinary) {
        // get a pointer to the first section header
        pCurSectionHeader = (SElf64SectionHeader *)(pBinary + sizeof(SElf64Header));

        // get a pointer to the data
        pData = pBinary +
                sizeof(SElf64Header) +
                ((m_numSections + 1) * sizeof(SElf64SectionHeader)); // +1 to account for string table entry

        // get a pointer to the string table
        pStringTable = pBinary + sizeof(SElf64Header) +
                       ((m_numSections + 1) * sizeof(SElf64SectionHeader)) + // +1 to account for string table entry
                       m_dataSize;

        pCurString = pStringTable;

        // Walk through the section nodes
        while (m_nodeQueue.empty() == false) {
            pNode = m_nodeQueue.front();

            if (pNode) {
                m_nodeQueue.pop();

                // Copy data into the section header
                memset(pCurSectionHeader, 0, sizeof(SElf64SectionHeader));
                pCurSectionHeader->Type = pNode->Type;
                pCurSectionHeader->Flags = pNode->Flags;
                pCurSectionHeader->DataSize = pNode->DataSize;
                pCurSectionHeader->DataOffset = pData - pBinary;
                pCurSectionHeader->Name = (Elf64_Word)(pCurString - pStringTable);
                pCurSectionHeader = (SElf64SectionHeader *)((unsigned char *)pCurSectionHeader + sizeof(SElf64SectionHeader));

                // copy the data, move the data pointer
                memcpy_s(pData, pNode->DataSize, pNode->pData, pNode->DataSize);
                pData += pNode->DataSize;

                // copy the name into the string table, move the string pointer
                if (pNode->Name.size() > 0) {
                    memcpy_s(pCurString, pNode->Name.size(), pNode->Name.c_str(), pNode->Name.size());
                    pCurString += pNode->Name.size();
                }
                *(pCurString++) = '\0'; // NOLINT

                // delete the node and it's data
                if (pNode->pData) {
                    delete[] pNode->pData;
                    pNode->pData = NULL;
                }

                delete pNode;
                pNode = nullptr;
            }
        }

        // add the string table section header
        SElf64SectionHeader stringSectionHeader = {0};
        stringSectionHeader.Type = SH_TYPE_STR_TBL;
        stringSectionHeader.Flags = 0;
        stringSectionHeader.DataOffset = pStringTable - pBinary;
        stringSectionHeader.DataSize = m_stringTableSize;
        stringSectionHeader.Name = 0;

        // Copy into the last section header
        memcpy_s(pCurSectionHeader, sizeof(SElf64SectionHeader),
                 &stringSectionHeader, sizeof(SElf64SectionHeader));

        // Add to our section number
        m_numSections++;

        // patch up the ELF header
        retVal = patchElfHeader(pBinary);
    }

    if (retVal) {
        binarySize = m_totalBinarySize;
    }

    return retVal;
}

/******************************************************************************\
 Member Function: CElfWriter::Initialize
\******************************************************************************/
bool CElfWriter::initialize() {
    SSectionNode emptySection;

    // Add an empty section 0 (points to "no-bits")
    return addSection(&emptySection);
}

/******************************************************************************\
 Member Function: CElfWriter::PatchElfHeader
\******************************************************************************/
bool CElfWriter::patchElfHeader(char *const pBinary) {
    SElf64Header *pElfHeader = (SElf64Header *)pBinary;

    if (pElfHeader) {
        // Setup the identity
        memset(pElfHeader, 0x00, sizeof(SElf64Header));
        pElfHeader->Identity[ID_IDX_MAGIC0] = ELF_MAG0;
        pElfHeader->Identity[ID_IDX_MAGIC1] = ELF_MAG1;
        pElfHeader->Identity[ID_IDX_MAGIC2] = ELF_MAG2;
        pElfHeader->Identity[ID_IDX_MAGIC3] = ELF_MAG3;
        pElfHeader->Identity[ID_IDX_CLASS] = EH_CLASS_64;
        pElfHeader->Identity[ID_IDX_VERSION] = EH_VERSION_CURRENT;

        // Add other non-zero info
        pElfHeader->Type = m_type;
        pElfHeader->Machine = m_machine;
        pElfHeader->Flags = (unsigned int)m_flags;
        pElfHeader->ElfHeaderSize = sizeof(SElf64Header);
        pElfHeader->SectionHeaderEntrySize = sizeof(SElf64SectionHeader);
        pElfHeader->NumSectionHeaderEntries = (Elf64_Short)m_numSections;
        pElfHeader->SectionHeadersOffset = (unsigned int)(sizeof(SElf64Header));
        pElfHeader->SectionNameTableIndex = m_numSections - 1; // last index

        return true;
    }

    return false;
}

} // namespace CLElfLib
