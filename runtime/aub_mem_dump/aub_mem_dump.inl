/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_mem_dump.h"
#include "runtime/aub/aub_helper.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/memory_constants.h"
#include <algorithm>
#include <cstring>

namespace AubMemDump {

template <typename Traits>
void AubPageTableHelper32<Traits>::fixupLRC(uint8_t *pLRC) {
    uint32_t pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x600) >> 32;
    *(uint32_t *)(pLRC + 0x1094) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x600) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x109c) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x400) >> 32;
    *(uint32_t *)(pLRC + 0x10a4) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x400) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x10ac) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x200) >> 32;
    *(uint32_t *)(pLRC + 0x10b4) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0x200) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x10bc) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0) >> 32;
    *(uint32_t *)(pLRC + 0x10c4) = pdAddress;
    pdAddress = BaseClass::getPDEAddress(0) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x10cc) = pdAddress;
}

template <typename Traits>
void AubPageTableHelper64<Traits>::fixupLRC(uint8_t *pLRC) {
    uint32_t pml4Address = getPML4Address(0) >> 32;
    *(uint32_t *)(pLRC + 0x10c4) = pml4Address;
    pml4Address = getPML4Address(0) & 0xffffffff;
    *(uint32_t *)(pLRC + 0x10cc) = pml4Address;
}

// Write a block of memory to a given address space using an optional hint
template <typename Traits>
void AubDump<Traits>::addMemoryWrite(typename Traits::Stream &stream, uint64_t addr, const void *memory, size_t sizeRemaining, int addressSpace, int hint) {
    // We can only dump a relatively small amount per CmdServicesMemTraceMemoryWrite
    auto sizeMemoryWriteHeader = sizeof(CmdServicesMemTraceMemoryWrite) - sizeof(CmdServicesMemTraceMemoryWrite::data);
    auto blockSizeMax = g_dwordCountMax * sizeof(uint32_t) - sizeMemoryWriteHeader;

    if (hint == CmdServicesMemTraceMemoryWrite::DataTypeHintValues::TraceLogicalRingContextRcs ||
        hint == CmdServicesMemTraceMemoryWrite::DataTypeHintValues::TraceLogicalRingContextBcs ||
        hint == CmdServicesMemTraceMemoryWrite::DataTypeHintValues::TraceLogicalRingContextVcs ||
        hint == CmdServicesMemTraceMemoryWrite::DataTypeHintValues::TraceLogicalRingContextVecs) {
        DEBUG_BREAK_IF(sizeRemaining <= 0x10cc);
        uint8_t *pLRC = reinterpret_cast<uint8_t *>(const_cast<void *>(memory));
        BaseHelper::fixupLRC(pLRC);
    }

    // loop to dump all of the blocks
    while (sizeRemaining > 0) {
        auto sizeThisIteration = std::min(blockSizeMax, sizeRemaining);
        stream.writeMemory(addr, memory, sizeThisIteration, addressSpace, hint);

        sizeRemaining -= sizeThisIteration;
        memory = (uint8_t *)memory + sizeThisIteration;
        addr += sizeThisIteration;
    }
}

// Reserve memory in the GGTT.
template <typename Traits>
uint64_t AubDump<Traits>::reserveAddress(typename Traits::Stream &stream, uint32_t addr, size_t size, unsigned int addressSpace, uint64_t physStart, AubGTTData data) {
    auto startPage = addr & g_pageMask;
    auto endPage = (addr + size - 1) & g_pageMask;
    auto numPages = (uint32_t)(((endPage - startPage) / 4096) + 1);

    // Can only handle 16 bits of dwordCount.
    DEBUG_BREAK_IF(!(numPages > 0 && (numPages + 4) < 65536));
    auto gttTableOffset = static_cast<uint32_t>((((uint32_t)startPage) / 4096) * sizeof(MiGttEntry));

    // Write header
    {
        typedef AubMemDump::CmdServicesMemTraceMemoryWrite CmdServicesMemTraceMemoryWrite;
        stream.writeMemoryWriteHeader(gttTableOffset, numPages * sizeof(AubMemDump::MiGttEntry), addressSpace, CmdServicesMemTraceMemoryWrite::DataTypeHintValues::TraceNotype);
    }

    uint64_t physAddress = physStart;
    while (startPage <= endPage) {
        MiGttEntry entry;
        setGttEntry(entry, physAddress, data);

        stream.writeGTT(gttTableOffset, entry.uiData);
        gttTableOffset += sizeof(entry);

        physAddress += 4096;
        startPage += 4096;
    }

    return physStart;
}

template <typename Traits>
uint64_t AubDump<Traits>::reserveAddressGGTT(typename Traits::Stream &stream, uint32_t addr, size_t size, uint64_t physStart, AubGTTData data) {
    return AubDump<Traits>::reserveAddress(stream, addr, size, AddressSpaceValues::TraceGttEntry, physStart, data);
}

template <typename Traits>
uint64_t AubDump<Traits>::reserveAddressGGTT(typename Traits::Stream &stream, const void *memory, size_t size, uint64_t physStart, AubGTTData data) {
    auto gfxAddress = BaseHelper::ptrToGGTT(memory);
    return AubDump<Traits>::reserveAddress(stream, gfxAddress, size, AddressSpaceValues::TraceGttEntry, physStart, data);
}

template <typename Traits>
void AubDump<Traits>::reserveAddressGGTTAndWriteMmeory(typename Traits::Stream &stream, uintptr_t gfxAddress,
                                                       const void *memory, uint64_t physAddress,
                                                       size_t size, size_t offset,
                                                       uint64_t additionalBits, const OCLRT::AubHelper &aubHelper) {
    auto vmAddr = (gfxAddress + offset) & ~(MemoryConstants::pageSize - 1);
    auto pAddr = physAddress & ~(MemoryConstants::pageSize - 1);

    AubDump<Traits>::reserveAddressPPGTT(stream, vmAddr, MemoryConstants::pageSize, pAddr, additionalBits, aubHelper);

    int hint = OCLRT::AubHelper::getMemTrace(additionalBits);

    AubDump<Traits>::addMemoryWrite(stream, physAddress,
                                    reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(memory) + offset),
                                    size, hint);
}

template <typename Traits>
void AubDump<Traits>::setGttEntry(MiGttEntry &entry, uint64_t address, AubGTTData data) {
    entry.uiData = 0;
    entry.pageConfig.PhysicalAddress = address / 4096;
    entry.pageConfig.Present = data.present;
    entry.pageConfig.LocalMemory = data.localMemory;
}

template <typename Traits>
uint64_t AubPageTableHelper32<Traits>::reserveAddressPPGTT(typename Traits::Stream &stream, uintptr_t gfxAddress,
                                                           size_t blockSize, uint64_t physAddress,
                                                           uint64_t additionalBits, const OCLRT::AubHelper &aubHelper) {
    auto startAddress = gfxAddress;
    auto endAddress = gfxAddress + blockSize - 1;

    auto startPTE = startAddress >> 12;
    auto endPTE = endAddress >> 12;
    auto numPTEs = endPTE - startPTE + 1;

    auto startPDE = startPTE >> 9;
    auto endPDE = endPTE >> 9;
    auto numPDEs = endPDE - startPDE + 1;

    // Process the PD entries
    bool writePDE = true;
    if (writePDE) {
        auto startAddress = BaseClass::getPDEAddress(startPDE);
        auto addressSpace = aubHelper.getMemTraceForPdEntry();
        auto hint = aubHelper.getDataHintForPdEntry();

        stream.writeMemoryWriteHeader(startAddress, numPDEs * sizeof(uint64_t),
                                      addressSpace, hint);

        auto currPDE = startPDE;
        auto physPage = BaseClass::getPTEAddress(startPTE) & g_pageMask;
        while (currPDE <= endPDE) {
            auto pde = physPage | OCLRT::AubHelper::getPTEntryBits(additionalBits);

            stream.writePTE(startAddress, pde, addressSpace);
            startAddress += sizeof(pde);

            physPage += 4096;
            currPDE++;
        }
    }

    // Process the PT entries
    bool writePTE = true;
    if (writePTE) {
        auto startAddress = BaseClass::getPTEAddress(startPTE);
        auto addressSpace = aubHelper.getMemTraceForPtEntry();
        auto hint = aubHelper.getDataHintForPtEntry();

        stream.writeMemoryWriteHeader(startAddress, numPTEs * sizeof(uint64_t),
                                      addressSpace, hint);

        auto currPTE = startPTE;
        auto physPage = physAddress & g_pageMask;
        while (currPTE <= endPTE) {
            auto pte = physPage | additionalBits;

            stream.writePTE(startAddress, pte, addressSpace);
            startAddress += sizeof(pte);

            physPage += 4096;
            currPTE++;
        }
    }

    return physAddress;
}

template <typename Traits>
uint64_t AubPageTableHelper64<Traits>::reserveAddressPPGTT(typename Traits::Stream &stream, uintptr_t gfxAddress,
                                                           size_t blockSize, uint64_t physAddress,
                                                           uint64_t additionalBits, const OCLRT::AubHelper &aubHelper) {
    auto startAddress = gfxAddress;
    auto endAddress = gfxAddress + blockSize - 1;

    auto startPTE = startAddress >> 12;
    auto endPTE = endAddress >> 12;
    auto numPTEs = endPTE - startPTE + 1;

    auto startPDE = startPTE >> 9;
    auto endPDE = endPTE >> 9;
    auto numPDEs = endPDE - startPDE + 1;

    auto startPDP = startPDE >> 9;
    auto endPDP = endPDE >> 9;
    auto numPDPs = endPDP - startPDP + 1;

    auto startPML4 = startPDP >> 9;
    auto endPML4 = endPDP >> 9;
    auto numPML4s = endPML4 - startPML4 + 1;

    // Process the PML4 entries
    bool writePML4 = true;
    if (writePML4) {
        auto startAddress = getPML4Address(startPML4);
        auto addressSpace = aubHelper.getMemTraceForPml4Entry();
        auto hint = aubHelper.getDataHintForPml4Entry();

        stream.writeMemoryWriteHeader(startAddress, numPML4s * sizeof(uint64_t),
                                      addressSpace, hint);

        auto currPML4 = startPML4;
        auto physPage = BaseClass::getPDPAddress(startPDP) & g_pageMask;
        while (currPML4 <= endPML4) {
            auto pml4 = physPage | OCLRT::AubHelper::getPTEntryBits(additionalBits);

            stream.writePTE(startAddress, pml4, addressSpace);
            startAddress += sizeof(pml4);

            physPage += 4096;
            currPML4++;
        }
    }

    // Process the PDP entries
    bool writePDPE = true;
    if (writePDPE) {
        auto startAddress = BaseClass::getPDPAddress(startPDP);
        auto addressSpace = aubHelper.getMemTraceForPdpEntry();
        auto hint = aubHelper.getDataHintForPdpEntry();

        stream.writeMemoryWriteHeader(startAddress, numPDPs * sizeof(uint64_t),
                                      addressSpace, hint);

        auto currPDP = startPDP;
        auto physPage = BaseClass::getPDEAddress(startPDE) & g_pageMask;
        while (currPDP <= endPDP) {
            auto pdp = physPage | OCLRT::AubHelper::getPTEntryBits(additionalBits);

            stream.writePTE(startAddress, pdp, addressSpace);
            startAddress += sizeof(pdp);

            physPage += 4096;
            currPDP++;
        }
    }

    // Process the PD entries
    bool writePDE = true;
    if (writePDE) {
        auto startAddress = BaseClass::getPDEAddress(startPDE);
        auto addressSpace = aubHelper.getMemTraceForPdEntry();
        auto hint = aubHelper.getDataHintForPdEntry();

        stream.writeMemoryWriteHeader(startAddress, numPDEs * sizeof(uint64_t),
                                      addressSpace, hint);

        auto currPDE = startPDE;
        auto physPage = BaseClass::getPTEAddress(startPTE) & g_pageMask;
        while (currPDE <= endPDE) {
            auto pde = physPage | OCLRT::AubHelper::getPTEntryBits(additionalBits);

            stream.writePTE(startAddress, pde, addressSpace);
            startAddress += sizeof(pde);

            physPage += 4096;
            currPDE++;
        }
    }

    // Process the PT entries
    bool writePTE = true;
    if (writePTE) {
        auto startAddress = BaseClass::getPTEAddress(startPTE);
        auto addressSpace = aubHelper.getMemTraceForPtEntry();
        auto hint = aubHelper.getDataHintForPtEntry();

        stream.writeMemoryWriteHeader(startAddress, numPTEs * sizeof(uint64_t),
                                      addressSpace, hint);

        auto currPTE = startPTE;
        auto physPage = physAddress & g_pageMask;
        while (currPTE <= endPTE) {
            auto pte = physPage | additionalBits;

            stream.writePTE(startAddress, pte, addressSpace);
            startAddress += sizeof(pte);
            OCLRT::AubHelper::checkPTEAddress(startAddress);

            physPage += 4096;
            currPTE++;
        }
    }

    return physAddress;
}

template <typename Traits>
void AubPageTableHelper32<Traits>::createContext(typename Traits::Stream &stream, uint32_t context) {
    AubPpgttContextCreate cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Header.Type = 0x7;
    cmd.Header.Opcode = 0x1;
    cmd.Header.SubOp = 0x14;
    cmd.Header.DwordLength = ((sizeof(cmd) - sizeof(cmd.Header)) / sizeof(uint32_t)) - 1;
    cmd.Handle = context;
    cmd.AdvancedContext = false;

    cmd.SixtyFourBit = 0;
    cmd.PageDirPointer[0] = BaseClass::getPDEAddress(0x000);
    cmd.PageDirPointer[1] = BaseClass::getPDEAddress(0x200);
    cmd.PageDirPointer[2] = BaseClass::getPDEAddress(0x400);
    cmd.PageDirPointer[3] = BaseClass::getPDEAddress(0x600);

    stream.createContext(cmd);
}

template <typename Traits>
void AubPageTableHelper64<Traits>::createContext(typename Traits::Stream &stream, uint32_t context) {
    AubPpgttContextCreate cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.Header.Type = 0x7;
    cmd.Header.Opcode = 0x1;
    cmd.Header.SubOp = 0x14;
    cmd.Header.DwordLength = ((sizeof(cmd) - sizeof(cmd.Header)) / sizeof(uint32_t)) - 1;
    cmd.Handle = context;
    cmd.AdvancedContext = false;

    cmd.SixtyFourBit = 1;
    cmd.PageDirPointer[0] = getPML4Address(0);

    stream.createContext(cmd);
}

} // namespace AubMemDump
