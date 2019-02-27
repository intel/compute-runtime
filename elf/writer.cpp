/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "writer.h"

#include <cstring>

// Need for linux compatibility with memcpy_s
#include "runtime/helpers/string.h"

namespace CLElfLib {
void CElfWriter::resolveBinary(ElfBinaryStorage &binary) {
    SElf64SectionHeader *curSectionHeader = nullptr;
    char *data = nullptr;
    char *stringTable = nullptr;
    char *curString = nullptr;

    if (binary.size() < getTotalBinarySize()) {
        binary.resize(getTotalBinarySize());
    }

    // get a pointer to the first section header
    curSectionHeader = reinterpret_cast<SElf64SectionHeader *>(binary.data() + sizeof(SElf64Header));

    // get a pointer to the data
    data = binary.data() +
           sizeof(SElf64Header) +
           ((numSections + 1) * sizeof(SElf64SectionHeader)); // +1 to account for string table entry

    // get a pointer to the string table
    stringTable = binary.data() + sizeof(SElf64Header) +
                  ((numSections + 1) * sizeof(SElf64SectionHeader)) + // +1 to account for string table entry
                  dataSize;

    curString = stringTable;

    // Walk through the section nodes
    while (nodeQueue.empty() == false) {
        // Copy data into the section header
        const auto &queueFront = nodeQueue.front();

        curSectionHeader->Type = queueFront.type;
        curSectionHeader->Flags = queueFront.flag;
        curSectionHeader->DataSize = queueFront.dataSize;
        curSectionHeader->DataOffset = data - binary.data();
        curSectionHeader->Name = static_cast<Elf64_Word>(curString - stringTable);
        curSectionHeader = reinterpret_cast<SElf64SectionHeader *>(reinterpret_cast<unsigned char *>(curSectionHeader) + sizeof(SElf64SectionHeader));

        // copy the data, move the data pointer
        memcpy_s(data, queueFront.dataSize, queueFront.data.c_str(), queueFront.dataSize);
        data += queueFront.dataSize;

        // copy the name into the string table, move the string pointer
        if (queueFront.name.size() > 0) {
            memcpy_s(curString, queueFront.name.size(), queueFront.name.c_str(), queueFront.name.size());
            curString += queueFront.name.size();
        }
        *(curString++) = '\0'; // NOLINT

        nodeQueue.pop();
    }

    // add the string table section header
    SElf64SectionHeader stringSectionHeader = {0};
    stringSectionHeader.Type = E_SH_TYPE::SH_TYPE_STR_TBL;
    stringSectionHeader.Flags = E_SH_FLAG::SH_FLAG_NONE;
    stringSectionHeader.DataOffset = stringTable - &binary[0];
    stringSectionHeader.DataSize = stringTableSize;
    stringSectionHeader.Name = 0;

    // Copy into the last section header
    memcpy_s(curSectionHeader, sizeof(SElf64SectionHeader),
             &stringSectionHeader, sizeof(SElf64SectionHeader));

    // Add to our section number
    numSections++;

    // patch up the ELF header
    patchElfHeader(*reinterpret_cast<SElf64Header *>(binary.data()));
}

void CElfWriter::patchElfHeader(SElf64Header &binary) {
    // Setup the identity
    binary.Identity[ELFConstants::idIdxMagic0] = ELFConstants::elfMag0;
    binary.Identity[ELFConstants::idIdxMagic1] = ELFConstants::elfMag1;
    binary.Identity[ELFConstants::idIdxMagic2] = ELFConstants::elfMag2;
    binary.Identity[ELFConstants::idIdxMagic3] = ELFConstants::elfMag3;
    binary.Identity[ELFConstants::idIdxClass] = static_cast<uint32_t>(E_EH_CLASS::EH_CLASS_64);
    binary.Identity[ELFConstants::idIdxVersion] = static_cast<uint32_t>(E_EHT_VERSION::EH_VERSION_CURRENT);

    // Add other non-zero info
    binary.Type = type;
    binary.Machine = machine;
    binary.Flags = static_cast<uint32_t>(flag);
    binary.ElfHeaderSize = static_cast<uint32_t>(sizeof(SElf64Header));
    binary.SectionHeaderEntrySize = static_cast<uint32_t>(sizeof(SElf64SectionHeader));
    binary.NumSectionHeaderEntries = numSections;
    binary.SectionHeadersOffset = static_cast<uint32_t>(sizeof(SElf64Header));
    binary.SectionNameTableIndex = numSections - 1; // last index
}
} // namespace CLElfLib
