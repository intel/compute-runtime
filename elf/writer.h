/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "types.h"
#include <queue>
#include <string>

namespace CLElfLib {
struct SSectionNode {
    E_SH_TYPE type = E_SH_TYPE::SH_TYPE_NULL;
    E_SH_FLAG flag = E_SH_FLAG::SH_FLAG_NONE;
    std::string name;
    std::string data;
    uint32_t dataSize = 0u;

    SSectionNode() = default;

    template <typename T1, typename T2>
    SSectionNode(E_SH_TYPE type, E_SH_FLAG flag, T1 &&name, T2 &&data, uint32_t dataSize)
        : type(type), flag(flag), name(std::forward<T1>(name)), data(std::forward<T2>(data)), dataSize(dataSize) {}

    ~SSectionNode() = default;
};

/******************************************************************************\

 Class:         CElfWriter

 Description:   Class to provide simpler interaction with the ELF standard
                binary object.  SElf64Header defines the ELF header type and
                SElf64SectionHeader defines the section header type.

\******************************************************************************/
class CElfWriter {
  public:
    CElfWriter(
        E_EH_TYPE type,
        E_EH_MACHINE machine,
        Elf64_Xword flag) : type(type), machine(machine), flag(flag) {
        addSection(SSectionNode());
    }

    ~CElfWriter() {}

    template <typename T>
    void addSection(T &&sectionNode) {
        size_t nameSize = 0;
        uint32_t dataSize = 0;

        nameSize = sectionNode.name.size() + 1u;
        dataSize = sectionNode.dataSize;

        // push the node onto the queue
        nodeQueue.push(std::forward<T>(sectionNode));

        // increment the sizes for each section
        this->dataSize += dataSize;
        stringTableSize += nameSize;
        numSections++;
    }

    void resolveBinary(ElfBinaryStorage &binary);

    size_t getTotalBinarySize() {
        return sizeof(SElf64Header) +
               ((numSections + 1) * sizeof(SElf64SectionHeader)) + // +1 to account for string table entry
               dataSize + stringTableSize;
    }

  protected:
    E_EH_TYPE type = E_EH_TYPE::EH_TYPE_NONE;
    E_EH_MACHINE machine = E_EH_MACHINE::EH_MACHINE_NONE;
    Elf64_Xword flag = 0U;

    std::queue<SSectionNode> nodeQueue;

    uint32_t dataSize = 0U;
    uint32_t numSections = 0U;
    size_t stringTableSize = 0U;

    void patchElfHeader(SElf64Header &pBinary);
};
} // namespace CLElfLib
