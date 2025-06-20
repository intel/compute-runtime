/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_data.h"

#include <fstream>
#include <string>

namespace NEO {
class AubHelper;
}

#include "aub_services.h"
namespace AubMemDump {
inline constexpr uint32_t rcsRegisterBase = 0x2000;

#ifndef BIT
#define BIT(x) (((uint64_t)1) << (x))
#endif

inline uint32_t computeRegisterOffset(uint32_t mmioBase, uint32_t rcsRegisterOffset) {
    return mmioBase + rcsRegisterOffset - rcsRegisterBase;
}

template <typename Cmd>
inline void setAddress(Cmd &cmd, uint64_t address) {
    cmd.address = address;
}

template <>
inline void setAddress(CmdServicesMemTraceMemoryCompare &cmd, uint64_t address) {
    cmd.address = static_cast<uint32_t>(address);
    cmd.addressHigh = static_cast<uint32_t>(address >> 32);
}

union MiGttEntry {
    struct
    {
        uint64_t present : 1;          //[0]
        uint64_t localMemory : 1;      //[1]
        uint64_t functionNumber : 10;  //[11:2]
        uint64_t physicalAddress : 35; //[46:12]
        uint64_t ignored : 17;         //[63:47]
    } pageConfig;
    uint32_t dwordData[2];
    uint64_t uiData;
};

// Use the latest DeviceValues enumerations available
typedef CmdServicesMemTraceVersion::DeviceValues DeviceValues;
typedef CmdServicesMemTraceVersion::SteppingValues SteppingValues;
typedef CmdServicesMemTraceMemoryWrite::AddressSpaceValues AddressSpaceValues;
typedef CmdServicesMemTraceMemoryWrite::DataTypeHintValues DataTypeHintValues;
typedef CmdServicesMemTraceMemoryDump::TilingValues TilingValues;
typedef CmdServicesMemTraceMemoryWrite::RepeatMemoryValues RepeatMemoryValues;
typedef CmdServicesMemTraceRegisterWrite::MessageSourceIdValues MessageSourceIdValues;
typedef CmdServicesMemTraceRegisterWrite::RegisterSizeValues RegisterSizeValues;
typedef CmdServicesMemTraceRegisterWrite::RegisterSpaceValues RegisterSpaceValues;
typedef CmdServicesMemTraceMemoryPoll::DataSizeValues DataSizeValues;

template <int addressingBitsIn>
struct Traits {
    typedef struct AubStream Stream;

    enum {
        addressingBits = addressingBitsIn,
    };
};

template <int addressingBits>
struct PageTableTraits {
};

template <>
struct PageTableTraits<32> {
    // clang-format off
    enum {
        addressingBits  = 32,
        NUM_OFFSET_BITS = 12,
        NUM_PTE_BITS    =  9,
        NUM_PDE_BITS    =  9,
        NUM_PDP_BITS    = addressingBits - NUM_PDE_BITS - NUM_PTE_BITS - NUM_OFFSET_BITS,
    };

    static const uint64_t physicalMemory;
    static const uint64_t numPTEntries;
    static const uint64_t sizePT;
    static const uint64_t ptBaseAddress;

    static const uint64_t numPDEntries;
    static const uint64_t sizePD;
    static const uint64_t pdBaseAddress;

    static const uint64_t numPDPEntries;
    static const uint64_t sizePDP;
    static const uint64_t pdpBaseAddress;
    // clang-format on
};

template <>
struct PageTableTraits<48> {
    // clang-format off
    enum {
        addressingBits  = 48,
        NUM_OFFSET_BITS = PageTableTraits<32>::NUM_OFFSET_BITS,
        NUM_PTE_BITS    = PageTableTraits<32>::NUM_PTE_BITS,
        NUM_PDE_BITS    = PageTableTraits<32>::NUM_PDE_BITS,
        NUM_PDP_BITS    = PageTableTraits<32>::NUM_PDP_BITS,
        NUM_PML4_BITS   = addressingBits - NUM_PDP_BITS - NUM_PDE_BITS - NUM_PTE_BITS - NUM_OFFSET_BITS
    };

    static const uint64_t physicalMemory;
    static const uint64_t numPTEntries;
    static const uint64_t sizePT;
    static const uint64_t ptBaseAddress;

    static const uint64_t numPDEntries;
    static const uint64_t sizePD;
    static const uint64_t pdBaseAddress;

    static const uint64_t numPDPEntries;
    static const uint64_t sizePDP;
    static const uint64_t pdpBaseAddress;

    static const uint64_t numPML4Entries;
    static const uint64_t sizePML4;
    static const uint64_t pml4BaseAddress;
    // clang-format on
};

template <typename Traits>
struct AubPageTableHelper {
    typedef AubMemDump::PageTableTraits<Traits::addressingBits> PageTableTraits;

    enum {
        addressingBits = Traits::addressingBits
    };

    static inline uint32_t ptrToGGTT(const void *memory) {
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(memory));
    }

    static inline uintptr_t ptrToPPGTT(const void *memory) {
        return reinterpret_cast<uintptr_t>(memory);
    }

    static inline uint64_t getPTEAddress(uint64_t ptIndex) {
        return PageTableTraits::ptBaseAddress + ptIndex * sizeof(uint64_t);
    }

    static inline uint64_t getPDEAddress(uint64_t pdIndex) {
        return PageTableTraits::pdBaseAddress + pdIndex * sizeof(uint64_t);
    }

    static inline uint64_t getPDPAddress(uint64_t pdpIndex) {
        return PageTableTraits::pdpBaseAddress + pdpIndex * sizeof(uint64_t);
    }
};

template <typename Traits>
struct AubPageTableHelper32 : public AubPageTableHelper<Traits>, PageTableTraits<32> {
    typedef AubPageTableHelper<Traits> BaseClass;

    static void createContext(typename Traits::Stream &stream, uint32_t context);
    static uint64_t reserveAddressPPGTT(typename Traits::Stream &stream, uintptr_t gfxAddress,
                                        size_t blockSize, uint64_t physAddress,
                                        uint64_t additionalBits, const NEO::AubHelper &aubHelper);

    static void fixupLRC(uint8_t *pLrc);
};

template <typename Traits>
struct AubPageTableHelper64 : public AubPageTableHelper<Traits>, PageTableTraits<48> {
    typedef AubPageTableHelper<Traits> BaseClass;

    static inline uint64_t getPML4Address(uint64_t pml4Index) {
        return pml4BaseAddress + pml4Index * sizeof(uint64_t);
    }

    static void createContext(typename Traits::Stream &stream, uint32_t context);
    static uint64_t reserveAddressPPGTT(typename Traits::Stream &stream, uintptr_t gfxAddress,
                                        size_t blockSize, uint64_t physAddress,
                                        uint64_t additionalBits, const NEO::AubHelper &aubHelper);

    static void fixupLRC(uint8_t *pLrc);
};

template <typename TraitsIn>
struct AubDump : public std::conditional<TraitsIn::addressingBits == 32, AubPageTableHelper32<TraitsIn>, AubPageTableHelper64<TraitsIn>>::type {
    using Traits = TraitsIn;
    using AddressType = typename std::conditional<Traits::addressingBits == 32, uint32_t, uint64_t>::type;
    using BaseHelper = typename std::conditional<Traits::addressingBits == 32, AubPageTableHelper32<Traits>, AubPageTableHelper64<Traits>>::type;
    using Stream = typename Traits::Stream;

    union MiContextDescriptorReg {
        struct {
            uint64_t valid : 1;                  //[0]
            uint64_t forcePageDirRestore : 1;    //[1]
            uint64_t forceRestore : 1;           //[2]
            uint64_t legacy : 1;                 //[3]
            uint64_t aDor64bitSupport : 1;       //[4] Selects 64-bit PPGTT in Legacy mode
            uint64_t llcCoherencySupport : 1;    //[5]
            uint64_t faultSupport : 2;           //[7:6]
            uint64_t privilegeAccessOrPPGTT : 1; //[8] Selects PPGTT in Legacy mode
            uint64_t functionType : 3;           //[11:9]
            uint64_t logicalRingCtxAddress : 20; //[31:12]
            uint64_t contextID : 32;             //[63:32]
        } sData;
        uint32_t ulData[2];
        uint64_t qwordData[2 / 2];
    };
};

struct LrcaHelper {
    LrcaHelper(uint32_t base) : mmioBase(base) {
    }

    int aubHintLRCA = DataTypeHintValues::TraceNotype;
    int aubHintCommandBuffer = DataTypeHintValues::TraceCommandBuffer;
    int aubHintBatchBuffer = DataTypeHintValues::TraceBatchBuffer;

    std::string name = "XCS";
    uint32_t mmioBase = 0;

    size_t sizeLRCA = 0x2000;
    uint32_t alignLRCA = 0x1000;
    uint32_t offsetContext = 0x1000;

    uint32_t offsetLRI0 = 0x01 * sizeof(uint32_t);
    uint32_t numRegsLRI0 = 14;

    uint32_t numNoops0 = 3;

    uint32_t offsetLRI1 = offsetLRI0 + (1 + numRegsLRI0 * 2 + numNoops0) * sizeof(uint32_t); // offsetLRI == 0x21 * sizeof(uint32_t);
    uint32_t numRegsLRI1 = 9;

    uint32_t numNoops1 = 13;

    uint32_t offsetLRI2 = offsetLRI1 + (1 + numRegsLRI1 * 2 + numNoops1) * sizeof(uint32_t); // offsetLR2 == 0x41 * sizeof(uint32_t);
    uint32_t numRegsLRI2 = 1;

    uint32_t offsetRingRegisters = offsetLRI0 + (3 * sizeof(uint32_t));
    uint32_t offsetRingHead = 0x0 * sizeof(uint32_t);
    uint32_t offsetRingTail = 0x2 * sizeof(uint32_t);
    uint32_t offsetRingBase = 0x4 * sizeof(uint32_t);
    uint32_t offsetRingCtrl = 0x6 * sizeof(uint32_t);

    uint32_t offsetPageTableRegisters = offsetLRI1 + (3 * sizeof(uint32_t));
    uint32_t offsetPDP0 = 0xc * sizeof(uint32_t);
    uint32_t offsetPDP1 = 0x8 * sizeof(uint32_t);
    uint32_t offsetPDP2 = 0x4 * sizeof(uint32_t);
    uint32_t offsetPDP3 = 0x0 * sizeof(uint32_t);

    void initialize(void *pLRCIn) const;
    void setRingHead(void *pLRCIn, uint32_t ringHead) const;
    void setRingTail(void *pLRCIn, uint32_t ringTail) const;
    void setRingBase(void *pLRCIn, uint32_t ringBase) const;
    void setRingCtrl(void *pLRCIn, uint32_t ringCtrl) const;

    void setPDP0(void *pLRCIn, uint64_t address) const;
    void setPDP1(void *pLRCIn, uint64_t address) const;
    void setPDP2(void *pLRCIn, uint64_t address) const;
    void setPDP3(void *pLRCIn, uint64_t address) const;

    void setPML4(void *pLRCIn, uint64_t address) const;
    MOCKABLE_VIRTUAL void setContextSaveRestoreFlags(uint32_t &value) const;
};

struct LrcaHelperRcs : public LrcaHelper {
    LrcaHelperRcs(uint32_t base) : LrcaHelper(base) {
        aubHintLRCA = DataTypeHintValues::TraceLogicalRingContextRcs;
        aubHintCommandBuffer = DataTypeHintValues::TraceCommandBufferPrimary;
        aubHintBatchBuffer = DataTypeHintValues::TraceBatchBufferPrimary;
        sizeLRCA = 0x11000;
        name = "RCS";
    }
};

struct LrcaHelperBcs : public LrcaHelper {
    LrcaHelperBcs(uint32_t base) : LrcaHelper(base) {
        aubHintLRCA = DataTypeHintValues::TraceLogicalRingContextBcs;
        aubHintCommandBuffer = DataTypeHintValues::TraceCommandBufferBlt;
        aubHintBatchBuffer = DataTypeHintValues::TraceBatchBufferBlt;
        name = "BCS";
    }
};

struct LrcaHelperVcs : public LrcaHelper {
    LrcaHelperVcs(uint32_t base) : LrcaHelper(base) {
        aubHintLRCA = DataTypeHintValues::TraceLogicalRingContextVcs;
        aubHintCommandBuffer = DataTypeHintValues::TraceCommandBufferMfx;
        aubHintBatchBuffer = DataTypeHintValues::TraceBatchBufferMfx;
        name = "VCS";
    }
};

struct LrcaHelperVecs : public LrcaHelper {
    LrcaHelperVecs(uint32_t base) : LrcaHelper(base) {
        aubHintLRCA = DataTypeHintValues::TraceLogicalRingContextVecs;
        name = "VECS";
    }
};

struct LrcaHelperCcs : public LrcaHelper {
    LrcaHelperCcs(uint32_t base) : LrcaHelper(base) {
        aubHintLRCA = DataTypeHintValues::TraceLogicalRingContextCcs;
        name = "CCS";
    }
};

struct LrcaHelperLinkBcs : public LrcaHelperBcs {
    LrcaHelperLinkBcs(uint32_t base, uint32_t engineId) : LrcaHelperBcs(base) {
        name = "BCS" + std::to_string(engineId);
    }
};

struct LrcaHelperCccs : public LrcaHelper {
    LrcaHelperCccs(uint32_t base) : LrcaHelper(base) {
        name = "CCCS";
    }
};

extern const uint64_t pageMask;
} // namespace AubMemDump
