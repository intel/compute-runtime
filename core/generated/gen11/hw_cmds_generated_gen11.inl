/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma pack(1)
typedef struct tagBINDING_TABLE_STATE {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 5);
            uint32_t SurfaceStatePointer : BITFIELD_RANGE(6, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagPATCH_CONSTANTS {
        SURFACESTATEPOINTER_BYTEOFFSET = 0x0,
        SURFACESTATEPOINTER_INDEX = 0x0,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
    }
    static tagBINDING_TABLE_STATE sInit(void) {
        BINDING_TABLE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline const uint32_t &getRawData(const uint32_t index) const {
        DEBUG_BREAK_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    typedef enum tagSURFACESTATEPOINTER {
        SURFACESTATEPOINTER_BIT_SHIFT = 0x6,
        SURFACESTATEPOINTER_ALIGN_SIZE = 0x40,
    } SURFACESTATEPOINTER;
    inline void setSurfaceStatePointer(const uint64_t value) {
        DEBUG_BREAK_IF(value >= 0x100000000);
        TheStructure.Common.SurfaceStatePointer = (uint32_t)value >> SURFACESTATEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getSurfaceStatePointer(void) const {
        return (TheStructure.Common.SurfaceStatePointer << SURFACESTATEPOINTER_BIT_SHIFT);
    }
} BINDING_TABLE_STATE;
STATIC_ASSERT(4 == sizeof(BINDING_TABLE_STATE));
typedef struct tagGPGPU_WALKER {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t PredicateEnable : BITFIELD_RANGE(8, 8);
            uint32_t Reserved_9 : BITFIELD_RANGE(9, 9);
            uint32_t IndirectParameterEnable : BITFIELD_RANGE(10, 10);
            uint32_t Reserved_11 : BITFIELD_RANGE(11, 15);
            uint32_t Subopcode : BITFIELD_RANGE(16, 23);
            uint32_t MediaCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t Pipeline : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t InterfaceDescriptorOffset : BITFIELD_RANGE(0, 5);
            uint32_t Reserved_38 : BITFIELD_RANGE(6, 31);
            uint32_t IndirectDataLength : BITFIELD_RANGE(0, 16);
            uint32_t Reserved_81 : BITFIELD_RANGE(17, 31);
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 5);
            uint32_t IndirectDataStartAddress : BITFIELD_RANGE(6, 31);
            uint32_t ThreadWidthCounterMaximum : BITFIELD_RANGE(0, 5);
            uint32_t Reserved_134 : BITFIELD_RANGE(6, 7);
            uint32_t ThreadHeightCounterMaximum : BITFIELD_RANGE(8, 13);
            uint32_t Reserved_142 : BITFIELD_RANGE(14, 15);
            uint32_t ThreadDepthCounterMaximum : BITFIELD_RANGE(16, 21);
            uint32_t Reserved_150 : BITFIELD_RANGE(22, 29);
            uint32_t SimdSize : BITFIELD_RANGE(30, 31);
            uint32_t ThreadGroupIdStartingX;
            uint32_t Reserved_192;
            uint32_t ThreadGroupIdXDimension;
            uint32_t ThreadGroupIdStartingY;
            uint32_t Reserved_288;
            uint32_t ThreadGroupIdYDimension;
            uint32_t ThreadGroupIdStartingResumeZ;
            uint32_t ThreadGroupIdZDimension;
            uint32_t RightExecutionMask;
            uint32_t BottomExecutionMask;
        } Common;
        uint32_t RawData[15];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0xd,
    } DWORD_LENGTH;
    typedef enum tagSUBOPCODE {
        SUBOPCODE_GPGPU_WALKER_SUBOP = 0x5,
    } SUBOPCODE;
    typedef enum tagMEDIA_COMMAND_OPCODE {
        MEDIA_COMMAND_OPCODE_GPGPU_WALKER = 0x1,
    } MEDIA_COMMAND_OPCODE;
    typedef enum tagPIPELINE {
        PIPELINE_MEDIA = 0x2,
    } PIPELINE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagSIMD_SIZE {
        SIMD_SIZE_SIMD8 = 0x0,
        SIMD_SIZE_SIMD16 = 0x1,
        SIMD_SIZE_SIMD32 = 0x2,
    } SIMD_SIZE;
    typedef enum tagPATCH_CONSTANTS {
        INDIRECTDATASTARTADDRESS_BYTEOFFSET = 0xc,
        INDIRECTDATASTARTADDRESS_INDEX = 0x3,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common.Subopcode = SUBOPCODE_GPGPU_WALKER_SUBOP;
        TheStructure.Common.MediaCommandOpcode = MEDIA_COMMAND_OPCODE_GPGPU_WALKER;
        TheStructure.Common.Pipeline = PIPELINE_MEDIA;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.SimdSize = SIMD_SIZE_SIMD8;
    }
    static tagGPGPU_WALKER sInit(void) {
        GPGPU_WALKER state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 15);
        return TheStructure.RawData[index];
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable(void) const {
        return (TheStructure.Common.PredicateEnable);
    }
    inline void setIndirectParameterEnable(const bool value) {
        TheStructure.Common.IndirectParameterEnable = value;
    }
    inline bool getIndirectParameterEnable(void) const {
        return (TheStructure.Common.IndirectParameterEnable);
    }
    inline void setInterfaceDescriptorOffset(const uint32_t value) {
        TheStructure.Common.InterfaceDescriptorOffset = value;
    }
    inline uint32_t getInterfaceDescriptorOffset(void) const {
        return (TheStructure.Common.InterfaceDescriptorOffset);
    }
    inline void setIndirectDataLength(const uint32_t value) {
        TheStructure.Common.IndirectDataLength = value;
    }
    inline uint32_t getIndirectDataLength(void) const {
        return (TheStructure.Common.IndirectDataLength);
    }
    typedef enum tagINDIRECTDATASTARTADDRESS {
        INDIRECTDATASTARTADDRESS_BIT_SHIFT = 0x6,
        INDIRECTDATASTARTADDRESS_ALIGN_SIZE = 0x40,
    } INDIRECTDATASTARTADDRESS;
    inline void setIndirectDataStartAddress(const uint32_t value) {
        TheStructure.Common.IndirectDataStartAddress = value >> INDIRECTDATASTARTADDRESS_BIT_SHIFT;
    }
    inline uint32_t getIndirectDataStartAddress(void) const {
        return (TheStructure.Common.IndirectDataStartAddress << INDIRECTDATASTARTADDRESS_BIT_SHIFT);
    }
    inline void setThreadWidthCounterMaximum(const uint32_t value) {
        TheStructure.Common.ThreadWidthCounterMaximum = value - 1;
    }
    inline uint32_t getThreadWidthCounterMaximum(void) const {
        return (TheStructure.Common.ThreadWidthCounterMaximum + 1);
    }
    inline void setThreadHeightCounterMaximum(const uint32_t value) {
        TheStructure.Common.ThreadHeightCounterMaximum = value - 1;
    }
    inline uint32_t getThreadHeightCounterMaximum(void) const {
        return (TheStructure.Common.ThreadHeightCounterMaximum + 1);
    }
    inline void setThreadDepthCounterMaximum(const uint32_t value) {
        TheStructure.Common.ThreadDepthCounterMaximum = value;
    }
    inline uint32_t getThreadDepthCounterMaximum(void) const {
        return (TheStructure.Common.ThreadDepthCounterMaximum);
    }
    inline void setSimdSize(const SIMD_SIZE value) {
        TheStructure.Common.SimdSize = value;
    }
    inline SIMD_SIZE getSimdSize(void) const {
        return static_cast<SIMD_SIZE>(TheStructure.Common.SimdSize);
    }
    inline void setThreadGroupIdStartingX(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdStartingX = value;
    }
    inline uint32_t getThreadGroupIdStartingX(void) const {
        return (TheStructure.Common.ThreadGroupIdStartingX);
    }
    inline void setThreadGroupIdXDimension(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdXDimension = value;
    }
    inline uint32_t getThreadGroupIdXDimension(void) const {
        return (TheStructure.Common.ThreadGroupIdXDimension);
    }
    inline void setThreadGroupIdStartingY(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdStartingY = value;
    }
    inline uint32_t getThreadGroupIdStartingY(void) const {
        return (TheStructure.Common.ThreadGroupIdStartingY);
    }
    inline void setThreadGroupIdYDimension(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdYDimension = value;
    }
    inline uint32_t getThreadGroupIdYDimension(void) const {
        return (TheStructure.Common.ThreadGroupIdYDimension);
    }
    inline void setThreadGroupIdStartingResumeZ(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdStartingResumeZ = value;
    }
    inline uint32_t getThreadGroupIdStartingResumeZ(void) const {
        return (TheStructure.Common.ThreadGroupIdStartingResumeZ);
    }
    inline void setThreadGroupIdZDimension(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdZDimension = value;
    }
    inline uint32_t getThreadGroupIdZDimension(void) const {
        return (TheStructure.Common.ThreadGroupIdZDimension);
    }
    inline void setRightExecutionMask(const uint32_t value) {
        TheStructure.Common.RightExecutionMask = value;
    }
    inline uint32_t getRightExecutionMask(void) const {
        return (TheStructure.Common.RightExecutionMask);
    }
    inline void setBottomExecutionMask(const uint32_t value) {
        TheStructure.Common.BottomExecutionMask = value;
    }
    inline uint32_t getBottomExecutionMask(void) const {
        return (TheStructure.Common.BottomExecutionMask);
    }
} GPGPU_WALKER;
STATIC_ASSERT(60 == sizeof(GPGPU_WALKER));

typedef struct tagINTERFACE_DESCRIPTOR_DATA {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 5);
            uint32_t KernelStartPointer : BITFIELD_RANGE(6, 31);
            uint32_t KernelStartPointerHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_48 : BITFIELD_RANGE(16, 31);
            uint32_t Reserved_64 : BITFIELD_RANGE(0, 6);
            uint32_t SoftwareExceptionEnable : BITFIELD_RANGE(7, 7);
            uint32_t Reserved_72 : BITFIELD_RANGE(8, 10);
            uint32_t MaskStackExceptionEnable : BITFIELD_RANGE(11, 11);
            uint32_t Reserved_76 : BITFIELD_RANGE(12, 12);
            uint32_t IllegalOpcodeExceptionEnable : BITFIELD_RANGE(13, 13);
            uint32_t Reserved_78 : BITFIELD_RANGE(14, 15);
            uint32_t FloatingPointMode : BITFIELD_RANGE(16, 16);
            uint32_t ThreadPriority : BITFIELD_RANGE(17, 17);
            uint32_t SingleProgramFlow : BITFIELD_RANGE(18, 18);
            uint32_t DenormMode : BITFIELD_RANGE(19, 19);
            uint32_t ThreadPreemptionDisable : BITFIELD_RANGE(20, 20);
            uint32_t Reserved_85 : BITFIELD_RANGE(21, 31);
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 1);
            uint32_t SamplerCount : BITFIELD_RANGE(2, 4);
            uint32_t SamplerStatePointer : BITFIELD_RANGE(5, 31);
            uint32_t BindingTableEntryCount : BITFIELD_RANGE(0, 4);
            uint32_t BindingTablePointer : BITFIELD_RANGE(5, 15);
            uint32_t Reserved_144 : BITFIELD_RANGE(16, 31);
            uint32_t ConstantUrbEntryReadOffset : BITFIELD_RANGE(0, 15);
            uint32_t ConstantIndirectUrbEntryReadLength : BITFIELD_RANGE(16, 31);
            uint32_t NumberOfThreadsInGpgpuThreadGroup : BITFIELD_RANGE(0, 9);
            uint32_t Reserved_202 : BITFIELD_RANGE(10, 15);
            uint32_t SharedLocalMemorySize : BITFIELD_RANGE(16, 20);
            uint32_t BarrierEnable : BITFIELD_RANGE(21, 21);
            uint32_t RoundingMode : BITFIELD_RANGE(22, 23);
            uint32_t Reserved_216 : BITFIELD_RANGE(24, 31);
            uint32_t Cross_ThreadConstantDataReadLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_232 : BITFIELD_RANGE(8, 31);
        } Common;
        uint32_t RawData[8];
    } TheStructure;
    typedef enum tagFLOATING_POINT_MODE {
        FLOATING_POINT_MODE_IEEE_754 = 0x0,
        FLOATING_POINT_MODE_ALTERNATE = 0x1,
    } FLOATING_POINT_MODE;
    typedef enum tagTHREAD_PRIORITY {
        THREAD_PRIORITY_NORMAL_PRIORITY = 0x0,
        THREAD_PRIORITY_HIGH_PRIORITY = 0x1,
    } THREAD_PRIORITY;
    typedef enum tagSINGLE_PROGRAM_FLOW {
        SINGLE_PROGRAM_FLOW_MULTIPLE = 0x0,
        SINGLE_PROGRAM_FLOW_SINGLE = 0x1,
    } SINGLE_PROGRAM_FLOW;
    typedef enum tagDENORM_MODE {
        DENORM_MODE_FTZ = 0x0,
        DENORM_MODE_SETBYKERNEL = 0x1,
    } DENORM_MODE;
    typedef enum tagTHREAD_PREEMPTION_DISABLE {
        THREAD_PREEMPTION_DISABLE_DISABLE = 0x0,
        THREAD_PREEMPTION_DISABLE_ENABLE = 0x1,
    } THREAD_PREEMPTION_DISABLE;
    typedef enum tagSAMPLER_COUNT {
        SAMPLER_COUNT_NO_SAMPLERS_USED = 0x0,
        SAMPLER_COUNT_BETWEEN_1_AND_4_SAMPLERS_USED = 0x1,
        SAMPLER_COUNT_BETWEEN_5_AND_8_SAMPLERS_USED = 0x2,
        SAMPLER_COUNT_BETWEEN_9_AND_12_SAMPLERS_USED = 0x3,
        SAMPLER_COUNT_BETWEEN_13_AND_16_SAMPLERS_USED = 0x4,
    } SAMPLER_COUNT;
    typedef enum tagSHARED_LOCAL_MEMORY_SIZE {
        SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K = 0x0,
        SHARED_LOCAL_MEMORY_SIZE_ENCODES_1K = 0x1,
        SHARED_LOCAL_MEMORY_SIZE_ENCODES_2K = 0x2,
        SHARED_LOCAL_MEMORY_SIZE_ENCODES_4K = 0x3,
        SHARED_LOCAL_MEMORY_SIZE_ENCODES_8K = 0x4,
        SHARED_LOCAL_MEMORY_SIZE_ENCODES_16K = 0x5,
        SHARED_LOCAL_MEMORY_SIZE_ENCODES_32K = 0x6,
        SHARED_LOCAL_MEMORY_SIZE_ENCODES_64K = 0x7,
    } SHARED_LOCAL_MEMORY_SIZE;
    typedef enum tagROUNDING_MODE {
        ROUNDING_MODE_RTNE = 0x0,
        ROUNDING_MODE_RU = 0x1,
        ROUNDING_MODE_RD = 0x2,
        ROUNDING_MODE_RTZ = 0x3,
    } ROUNDING_MODE;
    typedef enum tagPATCH_CONSTANTS {
        KERNELSTARTPOINTER_BYTEOFFSET = 0x0,
        KERNELSTARTPOINTER_INDEX = 0x0,
        KERNELSTARTPOINTERHIGH_BYTEOFFSET = 0x4,
        KERNELSTARTPOINTERHIGH_INDEX = 0x1,
        SAMPLERSTATEPOINTER_BYTEOFFSET = 0xc,
        SAMPLERSTATEPOINTER_INDEX = 0x3,
        BINDINGTABLEPOINTER_BYTEOFFSET = 0x10,
        BINDINGTABLEPOINTER_INDEX = 0x4,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.FloatingPointMode = FLOATING_POINT_MODE_IEEE_754;
        TheStructure.Common.ThreadPriority = THREAD_PRIORITY_NORMAL_PRIORITY;
        TheStructure.Common.SingleProgramFlow = SINGLE_PROGRAM_FLOW_MULTIPLE;
        TheStructure.Common.DenormMode = DENORM_MODE_FTZ;
        TheStructure.Common.ThreadPreemptionDisable = THREAD_PREEMPTION_DISABLE_DISABLE;
        TheStructure.Common.SamplerCount = SAMPLER_COUNT_NO_SAMPLERS_USED;
        TheStructure.Common.SharedLocalMemorySize = SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K;
        TheStructure.Common.RoundingMode = ROUNDING_MODE_RTNE;
    }
    static tagINTERFACE_DESCRIPTOR_DATA sInit(void) {
        INTERFACE_DESCRIPTOR_DATA state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 8);
        return TheStructure.RawData[index];
    }
    typedef enum tagKERNELSTARTPOINTER {
        KERNELSTARTPOINTER_BIT_SHIFT = 0x6,
        KERNELSTARTPOINTER_ALIGN_SIZE = 0x40,
    } KERNELSTARTPOINTER;
    inline void setKernelStartPointer(const uint64_t value) {
        DEBUG_BREAK_IF(value >= 0x100000000);
        TheStructure.Common.KernelStartPointer = (uint32_t)value >> KERNELSTARTPOINTER_BIT_SHIFT;
    }
    inline uint32_t getKernelStartPointer(void) const {
        return (TheStructure.Common.KernelStartPointer << KERNELSTARTPOINTER_BIT_SHIFT);
    }
    inline void setKernelStartPointerHigh(const uint32_t value) {
        TheStructure.Common.KernelStartPointerHigh = value;
    }
    inline uint32_t getKernelStartPointerHigh(void) const {
        return (TheStructure.Common.KernelStartPointerHigh);
    }
    inline void setSoftwareExceptionEnable(const bool value) {
        TheStructure.Common.SoftwareExceptionEnable = value;
    }
    inline bool getSoftwareExceptionEnable(void) const {
        return (TheStructure.Common.SoftwareExceptionEnable);
    }
    inline void setMaskStackExceptionEnable(const bool value) {
        TheStructure.Common.MaskStackExceptionEnable = value;
    }
    inline bool getMaskStackExceptionEnable(void) const {
        return (TheStructure.Common.MaskStackExceptionEnable);
    }
    inline void setIllegalOpcodeExceptionEnable(const bool value) {
        TheStructure.Common.IllegalOpcodeExceptionEnable = value;
    }
    inline bool getIllegalOpcodeExceptionEnable(void) const {
        return (TheStructure.Common.IllegalOpcodeExceptionEnable);
    }
    inline void setFloatingPointMode(const FLOATING_POINT_MODE value) {
        TheStructure.Common.FloatingPointMode = value;
    }
    inline FLOATING_POINT_MODE getFloatingPointMode(void) const {
        return static_cast<FLOATING_POINT_MODE>(TheStructure.Common.FloatingPointMode);
    }
    inline void setThreadPriority(const THREAD_PRIORITY value) {
        TheStructure.Common.ThreadPriority = value;
    }
    inline THREAD_PRIORITY getThreadPriority(void) const {
        return static_cast<THREAD_PRIORITY>(TheStructure.Common.ThreadPriority);
    }
    inline void setSingleProgramFlow(const SINGLE_PROGRAM_FLOW value) {
        TheStructure.Common.SingleProgramFlow = value;
    }
    inline SINGLE_PROGRAM_FLOW getSingleProgramFlow(void) const {
        return static_cast<SINGLE_PROGRAM_FLOW>(TheStructure.Common.SingleProgramFlow);
    }
    inline void setDenormMode(const DENORM_MODE value) {
        TheStructure.Common.DenormMode = value;
    }
    inline DENORM_MODE getDenormMode(void) const {
        return static_cast<DENORM_MODE>(TheStructure.Common.DenormMode);
    }
    inline void setThreadPreemptionDisable(const THREAD_PREEMPTION_DISABLE value) {
        TheStructure.Common.ThreadPreemptionDisable = value;
    }
    inline THREAD_PREEMPTION_DISABLE getThreadPreemptionDisable(void) const {
        return static_cast<THREAD_PREEMPTION_DISABLE>(TheStructure.Common.ThreadPreemptionDisable);
    }
    inline void setSamplerCount(const SAMPLER_COUNT value) {
        TheStructure.Common.SamplerCount = value;
    }
    inline SAMPLER_COUNT getSamplerCount(void) const {
        return static_cast<SAMPLER_COUNT>(TheStructure.Common.SamplerCount);
    }
    typedef enum tagSAMPLERSTATEPOINTER {
        SAMPLERSTATEPOINTER_BIT_SHIFT = 0x5,
        SAMPLERSTATEPOINTER_ALIGN_SIZE = 0x20,
    } SAMPLERSTATEPOINTER;
    inline void setSamplerStatePointer(const uint64_t value) {
        DEBUG_BREAK_IF(value >= 0x100000000);
        TheStructure.Common.SamplerStatePointer = (uint32_t)value >> SAMPLERSTATEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getSamplerStatePointer(void) const {
        return (TheStructure.Common.SamplerStatePointer << SAMPLERSTATEPOINTER_BIT_SHIFT);
    }
    inline void setBindingTableEntryCount(const uint32_t value) {
        TheStructure.Common.BindingTableEntryCount = value;
    }
    inline uint32_t getBindingTableEntryCount(void) const {
        return (TheStructure.Common.BindingTableEntryCount);
    }
    typedef enum tagBINDINGTABLEPOINTER {
        BINDINGTABLEPOINTER_BIT_SHIFT = 0x5,
        BINDINGTABLEPOINTER_ALIGN_SIZE = 0x20,
    } BINDINGTABLEPOINTER;
    inline void setBindingTablePointer(const uint64_t value) {
        DEBUG_BREAK_IF(value >= 0x100000000);
        TheStructure.Common.BindingTablePointer = (uint32_t)value >> BINDINGTABLEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getBindingTablePointer(void) const {
        return (TheStructure.Common.BindingTablePointer << BINDINGTABLEPOINTER_BIT_SHIFT);
    }
    inline void setConstantUrbEntryReadOffset(const uint32_t value) {
        TheStructure.Common.ConstantUrbEntryReadOffset = value;
    }
    inline uint32_t getConstantUrbEntryReadOffset(void) const {
        return (TheStructure.Common.ConstantUrbEntryReadOffset);
    }
    inline void setConstantIndirectUrbEntryReadLength(const uint32_t value) {
        TheStructure.Common.ConstantIndirectUrbEntryReadLength = value;
    }
    inline uint32_t getConstantIndirectUrbEntryReadLength(void) const {
        return (TheStructure.Common.ConstantIndirectUrbEntryReadLength);
    }
    inline void setNumberOfThreadsInGpgpuThreadGroup(const uint32_t value) {
        TheStructure.Common.NumberOfThreadsInGpgpuThreadGroup = value;
    }
    inline uint32_t getNumberOfThreadsInGpgpuThreadGroup(void) const {
        return (TheStructure.Common.NumberOfThreadsInGpgpuThreadGroup);
    }
    inline void setSharedLocalMemorySize(const SHARED_LOCAL_MEMORY_SIZE value) {
        TheStructure.Common.SharedLocalMemorySize = value;
    }
    inline SHARED_LOCAL_MEMORY_SIZE getSharedLocalMemorySize(void) const {
        return static_cast<SHARED_LOCAL_MEMORY_SIZE>(TheStructure.Common.SharedLocalMemorySize);
    }
    inline void setBarrierEnable(const uint32_t value) {
        TheStructure.Common.BarrierEnable = (value > 0u) ? 1u : 0u;
    }
    inline bool getBarrierEnable(void) const {
        return (TheStructure.Common.BarrierEnable);
    }
    inline void setRoundingMode(const ROUNDING_MODE value) {
        TheStructure.Common.RoundingMode = value;
    }
    inline ROUNDING_MODE getRoundingMode(void) const {
        return static_cast<ROUNDING_MODE>(TheStructure.Common.RoundingMode);
    }
    inline void setCrossThreadConstantDataReadLength(const uint32_t value) {
        TheStructure.Common.Cross_ThreadConstantDataReadLength = value;
    }
    inline uint32_t getCrossThreadConstantDataReadLength(void) const {
        return (TheStructure.Common.Cross_ThreadConstantDataReadLength);
    }
} INTERFACE_DESCRIPTOR_DATA;
STATIC_ASSERT(32 == sizeof(INTERFACE_DESCRIPTOR_DATA));

typedef struct tagMEDIA_INTERFACE_DESCRIPTOR_LOAD {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 15);
            uint32_t Subopcode : BITFIELD_RANGE(16, 23);
            uint32_t MediaCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t Pipeline : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t Reserved_32;
            uint32_t InterfaceDescriptorTotalLength : BITFIELD_RANGE(0, 16);
            uint32_t Reserved_81 : BITFIELD_RANGE(17, 31);
            uint32_t InterfaceDescriptorDataStartAddress;
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x2,
    } DWORD_LENGTH;
    typedef enum tagSUBOPCODE {
        SUBOPCODE_MEDIA_INTERFACE_DESCRIPTOR_LOAD_SUBOP = 0x2,
    } SUBOPCODE;
    typedef enum tagMEDIA_COMMAND_OPCODE {
        MEDIA_COMMAND_OPCODE_MEDIA_INTERFACE_DESCRIPTOR_LOAD = 0x0,
    } MEDIA_COMMAND_OPCODE;
    typedef enum tagPIPELINE {
        PIPELINE_MEDIA = 0x2,
    } PIPELINE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagPATCH_CONSTANTS {
        INTERFACEDESCRIPTORDATASTARTADDRESS_BYTEOFFSET = 0xc,
        INTERFACEDESCRIPTORDATASTARTADDRESS_INDEX = 0x3,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common.Subopcode = SUBOPCODE_MEDIA_INTERFACE_DESCRIPTOR_LOAD_SUBOP;
        TheStructure.Common.MediaCommandOpcode = MEDIA_COMMAND_OPCODE_MEDIA_INTERFACE_DESCRIPTOR_LOAD;
        TheStructure.Common.Pipeline = PIPELINE_MEDIA;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagMEDIA_INTERFACE_DESCRIPTOR_LOAD sInit(void) {
        MEDIA_INTERFACE_DESCRIPTOR_LOAD state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setInterfaceDescriptorTotalLength(const uint32_t value) {
        TheStructure.Common.InterfaceDescriptorTotalLength = value;
    }
    inline uint32_t getInterfaceDescriptorTotalLength(void) const {
        return (TheStructure.Common.InterfaceDescriptorTotalLength);
    }
    inline void setInterfaceDescriptorDataStartAddress(const uint32_t value) {
        TheStructure.Common.InterfaceDescriptorDataStartAddress = value;
    }
    inline uint32_t getInterfaceDescriptorDataStartAddress(void) const {
        return (TheStructure.Common.InterfaceDescriptorDataStartAddress);
    }
} MEDIA_INTERFACE_DESCRIPTOR_LOAD;
STATIC_ASSERT(16 == sizeof(MEDIA_INTERFACE_DESCRIPTOR_LOAD));

typedef struct tagMEDIA_STATE_FLUSH {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 15);
            uint32_t Subopcode : BITFIELD_RANGE(16, 23);
            uint32_t MediaCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t Pipeline : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t InterfaceDescriptorOffset : BITFIELD_RANGE(0, 5);
            uint32_t Reserved_38 : BITFIELD_RANGE(6, 6);
            uint32_t FlushToGo : BITFIELD_RANGE(7, 7);
            uint32_t Reserved_40 : BITFIELD_RANGE(8, 31);
        } Common;
        uint32_t RawData[2];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x0,
    } DWORD_LENGTH;
    typedef enum tagSUBOPCODE {
        SUBOPCODE_MEDIA_STATE_FLUSH_SUBOP = 0x4,
    } SUBOPCODE;
    typedef enum tagMEDIA_COMMAND_OPCODE {
        MEDIA_COMMAND_OPCODE_MEDIA_STATE_FLUSH = 0x0,
    } MEDIA_COMMAND_OPCODE;
    typedef enum tagPIPELINE {
        PIPELINE_MEDIA = 0x2,
    } PIPELINE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common.Subopcode = SUBOPCODE_MEDIA_STATE_FLUSH_SUBOP;
        TheStructure.Common.MediaCommandOpcode = MEDIA_COMMAND_OPCODE_MEDIA_STATE_FLUSH;
        TheStructure.Common.Pipeline = PIPELINE_MEDIA;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagMEDIA_STATE_FLUSH sInit(void) {
        MEDIA_STATE_FLUSH state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 2);
        return TheStructure.RawData[index];
    }
    inline void setInterfaceDescriptorOffset(const uint32_t value) {
        TheStructure.Common.InterfaceDescriptorOffset = value;
    }
    inline uint32_t getInterfaceDescriptorOffset(void) const {
        return (TheStructure.Common.InterfaceDescriptorOffset);
    }
    inline void setFlushToGo(const bool value) {
        TheStructure.Common.FlushToGo = value;
    }
    inline bool getFlushToGo(void) const {
        return (TheStructure.Common.FlushToGo);
    }
} MEDIA_STATE_FLUSH;
STATIC_ASSERT(8 == sizeof(MEDIA_STATE_FLUSH));

typedef struct tagMEDIA_VFE_STATE {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 15);
            uint32_t Subopcode : BITFIELD_RANGE(16, 23);
            uint32_t MediaCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t Pipeline : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t PerThreadScratchSpace : BITFIELD_RANGE(0, 3);
            uint32_t StackSize : BITFIELD_RANGE(4, 7);
            uint32_t Reserved_40 : BITFIELD_RANGE(8, 9);
            uint32_t ScratchSpaceBasePointer : BITFIELD_RANGE(10, 31);
            uint32_t ScratchSpaceBasePointerHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_80 : BITFIELD_RANGE(16, 31);
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 1);
            uint32_t DispatchLoadBalance : BITFIELD_RANGE(2, 2);
            uint32_t Reserved_99 : BITFIELD_RANGE(3, 5);
            uint32_t DisableSlice0Subslice2 : BITFIELD_RANGE(6, 6);
            uint32_t Reserved_103 : BITFIELD_RANGE(7, 7);
            uint32_t NumberOfUrbEntries : BITFIELD_RANGE(8, 15);
            uint32_t MaximumNumberOfThreads : BITFIELD_RANGE(16, 31);
            uint32_t MaximumNumberOfDual_Subslices : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_136 : BITFIELD_RANGE(8, 31);
            uint32_t CurbeAllocationSize : BITFIELD_RANGE(0, 15);
            uint32_t UrbEntryAllocationSize : BITFIELD_RANGE(16, 31);
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint32_t Reserved_256;
        } Common;
        uint32_t RawData[9];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x7,
    } DWORD_LENGTH;
    typedef enum tagSUBOPCODE {
        SUBOPCODE_MEDIA_VFE_STATE_SUBOP = 0x0,
    } SUBOPCODE;
    typedef enum tagMEDIA_COMMAND_OPCODE {
        MEDIA_COMMAND_OPCODE_MEDIA_VFE_STATE = 0x0,
    } MEDIA_COMMAND_OPCODE;
    typedef enum tagPIPELINE {
        PIPELINE_MEDIA = 0x2,
    } PIPELINE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagDISPATCH_LOAD_BALANCE {
        DISPATCH_LOAD_BALANCE_LEAST_LOADED = 0x0,
        DISPATCH_LOAD_BALANCE_COLOR_LSB = 0x1,
    } DISPATCH_LOAD_BALANCE;
    typedef enum tagPATCH_CONSTANTS {
        SCRATCHSPACEBASEPOINTER_BYTEOFFSET = 0x4,
        SCRATCHSPACEBASEPOINTER_INDEX = 0x1,
        SCRATCHSPACEBASEPOINTERHIGH_BYTEOFFSET = 0x8,
        SCRATCHSPACEBASEPOINTERHIGH_INDEX = 0x2,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common.Subopcode = SUBOPCODE_MEDIA_VFE_STATE_SUBOP;
        TheStructure.Common.MediaCommandOpcode = MEDIA_COMMAND_OPCODE_MEDIA_VFE_STATE;
        TheStructure.Common.Pipeline = PIPELINE_MEDIA;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.DispatchLoadBalance = DISPATCH_LOAD_BALANCE_LEAST_LOADED;
    }
    static tagMEDIA_VFE_STATE sInit(void) {
        MEDIA_VFE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 9);
        return TheStructure.RawData[index];
    }
    inline void setPerThreadScratchSpace(const uint32_t value) {
        TheStructure.Common.PerThreadScratchSpace = value;
    }
    inline uint32_t getPerThreadScratchSpace(void) const {
        return (TheStructure.Common.PerThreadScratchSpace);
    }
    inline void setStackSize(const uint32_t value) {
        TheStructure.Common.StackSize = value;
    }
    inline uint32_t getStackSize(void) const {
        return (TheStructure.Common.StackSize);
    }
    typedef enum tagSCRATCHSPACEBASEPOINTER {
        SCRATCHSPACEBASEPOINTER_BIT_SHIFT = 0xa,
        SCRATCHSPACEBASEPOINTER_ALIGN_SIZE = 0x400,
    } SCRATCHSPACEBASEPOINTER;
    inline void setScratchSpaceBasePointer(const uint32_t value) {
        TheStructure.Common.ScratchSpaceBasePointer = value >> SCRATCHSPACEBASEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getScratchSpaceBasePointer(void) const {
        return (TheStructure.Common.ScratchSpaceBasePointer << SCRATCHSPACEBASEPOINTER_BIT_SHIFT);
    }
    inline void setScratchSpaceBasePointerHigh(const uint32_t value) {
        TheStructure.Common.ScratchSpaceBasePointerHigh = value;
    }
    inline uint32_t getScratchSpaceBasePointerHigh(void) const {
        return (TheStructure.Common.ScratchSpaceBasePointerHigh);
    }
    inline void setDispatchLoadBalance(const DISPATCH_LOAD_BALANCE value) {
        TheStructure.Common.DispatchLoadBalance = value;
    }
    inline DISPATCH_LOAD_BALANCE getDispatchLoadBalance(void) const {
        return static_cast<DISPATCH_LOAD_BALANCE>(TheStructure.Common.DispatchLoadBalance);
    }
    inline void setDisableSlice0Subslice2(const bool value) {
        TheStructure.Common.DisableSlice0Subslice2 = value;
    }
    inline bool getDisableSlice0Subslice2(void) const {
        return (TheStructure.Common.DisableSlice0Subslice2);
    }
    inline void setNumberOfUrbEntries(const uint32_t value) {
        TheStructure.Common.NumberOfUrbEntries = value;
    }
    inline uint32_t getNumberOfUrbEntries(void) const {
        return (TheStructure.Common.NumberOfUrbEntries);
    }
    inline void setMaximumNumberOfThreads(const uint32_t value) {
        TheStructure.Common.MaximumNumberOfThreads = value - 1;
    }
    inline uint32_t getMaximumNumberOfThreads(void) const {
        return (TheStructure.Common.MaximumNumberOfThreads + 1);
    }
    inline void setMaximumNumberOfDualSubslices(const uint32_t value) {
        TheStructure.Common.MaximumNumberOfDual_Subslices = value;
    }
    inline uint32_t getMaximumNumberOfDualSubslices(void) const {
        return (TheStructure.Common.MaximumNumberOfDual_Subslices);
    }
    inline void setCurbeAllocationSize(const uint32_t value) {
        TheStructure.Common.CurbeAllocationSize = value;
    }
    inline uint32_t getCurbeAllocationSize(void) const {
        return (TheStructure.Common.CurbeAllocationSize);
    }
    inline void setUrbEntryAllocationSize(const uint32_t value) {
        TheStructure.Common.UrbEntryAllocationSize = value;
    }
    inline uint32_t getUrbEntryAllocationSize(void) const {
        return (TheStructure.Common.UrbEntryAllocationSize);
    }
} MEDIA_VFE_STATE;
STATIC_ASSERT(36 == sizeof(MEDIA_VFE_STATE));

typedef struct tagMI_ARB_CHECK {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 22);
            uint32_t MiInstructionOpcode : BITFIELD_RANGE(23, 28);
            uint32_t MiInstructionType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagMI_INSTRUCTION_OPCODE {
        MI_INSTRUCTION_OPCODE_MI_ARB_CHECK = 0x5,
    } MI_INSTRUCTION_OPCODE;
    typedef enum tagMI_INSTRUCTION_TYPE {
        MI_INSTRUCTION_TYPE_MI_INSTRUCTION = 0x0,
    } MI_INSTRUCTION_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiInstructionOpcode = MI_INSTRUCTION_OPCODE_MI_ARB_CHECK;
        TheStructure.Common.MiInstructionType = MI_INSTRUCTION_TYPE_MI_INSTRUCTION;
    }
    static tagMI_ARB_CHECK sInit(void) {
        MI_ARB_CHECK state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setMiInstructionOpcode(const MI_INSTRUCTION_OPCODE value) {
        TheStructure.Common.MiInstructionOpcode = value;
    }
    inline MI_INSTRUCTION_OPCODE getMiInstructionOpcode(void) const {
        return static_cast<MI_INSTRUCTION_OPCODE>(TheStructure.Common.MiInstructionOpcode);
    }
    inline void setMiInstructionType(const MI_INSTRUCTION_TYPE value) {
        TheStructure.Common.MiInstructionType = value;
    }
    inline MI_INSTRUCTION_TYPE getMiInstructionType(void) const {
        return static_cast<MI_INSTRUCTION_TYPE>(TheStructure.Common.MiInstructionType);
    }
} MI_ARB_CHECK;
STATIC_ASSERT(4 == sizeof(MI_ARB_CHECK));

typedef struct tagMI_ATOMIC {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t AtomicOpcode : BITFIELD_RANGE(8, 15);
            uint32_t ReturnDataControl : BITFIELD_RANGE(16, 16);
            uint32_t CsStall : BITFIELD_RANGE(17, 17);
            uint32_t InlineData : BITFIELD_RANGE(18, 18);
            uint32_t DataSize : BITFIELD_RANGE(19, 20);
            uint32_t Post_SyncOperation : BITFIELD_RANGE(21, 21);
            uint32_t MemoryType : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint32_t MemoryAddress : BITFIELD_RANGE(2, 31);
            uint32_t MemoryAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_80 : BITFIELD_RANGE(16, 31);
            uint32_t Operand1DataDword0;
            uint32_t Operand2DataDword0;
            uint32_t Operand1DataDword1;
            uint32_t Operand2DataDword1;
            uint32_t Operand1DataDword2;
            uint32_t Operand2DataDword2;
            uint32_t Operand1DataDword3;
            uint32_t Operand2DataDword3;
        } Common;
        uint32_t RawData[11];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_INLINE_DATA_0 = 0x1,
        DWORD_LENGTH_INLINE_DATA_1 = 0x9,
    } DWORD_LENGTH;
    typedef enum tagDATA_SIZE {
        DATA_SIZE_DWORD = 0x0,
        DATA_SIZE_QWORD = 0x1,
        DATA_SIZE_OCTWORD = 0x2,
    } DATA_SIZE;
    typedef enum tagPOST_SYNC_OPERATION {
        POST_SYNC_OPERATION_NO_POST_SYNC_OPERATION = 0x0,
        POST_SYNC_OPERATION_POST_SYNC_OPERATION = 0x1,
    } POST_SYNC_OPERATION;
    typedef enum tagMEMORY_TYPE {
        MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS = 0x0,
        MEMORY_TYPE_GLOBAL_GRAPHICS_ADDRESS = 0x1,
    } MEMORY_TYPE;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_ATOMIC = 0x2f,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    typedef enum tagPATCH_CONSTANTS {
        MEMORYADDRESS_BYTEOFFSET = 0x4,
        MEMORYADDRESS_INDEX = 0x1,
    } PATCH_CONSTANTS;
    typedef enum tagATOMIC_OPCODES {
        ATOMIC_4B_DECREMENT = 0x6,
        ATOMIC_8B_INCREMENT = 0x25,
        ATOMIC_8B_DECREMENT = 0x26,
    } ATOMIC_OPCODES;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_INLINE_DATA_0;
        TheStructure.Common.DataSize = DATA_SIZE_DWORD;
        TheStructure.Common.Post_SyncOperation = POST_SYNC_OPERATION_NO_POST_SYNC_OPERATION;
        TheStructure.Common.MemoryType = MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_ATOMIC;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_ATOMIC sInit(void) {
        MI_ATOMIC state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 11);
        return TheStructure.RawData[index];
    }
    inline void setDwordLength(const DWORD_LENGTH value) {
        TheStructure.Common.DwordLength = value;
    }
    inline DWORD_LENGTH getDwordLength(void) const {
        return static_cast<DWORD_LENGTH>(TheStructure.Common.DwordLength);
    }
    inline void setAtomicOpcode(const uint32_t value) {
        TheStructure.Common.AtomicOpcode = value;
    }
    inline uint32_t getAtomicOpcode(void) const {
        return (TheStructure.Common.AtomicOpcode);
    }
    inline void setReturnDataControl(const uint32_t value) {
        TheStructure.Common.ReturnDataControl = value;
    }
    inline uint32_t getReturnDataControl(void) const {
        return (TheStructure.Common.ReturnDataControl);
    }
    inline void setCsStall(const uint32_t value) {
        TheStructure.Common.CsStall = value;
    }
    inline uint32_t getCsStall(void) const {
        return (TheStructure.Common.CsStall);
    }
    inline void setInlineData(const uint32_t value) {
        TheStructure.Common.InlineData = value;
    }
    inline uint32_t getInlineData(void) const {
        return (TheStructure.Common.InlineData);
    }
    inline void setDataSize(const DATA_SIZE value) {
        TheStructure.Common.DataSize = value;
    }
    inline DATA_SIZE getDataSize(void) const {
        return static_cast<DATA_SIZE>(TheStructure.Common.DataSize);
    }
    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.Post_SyncOperation = value;
    }
    inline POST_SYNC_OPERATION getPostSyncOperation(void) const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.Post_SyncOperation);
    }
    inline void setMemoryType(const MEMORY_TYPE value) {
        TheStructure.Common.MemoryType = value;
    }
    inline MEMORY_TYPE getMemoryType(void) const {
        return static_cast<MEMORY_TYPE>(TheStructure.Common.MemoryType);
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x2,
        MEMORYADDRESS_ALIGN_SIZE = 0x4,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint32_t value) {
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint32_t getMemoryAddress(void) const {
        return (TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT);
    }
    inline void setMemoryAddressHigh(const uint32_t value) {
        TheStructure.Common.MemoryAddressHigh = value;
    }
    inline uint32_t getMemoryAddressHigh(void) const {
        return (TheStructure.Common.MemoryAddressHigh);
    }
    inline void setOperand1DataDword0(const uint32_t value) {
        TheStructure.Common.Operand1DataDword0 = value;
    }
    inline uint32_t getOperand1DataDword0(void) const {
        return (TheStructure.Common.Operand1DataDword0);
    }
    inline void setOperand2DataDword0(const uint32_t value) {
        TheStructure.Common.Operand2DataDword0 = value;
    }
    inline uint32_t getOperand2DataDword0(void) const {
        return (TheStructure.Common.Operand2DataDword0);
    }
    inline void setOperand1DataDword1(const uint32_t value) {
        TheStructure.Common.Operand1DataDword1 = value;
    }
    inline uint32_t getOperand1DataDword1(void) const {
        return (TheStructure.Common.Operand1DataDword1);
    }
    inline void setOperand2DataDword1(const uint32_t value) {
        TheStructure.Common.Operand2DataDword1 = value;
    }
    inline uint32_t getOperand2DataDword1(void) const {
        return (TheStructure.Common.Operand2DataDword1);
    }
    inline void setOperand1DataDword2(const uint32_t value) {
        TheStructure.Common.Operand1DataDword2 = value;
    }
    inline uint32_t getOperand1DataDword2(void) const {
        return (TheStructure.Common.Operand1DataDword2);
    }
    inline void setOperand2DataDword2(const uint32_t value) {
        TheStructure.Common.Operand2DataDword2 = value;
    }
    inline uint32_t getOperand2DataDword2(void) const {
        return (TheStructure.Common.Operand2DataDword2);
    }
    inline void setOperand1DataDword3(const uint32_t value) {
        TheStructure.Common.Operand1DataDword3 = value;
    }
    inline uint32_t getOperand1DataDword3(void) const {
        return (TheStructure.Common.Operand1DataDword3);
    }
    inline void setOperand2DataDword3(const uint32_t value) {
        TheStructure.Common.Operand2DataDword3 = value;
    }
    inline uint32_t getOperand2DataDword3(void) const {
        return (TheStructure.Common.Operand2DataDword3);
    }
} MI_ATOMIC;
STATIC_ASSERT(44 == sizeof(MI_ATOMIC));

typedef struct tagMI_BATCH_BUFFER_END {
    union tagTheStructure {
        struct tagCommon {
            uint32_t EndContext : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_1 : BITFIELD_RANGE(1, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_BATCH_BUFFER_END = 0xa,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_BATCH_BUFFER_END;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_BATCH_BUFFER_END sInit(void) {
        MI_BATCH_BUFFER_END state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setEndContext(const bool value) {
        TheStructure.Common.EndContext = value;
    }
    inline bool getEndContext(void) const {
        return (TheStructure.Common.EndContext);
    }
} MI_BATCH_BUFFER_END;
STATIC_ASSERT(4 == sizeof(MI_BATCH_BUFFER_END));

typedef struct tagMI_LOAD_REGISTER_IMM {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t ByteWriteDisables : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_12 : BITFIELD_RANGE(12, 18);
            uint32_t AddCsMmioStartOffset : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint32_t RegisterOffset : BITFIELD_RANGE(2, 22);
            uint32_t Reserved_55 : BITFIELD_RANGE(23, 31);
            uint32_t DataDword;
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_LOAD_REGISTER_IMM = 0x22,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_LOAD_REGISTER_IMM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_LOAD_REGISTER_IMM sInit(void) {
        MI_LOAD_REGISTER_IMM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setByteWriteDisables(const uint32_t value) {
        TheStructure.Common.ByteWriteDisables = value;
    }
    inline uint32_t getByteWriteDisables(void) const {
        return (TheStructure.Common.ByteWriteDisables);
    }
    inline void setAddCsMmioStartOffset(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffset = value;
    }
    inline uint32_t getAddCsMmioStartOffset(void) const {
        return (TheStructure.Common.AddCsMmioStartOffset);
    }
    typedef enum tagREGISTEROFFSET {
        REGISTEROFFSET_BIT_SHIFT = 0x2,
        REGISTEROFFSET_ALIGN_SIZE = 0x4,
    } REGISTEROFFSET;
    inline void setRegisterOffset(const uint32_t value) {
        TheStructure.Common.RegisterOffset = value >> REGISTEROFFSET_BIT_SHIFT;
    }
    inline uint32_t getRegisterOffset(void) const {
        return (TheStructure.Common.RegisterOffset << REGISTEROFFSET_BIT_SHIFT);
    }
    inline void setDataDword(const uint32_t value) {
        TheStructure.Common.DataDword = value;
    }
    inline uint32_t getDataDword(void) const {
        return (TheStructure.Common.DataDword);
    }
} MI_LOAD_REGISTER_IMM;
STATIC_ASSERT(12 == sizeof(MI_LOAD_REGISTER_IMM));

typedef struct tagMI_LOAD_REGISTER_MEM {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 18);
            uint32_t AddCsMmioStartOffset : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 20);
            uint32_t AsyncModeEnable : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint32_t RegisterAddress : BITFIELD_RANGE(2, 22);
            uint32_t Reserved_55 : BITFIELD_RANGE(23, 31);
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint64_t MemoryAddress : BITFIELD_RANGE(2, 63);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_LOAD_REGISTER_MEM = 0x29,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    typedef enum tagPATCH_CONSTANTS {
        MEMORYADDRESS_BYTEOFFSET = 0x8,
        MEMORYADDRESS_INDEX = 0x2,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_LOAD_REGISTER_MEM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_LOAD_REGISTER_MEM sInit(void) {
        MI_LOAD_REGISTER_MEM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setAddCsMmioStartOffset(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffset = value;
    }
    inline uint32_t getAddCsMmioStartOffset(void) const {
        return (TheStructure.Common.AddCsMmioStartOffset);
    }
    inline void setAsyncModeEnable(const bool value) {
        TheStructure.Common.AsyncModeEnable = value;
    }
    inline bool getAsyncModeEnable(void) const {
        return (TheStructure.Common.AsyncModeEnable);
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt(void) const {
        return (TheStructure.Common.UseGlobalGtt);
    }
    typedef enum tagREGISTERADDRESS {
        REGISTERADDRESS_BIT_SHIFT = 0x2,
        REGISTERADDRESS_ALIGN_SIZE = 0x4,
    } REGISTERADDRESS;
    inline void setRegisterAddress(const uint32_t value) {
        TheStructure.Common.RegisterAddress = value >> REGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getRegisterAddress(void) const {
        return (TheStructure.Common.RegisterAddress << REGISTERADDRESS_BIT_SHIFT);
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x2,
        MEMORYADDRESS_ALIGN_SIZE = 0x4,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress(void) const {
        return (TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT);
    }
} MI_LOAD_REGISTER_MEM;
STATIC_ASSERT(16 == sizeof(MI_LOAD_REGISTER_MEM));

typedef struct tagMI_LOAD_REGISTER_REG {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 17);
            uint32_t AddCsMmioStartOffsetSource : BITFIELD_RANGE(18, 18);
            uint32_t AddCsMmioStartOffsetDestination : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint32_t SourceRegisterAddress : BITFIELD_RANGE(2, 22);
            uint32_t Reserved_55 : BITFIELD_RANGE(23, 31);
            uint32_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint32_t DestinationRegisterAddress : BITFIELD_RANGE(2, 22);
            uint32_t Reserved_87 : BITFIELD_RANGE(23, 31);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_LOAD_REGISTER_REG = 0x2a,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_LOAD_REGISTER_REG;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_LOAD_REGISTER_REG sInit(void) {
        MI_LOAD_REGISTER_REG state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setAddCsMmioStartOffsetSource(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffsetSource = value;
    }
    inline uint32_t getAddCsMmioStartOffsetSource(void) const {
        return (TheStructure.Common.AddCsMmioStartOffsetSource);
    }
    inline void setAddCsMmioStartOffsetDestination(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffsetDestination = value;
    }
    inline uint32_t getAddCsMmioStartOffsetDestination(void) const {
        return (TheStructure.Common.AddCsMmioStartOffsetDestination);
    }
    typedef enum tagSOURCEREGISTERADDRESS {
        SOURCEREGISTERADDRESS_BIT_SHIFT = 0x2,
        SOURCEREGISTERADDRESS_ALIGN_SIZE = 0x4,
    } SOURCEREGISTERADDRESS;
    inline void setSourceRegisterAddress(const uint32_t value) {
        TheStructure.Common.SourceRegisterAddress = value >> SOURCEREGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getSourceRegisterAddress(void) const {
        return (TheStructure.Common.SourceRegisterAddress << SOURCEREGISTERADDRESS_BIT_SHIFT);
    }
    typedef enum tagDESTINATIONREGISTERADDRESS {
        DESTINATIONREGISTERADDRESS_BIT_SHIFT = 0x2,
        DESTINATIONREGISTERADDRESS_ALIGN_SIZE = 0x4,
    } DESTINATIONREGISTERADDRESS;
    inline void setDestinationRegisterAddress(const uint32_t value) {
        TheStructure.Common.DestinationRegisterAddress = value >> DESTINATIONREGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getDestinationRegisterAddress(void) const {
        return (TheStructure.Common.DestinationRegisterAddress << DESTINATIONREGISTERADDRESS_BIT_SHIFT);
    }
} MI_LOAD_REGISTER_REG;
STATIC_ASSERT(12 == sizeof(MI_LOAD_REGISTER_REG));

typedef struct tagMI_NOOP {
    union tagTheStructure {
        struct tagCommon {
            uint32_t IdentificationNumber : BITFIELD_RANGE(0, 21);
            uint32_t IdentificationNumberRegisterWriteEnable : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_NOOP = 0x0,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_NOOP;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_NOOP sInit(void) {
        MI_NOOP state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setIdentificationNumber(const uint32_t value) {
        TheStructure.Common.IdentificationNumber = value;
    }
    inline uint32_t getIdentificationNumber(void) const {
        return (TheStructure.Common.IdentificationNumber);
    }
    inline void setIdentificationNumberRegisterWriteEnable(const bool value) {
        TheStructure.Common.IdentificationNumberRegisterWriteEnable = value;
    }
    inline bool getIdentificationNumberRegisterWriteEnable(void) const {
        return (TheStructure.Common.IdentificationNumberRegisterWriteEnable);
    }
} MI_NOOP;
STATIC_ASSERT(4 == sizeof(MI_NOOP));

typedef struct tagMI_STORE_REGISTER_MEM {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 18);
            uint32_t AddCsMmioStartOffset : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 20);
            uint32_t PredicateEnable : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint32_t RegisterAddress : BITFIELD_RANGE(2, 22);
            uint32_t Reserved_55 : BITFIELD_RANGE(23, 31);
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint64_t MemoryAddress : BITFIELD_RANGE(2, 63);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_STORE_REGISTER_MEM = 0x24,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    typedef enum tagPATCH_CONSTANTS {
        MEMORYADDRESS_BYTEOFFSET = 0x8,
        MEMORYADDRESS_INDEX = 0x2,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_STORE_REGISTER_MEM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_STORE_REGISTER_MEM sInit(void) {
        MI_STORE_REGISTER_MEM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setAddCsMmioStartOffset(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffset = value;
    }
    inline uint32_t getAddCsMmioStartOffset(void) const {
        return (TheStructure.Common.AddCsMmioStartOffset);
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable(void) const {
        return (TheStructure.Common.PredicateEnable);
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt(void) const {
        return (TheStructure.Common.UseGlobalGtt);
    }
    typedef enum tagREGISTERADDRESS {
        REGISTERADDRESS_BIT_SHIFT = 0x2,
        REGISTERADDRESS_ALIGN_SIZE = 0x4,
    } REGISTERADDRESS;
    inline void setRegisterAddress(const uint32_t value) {
        TheStructure.Common.RegisterAddress = value >> REGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getRegisterAddress(void) const {
        return (TheStructure.Common.RegisterAddress << REGISTERADDRESS_BIT_SHIFT);
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x2,
        MEMORYADDRESS_ALIGN_SIZE = 0x4,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress(void) const {
        return (TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT);
    }
} MI_STORE_REGISTER_MEM;
STATIC_ASSERT(16 == sizeof(MI_STORE_REGISTER_MEM));

typedef struct tagPIPELINE_SELECT {
    union tagTheStructure {
        struct tagCommon {
            uint32_t PipelineSelection : BITFIELD_RANGE(0, 1);
            uint32_t RenderSliceCommonPowerGateEnable : BITFIELD_RANGE(2, 2);
            uint32_t RenderSamplerPowerGateEnable : BITFIELD_RANGE(3, 3);
            uint32_t MediaSamplerDopClockGateEnable : BITFIELD_RANGE(4, 4);
            uint32_t ForceMediaAwake : BITFIELD_RANGE(5, 5);
            uint32_t MediaSamplerPowerClockGateDisable : BITFIELD_RANGE(6, 6);
            uint32_t Reserved_7 : BITFIELD_RANGE(7, 7);
            uint32_t MaskBits : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagPIPELINE_SELECTION {
        PIPELINE_SELECTION_3D = 0x0,
        PIPELINE_SELECTION_MEDIA = 0x1,
        PIPELINE_SELECTION_GPGPU = 0x2,
    } PIPELINE_SELECTION;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_PIPELINE_SELECT = 0x4,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_SINGLE_DW = 0x1,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.PipelineSelection = PIPELINE_SELECTION_3D;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_PIPELINE_SELECT;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_SINGLE_DW;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagPIPELINE_SELECT sInit(void) {
        PIPELINE_SELECT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setPipelineSelection(const PIPELINE_SELECTION value) {
        TheStructure.Common.PipelineSelection = value;
    }
    inline PIPELINE_SELECTION getPipelineSelection(void) const {
        return static_cast<PIPELINE_SELECTION>(TheStructure.Common.PipelineSelection);
    }
    inline void setRenderSliceCommonPowerGateEnable(const bool value) {
        TheStructure.Common.RenderSliceCommonPowerGateEnable = value;
    }
    inline bool getRenderSliceCommonPowerGateEnable(void) const {
        return (TheStructure.Common.RenderSliceCommonPowerGateEnable);
    }
    inline void setRenderSamplerPowerGateEnable(const bool value) {
        TheStructure.Common.RenderSamplerPowerGateEnable = value;
    }
    inline bool getRenderSamplerPowerGateEnable(void) const {
        return (TheStructure.Common.RenderSamplerPowerGateEnable);
    }
    inline void setMediaSamplerDopClockGateEnable(const bool value) {
        TheStructure.Common.MediaSamplerDopClockGateEnable = value;
    }
    inline bool getMediaSamplerDopClockGateEnable(void) const {
        return (TheStructure.Common.MediaSamplerDopClockGateEnable);
    }
    inline void setForceMediaAwake(const bool value) {
        TheStructure.Common.ForceMediaAwake = value;
    }
    inline bool getForceMediaAwake(void) const {
        return (TheStructure.Common.ForceMediaAwake);
    }
    inline void setMediaSamplerPowerClockGateDisable(const uint32_t value) {
        TheStructure.Common.MediaSamplerPowerClockGateDisable = value;
    }
    inline uint32_t getMediaSamplerPowerClockGateDisable(void) const {
        return (TheStructure.Common.MediaSamplerPowerClockGateDisable);
    }
    inline void setMaskBits(const uint32_t value) {
        TheStructure.Common.MaskBits = value;
    }
    inline uint32_t getMaskBits(void) const {
        return (TheStructure.Common.MaskBits);
    }
} PIPELINE_SELECT;
STATIC_ASSERT(4 == sizeof(PIPELINE_SELECT));

typedef struct tagPIPE_CONTROL {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t DepthCacheFlushEnable : BITFIELD_RANGE(0, 0);
            uint32_t StallAtPixelScoreboard : BITFIELD_RANGE(1, 1);
            uint32_t StateCacheInvalidationEnable : BITFIELD_RANGE(2, 2);
            uint32_t ConstantCacheInvalidationEnable : BITFIELD_RANGE(3, 3);
            uint32_t VfCacheInvalidationEnable : BITFIELD_RANGE(4, 4);
            uint32_t DcFlushEnable : BITFIELD_RANGE(5, 5);
            uint32_t ProtectedMemoryApplicationId : BITFIELD_RANGE(6, 6);
            uint32_t PipeControlFlushEnable : BITFIELD_RANGE(7, 7);
            uint32_t NotifyEnable : BITFIELD_RANGE(8, 8);
            uint32_t IndirectStatePointersDisable : BITFIELD_RANGE(9, 9);
            uint32_t TextureCacheInvalidationEnable : BITFIELD_RANGE(10, 10);
            uint32_t InstructionCacheInvalidateEnable : BITFIELD_RANGE(11, 11);
            uint32_t RenderTargetCacheFlushEnable : BITFIELD_RANGE(12, 12);
            uint32_t DepthStallEnable : BITFIELD_RANGE(13, 13);
            uint32_t PostSyncOperation : BITFIELD_RANGE(14, 15);
            uint32_t GenericMediaStateClear : BITFIELD_RANGE(16, 16);
            uint32_t PsdSyncEnable : BITFIELD_RANGE(17, 17);
            uint32_t TlbInvalidate : BITFIELD_RANGE(18, 18);
            uint32_t GlobalSnapshotCountReset : BITFIELD_RANGE(19, 19);
            uint32_t CommandStreamerStallEnable : BITFIELD_RANGE(20, 20);
            uint32_t StoreDataIndex : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_54 : BITFIELD_RANGE(22, 22);
            uint32_t LriPostSyncOperation : BITFIELD_RANGE(23, 23);
            uint32_t DestinationAddressType : BITFIELD_RANGE(24, 24);
            uint32_t Reserved_57 : BITFIELD_RANGE(25, 25);
            uint32_t FlushLlc : BITFIELD_RANGE(26, 26);
            uint32_t ProtectedMemoryDisable : BITFIELD_RANGE(27, 27);
            uint32_t TileCacheFlushEnable : BITFIELD_RANGE(28, 28);
            uint32_t Reserved_61 : BITFIELD_RANGE(29, 31);
            uint32_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint32_t Address : BITFIELD_RANGE(2, 31);
            uint32_t AddressHigh;
            uint64_t ImmediateData;
        } Common;
        uint32_t RawData[6];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x4,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_PIPE_CONTROL = 0x0,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_PIPE_CONTROL = 0x2,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_3D = 0x3,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagPOST_SYNC_OPERATION {
        POST_SYNC_OPERATION_NO_WRITE = 0x0,
        POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA = 0x1,
        POST_SYNC_OPERATION_WRITE_PS_DEPTH_COUNT = 0x2,
        POST_SYNC_OPERATION_WRITE_TIMESTAMP = 0x3,
    } POST_SYNC_OPERATION;
    typedef enum tagGLOBAL_SNAPSHOT_COUNT_RESET {
        GLOBAL_SNAPSHOT_COUNT_RESET_DON_T_RESET = 0x0,
        GLOBAL_SNAPSHOT_COUNT_RESET_RESET = 0x1,
    } GLOBAL_SNAPSHOT_COUNT_RESET;
    typedef enum tagLRI_POST_SYNC_OPERATION {
        LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION = 0x0,
        LRI_POST_SYNC_OPERATION_MMIO_WRITE_IMMEDIATE_DATA = 0x1,
    } LRI_POST_SYNC_OPERATION;
    typedef enum tagDESTINATION_ADDRESS_TYPE {
        DESTINATION_ADDRESS_TYPE_PPGTT = 0x0,
        DESTINATION_ADDRESS_TYPE_GGTT = 0x1,
    } DESTINATION_ADDRESS_TYPE;
    typedef enum tagPATCH_CONSTANTS {
        ADDRESS_BYTEOFFSET = 0x8,
        ADDRESS_INDEX = 0x2,
        ADDRESSHIGH_BYTEOFFSET = 0xc,
        ADDRESSHIGH_INDEX = 0x3,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_PIPE_CONTROL;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_PIPE_CONTROL;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_3D;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.PostSyncOperation = POST_SYNC_OPERATION_NO_WRITE;
        TheStructure.Common.GlobalSnapshotCountReset = GLOBAL_SNAPSHOT_COUNT_RESET_DON_T_RESET;
        TheStructure.Common.LriPostSyncOperation = LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION;
        TheStructure.Common.DestinationAddressType = DESTINATION_ADDRESS_TYPE_PPGTT;
    }
    static tagPIPE_CONTROL sInit(void) {
        PIPE_CONTROL state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 6);
        return TheStructure.RawData[index];
    }
    inline void setDepthCacheFlushEnable(const bool value) {
        TheStructure.Common.DepthCacheFlushEnable = value;
    }
    inline bool getDepthCacheFlushEnable(void) const {
        return (TheStructure.Common.DepthCacheFlushEnable);
    }
    inline void setStallAtPixelScoreboard(const bool value) {
        TheStructure.Common.StallAtPixelScoreboard = value;
    }
    inline bool getStallAtPixelScoreboard(void) const {
        return (TheStructure.Common.StallAtPixelScoreboard);
    }
    inline void setStateCacheInvalidationEnable(const bool value) {
        TheStructure.Common.StateCacheInvalidationEnable = value;
    }
    inline bool getStateCacheInvalidationEnable(void) const {
        return (TheStructure.Common.StateCacheInvalidationEnable);
    }
    inline void setConstantCacheInvalidationEnable(const bool value) {
        TheStructure.Common.ConstantCacheInvalidationEnable = value;
    }
    inline bool getConstantCacheInvalidationEnable(void) const {
        return (TheStructure.Common.ConstantCacheInvalidationEnable);
    }
    inline void setVfCacheInvalidationEnable(const bool value) {
        TheStructure.Common.VfCacheInvalidationEnable = value;
    }
    inline bool getVfCacheInvalidationEnable(void) const {
        return (TheStructure.Common.VfCacheInvalidationEnable);
    }
    inline void setDcFlushEnable(const bool value) {
        TheStructure.Common.DcFlushEnable = value;
    }
    inline bool getDcFlushEnable(void) const {
        return (TheStructure.Common.DcFlushEnable);
    }
    inline void setProtectedMemoryApplicationId(const uint32_t value) {
        TheStructure.Common.ProtectedMemoryApplicationId = value;
    }
    inline uint32_t getProtectedMemoryApplicationId(void) const {
        return (TheStructure.Common.ProtectedMemoryApplicationId);
    }
    inline void setPipeControlFlushEnable(const bool value) {
        TheStructure.Common.PipeControlFlushEnable = value;
    }
    inline bool getPipeControlFlushEnable(void) const {
        return (TheStructure.Common.PipeControlFlushEnable);
    }
    inline void setNotifyEnable(const bool value) {
        TheStructure.Common.NotifyEnable = value;
    }
    inline bool getNotifyEnable(void) const {
        return (TheStructure.Common.NotifyEnable);
    }
    inline void setIndirectStatePointersDisable(const bool value) {
        TheStructure.Common.IndirectStatePointersDisable = value;
    }
    inline bool getIndirectStatePointersDisable(void) const {
        return (TheStructure.Common.IndirectStatePointersDisable);
    }
    inline void setTextureCacheInvalidationEnable(const bool value) {
        TheStructure.Common.TextureCacheInvalidationEnable = value;
    }
    inline bool getTextureCacheInvalidationEnable(void) const {
        return (TheStructure.Common.TextureCacheInvalidationEnable);
    }
    inline void setInstructionCacheInvalidateEnable(const bool value) {
        TheStructure.Common.InstructionCacheInvalidateEnable = value;
    }
    inline bool getInstructionCacheInvalidateEnable(void) const {
        return (TheStructure.Common.InstructionCacheInvalidateEnable);
    }
    inline void setRenderTargetCacheFlushEnable(const bool value) {
        TheStructure.Common.RenderTargetCacheFlushEnable = value;
    }
    inline bool getRenderTargetCacheFlushEnable(void) const {
        return (TheStructure.Common.RenderTargetCacheFlushEnable);
    }
    inline void setDepthStallEnable(const bool value) {
        TheStructure.Common.DepthStallEnable = value;
    }
    inline bool getDepthStallEnable(void) const {
        return (TheStructure.Common.DepthStallEnable);
    }
    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }
    inline POST_SYNC_OPERATION getPostSyncOperation(void) const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.PostSyncOperation);
    }
    inline void setGenericMediaStateClear(const bool value) {
        TheStructure.Common.GenericMediaStateClear = value;
    }
    inline bool getGenericMediaStateClear(void) const {
        return (TheStructure.Common.GenericMediaStateClear);
    }
    inline void setPsdSyncEnable(const bool value) {
        TheStructure.Common.PsdSyncEnable = value;
    }
    inline bool getPsdSyncEnable(void) const {
        return (TheStructure.Common.PsdSyncEnable);
    }
    inline void setTlbInvalidate(const uint32_t value) {
        TheStructure.Common.TlbInvalidate = value;
    }
    inline uint32_t getTlbInvalidate(void) const {
        return (TheStructure.Common.TlbInvalidate);
    }
    inline void setGlobalSnapshotCountReset(const GLOBAL_SNAPSHOT_COUNT_RESET value) {
        TheStructure.Common.GlobalSnapshotCountReset = value;
    }
    inline GLOBAL_SNAPSHOT_COUNT_RESET getGlobalSnapshotCountReset(void) const {
        return static_cast<GLOBAL_SNAPSHOT_COUNT_RESET>(TheStructure.Common.GlobalSnapshotCountReset);
    }
    inline void setCommandStreamerStallEnable(const uint32_t value) {
        TheStructure.Common.CommandStreamerStallEnable = value;
    }
    inline uint32_t getCommandStreamerStallEnable(void) const {
        return (TheStructure.Common.CommandStreamerStallEnable);
    }
    inline void setStoreDataIndex(const uint32_t value) {
        TheStructure.Common.StoreDataIndex = value;
    }
    inline uint32_t getStoreDataIndex(void) const {
        return (TheStructure.Common.StoreDataIndex);
    }
    inline void setLriPostSyncOperation(const LRI_POST_SYNC_OPERATION value) {
        TheStructure.Common.LriPostSyncOperation = value;
    }
    inline LRI_POST_SYNC_OPERATION getLriPostSyncOperation(void) const {
        return static_cast<LRI_POST_SYNC_OPERATION>(TheStructure.Common.LriPostSyncOperation);
    }
    inline void setDestinationAddressType(const DESTINATION_ADDRESS_TYPE value) {
        TheStructure.Common.DestinationAddressType = value;
    }
    inline DESTINATION_ADDRESS_TYPE getDestinationAddressType(void) const {
        return static_cast<DESTINATION_ADDRESS_TYPE>(TheStructure.Common.DestinationAddressType);
    }
    inline void setFlushLlc(const bool value) {
        TheStructure.Common.FlushLlc = value;
    }
    inline bool getFlushLlc(void) const {
        return (TheStructure.Common.FlushLlc);
    }
    inline void setProtectedMemoryDisable(const uint32_t value) {
        TheStructure.Common.ProtectedMemoryDisable = value;
    }
    inline uint32_t getProtectedMemoryDisable(void) const {
        return (TheStructure.Common.ProtectedMemoryDisable);
    }
    inline void setTileCacheFlushEnable(const uint32_t value) {
        TheStructure.Common.TileCacheFlushEnable = value;
    }
    inline uint32_t getTileCacheFlushEnable(void) const {
        return (TheStructure.Common.TileCacheFlushEnable);
    }
    typedef enum tagADDRESS {
        ADDRESS_BIT_SHIFT = 0x2,
        ADDRESS_ALIGN_SIZE = 0x4,
    } ADDRESS;
    inline void setAddress(const uint32_t value) {
        TheStructure.Common.Address = value >> ADDRESS_BIT_SHIFT;
    }
    inline uint32_t getAddress(void) const {
        return (TheStructure.Common.Address << ADDRESS_BIT_SHIFT);
    }
    inline void setAddressHigh(const uint32_t value) {
        TheStructure.Common.AddressHigh = value;
    }
    inline uint32_t getAddressHigh(void) const {
        return (TheStructure.Common.AddressHigh);
    }
    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }
    inline uint64_t getImmediateData(void) const {
        return (TheStructure.Common.ImmediateData);
    }
} PIPE_CONTROL;
STATIC_ASSERT(24 == sizeof(PIPE_CONTROL));

typedef struct tagRENDER_SURFACE_STATE {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 5);
            uint32_t MediaBoundaryPixelMode : BITFIELD_RANGE(6, 7);
            uint32_t RenderCacheReadWriteMode : BITFIELD_RANGE(8, 8);
            uint32_t SamplerL2OutOfOrderModeDisable : BITFIELD_RANGE(9, 9);
            uint32_t VerticalLineStrideOffset : BITFIELD_RANGE(10, 10);
            uint32_t VerticalLineStride : BITFIELD_RANGE(11, 11);
            uint32_t TileMode : BITFIELD_RANGE(12, 13);
            uint32_t SurfaceHorizontalAlignment : BITFIELD_RANGE(14, 15);
            uint32_t SurfaceVerticalAlignment : BITFIELD_RANGE(16, 17);
            uint32_t SurfaceFormat : BITFIELD_RANGE(18, 26);
            uint32_t Astc_Enable : BITFIELD_RANGE(27, 27);
            uint32_t SurfaceArray : BITFIELD_RANGE(28, 28);
            uint32_t SurfaceType : BITFIELD_RANGE(29, 31);
            uint32_t SurfaceQpitch : BITFIELD_RANGE(0, 14);
            uint32_t SampleTapDiscardDisable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_48 : BITFIELD_RANGE(16, 17);
            uint32_t CornerTexelMode : BITFIELD_RANGE(18, 18);
            uint32_t BaseMipLevel : BITFIELD_RANGE(19, 23);
            uint32_t MemoryObjectControlState_Reserved : BITFIELD_RANGE(24, 24);
            uint32_t MemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(25, 30);
            uint32_t EnableUnormPathInColorPipe : BITFIELD_RANGE(31, 31);
            uint32_t Width : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_78 : BITFIELD_RANGE(14, 15);
            uint32_t Height : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_94 : BITFIELD_RANGE(30, 31);
            uint32_t SurfacePitch : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_114 : BITFIELD_RANGE(18, 18);
            uint32_t StandardTilingModeExtensions : BITFIELD_RANGE(19, 19);
            uint32_t TileAddressMappingMode : BITFIELD_RANGE(20, 20);
            uint32_t Depth : BITFIELD_RANGE(21, 31);
            uint32_t Reserved_128;
            uint32_t MipCountLod : BITFIELD_RANGE(0, 3);
            uint32_t SurfaceMinLod : BITFIELD_RANGE(4, 7);
            uint32_t MipTailStartLod : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_172 : BITFIELD_RANGE(12, 13);
            uint32_t CoherencyType : BITFIELD_RANGE(14, 14);
            uint32_t Reserved_175 : BITFIELD_RANGE(15, 17);
            uint32_t TiledResourceMode : BITFIELD_RANGE(18, 19);
            uint32_t EwaDisableForCube : BITFIELD_RANGE(20, 20);
            uint32_t YOffset : BITFIELD_RANGE(21, 23);
            uint32_t Reserved_184 : BITFIELD_RANGE(24, 24);
            uint32_t XOffset : BITFIELD_RANGE(25, 31);
            uint32_t Reserved_192 : BITFIELD_RANGE(0, 14);
            uint32_t YuvInterpolationEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_208 : BITFIELD_RANGE(16, 31);
            uint32_t ResourceMinLod : BITFIELD_RANGE(0, 11);
            uint32_t Reserved_236 : BITFIELD_RANGE(12, 15);
            uint32_t ShaderChannelSelectAlpha : BITFIELD_RANGE(16, 18);
            uint32_t ShaderChannelSelectBlue : BITFIELD_RANGE(19, 21);
            uint32_t ShaderChannelSelectGreen : BITFIELD_RANGE(22, 24);
            uint32_t ShaderChannelSelectRed : BITFIELD_RANGE(25, 27);
            uint32_t Reserved_252 : BITFIELD_RANGE(28, 29);
            uint32_t MemoryCompressionEnable : BITFIELD_RANGE(30, 30);
            uint32_t MemoryCompressionMode : BITFIELD_RANGE(31, 31);
            uint64_t SurfaceBaseAddress;
            uint64_t QuiltWidth : BITFIELD_RANGE(0, 4);
            uint64_t QuiltHeight : BITFIELD_RANGE(5, 9);
            uint64_t ClearValueAddressEnable : BITFIELD_RANGE(10, 10);
            uint64_t Reserved_331 : BITFIELD_RANGE(11, 63);
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } Common;
        struct tagSurfaceTypeIsnotSurftype_Cube {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } SurfaceTypeIsnotSurftype_Cube;
        struct tagSurfaceTypeIsSurftype_Cube {
            uint32_t CubeFaceEnable_PositiveZ : BITFIELD_RANGE(0, 0);
            uint32_t CubeFaceEnable_NegativeZ : BITFIELD_RANGE(1, 1);
            uint32_t CubeFaceEnable_PositiveY : BITFIELD_RANGE(2, 2);
            uint32_t CubeFaceEnable_NegativeY : BITFIELD_RANGE(3, 3);
            uint32_t CubeFaceEnable_PositiveX : BITFIELD_RANGE(4, 4);
            uint32_t CubeFaceEnable_NegativeX : BITFIELD_RANGE(5, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 31);
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } SurfaceTypeIsSurftype_Cube;
        struct tagSurfaceTypeIsSurftype_Strbuf {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } SurfaceTypeIsSurftype_Strbuf;
        struct tagSurfaceTypeIsnotSurftype_Strbuf {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t MultisamplePositionPaletteIndex : BITFIELD_RANGE(0, 2);
            uint32_t NumberOfMultisamples : BITFIELD_RANGE(3, 5);
            uint32_t MultisampledSurfaceStorageFormat : BITFIELD_RANGE(6, 6);
            uint32_t RenderTargetViewExtent : BITFIELD_RANGE(7, 17);
            uint32_t MinimumArrayElement : BITFIELD_RANGE(18, 28);
            uint32_t RenderTargetAndSampleUnormRotation : BITFIELD_RANGE(29, 30);
            uint32_t Reserved_159 : BITFIELD_RANGE(31, 31);
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } SurfaceTypeIsnotSurftype_Strbuf;
        struct tag_SurfaceFormatIsnotPlanar {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t AuxiliarySurfaceMode : BITFIELD_RANGE(0, 2);
            uint32_t AuxiliarySurfacePitch : BITFIELD_RANGE(3, 11);
            uint32_t Reserved_204 : BITFIELD_RANGE(12, 15);
            uint32_t AuxiliarySurfaceQpitch : BITFIELD_RANGE(16, 30);
            uint32_t Reserved_223 : BITFIELD_RANGE(31, 31);
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } _SurfaceFormatIsnotPlanar;
        struct tag_SurfaceFormatIsPlanar {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t YOffsetForUOrUvPlane : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t XOffsetForUOrUvPlane : BITFIELD_RANGE(16, 29);
            uint32_t HalfPitchForChroma : BITFIELD_RANGE(30, 30);
            uint32_t SeparateUvPlaneEnable : BITFIELD_RANGE(31, 31);
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320 : BITFIELD_RANGE(0, 31);
            uint64_t YOffsetForVPlane : BITFIELD_RANGE(32, 45);
            uint64_t Reserved_366 : BITFIELD_RANGE(46, 47);
            uint64_t XOffsetForVPlane : BITFIELD_RANGE(48, 61);
            uint64_t Reserved_382 : BITFIELD_RANGE(62, 63);
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } _SurfaceFormatIsPlanar;
        struct tag_SurfaceFormatIsnotPlanarAndMemoryCompressionEnableIs0 {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320 : BITFIELD_RANGE(0, 11);
            uint64_t AuxiliarySurfaceBaseAddress : BITFIELD_RANGE(12, 63);
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } _SurfaceFormatIsnotPlanarAndMemoryCompressionEnableIs0;
        struct tagMemoryCompressionEnableIs1 {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320 : BITFIELD_RANGE(0, 20);
            uint64_t AuxiliaryTableIndexForMediaCompressedSurface : BITFIELD_RANGE(21, 31);
            uint64_t Reserved_352 : BITFIELD_RANGE(32, 63);
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } MemoryCompressionEnableIs1;
        struct tag__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsEnable {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384 : BITFIELD_RANGE(0, 4);
            uint32_t ClearColorConversionEnable : BITFIELD_RANGE(5, 5);
            uint32_t Reserved_390 : BITFIELD_RANGE(6, 31);
            uint32_t ClearColorAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_432 : BITFIELD_RANGE(16, 31);
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } __AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsEnable;
        struct tag_AuxiliarySurfaceModeIsnotAux_Ccs_DAnd_AuxiliarySurfaceModeIsnotAux_Ccs_EAnd_AuxiliarySurfaceModeIsnotAux_Hiz {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } _AuxiliarySurfaceModeIsnotAux_Ccs_DAnd_AuxiliarySurfaceModeIsnotAux_Ccs_EAnd_AuxiliarySurfaceModeIsnotAux_Hiz;
        struct tag_AuxiliarySurfaceModeIsAux_HizAnd_ClearValueAddressEnableIsEnable {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384 : BITFIELD_RANGE(0, 5);
            uint32_t ClearDepthAddressLow : BITFIELD_RANGE(6, 31);
            uint32_t ClearDepthAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_432 : BITFIELD_RANGE(16, 31);
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } _AuxiliarySurfaceModeIsAux_HizAnd_ClearValueAddressEnableIsEnable;
        struct tag__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EOrAuxiliarySurfaceModeIsAux_HizAndClearValueAddressEnableIsDisable {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t RedClearColor;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } __AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EOrAuxiliarySurfaceModeIsAux_HizAndClearValueAddressEnableIsDisable;
        struct tagClearValueAddressEnableIsEnable {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384 : BITFIELD_RANGE(0, 5);
            uint32_t ClearColorAddress : BITFIELD_RANGE(6, 31);
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } ClearValueAddressEnableIsEnable;
        struct tagAuxiliarySurfaceModeIsAux_Hiz {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } AuxiliarySurfaceModeIsAux_Hiz;
        struct tag__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsDisable {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384;
            uint32_t GreenClearColor;
            uint32_t BlueClearColor;
            uint32_t AlphaClearColor;
        } __AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsDisable;
        struct tag__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EOrAuxiliarySurfaceModeIsAux_HizAndClearValueAddressEnableIsEnable {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
            uint64_t Reserved_256;
            uint64_t Reserved_320;
            uint32_t Reserved_384;
            uint32_t Reserved_416;
            uint32_t Reserved_448;
            uint32_t Reserved_480;
        } __AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EOrAuxiliarySurfaceModeIsAux_HizAndClearValueAddressEnableIsEnable;
        uint32_t RawData[16];
    } TheStructure;
    typedef enum tagMEDIA_BOUNDARY_PIXEL_MODE {
        MEDIA_BOUNDARY_PIXEL_MODE_NORMAL_MODE = 0x0,
        MEDIA_BOUNDARY_PIXEL_MODE_PROGRESSIVE_FRAME = 0x2,
        MEDIA_BOUNDARY_PIXEL_MODE_INTERLACED_FRAME = 0x3,
    } MEDIA_BOUNDARY_PIXEL_MODE;
    typedef enum tagRENDER_CACHE_READ_WRITE_MODE {
        RENDER_CACHE_READ_WRITE_MODE_WRITE_ONLY_CACHE = 0x0,
        RENDER_CACHE_READ_WRITE_MODE_READ_WRITE_CACHE = 0x1,
    } RENDER_CACHE_READ_WRITE_MODE;
    typedef enum tagTILE_MODE {
        TILE_MODE_LINEAR = 0x0,
        TILE_MODE_WMAJOR = 0x1,
        TILE_MODE_XMAJOR = 0x2,
        TILE_MODE_YMAJOR = 0x3,
    } TILE_MODE;
    typedef enum tagSURFACE_HORIZONTAL_ALIGNMENT {
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4 = 0x1,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_8 = 0x2,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_16 = 0x3,
    } SURFACE_HORIZONTAL_ALIGNMENT;
    typedef enum tagSURFACE_VERTICAL_ALIGNMENT {
        SURFACE_VERTICAL_ALIGNMENT_VALIGN_4 = 0x1,
        SURFACE_VERTICAL_ALIGNMENT_VALIGN_8 = 0x2,
        SURFACE_VERTICAL_ALIGNMENT_VALIGN_16 = 0x3,
    } SURFACE_VERTICAL_ALIGNMENT;
    typedef enum tagSURFACE_FORMAT {
        SURFACE_FORMAT_R32G32B32A32_FLOAT = 0x0,
        SURFACE_FORMAT_R32G32B32A32_SINT = 0x1,
        SURFACE_FORMAT_R32G32B32A32_UINT = 0x2,
        SURFACE_FORMAT_R32G32B32A32_UNORM = 0x3,
        SURFACE_FORMAT_R32G32B32A32_SNORM = 0x4,
        SURFACE_FORMAT_R64G64_FLOAT = 0x5,
        SURFACE_FORMAT_R32G32B32X32_FLOAT = 0x6,
        SURFACE_FORMAT_R32G32B32A32_SSCALED = 0x7,
        SURFACE_FORMAT_R32G32B32A32_USCALED = 0x8,
        SURFACE_FORMAT_R32G32B32A32_SFIXED = 0x20,
        SURFACE_FORMAT_R64G64_PASSTHRU = 0x21,
        SURFACE_FORMAT_R32G32B32_FLOAT = 0x40,
        SURFACE_FORMAT_R32G32B32_SINT = 0x41,
        SURFACE_FORMAT_R32G32B32_UINT = 0x42,
        SURFACE_FORMAT_R32G32B32_UNORM = 0x43,
        SURFACE_FORMAT_R32G32B32_SNORM = 0x44,
        SURFACE_FORMAT_R32G32B32_SSCALED = 0x45,
        SURFACE_FORMAT_R32G32B32_USCALED = 0x46,
        SURFACE_FORMAT_R32G32B32_SFIXED = 0x50,
        SURFACE_FORMAT_R16G16B16A16_UNORM = 0x80,
        SURFACE_FORMAT_R16G16B16A16_SNORM = 0x81,
        SURFACE_FORMAT_R16G16B16A16_SINT = 0x82,
        SURFACE_FORMAT_R16G16B16A16_UINT = 0x83,
        SURFACE_FORMAT_R16G16B16A16_FLOAT = 0x84,
        SURFACE_FORMAT_R32G32_FLOAT = 0x85,
        SURFACE_FORMAT_R32G32_SINT = 0x86,
        SURFACE_FORMAT_R32G32_UINT = 0x87,
        SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS = 0x88,
        SURFACE_FORMAT_X32_TYPELESS_G8X24_UINT = 0x89,
        SURFACE_FORMAT_L32A32_FLOAT = 0x8a,
        SURFACE_FORMAT_R32G32_UNORM = 0x8b,
        SURFACE_FORMAT_R32G32_SNORM = 0x8c,
        SURFACE_FORMAT_R64_FLOAT = 0x8d,
        SURFACE_FORMAT_R16G16B16X16_UNORM = 0x8e,
        SURFACE_FORMAT_R16G16B16X16_FLOAT = 0x8f,
        SURFACE_FORMAT_A32X32_FLOAT = 0x90,
        SURFACE_FORMAT_L32X32_FLOAT = 0x91,
        SURFACE_FORMAT_I32X32_FLOAT = 0x92,
        SURFACE_FORMAT_R16G16B16A16_SSCALED = 0x93,
        SURFACE_FORMAT_R16G16B16A16_USCALED = 0x94,
        SURFACE_FORMAT_R32G32_SSCALED = 0x95,
        SURFACE_FORMAT_R32G32_USCALED = 0x96,
        SURFACE_FORMAT_R32G32_SFIXED = 0xa0,
        SURFACE_FORMAT_R64_PASSTHRU = 0xa1,
        SURFACE_FORMAT_B8G8R8A8_UNORM = 0xc0,
        SURFACE_FORMAT_B8G8R8A8_UNORM_SRGB = 0xc1,
        SURFACE_FORMAT_R10G10B10A2_UNORM = 0xc2,
        SURFACE_FORMAT_R10G10B10A2_UNORM_SRGB = 0xc3,
        SURFACE_FORMAT_R10G10B10A2_UINT = 0xc4,
        SURFACE_FORMAT_R10G10B10_SNORM_A2_UNORM = 0xc5,
        SURFACE_FORMAT_R8G8B8A8_UNORM = 0xc7,
        SURFACE_FORMAT_R8G8B8A8_UNORM_SRGB = 0xc8,
        SURFACE_FORMAT_R8G8B8A8_SNORM = 0xc9,
        SURFACE_FORMAT_R8G8B8A8_SINT = 0xca,
        SURFACE_FORMAT_R8G8B8A8_UINT = 0xcb,
        SURFACE_FORMAT_R16G16_UNORM = 0xcc,
        SURFACE_FORMAT_R16G16_SNORM = 0xcd,
        SURFACE_FORMAT_R16G16_SINT = 0xce,
        SURFACE_FORMAT_R16G16_UINT = 0xcf,
        SURFACE_FORMAT_R16G16_FLOAT = 0xd0,
        SURFACE_FORMAT_B10G10R10A2_UNORM = 0xd1,
        SURFACE_FORMAT_B10G10R10A2_UNORM_SRGB = 0xd2,
        SURFACE_FORMAT_R11G11B10_FLOAT = 0xd3,
        SURFACE_FORMAT_R32_SINT = 0xd6,
        SURFACE_FORMAT_R32_UINT = 0xd7,
        SURFACE_FORMAT_R32_FLOAT = 0xd8,
        SURFACE_FORMAT_R24_UNORM_X8_TYPELESS = 0xd9,
        SURFACE_FORMAT_X24_TYPELESS_G8_UINT = 0xda,
        SURFACE_FORMAT_L32_UNORM = 0xdd,
        SURFACE_FORMAT_A32_UNORM = 0xde,
        SURFACE_FORMAT_L16A16_UNORM = 0xdf,
        SURFACE_FORMAT_I24X8_UNORM = 0xe0,
        SURFACE_FORMAT_L24X8_UNORM = 0xe1,
        SURFACE_FORMAT_A24X8_UNORM = 0xe2,
        SURFACE_FORMAT_I32_FLOAT = 0xe3,
        SURFACE_FORMAT_L32_FLOAT = 0xe4,
        SURFACE_FORMAT_A32_FLOAT = 0xe5,
        SURFACE_FORMAT_X8B8_UNORM_G8R8_SNORM = 0xe6,
        SURFACE_FORMAT_A8X8_UNORM_G8R8_SNORM = 0xe7,
        SURFACE_FORMAT_B8X8_UNORM_G8R8_SNORM = 0xe8,
        SURFACE_FORMAT_B8G8R8X8_UNORM = 0xe9,
        SURFACE_FORMAT_B8G8R8X8_UNORM_SRGB = 0xea,
        SURFACE_FORMAT_R8G8B8X8_UNORM = 0xeb,
        SURFACE_FORMAT_R8G8B8X8_UNORM_SRGB = 0xec,
        SURFACE_FORMAT_R9G9B9E5_SHAREDEXP = 0xed,
        SURFACE_FORMAT_B10G10R10X2_UNORM = 0xee,
        SURFACE_FORMAT_L16A16_FLOAT = 0xf0,
        SURFACE_FORMAT_R32_UNORM = 0xf1,
        SURFACE_FORMAT_R32_SNORM = 0xf2,
        SURFACE_FORMAT_R10G10B10X2_USCALED = 0xf3,
        SURFACE_FORMAT_R8G8B8A8_SSCALED = 0xf4,
        SURFACE_FORMAT_R8G8B8A8_USCALED = 0xf5,
        SURFACE_FORMAT_R16G16_SSCALED = 0xf6,
        SURFACE_FORMAT_R16G16_USCALED = 0xf7,
        SURFACE_FORMAT_R32_SSCALED = 0xf8,
        SURFACE_FORMAT_R32_USCALED = 0xf9,
        SURFACE_FORMAT_B5G6R5_UNORM = 0x100,
        SURFACE_FORMAT_B5G6R5_UNORM_SRGB = 0x101,
        SURFACE_FORMAT_B5G5R5A1_UNORM = 0x102,
        SURFACE_FORMAT_B5G5R5A1_UNORM_SRGB = 0x103,
        SURFACE_FORMAT_B4G4R4A4_UNORM = 0x104,
        SURFACE_FORMAT_B4G4R4A4_UNORM_SRGB = 0x105,
        SURFACE_FORMAT_R8G8_UNORM = 0x106,
        SURFACE_FORMAT_R8G8_SNORM = 0x107,
        SURFACE_FORMAT_R8G8_SINT = 0x108,
        SURFACE_FORMAT_R8G8_UINT = 0x109,
        SURFACE_FORMAT_R16_UNORM = 0x10a,
        SURFACE_FORMAT_R16_SNORM = 0x10b,
        SURFACE_FORMAT_R16_SINT = 0x10c,
        SURFACE_FORMAT_R16_UINT = 0x10d,
        SURFACE_FORMAT_R16_FLOAT = 0x10e,
        SURFACE_FORMAT_A8P8_UNORM_PALETTE0 = 0x10f,
        SURFACE_FORMAT_A8P8_UNORM_PALETTE1 = 0x110,
        SURFACE_FORMAT_I16_UNORM = 0x111,
        SURFACE_FORMAT_L16_UNORM = 0x112,
        SURFACE_FORMAT_A16_UNORM = 0x113,
        SURFACE_FORMAT_L8A8_UNORM = 0x114,
        SURFACE_FORMAT_I16_FLOAT = 0x115,
        SURFACE_FORMAT_L16_FLOAT = 0x116,
        SURFACE_FORMAT_A16_FLOAT = 0x117,
        SURFACE_FORMAT_L8A8_UNORM_SRGB = 0x118,
        SURFACE_FORMAT_R5G5_SNORM_B6_UNORM = 0x119,
        SURFACE_FORMAT_B5G5R5X1_UNORM = 0x11a,
        SURFACE_FORMAT_B5G5R5X1_UNORM_SRGB = 0x11b,
        SURFACE_FORMAT_R8G8_SSCALED = 0x11c,
        SURFACE_FORMAT_R8G8_USCALED = 0x11d,
        SURFACE_FORMAT_R16_SSCALED = 0x11e,
        SURFACE_FORMAT_R16_USCALED = 0x11f,
        SURFACE_FORMAT_P8A8_UNORM_PALETTE0 = 0x122,
        SURFACE_FORMAT_P8A8_UNORM_PALETTE1 = 0x123,
        SURFACE_FORMAT_A1B5G5R5_UNORM = 0x124,
        SURFACE_FORMAT_A4B4G4R4_UNORM = 0x125,
        SURFACE_FORMAT_L8A8_UINT = 0x126,
        SURFACE_FORMAT_L8A8_SINT = 0x127,
        SURFACE_FORMAT_R8_UNORM = 0x140,
        SURFACE_FORMAT_R8_SNORM = 0x141,
        SURFACE_FORMAT_R8_SINT = 0x142,
        SURFACE_FORMAT_R8_UINT = 0x143,
        SURFACE_FORMAT_A8_UNORM = 0x144,
        SURFACE_FORMAT_I8_UNORM = 0x145,
        SURFACE_FORMAT_L8_UNORM = 0x146,
        SURFACE_FORMAT_P4A4_UNORM_PALETTE0 = 0x147,
        SURFACE_FORMAT_A4P4_UNORM_PALETTE0 = 0x148,
        SURFACE_FORMAT_R8_SSCALED = 0x149,
        SURFACE_FORMAT_R8_USCALED = 0x14a,
        SURFACE_FORMAT_P8_UNORM_PALETTE0 = 0x14b,
        SURFACE_FORMAT_L8_UNORM_SRGB = 0x14c,
        SURFACE_FORMAT_P8_UNORM_PALETTE1 = 0x14d,
        SURFACE_FORMAT_P4A4_UNORM_PALETTE1 = 0x14e,
        SURFACE_FORMAT_A4P4_UNORM_PALETTE1 = 0x14f,
        SURFACE_FORMAT_Y8_UNORM = 0x150,
        SURFACE_FORMAT_L8_UINT = 0x152,
        SURFACE_FORMAT_L8_SINT = 0x153,
        SURFACE_FORMAT_I8_UINT = 0x154,
        SURFACE_FORMAT_I8_SINT = 0x155,
        SURFACE_FORMAT_DXT1_RGB_SRGB = 0x180,
        SURFACE_FORMAT_R1_UNORM = 0x181,
        SURFACE_FORMAT_YCRCB_NORMAL = 0x182,
        SURFACE_FORMAT_YCRCB_SWAPUVY = 0x183,
        SURFACE_FORMAT_P2_UNORM_PALETTE0 = 0x184,
        SURFACE_FORMAT_P2_UNORM_PALETTE1 = 0x185,
        SURFACE_FORMAT_BC1_UNORM = 0x186,
        SURFACE_FORMAT_BC2_UNORM = 0x187,
        SURFACE_FORMAT_BC3_UNORM = 0x188,
        SURFACE_FORMAT_BC4_UNORM = 0x189,
        SURFACE_FORMAT_BC5_UNORM = 0x18a,
        SURFACE_FORMAT_BC1_UNORM_SRGB = 0x18b,
        SURFACE_FORMAT_BC2_UNORM_SRGB = 0x18c,
        SURFACE_FORMAT_BC3_UNORM_SRGB = 0x18d,
        SURFACE_FORMAT_MONO8 = 0x18e,
        SURFACE_FORMAT_YCRCB_SWAPUV = 0x18f,
        SURFACE_FORMAT_YCRCB_SWAPY = 0x190,
        SURFACE_FORMAT_DXT1_RGB = 0x191,
        SURFACE_FORMAT_FXT1 = 0x192,
        SURFACE_FORMAT_R8G8B8_UNORM = 0x193,
        SURFACE_FORMAT_R8G8B8_SNORM = 0x194,
        SURFACE_FORMAT_R8G8B8_SSCALED = 0x195,
        SURFACE_FORMAT_R8G8B8_USCALED = 0x196,
        SURFACE_FORMAT_R64G64B64A64_FLOAT = 0x197,
        SURFACE_FORMAT_R64G64B64_FLOAT = 0x198,
        SURFACE_FORMAT_BC4_SNORM = 0x199,
        SURFACE_FORMAT_BC5_SNORM = 0x19a,
        SURFACE_FORMAT_R16G16B16_FLOAT = 0x19b,
        SURFACE_FORMAT_R16G16B16_UNORM = 0x19c,
        SURFACE_FORMAT_R16G16B16_SNORM = 0x19d,
        SURFACE_FORMAT_R16G16B16_SSCALED = 0x19e,
        SURFACE_FORMAT_R16G16B16_USCALED = 0x19f,
        SURFACE_FORMAT_BC6H_SF16 = 0x1a1,
        SURFACE_FORMAT_BC7_UNORM = 0x1a2,
        SURFACE_FORMAT_BC7_UNORM_SRGB = 0x1a3,
        SURFACE_FORMAT_BC6H_UF16 = 0x1a4,
        SURFACE_FORMAT_PLANAR_420_8 = 0x1a5,
        SURFACE_FORMAT_PLANAR_420_16 = 0x1a6,
        SURFACE_FORMAT_R8G8B8_UNORM_SRGB = 0x1a8,
        SURFACE_FORMAT_ETC1_RGB8 = 0x1a9,
        SURFACE_FORMAT_ETC2_RGB8 = 0x1aa,
        SURFACE_FORMAT_EAC_R11 = 0x1ab,
        SURFACE_FORMAT_EAC_RG11 = 0x1ac,
        SURFACE_FORMAT_EAC_SIGNED_R11 = 0x1ad,
        SURFACE_FORMAT_EAC_SIGNED_RG11 = 0x1ae,
        SURFACE_FORMAT_ETC2_SRGB8 = 0x1af,
        SURFACE_FORMAT_R16G16B16_UINT = 0x1b0,
        SURFACE_FORMAT_R16G16B16_SINT = 0x1b1,
        SURFACE_FORMAT_R32_SFIXED = 0x1b2,
        SURFACE_FORMAT_R10G10B10A2_SNORM = 0x1b3,
        SURFACE_FORMAT_R10G10B10A2_USCALED = 0x1b4,
        SURFACE_FORMAT_R10G10B10A2_SSCALED = 0x1b5,
        SURFACE_FORMAT_R10G10B10A2_SINT = 0x1b6,
        SURFACE_FORMAT_B10G10R10A2_SNORM = 0x1b7,
        SURFACE_FORMAT_B10G10R10A2_USCALED = 0x1b8,
        SURFACE_FORMAT_B10G10R10A2_SSCALED = 0x1b9,
        SURFACE_FORMAT_B10G10R10A2_UINT = 0x1ba,
        SURFACE_FORMAT_B10G10R10A2_SINT = 0x1bb,
        SURFACE_FORMAT_R64G64B64A64_PASSTHRU = 0x1bc,
        SURFACE_FORMAT_R64G64B64_PASSTHRU = 0x1bd,
        SURFACE_FORMAT_ETC2_RGB8_PTA = 0x1c0,
        SURFACE_FORMAT_ETC2_SRGB8_PTA = 0x1c1,
        SURFACE_FORMAT_ETC2_EAC_RGBA8 = 0x1c2,
        SURFACE_FORMAT_ETC2_EAC_SRGB8_A8 = 0x1c3,
        SURFACE_FORMAT_R8G8B8_UINT = 0x1c8,
        SURFACE_FORMAT_R8G8B8_SINT = 0x1c9,
        SURFACE_FORMAT_RAW = 0x1ff,
    } SURFACE_FORMAT;
    typedef enum tagSURFACE_TYPE {
        SURFACE_TYPE_SURFTYPE_1D = 0x0,
        SURFACE_TYPE_SURFTYPE_2D = 0x1,
        SURFACE_TYPE_SURFTYPE_3D = 0x2,
        SURFACE_TYPE_SURFTYPE_CUBE = 0x3,
        SURFACE_TYPE_SURFTYPE_BUFFER = 0x4,
        SURFACE_TYPE_SURFTYPE_STRBUF = 0x5,
        SURFACE_TYPE_SURFTYPE_NULL = 0x7,
    } SURFACE_TYPE;
    typedef enum tagSAMPLE_TAP_DISCARD_DISABLE {
        SAMPLE_TAP_DISCARD_DISABLE_DISABLE = 0x0,
        SAMPLE_TAP_DISCARD_DISABLE_ENABLE = 0x1,
    } SAMPLE_TAP_DISCARD_DISABLE;
    typedef enum tagCORNER_TEXEL_MODE {
        CORNER_TEXEL_MODE_DISABLE = 0x0,
        CORNER_TEXEL_MODE_ENABLE = 0x1,
    } CORNER_TEXEL_MODE;
    typedef enum tagSTANDARD_TILING_MODE_EXTENSIONS {
        STANDARD_TILING_MODE_EXTENSIONS_DISABLE = 0x0,
        STANDARD_TILING_MODE_EXTENSIONS_ENABLE = 0x1,
    } STANDARD_TILING_MODE_EXTENSIONS;
    typedef enum tagTILE_ADDRESS_MAPPING_MODE {
        TILE_ADDRESS_MAPPING_MODE_GEN9 = 0x0,
        TILE_ADDRESS_MAPPING_MODE_GEN10 = 0x1,
    } TILE_ADDRESS_MAPPING_MODE;
    typedef enum tagNUMBER_OF_MULTISAMPLES {
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1 = 0x0,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2 = 0x1,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4 = 0x2,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8 = 0x3,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16 = 0x4,
    } NUMBER_OF_MULTISAMPLES;
    typedef enum tagMULTISAMPLED_SURFACE_STORAGE_FORMAT {
        MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS = 0x0,
        MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL = 0x1,
    } MULTISAMPLED_SURFACE_STORAGE_FORMAT;
    typedef enum tagRENDER_TARGET_AND_SAMPLE_UNORM_ROTATION {
        RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_0DEG = 0x0,
        RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_90DEG = 0x1,
        RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_180DEG = 0x2,
        RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_270DEG = 0x3,
    } RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION;
    typedef enum tagCOHERENCY_TYPE {
        COHERENCY_TYPE_GPU_COHERENT = 0x0,
        COHERENCY_TYPE_IA_COHERENT = 0x1,
    } COHERENCY_TYPE;
    typedef enum tagTILED_RESOURCE_MODE {
        TILED_RESOURCE_MODE_NONE = 0x0,
        TILED_RESOURCE_MODE_4KB = 0x1,
        TILED_RESOURCE_MODE_TILEYF = 0x1,
        TILED_RESOURCE_MODE_64KB = 0x2,
        TILED_RESOURCE_MODE_TILEYS = 0x2,
    } TILED_RESOURCE_MODE;
    typedef enum tagAUXILIARY_SURFACE_MODE {
        AUXILIARY_SURFACE_MODE_AUX_NONE = 0x0,
        AUXILIARY_SURFACE_MODE_AUX_CCS_D = 0x1,
        AUXILIARY_SURFACE_MODE_AUX_APPEND = 0x2,
        AUXILIARY_SURFACE_MODE_AUX_HIZ = 0x3,
        AUXILIARY_SURFACE_MODE_AUX_CCS_E = 0x5,
    } AUXILIARY_SURFACE_MODE;
    typedef enum tagHALF_PITCH_FOR_CHROMA {
        HALF_PITCH_FOR_CHROMA_DISABLE = 0x0,
        HALF_PITCH_FOR_CHROMA_ENABLE = 0x1,
    } HALF_PITCH_FOR_CHROMA;
    typedef enum tagSHADER_CHANNEL_SELECT {
        SHADER_CHANNEL_SELECT_ZERO = 0x0,
        SHADER_CHANNEL_SELECT_ONE = 0x1,
        SHADER_CHANNEL_SELECT_RED = 0x4,
        SHADER_CHANNEL_SELECT_GREEN = 0x5,
        SHADER_CHANNEL_SELECT_BLUE = 0x6,
        SHADER_CHANNEL_SELECT_ALPHA = 0x7,
    } SHADER_CHANNEL_SELECT;
    typedef enum tagMEMORY_COMPRESSION_MODE {
        MEMORY_COMPRESSION_MODE_HORIZONTAL = 0x0,
        MEMORY_COMPRESSION_MODE_VERTICAL = 0x1,
    } MEMORY_COMPRESSION_MODE;
    typedef enum tagCLEAR_COLOR_CONVERSION_ENABLE {
        CLEAR_COLOR_CONVERSION_ENABLE_DISABLE = 0x0,
        CLEAR_COLOR_CONVERSION_ENABLE_ENABLE = 0x1,
    } CLEAR_COLOR_CONVERSION_ENABLE;
    typedef enum tagPATCH_CONSTANTS {
        SURFACEBASEADDRESS_BYTEOFFSET = 0x20,
        SURFACEBASEADDRESS_INDEX = 0x8,
        AUXILIARYSURFACEBASEADDRESS_BYTEOFFSET = 0x28,
        AUXILIARYSURFACEBASEADDRESS_INDEX = 0xa,
        CLEARDEPTHADDRESSLOW_BYTEOFFSET = 0x30,
        CLEARDEPTHADDRESSLOW_INDEX = 0xc,
        CLEARCOLORADDRESS_BYTEOFFSET = 0x30,
        CLEARCOLORADDRESS_INDEX = 0xc,
        CLEARCOLORADDRESSHIGH_BYTEOFFSET = 0x34,
        CLEARCOLORADDRESSHIGH_INDEX = 0xd,
        CLEARDEPTHADDRESSHIGH_BYTEOFFSET = 0x34,
        CLEARDEPTHADDRESSHIGH_INDEX = 0xd,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MediaBoundaryPixelMode = MEDIA_BOUNDARY_PIXEL_MODE_NORMAL_MODE;
        TheStructure.Common.RenderCacheReadWriteMode = RENDER_CACHE_READ_WRITE_MODE_WRITE_ONLY_CACHE;
        TheStructure.Common.TileMode = TILE_MODE_LINEAR;
        TheStructure.Common.SurfaceHorizontalAlignment = SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4;
        TheStructure.Common.SurfaceVerticalAlignment = SURFACE_VERTICAL_ALIGNMENT_VALIGN_4;
        TheStructure.Common.SurfaceType = SURFACE_TYPE_SURFTYPE_1D;
        TheStructure.Common.SampleTapDiscardDisable = SAMPLE_TAP_DISCARD_DISABLE_DISABLE;
        TheStructure.Common.CornerTexelMode = CORNER_TEXEL_MODE_DISABLE;
        TheStructure.Common.StandardTilingModeExtensions = STANDARD_TILING_MODE_EXTENSIONS_DISABLE;
        TheStructure.Common.TileAddressMappingMode = TILE_ADDRESS_MAPPING_MODE_GEN9;
        TheStructure.Common.CoherencyType = COHERENCY_TYPE_GPU_COHERENT;
        TheStructure.Common.TiledResourceMode = TILED_RESOURCE_MODE_NONE;
        TheStructure.Common.ShaderChannelSelectAlpha = SHADER_CHANNEL_SELECT_ZERO;
        TheStructure.Common.ShaderChannelSelectBlue = SHADER_CHANNEL_SELECT_ZERO;
        TheStructure.Common.ShaderChannelSelectGreen = SHADER_CHANNEL_SELECT_ZERO;
        TheStructure.Common.ShaderChannelSelectRed = SHADER_CHANNEL_SELECT_ZERO;
        TheStructure.Common.MemoryCompressionMode = MEMORY_COMPRESSION_MODE_HORIZONTAL;
        TheStructure.SurfaceTypeIsnotSurftype_Strbuf.NumberOfMultisamples = NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1;
        TheStructure.SurfaceTypeIsnotSurftype_Strbuf.MultisampledSurfaceStorageFormat = MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS;
        TheStructure.SurfaceTypeIsnotSurftype_Strbuf.RenderTargetAndSampleUnormRotation = RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_0DEG;
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceMode = AUXILIARY_SURFACE_MODE_AUX_NONE;
        TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma = HALF_PITCH_FOR_CHROMA_DISABLE;
        TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsEnable.ClearColorConversionEnable = CLEAR_COLOR_CONVERSION_ENABLE_ENABLE;
    }
    static tagRENDER_SURFACE_STATE sInit(void) {
        RENDER_SURFACE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 16);
        return TheStructure.RawData[index];
    }
    inline void setMediaBoundaryPixelMode(const MEDIA_BOUNDARY_PIXEL_MODE value) {
        TheStructure.Common.MediaBoundaryPixelMode = value;
    }
    inline MEDIA_BOUNDARY_PIXEL_MODE getMediaBoundaryPixelMode(void) const {
        return static_cast<MEDIA_BOUNDARY_PIXEL_MODE>(TheStructure.Common.MediaBoundaryPixelMode);
    }
    inline void setRenderCacheReadWriteMode(const RENDER_CACHE_READ_WRITE_MODE value) {
        TheStructure.Common.RenderCacheReadWriteMode = value;
    }
    inline RENDER_CACHE_READ_WRITE_MODE getRenderCacheReadWriteMode(void) const {
        return static_cast<RENDER_CACHE_READ_WRITE_MODE>(TheStructure.Common.RenderCacheReadWriteMode);
    }
    inline void setSamplerL2OutOfOrderModeDisable(const bool value) {
        TheStructure.Common.SamplerL2OutOfOrderModeDisable = value;
    }
    inline bool getSamplerL2OutOfOrderModeDisable(void) const {
        return (TheStructure.Common.SamplerL2OutOfOrderModeDisable);
    }
    inline void setVerticalLineStrideOffset(const uint32_t value) {
        TheStructure.Common.VerticalLineStrideOffset = value;
    }
    inline uint32_t getVerticalLineStrideOffset(void) const {
        return (TheStructure.Common.VerticalLineStrideOffset);
    }
    inline void setVerticalLineStride(const uint32_t value) {
        TheStructure.Common.VerticalLineStride = value;
    }
    inline uint32_t getVerticalLineStride(void) const {
        return (TheStructure.Common.VerticalLineStride);
    }
    inline void setTileMode(const TILE_MODE value) {
        TheStructure.Common.TileMode = value;
    }
    inline TILE_MODE getTileMode(void) const {
        return static_cast<TILE_MODE>(TheStructure.Common.TileMode);
    }
    inline void setSurfaceHorizontalAlignment(const SURFACE_HORIZONTAL_ALIGNMENT value) {
        TheStructure.Common.SurfaceHorizontalAlignment = value;
    }
    inline SURFACE_HORIZONTAL_ALIGNMENT getSurfaceHorizontalAlignment(void) const {
        return static_cast<SURFACE_HORIZONTAL_ALIGNMENT>(TheStructure.Common.SurfaceHorizontalAlignment);
    }
    inline void setSurfaceVerticalAlignment(const SURFACE_VERTICAL_ALIGNMENT value) {
        TheStructure.Common.SurfaceVerticalAlignment = value;
    }
    inline SURFACE_VERTICAL_ALIGNMENT getSurfaceVerticalAlignment(void) const {
        return static_cast<SURFACE_VERTICAL_ALIGNMENT>(TheStructure.Common.SurfaceVerticalAlignment);
    }
    inline void setSurfaceFormat(const SURFACE_FORMAT value) {
        TheStructure.Common.SurfaceFormat = value;
    }
    inline SURFACE_FORMAT getSurfaceFormat(void) const {
        return static_cast<SURFACE_FORMAT>(TheStructure.Common.SurfaceFormat);
    }
    inline void setAstcEnable(const bool value) {
        TheStructure.Common.Astc_Enable = value;
    }
    inline bool getAstcEnable(void) const {
        return (TheStructure.Common.Astc_Enable);
    }
    inline void setSurfaceArray(const bool value) {
        TheStructure.Common.SurfaceArray = value;
    }
    inline bool getSurfaceArray(void) const {
        return (TheStructure.Common.SurfaceArray);
    }
    inline void setSurfaceType(const SURFACE_TYPE value) {
        TheStructure.Common.SurfaceType = value;
    }
    inline SURFACE_TYPE getSurfaceType(void) const {
        return static_cast<SURFACE_TYPE>(TheStructure.Common.SurfaceType);
    }
    typedef enum tagSURFACEQPITCH {
        SURFACEQPITCH_BIT_SHIFT = 0x2,
        SURFACEQPITCH_ALIGN_SIZE = 0x4,
    } SURFACEQPITCH;
    inline void setSurfaceQpitch(const uint32_t value) {
        TheStructure.Common.SurfaceQpitch = value >> SURFACEQPITCH_BIT_SHIFT;
    }
    inline uint32_t getSurfaceQpitch(void) const {
        return (TheStructure.Common.SurfaceQpitch << SURFACEQPITCH_BIT_SHIFT);
    }
    inline void setSampleTapDiscardDisable(const SAMPLE_TAP_DISCARD_DISABLE value) {
        TheStructure.Common.SampleTapDiscardDisable = value;
    }
    inline SAMPLE_TAP_DISCARD_DISABLE getSampleTapDiscardDisable(void) const {
        return static_cast<SAMPLE_TAP_DISCARD_DISABLE>(TheStructure.Common.SampleTapDiscardDisable);
    }
    inline void setCornerTexelMode(const CORNER_TEXEL_MODE value) {
        TheStructure.Common.CornerTexelMode = value;
    }
    inline CORNER_TEXEL_MODE getCornerTexelMode(void) const {
        return static_cast<CORNER_TEXEL_MODE>(TheStructure.Common.CornerTexelMode);
    }
    inline void setBaseMipLevel(const uint32_t value) {
        TheStructure.Common.BaseMipLevel = value;
    }
    inline uint32_t getBaseMipLevel(void) const {
        return (TheStructure.Common.BaseMipLevel);
    }
    inline void setMemoryObjectControlStateReserved(const uint32_t value) {
        TheStructure.Common.MemoryObjectControlState_Reserved = value;
    }
    inline uint32_t getMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.MemoryObjectControlState_Reserved);
    }
    inline void setMemoryObjectControlStateIndexToMocsTables(const uint32_t value) {
        TheStructure.Common.MemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint32_t getMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.MemoryObjectControlState_IndexToMocsTables << 1);
    }
    inline void setMemoryObjectControlState(const uint32_t value) {
        TheStructure.Common.MemoryObjectControlState_Reserved = value;
        TheStructure.Common.MemoryObjectControlState_IndexToMocsTables = (value >> 1);
    }
    inline uint32_t getMemoryObjectControlState(void) const {
        uint32_t mocs = TheStructure.Common.MemoryObjectControlState_Reserved;
        mocs |= (TheStructure.Common.MemoryObjectControlState_IndexToMocsTables << 1);
        return (mocs);
    }
    inline void setEnableUnormPathInColorPipe(const uint32_t value) {
        TheStructure.Common.EnableUnormPathInColorPipe = value;
    }
    inline uint32_t getEnableUnormPathInColorPipe(void) const {
        return (TheStructure.Common.EnableUnormPathInColorPipe);
    }
    inline void setWidth(const uint32_t value) {
        TheStructure.Common.Width = value - 1;
    }
    inline uint32_t getWidth(void) const {
        return (TheStructure.Common.Width + 1);
    }
    inline void setHeight(const uint32_t value) {
        TheStructure.Common.Height = value - 1;
    }
    inline uint32_t getHeight(void) const {
        return (TheStructure.Common.Height + 1);
    }
    inline void setSurfacePitch(const uint32_t value) {
        TheStructure.Common.SurfacePitch = value - 1;
    }
    inline uint32_t getSurfacePitch(void) const {
        return (TheStructure.Common.SurfacePitch + 1);
    }
    inline void setStandardTilingModeExtensions(const STANDARD_TILING_MODE_EXTENSIONS value) {
        TheStructure.Common.StandardTilingModeExtensions = value;
    }
    inline STANDARD_TILING_MODE_EXTENSIONS getStandardTilingModeExtensions(void) const {
        return static_cast<STANDARD_TILING_MODE_EXTENSIONS>(TheStructure.Common.StandardTilingModeExtensions);
    }
    inline void setTileAddressMappingMode(const TILE_ADDRESS_MAPPING_MODE value) {
        TheStructure.Common.TileAddressMappingMode = value;
    }
    inline TILE_ADDRESS_MAPPING_MODE getTileAddressMappingMode(void) const {
        return static_cast<TILE_ADDRESS_MAPPING_MODE>(TheStructure.Common.TileAddressMappingMode);
    }
    inline void setDepth(const uint32_t value) {
        TheStructure.Common.Depth = value - 1;
    }
    inline uint32_t getDepth(void) const {
        return (TheStructure.Common.Depth + 1);
    }
    inline void setMipCountLod(const uint32_t value) {
        TheStructure.Common.MipCountLod = value;
    }
    inline uint32_t getMipCountLod(void) const {
        return (TheStructure.Common.MipCountLod);
    }
    inline void setSurfaceMinLod(const uint32_t value) {
        TheStructure.Common.SurfaceMinLod = value;
    }
    inline uint32_t getSurfaceMinLod(void) const {
        return (TheStructure.Common.SurfaceMinLod);
    }
    inline void setMipTailStartLod(const uint32_t value) {
        TheStructure.Common.MipTailStartLod = value;
    }
    inline uint32_t getMipTailStartLod(void) const {
        return (TheStructure.Common.MipTailStartLod);
    }
    inline void setCoherencyType(const COHERENCY_TYPE value) {
        TheStructure.Common.CoherencyType = value;
    }
    inline COHERENCY_TYPE getCoherencyType(void) const {
        return static_cast<COHERENCY_TYPE>(TheStructure.Common.CoherencyType);
    }
    inline void setTiledResourceMode(const TILED_RESOURCE_MODE value) {
        TheStructure.Common.TiledResourceMode = value;
    }
    inline TILED_RESOURCE_MODE getTiledResourceMode(void) const {
        return static_cast<TILED_RESOURCE_MODE>(TheStructure.Common.TiledResourceMode);
    }
    inline void setEwaDisableForCube(const bool value) {
        TheStructure.Common.EwaDisableForCube = value;
    }
    inline bool getEwaDisableForCube(void) const {
        return (TheStructure.Common.EwaDisableForCube);
    }
    typedef enum tagYOFFSET {
        YOFFSET_BIT_SHIFT = 0x2,
        YOFFSET_ALIGN_SIZE = 0x4,
    } YOFFSET;
    inline void setYOffset(const uint32_t value) {
        TheStructure.Common.YOffset = value >> YOFFSET_BIT_SHIFT;
    }
    inline uint32_t getYOffset(void) const {
        return (TheStructure.Common.YOffset << YOFFSET_BIT_SHIFT);
    }
    typedef enum tagXOFFSET {
        XOFFSET_BIT_SHIFT = 0x2,
        XOFFSET_ALIGN_SIZE = 0x4,
    } XOFFSET;
    inline void setXOffset(const uint32_t value) {
        TheStructure.Common.XOffset = value >> XOFFSET_BIT_SHIFT;
    }
    inline uint32_t getXOffset(void) const {
        return (TheStructure.Common.XOffset << XOFFSET_BIT_SHIFT);
    }
    inline void setYuvInterpolationEnable(const bool value) {
        TheStructure.Common.YuvInterpolationEnable = value;
    }
    inline bool getYuvInterpolationEnable(void) const {
        return (TheStructure.Common.YuvInterpolationEnable);
    }
    inline void setResourceMinLod(const uint32_t value) {
        TheStructure.Common.ResourceMinLod = value;
    }
    inline uint32_t getResourceMinLod(void) const {
        return (TheStructure.Common.ResourceMinLod);
    }
    inline void setShaderChannelSelectAlpha(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectAlpha = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectAlpha(void) const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectAlpha);
    }
    inline void setShaderChannelSelectBlue(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectBlue = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectBlue(void) const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectBlue);
    }
    inline void setShaderChannelSelectGreen(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectGreen = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectGreen(void) const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectGreen);
    }
    inline void setShaderChannelSelectRed(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectRed = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectRed(void) const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectRed);
    }
    inline void setMemoryCompressionEnable(const bool value) {
        TheStructure.Common.MemoryCompressionEnable = value;
    }
    inline bool getMemoryCompressionEnable(void) const {
        return (TheStructure.Common.MemoryCompressionEnable);
    }
    inline void setMemoryCompressionMode(const MEMORY_COMPRESSION_MODE value) {
        TheStructure.Common.MemoryCompressionMode = value;
    }
    inline MEMORY_COMPRESSION_MODE getMemoryCompressionMode(void) const {
        return static_cast<MEMORY_COMPRESSION_MODE>(TheStructure.Common.MemoryCompressionMode);
    }
    inline void setSurfaceBaseAddress(const uint64_t value) {
        TheStructure.Common.SurfaceBaseAddress = value;
    }
    inline uint64_t getSurfaceBaseAddress(void) const {
        return (TheStructure.Common.SurfaceBaseAddress);
    }
    inline void setQuiltWidth(const uint64_t value) {
        TheStructure.Common.QuiltWidth = value;
    }
    inline uint64_t getQuiltWidth(void) const {
        return (TheStructure.Common.QuiltWidth);
    }
    inline void setQuiltHeight(const uint64_t value) {
        TheStructure.Common.QuiltHeight = value;
    }
    inline uint64_t getQuiltHeight(void) const {
        return (TheStructure.Common.QuiltHeight);
    }
    inline void setClearValueAddressEnable(const bool value) {
        TheStructure.Common.ClearValueAddressEnable = value;
    }
    inline bool getClearValueAddressEnable(void) const {
        return (TheStructure.Common.ClearValueAddressEnable);
    }
    inline void setCubeFaceEnablePositiveZ(const bool value) {
        TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_PositiveZ = value;
    }
    inline bool getCubeFaceEnablePositiveZ(void) const {
        return (TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_PositiveZ);
    }
    inline void setCubeFaceEnableNegativeZ(const bool value) {
        TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_NegativeZ = value;
    }
    inline bool getCubeFaceEnableNegativeZ(void) const {
        return (TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_NegativeZ);
    }
    inline void setCubeFaceEnablePositiveY(const bool value) {
        TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_PositiveY = value;
    }
    inline bool getCubeFaceEnablePositiveY(void) const {
        return (TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_PositiveY);
    }
    inline void setCubeFaceEnableNegativeY(const bool value) {
        TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_NegativeY = value;
    }
    inline bool getCubeFaceEnableNegativeY(void) const {
        return (TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_NegativeY);
    }
    inline void setCubeFaceEnablePositiveX(const bool value) {
        TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_PositiveX = value;
    }
    inline bool getCubeFaceEnablePositiveX(void) const {
        return (TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_PositiveX);
    }
    inline void setCubeFaceEnableNegativeX(const bool value) {
        TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_NegativeX = value;
    }
    inline bool getCubeFaceEnableNegativeX(void) const {
        return (TheStructure.SurfaceTypeIsSurftype_Cube.CubeFaceEnable_NegativeX);
    }
    inline void setMultisamplePositionPaletteIndex(const uint32_t value) {
        TheStructure.SurfaceTypeIsnotSurftype_Strbuf.MultisamplePositionPaletteIndex = value;
    }
    inline uint32_t getMultisamplePositionPaletteIndex(void) const {
        return (TheStructure.SurfaceTypeIsnotSurftype_Strbuf.MultisamplePositionPaletteIndex);
    }
    inline void setNumberOfMultisamples(const NUMBER_OF_MULTISAMPLES value) {
        TheStructure.SurfaceTypeIsnotSurftype_Strbuf.NumberOfMultisamples = value;
    }
    inline NUMBER_OF_MULTISAMPLES getNumberOfMultisamples(void) const {
        return static_cast<NUMBER_OF_MULTISAMPLES>(TheStructure.SurfaceTypeIsnotSurftype_Strbuf.NumberOfMultisamples);
    }
    inline void setMultisampledSurfaceStorageFormat(const MULTISAMPLED_SURFACE_STORAGE_FORMAT value) {
        TheStructure.SurfaceTypeIsnotSurftype_Strbuf.MultisampledSurfaceStorageFormat = value;
    }
    inline MULTISAMPLED_SURFACE_STORAGE_FORMAT getMultisampledSurfaceStorageFormat(void) const {
        return static_cast<MULTISAMPLED_SURFACE_STORAGE_FORMAT>(TheStructure.SurfaceTypeIsnotSurftype_Strbuf.MultisampledSurfaceStorageFormat);
    }
    inline void setRenderTargetViewExtent(const uint32_t value) {
        TheStructure.SurfaceTypeIsnotSurftype_Strbuf.RenderTargetViewExtent = value - 1;
    }
    inline uint32_t getRenderTargetViewExtent(void) const {
        return (TheStructure.SurfaceTypeIsnotSurftype_Strbuf.RenderTargetViewExtent + 1);
    }
    inline void setMinimumArrayElement(const uint32_t value) {
        TheStructure.SurfaceTypeIsnotSurftype_Strbuf.MinimumArrayElement = value;
    }
    inline uint32_t getMinimumArrayElement(void) const {
        return (TheStructure.SurfaceTypeIsnotSurftype_Strbuf.MinimumArrayElement);
    }
    inline void setRenderTargetAndSampleUnormRotation(const RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION value) {
        TheStructure.SurfaceTypeIsnotSurftype_Strbuf.RenderTargetAndSampleUnormRotation = value;
    }
    inline RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION getRenderTargetAndSampleUnormRotation(void) const {
        return static_cast<RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION>(TheStructure.SurfaceTypeIsnotSurftype_Strbuf.RenderTargetAndSampleUnormRotation);
    }
    inline void setAuxiliarySurfaceMode(const AUXILIARY_SURFACE_MODE value) {
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceMode = value;
    }
    inline AUXILIARY_SURFACE_MODE getAuxiliarySurfaceMode(void) const {
        return static_cast<AUXILIARY_SURFACE_MODE>(TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceMode);
    }
    inline void setAuxiliarySurfacePitch(const uint32_t value) {
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfacePitch = value - 1;
    }
    inline uint32_t getAuxiliarySurfacePitch(void) const {
        return (TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfacePitch + 1);
    }
    typedef enum tagAUXILIARYSURFACEQPITCH {
        AUXILIARYSURFACEQPITCH_BIT_SHIFT = 0x2,
        AUXILIARYSURFACEQPITCH_ALIGN_SIZE = 0x4,
    } AUXILIARYSURFACEQPITCH;
    inline void setAuxiliarySurfaceQpitch(const uint32_t value) {
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceQpitch = value >> AUXILIARYSURFACEQPITCH_BIT_SHIFT;
    }
    inline uint32_t getAuxiliarySurfaceQpitch(void) const {
        return (TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceQpitch << AUXILIARYSURFACEQPITCH_BIT_SHIFT);
    }
    inline void setYOffsetForUOrUvPlane(const uint32_t value) {
        TheStructure._SurfaceFormatIsPlanar.YOffsetForUOrUvPlane = value;
    }
    inline uint32_t getYOffsetForUOrUvPlane(void) const {
        return (TheStructure._SurfaceFormatIsPlanar.YOffsetForUOrUvPlane);
    }
    inline void setXOffsetForUOrUvPlane(const uint32_t value) {
        TheStructure._SurfaceFormatIsPlanar.XOffsetForUOrUvPlane = value;
    }
    inline uint32_t getXOffsetForUOrUvPlane(void) const {
        return (TheStructure._SurfaceFormatIsPlanar.XOffsetForUOrUvPlane);
    }
    inline void setHalfPitchForChroma(const HALF_PITCH_FOR_CHROMA value) {
        TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma = value;
    }
    inline HALF_PITCH_FOR_CHROMA getHalfPitchForChroma(void) const {
        return static_cast<HALF_PITCH_FOR_CHROMA>(TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma);
    }
    inline void setSeparateUvPlaneEnable(const bool value) {
        TheStructure._SurfaceFormatIsPlanar.SeparateUvPlaneEnable = value;
    }
    inline bool getSeparateUvPlaneEnable(void) const {
        return (TheStructure._SurfaceFormatIsPlanar.SeparateUvPlaneEnable);
    }
    inline void setYOffsetForVPlane(const uint64_t value) {
        TheStructure._SurfaceFormatIsPlanar.YOffsetForVPlane = value;
    }
    inline uint64_t getYOffsetForVPlane(void) const {
        return (TheStructure._SurfaceFormatIsPlanar.YOffsetForVPlane);
    }
    inline void setXOffsetForVPlane(const uint64_t value) {
        TheStructure._SurfaceFormatIsPlanar.XOffsetForVPlane = value;
    }
    inline uint64_t getXOffsetForVPlane(void) const {
        return (TheStructure._SurfaceFormatIsPlanar.XOffsetForVPlane);
    }
    typedef enum tagAUXILIARYSURFACEBASEADDRESS {
        AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT = 0xc,
        AUXILIARYSURFACEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } AUXILIARYSURFACEBASEADDRESS;
    inline void setAuxiliarySurfaceBaseAddress(const uint64_t value) {
        TheStructure._SurfaceFormatIsnotPlanarAndMemoryCompressionEnableIs0.AuxiliarySurfaceBaseAddress = value >> AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getAuxiliarySurfaceBaseAddress(void) const {
        return (TheStructure._SurfaceFormatIsnotPlanarAndMemoryCompressionEnableIs0.AuxiliarySurfaceBaseAddress << AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT);
    }
    inline void setAuxiliaryTableIndexForMediaCompressedSurface(const uint64_t value) {
        TheStructure.MemoryCompressionEnableIs1.AuxiliaryTableIndexForMediaCompressedSurface = value;
    }
    inline uint64_t getAuxiliaryTableIndexForMediaCompressedSurface(void) const {
        return (TheStructure.MemoryCompressionEnableIs1.AuxiliaryTableIndexForMediaCompressedSurface);
    }
    inline void setClearColorConversionEnable(const CLEAR_COLOR_CONVERSION_ENABLE value) {
        TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsEnable.ClearColorConversionEnable = value;
    }
    inline CLEAR_COLOR_CONVERSION_ENABLE getClearColorConversionEnable(void) const {
        return static_cast<CLEAR_COLOR_CONVERSION_ENABLE>(TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsEnable.ClearColorConversionEnable);
    }
    inline void setClearColorAddressHigh(const uint32_t value) {
        TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsEnable.ClearColorAddressHigh = value;
    }
    inline uint32_t getClearColorAddressHigh(void) const {
        return (TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsEnable.ClearColorAddressHigh);
    }
    typedef enum tagCLEARDEPTHADDRESSLOW {
        CLEARDEPTHADDRESSLOW_BIT_SHIFT = 0x6,
        CLEARDEPTHADDRESSLOW_ALIGN_SIZE = 0x40,
    } CLEARDEPTHADDRESSLOW;
    inline void setClearDepthAddressLow(const uint32_t value) {
        TheStructure._AuxiliarySurfaceModeIsAux_HizAnd_ClearValueAddressEnableIsEnable.ClearDepthAddressLow = value >> CLEARDEPTHADDRESSLOW_BIT_SHIFT;
    }
    inline uint32_t getClearDepthAddressLow(void) const {
        return (TheStructure._AuxiliarySurfaceModeIsAux_HizAnd_ClearValueAddressEnableIsEnable.ClearDepthAddressLow << CLEARDEPTHADDRESSLOW_BIT_SHIFT);
    }
    inline void setClearDepthAddressHigh(const uint32_t value) {
        TheStructure._AuxiliarySurfaceModeIsAux_HizAnd_ClearValueAddressEnableIsEnable.ClearDepthAddressHigh = value;
    }
    inline uint32_t getClearDepthAddressHigh(void) const {
        return (TheStructure._AuxiliarySurfaceModeIsAux_HizAnd_ClearValueAddressEnableIsEnable.ClearDepthAddressHigh);
    }
    inline void setRedClearColor(const uint32_t value) {
        TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EOrAuxiliarySurfaceModeIsAux_HizAndClearValueAddressEnableIsDisable.RedClearColor = value;
    }
    inline uint32_t getRedClearColor(void) const {
        return (TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EOrAuxiliarySurfaceModeIsAux_HizAndClearValueAddressEnableIsDisable.RedClearColor);
    }
    typedef enum tagCLEARCOLORADDRESS {
        CLEARCOLORADDRESS_BIT_SHIFT = 0x6,
        CLEARCOLORADDRESS_ALIGN_SIZE = 0x40,
    } CLEARCOLORADDRESS;
    inline void setClearColorAddress(const uint32_t value) {
        TheStructure.ClearValueAddressEnableIsEnable.ClearColorAddress = value >> CLEARCOLORADDRESS_BIT_SHIFT;
    }
    inline uint32_t getClearColorAddress(void) const {
        return (TheStructure.ClearValueAddressEnableIsEnable.ClearColorAddress << CLEARCOLORADDRESS_BIT_SHIFT);
    }
    inline void setGreenClearColor(const uint32_t value) {
        TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsDisable.GreenClearColor = value;
    }
    inline uint32_t getGreenClearColor(void) const {
        return (TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsDisable.GreenClearColor);
    }
    inline void setBlueClearColor(const uint32_t value) {
        TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsDisable.BlueClearColor = value;
    }
    inline uint32_t getBlueClearColor(void) const {
        return (TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsDisable.BlueClearColor);
    }
    inline void setAlphaClearColor(const uint32_t value) {
        TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsDisable.AlphaClearColor = value;
    }
    inline uint32_t getAlphaClearColor(void) const {
        return (TheStructure.__AuxiliarySurfaceModeIsAux_Ccs_DOrAuxiliarySurfaceModeIsAux_Ccs_EAndClearValueAddressEnableIsDisable.AlphaClearColor);
    }
} RENDER_SURFACE_STATE;
STATIC_ASSERT(64 == sizeof(RENDER_SURFACE_STATE));

typedef struct tagSAMPLER_STATE {
    union tagTheStructure {
        struct tagCommon {
            uint32_t LodAlgorithm : BITFIELD_RANGE(0, 0);
            uint32_t TextureLodBias : BITFIELD_RANGE(1, 13);
            uint32_t MinModeFilter : BITFIELD_RANGE(14, 16);
            uint32_t MagModeFilter : BITFIELD_RANGE(17, 19);
            uint32_t MipModeFilter : BITFIELD_RANGE(20, 21);
            uint32_t CoarseLodQualityMode : BITFIELD_RANGE(22, 26);
            uint32_t LodPreclampMode : BITFIELD_RANGE(27, 28);
            uint32_t TextureBorderColorMode : BITFIELD_RANGE(29, 29);
            uint32_t CpsLodCompensationEnable : BITFIELD_RANGE(30, 30);
            uint32_t SamplerDisable : BITFIELD_RANGE(31, 31);
            uint32_t CubeSurfaceControlMode : BITFIELD_RANGE(0, 0);
            uint32_t ShadowFunction : BITFIELD_RANGE(1, 3);
            uint32_t ChromakeyMode : BITFIELD_RANGE(4, 4);
            uint32_t ChromakeyIndex : BITFIELD_RANGE(5, 6);
            uint32_t ChromakeyEnable : BITFIELD_RANGE(7, 7);
            uint32_t MaxLod : BITFIELD_RANGE(8, 19);
            uint32_t MinLod : BITFIELD_RANGE(20, 31);
            uint32_t LodClampMagnificationMode : BITFIELD_RANGE(0, 0);
            uint32_t SrgbDecode : BITFIELD_RANGE(1, 1);
            uint32_t ReturnFilterWeightForNullTexels : BITFIELD_RANGE(2, 2);
            uint32_t ReturnFilterWeightForBorderTexels : BITFIELD_RANGE(3, 3);
            uint32_t Reserved_68 : BITFIELD_RANGE(4, 5);
            uint32_t IndirectStatePointer : BITFIELD_RANGE(6, 23);
            uint32_t Reserved_88 : BITFIELD_RANGE(24, 31);
            uint32_t TczAddressControlMode : BITFIELD_RANGE(0, 2);
            uint32_t TcyAddressControlMode : BITFIELD_RANGE(3, 5);
            uint32_t TcxAddressControlMode : BITFIELD_RANGE(6, 8);
            uint32_t ReductionTypeEnable : BITFIELD_RANGE(9, 9);
            uint32_t Non_NormalizedCoordinateEnable : BITFIELD_RANGE(10, 10);
            uint32_t TrilinearFilterQuality : BITFIELD_RANGE(11, 12);
            uint32_t RAddressMinFilterRoundingEnable : BITFIELD_RANGE(13, 13);
            uint32_t RAddressMagFilterRoundingEnable : BITFIELD_RANGE(14, 14);
            uint32_t VAddressMinFilterRoundingEnable : BITFIELD_RANGE(15, 15);
            uint32_t VAddressMagFilterRoundingEnable : BITFIELD_RANGE(16, 16);
            uint32_t UAddressMinFilterRoundingEnable : BITFIELD_RANGE(17, 17);
            uint32_t UAddressMagFilterRoundingEnable : BITFIELD_RANGE(18, 18);
            uint32_t MaximumAnisotropy : BITFIELD_RANGE(19, 21);
            uint32_t ReductionType : BITFIELD_RANGE(22, 23);
            uint32_t Reserved_120 : BITFIELD_RANGE(24, 31);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagLOD_ALGORITHM {
        LOD_ALGORITHM_LEGACY = 0x0,
        LOD_ALGORITHM_EWA_APPROXIMATION = 0x1,
    } LOD_ALGORITHM;
    typedef enum tagMIN_MODE_FILTER {
        MIN_MODE_FILTER_NEAREST = 0x0,
        MIN_MODE_FILTER_LINEAR = 0x1,
        MIN_MODE_FILTER_ANISOTROPIC = 0x2,
        MIN_MODE_FILTER_MONO = 0x6,
    } MIN_MODE_FILTER;
    typedef enum tagMAG_MODE_FILTER {
        MAG_MODE_FILTER_NEAREST = 0x0,
        MAG_MODE_FILTER_LINEAR = 0x1,
        MAG_MODE_FILTER_ANISOTROPIC = 0x2,
        MAG_MODE_FILTER_MONO = 0x6,
    } MAG_MODE_FILTER;
    typedef enum tagMIP_MODE_FILTER {
        MIP_MODE_FILTER_NONE = 0x0,
        MIP_MODE_FILTER_NEAREST = 0x1,
        MIP_MODE_FILTER_LINEAR = 0x3,
    } MIP_MODE_FILTER;
    typedef enum tagCOARSE_LOD_QUALITY_MODE {
        COARSE_LOD_QUALITY_MODE_DISABLED = 0x0,
    } COARSE_LOD_QUALITY_MODE;
    typedef enum tagLOD_PRECLAMP_MODE {
        LOD_PRECLAMP_MODE_NONE = 0x0,
        LOD_PRECLAMP_MODE_OGL = 0x2,
    } LOD_PRECLAMP_MODE;
    typedef enum tagTEXTURE_BORDER_COLOR_MODE {
        TEXTURE_BORDER_COLOR_MODE_DX10_OGL = 0x0,
        TEXTURE_BORDER_COLOR_MODE_DX9 = 0x1,
    } TEXTURE_BORDER_COLOR_MODE;
    typedef enum tagCUBE_SURFACE_CONTROL_MODE {
        CUBE_SURFACE_CONTROL_MODE_PROGRAMMED = 0x0,
        CUBE_SURFACE_CONTROL_MODE_OVERRIDE = 0x1,
    } CUBE_SURFACE_CONTROL_MODE;
    typedef enum tagSHADOW_FUNCTION {
        SHADOW_FUNCTION_PREFILTEROP_ALWAYS = 0x0,
        SHADOW_FUNCTION_PREFILTEROP_NEVER = 0x1,
        SHADOW_FUNCTION_PREFILTEROP_LESS = 0x2,
        SHADOW_FUNCTION_PREFILTEROP_EQUAL = 0x3,
        SHADOW_FUNCTION_PREFILTEROP_LEQUAL = 0x4,
        SHADOW_FUNCTION_PREFILTEROP_GREATER = 0x5,
        SHADOW_FUNCTION_PREFILTEROP_NOTEQUAL = 0x6,
        SHADOW_FUNCTION_PREFILTEROP_GEQUAL = 0x7,
    } SHADOW_FUNCTION;
    typedef enum tagCHROMAKEY_MODE {
        CHROMAKEY_MODE_KEYFILTER_KILL_ON_ANY_MATCH = 0x0,
        CHROMAKEY_MODE_KEYFILTER_REPLACE_BLACK = 0x1,
    } CHROMAKEY_MODE;
    typedef enum tagLOD_CLAMP_MAGNIFICATION_MODE {
        LOD_CLAMP_MAGNIFICATION_MODE_MIPNONE = 0x0,
        LOD_CLAMP_MAGNIFICATION_MODE_MIPFILTER = 0x1,
    } LOD_CLAMP_MAGNIFICATION_MODE;
    typedef enum tagSRGB_DECODE {
        SRGB_DECODE_DECODE_EXT = 0x0,
        SRGB_DECODE_SKIP_DECODE_EXT = 0x1,
    } SRGB_DECODE;
    typedef enum tagRETURN_FILTER_WEIGHT_FOR_NULL_TEXELS {
        RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_DISABLE = 0x0,
        RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_ENABLE = 0x1,
    } RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS;
    typedef enum tagRETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS {
        RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_DISABLE = 0x0,
        RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_ENABLE = 0x1,
    } RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS;
    typedef enum tagTEXTURE_COORDINATE_MODE {
        TEXTURE_COORDINATE_MODE_WRAP = 0x0,
        TEXTURE_COORDINATE_MODE_MIRROR = 0x1,
        TEXTURE_COORDINATE_MODE_CLAMP = 0x2,
        TEXTURE_COORDINATE_MODE_CUBE = 0x3,
        TEXTURE_COORDINATE_MODE_CLAMP_BORDER = 0x4,
        TEXTURE_COORDINATE_MODE_MIRROR_ONCE = 0x5,
        TEXTURE_COORDINATE_MODE_HALF_BORDER = 0x6,
        TEXTURE_COORDINATE_MODE_MIRROR_101 = 0x7,
    } TEXTURE_COORDINATE_MODE;
    typedef enum tagTRILINEAR_FILTER_QUALITY {
        TRILINEAR_FILTER_QUALITY_FULL = 0x0,
        TRILINEAR_FILTER_QUALITY_TRIQUAL_HIGH_MAG_CLAMP_MIPFILTER = 0x1,
        TRILINEAR_FILTER_QUALITY_MED = 0x2,
        TRILINEAR_FILTER_QUALITY_LOW = 0x3,
    } TRILINEAR_FILTER_QUALITY;
    typedef enum tagMAXIMUM_ANISOTROPY {
        MAXIMUM_ANISOTROPY_RATIO_21 = 0x0,
        MAXIMUM_ANISOTROPY_RATIO_41 = 0x1,
        MAXIMUM_ANISOTROPY_RATIO_61 = 0x2,
        MAXIMUM_ANISOTROPY_RATIO_81 = 0x3,
        MAXIMUM_ANISOTROPY_RATIO_101 = 0x4,
        MAXIMUM_ANISOTROPY_RATIO_121 = 0x5,
        MAXIMUM_ANISOTROPY_RATIO_141 = 0x6,
        MAXIMUM_ANISOTROPY_RATIO_161 = 0x7,
    } MAXIMUM_ANISOTROPY;
    typedef enum tagREDUCTION_TYPE {
        REDUCTION_TYPE_STD_FILTER = 0x0,
        REDUCTION_TYPE_COMPARISON = 0x1,
        REDUCTION_TYPE_MINIMUM = 0x2,
        REDUCTION_TYPE_MAXIMUM = 0x3,
    } REDUCTION_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.LodAlgorithm = LOD_ALGORITHM_LEGACY;
        TheStructure.Common.MinModeFilter = MIN_MODE_FILTER_NEAREST;
        TheStructure.Common.MagModeFilter = MAG_MODE_FILTER_NEAREST;
        TheStructure.Common.MipModeFilter = MIP_MODE_FILTER_NONE;
        TheStructure.Common.CoarseLodQualityMode = COARSE_LOD_QUALITY_MODE_DISABLED;
        TheStructure.Common.LodPreclampMode = LOD_PRECLAMP_MODE_NONE;
        TheStructure.Common.TextureBorderColorMode = TEXTURE_BORDER_COLOR_MODE_DX10_OGL;
        TheStructure.Common.CubeSurfaceControlMode = CUBE_SURFACE_CONTROL_MODE_PROGRAMMED;
        TheStructure.Common.ShadowFunction = SHADOW_FUNCTION_PREFILTEROP_ALWAYS;
        TheStructure.Common.ChromakeyMode = CHROMAKEY_MODE_KEYFILTER_KILL_ON_ANY_MATCH;
        TheStructure.Common.LodClampMagnificationMode = LOD_CLAMP_MAGNIFICATION_MODE_MIPNONE;
        TheStructure.Common.SrgbDecode = SRGB_DECODE_DECODE_EXT;
        TheStructure.Common.ReturnFilterWeightForNullTexels = RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_DISABLE;
        TheStructure.Common.ReturnFilterWeightForBorderTexels = RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_DISABLE;
        TheStructure.Common.TczAddressControlMode = TEXTURE_COORDINATE_MODE_WRAP;
        TheStructure.Common.TcyAddressControlMode = TEXTURE_COORDINATE_MODE_WRAP;
        TheStructure.Common.TcxAddressControlMode = TEXTURE_COORDINATE_MODE_WRAP;
        TheStructure.Common.TrilinearFilterQuality = TRILINEAR_FILTER_QUALITY_FULL;
        TheStructure.Common.MaximumAnisotropy = MAXIMUM_ANISOTROPY_RATIO_21;
        TheStructure.Common.ReductionType = REDUCTION_TYPE_STD_FILTER;
    }
    static tagSAMPLER_STATE sInit(void) {
        SAMPLER_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setLodAlgorithm(const LOD_ALGORITHM value) {
        TheStructure.Common.LodAlgorithm = value;
    }
    inline LOD_ALGORITHM getLodAlgorithm(void) const {
        return static_cast<LOD_ALGORITHM>(TheStructure.Common.LodAlgorithm);
    }
    inline void setTextureLodBias(const uint32_t value) {
        TheStructure.Common.TextureLodBias = value;
    }
    inline uint32_t getTextureLodBias(void) const {
        return (TheStructure.Common.TextureLodBias);
    }
    inline void setMinModeFilter(const MIN_MODE_FILTER value) {
        TheStructure.Common.MinModeFilter = value;
    }
    inline MIN_MODE_FILTER getMinModeFilter(void) const {
        return static_cast<MIN_MODE_FILTER>(TheStructure.Common.MinModeFilter);
    }
    inline void setMagModeFilter(const MAG_MODE_FILTER value) {
        TheStructure.Common.MagModeFilter = value;
    }
    inline MAG_MODE_FILTER getMagModeFilter(void) const {
        return static_cast<MAG_MODE_FILTER>(TheStructure.Common.MagModeFilter);
    }
    inline void setMipModeFilter(const MIP_MODE_FILTER value) {
        TheStructure.Common.MipModeFilter = value;
    }
    inline MIP_MODE_FILTER getMipModeFilter(void) const {
        return static_cast<MIP_MODE_FILTER>(TheStructure.Common.MipModeFilter);
    }
    inline void setCoarseLodQualityMode(const COARSE_LOD_QUALITY_MODE value) {
        TheStructure.Common.CoarseLodQualityMode = value;
    }
    inline COARSE_LOD_QUALITY_MODE getCoarseLodQualityMode(void) const {
        return static_cast<COARSE_LOD_QUALITY_MODE>(TheStructure.Common.CoarseLodQualityMode);
    }
    inline void setLodPreclampMode(const LOD_PRECLAMP_MODE value) {
        TheStructure.Common.LodPreclampMode = value;
    }
    inline LOD_PRECLAMP_MODE getLodPreclampMode(void) const {
        return static_cast<LOD_PRECLAMP_MODE>(TheStructure.Common.LodPreclampMode);
    }
    inline void setTextureBorderColorMode(const TEXTURE_BORDER_COLOR_MODE value) {
        TheStructure.Common.TextureBorderColorMode = value;
    }
    inline TEXTURE_BORDER_COLOR_MODE getTextureBorderColorMode(void) const {
        return static_cast<TEXTURE_BORDER_COLOR_MODE>(TheStructure.Common.TextureBorderColorMode);
    }
    inline void setCpsLodCompensationEnable(const bool value) {
        TheStructure.Common.CpsLodCompensationEnable = value;
    }
    inline bool getCpsLodCompensationEnable(void) const {
        return (TheStructure.Common.CpsLodCompensationEnable);
    }
    inline void setSamplerDisable(const bool value) {
        TheStructure.Common.SamplerDisable = value;
    }
    inline bool getSamplerDisable(void) const {
        return (TheStructure.Common.SamplerDisable);
    }
    inline void setCubeSurfaceControlMode(const CUBE_SURFACE_CONTROL_MODE value) {
        TheStructure.Common.CubeSurfaceControlMode = value;
    }
    inline CUBE_SURFACE_CONTROL_MODE getCubeSurfaceControlMode(void) const {
        return static_cast<CUBE_SURFACE_CONTROL_MODE>(TheStructure.Common.CubeSurfaceControlMode);
    }
    inline void setShadowFunction(const SHADOW_FUNCTION value) {
        TheStructure.Common.ShadowFunction = value;
    }
    inline SHADOW_FUNCTION getShadowFunction(void) const {
        return static_cast<SHADOW_FUNCTION>(TheStructure.Common.ShadowFunction);
    }
    inline void setChromakeyMode(const CHROMAKEY_MODE value) {
        TheStructure.Common.ChromakeyMode = value;
    }
    inline CHROMAKEY_MODE getChromakeyMode(void) const {
        return static_cast<CHROMAKEY_MODE>(TheStructure.Common.ChromakeyMode);
    }
    inline void setChromakeyIndex(const uint32_t value) {
        TheStructure.Common.ChromakeyIndex = value;
    }
    inline uint32_t getChromakeyIndex(void) const {
        return (TheStructure.Common.ChromakeyIndex);
    }
    inline void setChromakeyEnable(const bool value) {
        TheStructure.Common.ChromakeyEnable = value;
    }
    inline bool getChromakeyEnable(void) const {
        return (TheStructure.Common.ChromakeyEnable);
    }
    inline void setMaxLod(const uint32_t value) {
        TheStructure.Common.MaxLod = value;
    }
    inline uint32_t getMaxLod(void) const {
        return (TheStructure.Common.MaxLod);
    }
    inline void setMinLod(const uint32_t value) {
        TheStructure.Common.MinLod = value;
    }
    inline uint32_t getMinLod(void) const {
        return (TheStructure.Common.MinLod);
    }
    inline void setLodClampMagnificationMode(const LOD_CLAMP_MAGNIFICATION_MODE value) {
        TheStructure.Common.LodClampMagnificationMode = value;
    }
    inline LOD_CLAMP_MAGNIFICATION_MODE getLodClampMagnificationMode(void) const {
        return static_cast<LOD_CLAMP_MAGNIFICATION_MODE>(TheStructure.Common.LodClampMagnificationMode);
    }
    inline void setSrgbDecode(const SRGB_DECODE value) {
        TheStructure.Common.SrgbDecode = value;
    }
    inline SRGB_DECODE getSrgbDecode(void) const {
        return static_cast<SRGB_DECODE>(TheStructure.Common.SrgbDecode);
    }
    inline void setReturnFilterWeightForNullTexels(const RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS value) {
        TheStructure.Common.ReturnFilterWeightForNullTexels = value;
    }
    inline RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS getReturnFilterWeightForNullTexels(void) const {
        return static_cast<RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS>(TheStructure.Common.ReturnFilterWeightForNullTexels);
    }
    inline void setReturnFilterWeightForBorderTexels(const RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS value) {
        TheStructure.Common.ReturnFilterWeightForBorderTexels = value;
    }
    inline RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS getReturnFilterWeightForBorderTexels(void) const {
        return static_cast<RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS>(TheStructure.Common.ReturnFilterWeightForBorderTexels);
    }
    typedef enum tagINDIRECTSTATEPOINTER {
        INDIRECTSTATEPOINTER_BIT_SHIFT = 0x6,
        INDIRECTSTATEPOINTER_ALIGN_SIZE = 0x40,
    } INDIRECTSTATEPOINTER;
    inline void setIndirectStatePointer(const uint64_t value) {
        DEBUG_BREAK_IF(value >= 0x100000000);
        TheStructure.Common.IndirectStatePointer = (uint32_t)value >> INDIRECTSTATEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getIndirectStatePointer(void) const {
        return (TheStructure.Common.IndirectStatePointer << INDIRECTSTATEPOINTER_BIT_SHIFT);
    }
    inline void setTczAddressControlMode(const TEXTURE_COORDINATE_MODE value) {
        TheStructure.Common.TczAddressControlMode = value;
    }
    inline TEXTURE_COORDINATE_MODE getTczAddressControlMode(void) const {
        return static_cast<TEXTURE_COORDINATE_MODE>(TheStructure.Common.TczAddressControlMode);
    }
    inline void setTcyAddressControlMode(const TEXTURE_COORDINATE_MODE value) {
        TheStructure.Common.TcyAddressControlMode = value;
    }
    inline TEXTURE_COORDINATE_MODE getTcyAddressControlMode(void) const {
        return static_cast<TEXTURE_COORDINATE_MODE>(TheStructure.Common.TcyAddressControlMode);
    }
    inline void setTcxAddressControlMode(const TEXTURE_COORDINATE_MODE value) {
        TheStructure.Common.TcxAddressControlMode = value;
    }
    inline TEXTURE_COORDINATE_MODE getTcxAddressControlMode(void) const {
        return static_cast<TEXTURE_COORDINATE_MODE>(TheStructure.Common.TcxAddressControlMode);
    }
    inline void setReductionTypeEnable(const bool value) {
        TheStructure.Common.ReductionTypeEnable = value;
    }
    inline bool getReductionTypeEnable(void) const {
        return (TheStructure.Common.ReductionTypeEnable);
    }
    inline void setNonNormalizedCoordinateEnable(const bool value) {
        TheStructure.Common.Non_NormalizedCoordinateEnable = value;
    }
    inline bool getNonNormalizedCoordinateEnable(void) const {
        return (TheStructure.Common.Non_NormalizedCoordinateEnable);
    }
    inline void setTrilinearFilterQuality(const TRILINEAR_FILTER_QUALITY value) {
        TheStructure.Common.TrilinearFilterQuality = value;
    }
    inline TRILINEAR_FILTER_QUALITY getTrilinearFilterQuality(void) const {
        return static_cast<TRILINEAR_FILTER_QUALITY>(TheStructure.Common.TrilinearFilterQuality);
    }
    inline void setRAddressMinFilterRoundingEnable(const bool value) {
        TheStructure.Common.RAddressMinFilterRoundingEnable = value;
    }
    inline bool getRAddressMinFilterRoundingEnable(void) const {
        return (TheStructure.Common.RAddressMinFilterRoundingEnable);
    }
    inline void setRAddressMagFilterRoundingEnable(const bool value) {
        TheStructure.Common.RAddressMagFilterRoundingEnable = value;
    }
    inline bool getRAddressMagFilterRoundingEnable(void) const {
        return (TheStructure.Common.RAddressMagFilterRoundingEnable);
    }
    inline void setVAddressMinFilterRoundingEnable(const bool value) {
        TheStructure.Common.VAddressMinFilterRoundingEnable = value;
    }
    inline bool getVAddressMinFilterRoundingEnable(void) const {
        return (TheStructure.Common.VAddressMinFilterRoundingEnable);
    }
    inline void setVAddressMagFilterRoundingEnable(const bool value) {
        TheStructure.Common.VAddressMagFilterRoundingEnable = value;
    }
    inline bool getVAddressMagFilterRoundingEnable(void) const {
        return (TheStructure.Common.VAddressMagFilterRoundingEnable);
    }
    inline void setUAddressMinFilterRoundingEnable(const bool value) {
        TheStructure.Common.UAddressMinFilterRoundingEnable = value;
    }
    inline bool getUAddressMinFilterRoundingEnable(void) const {
        return (TheStructure.Common.UAddressMinFilterRoundingEnable);
    }
    inline void setUAddressMagFilterRoundingEnable(const bool value) {
        TheStructure.Common.UAddressMagFilterRoundingEnable = value;
    }
    inline bool getUAddressMagFilterRoundingEnable(void) const {
        return (TheStructure.Common.UAddressMagFilterRoundingEnable);
    }
    inline void setMaximumAnisotropy(const MAXIMUM_ANISOTROPY value) {
        TheStructure.Common.MaximumAnisotropy = value;
    }
    inline MAXIMUM_ANISOTROPY getMaximumAnisotropy(void) const {
        return static_cast<MAXIMUM_ANISOTROPY>(TheStructure.Common.MaximumAnisotropy);
    }
    inline void setReductionType(const REDUCTION_TYPE value) {
        TheStructure.Common.ReductionType = value;
    }
    inline REDUCTION_TYPE getReductionType(void) const {
        return static_cast<REDUCTION_TYPE>(TheStructure.Common.ReductionType);
    }
} SAMPLER_STATE;
STATIC_ASSERT(16 == sizeof(SAMPLER_STATE));

typedef struct tagSTATE_BASE_ADDRESS {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t GeneralStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 3);
            uint64_t GeneralStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t GeneralStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_43 : BITFIELD_RANGE(11, 11);
            uint64_t GeneralStateBaseAddress : BITFIELD_RANGE(12, 63);
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 15);
            uint32_t StatelessDataPortAccessMemoryObjectControlState_Reserved : BITFIELD_RANGE(16, 16);
            uint32_t StatelessDataPortAccessMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(17, 22);
            uint32_t Reserved_119 : BITFIELD_RANGE(23, 31);
            uint64_t SurfaceStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_129 : BITFIELD_RANGE(1, 3);
            uint64_t SurfaceStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t SurfaceStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_139 : BITFIELD_RANGE(11, 11);
            uint64_t SurfaceStateBaseAddress : BITFIELD_RANGE(12, 63);
            uint64_t DynamicStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_193 : BITFIELD_RANGE(1, 3);
            uint64_t DynamicStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t DynamicStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_203 : BITFIELD_RANGE(11, 11);
            uint64_t DynamicStateBaseAddress : BITFIELD_RANGE(12, 63);
            uint64_t IndirectObjectBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_257 : BITFIELD_RANGE(1, 3);
            uint64_t IndirectObjectMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t IndirectObjectMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_267 : BITFIELD_RANGE(11, 11);
            uint64_t IndirectObjectBaseAddress : BITFIELD_RANGE(12, 63);
            uint64_t InstructionBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_321 : BITFIELD_RANGE(1, 3);
            uint64_t InstructionMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t InstructionMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_331 : BITFIELD_RANGE(11, 11);
            uint64_t InstructionBaseAddress : BITFIELD_RANGE(12, 63);
            uint32_t GeneralStateBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_385 : BITFIELD_RANGE(1, 11);
            uint32_t GeneralStateBufferSize : BITFIELD_RANGE(12, 31);
            uint32_t DynamicStateBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_417 : BITFIELD_RANGE(1, 11);
            uint32_t DynamicStateBufferSize : BITFIELD_RANGE(12, 31);
            uint32_t IndirectObjectBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_449 : BITFIELD_RANGE(1, 11);
            uint32_t IndirectObjectBufferSize : BITFIELD_RANGE(12, 31);
            uint32_t InstructionBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_481 : BITFIELD_RANGE(1, 11);
            uint32_t InstructionBufferSize : BITFIELD_RANGE(12, 31);
            uint64_t BindlessSurfaceStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_513 : BITFIELD_RANGE(1, 3);
            uint64_t BindlessSurfaceStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t BindlessSurfaceStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_523 : BITFIELD_RANGE(11, 11);
            uint64_t BindlessSurfaceStateBaseAddress : BITFIELD_RANGE(12, 63);
            uint32_t Reserved_576 : BITFIELD_RANGE(0, 11);
            uint32_t BindlessSurfaceStateSize : BITFIELD_RANGE(12, 31);
            uint64_t BindlessSamplerStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_609 : BITFIELD_RANGE(1, 3);
            uint64_t BindlessSamplerStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t BindlessSamplerStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_619 : BITFIELD_RANGE(11, 11);
            uint64_t BindlessSamplerStateBaseAddress : BITFIELD_RANGE(12, 63);
            uint32_t Reserved_672 : BITFIELD_RANGE(0, 11);
            uint32_t BindlessSamplerStateBufferSize : BITFIELD_RANGE(12, 31);
        } Common;
        uint32_t RawData[22];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x14,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_STATE_BASE_ADDRESS = 0x1,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagPATCH_CONSTANTS {
        GENERALSTATEBASEADDRESS_BYTEOFFSET = 0x4,
        GENERALSTATEBASEADDRESS_INDEX = 0x1,
        SURFACESTATEBASEADDRESS_BYTEOFFSET = 0x10,
        SURFACESTATEBASEADDRESS_INDEX = 0x4,
        DYNAMICSTATEBASEADDRESS_BYTEOFFSET = 0x18,
        DYNAMICSTATEBASEADDRESS_INDEX = 0x6,
        INDIRECTOBJECTBASEADDRESS_BYTEOFFSET = 0x20,
        INDIRECTOBJECTBASEADDRESS_INDEX = 0x8,
        INSTRUCTIONBASEADDRESS_BYTEOFFSET = 0x28,
        INSTRUCTIONBASEADDRESS_INDEX = 0xa,
        BINDLESSSURFACESTATEBASEADDRESS_BYTEOFFSET = 0x40,
        BINDLESSSURFACESTATEBASEADDRESS_INDEX = 0x10,
        BINDLESSSAMPLERSTATEBASEADDRESS_BYTEOFFSET = 0x4c,
        BINDLESSSAMPLERSTATEBASEADDRESS_INDEX = 0x13,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_BASE_ADDRESS;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagSTATE_BASE_ADDRESS sInit(void) {
        STATE_BASE_ADDRESS state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 22);
        return TheStructure.RawData[index];
    }
    inline void setGeneralStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.GeneralStateBaseAddressModifyEnable = value;
    }
    inline bool getGeneralStateBaseAddressModifyEnable(void) const {
        return (TheStructure.Common.GeneralStateBaseAddressModifyEnable);
    }
    inline void setGeneralStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.GeneralStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getGeneralStateMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.GeneralStateMemoryObjectControlState_Reserved);
    }
    inline void setGeneralStateMemoryObjectControlStateIndexToMocsTables(const uint64_t value) {
        TheStructure.Common.GeneralStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getGeneralStateMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.GeneralStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagGENERALSTATEBASEADDRESS {
        GENERALSTATEBASEADDRESS_BIT_SHIFT = 0xc,
        GENERALSTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } GENERALSTATEBASEADDRESS;
    inline void setGeneralStateBaseAddress(const uint64_t value) {
        TheStructure.Common.GeneralStateBaseAddress = value >> GENERALSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getGeneralStateBaseAddress(void) const {
        return (TheStructure.Common.GeneralStateBaseAddress << GENERALSTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setStatelessDataPortAccessMemoryObjectControlStateReserved(const uint32_t value) {
        TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_Reserved = value;
    }
    inline uint32_t getStatelessDataPortAccessMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_Reserved);
    }
    inline void setStatelessDataPortAccessMemoryObjectControlStateIndexToMocsTables(const uint32_t value) {
        TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint32_t getStatelessDataPortAccessMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_IndexToMocsTables << 1);
    }
    inline void setStatelessDataPortAccessMemoryObjectControlState(const uint32_t value) {
        TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_Reserved = value;
        TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_IndexToMocsTables = (value >> 1);
    }
    inline uint32_t getStatelessDataPortAccessMemoryObjectControlState(void) const {
        uint32_t mocs = TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_Reserved;
        mocs |= (TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_IndexToMocsTables << 1);
        return (mocs);
    }
    inline void setSurfaceStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.SurfaceStateBaseAddressModifyEnable = value;
    }
    inline bool getSurfaceStateBaseAddressModifyEnable(void) const {
        return (TheStructure.Common.SurfaceStateBaseAddressModifyEnable);
    }
    inline void setSurfaceStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.SurfaceStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getSurfaceStateMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.SurfaceStateMemoryObjectControlState_Reserved);
    }
    inline void setSurfaceStateMemoryObjectControlStateIndexToMocsTables(const uint64_t value) {
        TheStructure.Common.SurfaceStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getSurfaceStateMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.SurfaceStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagSURFACESTATEBASEADDRESS {
        SURFACESTATEBASEADDRESS_BIT_SHIFT = 0xc,
        SURFACESTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } SURFACESTATEBASEADDRESS;
    inline void setSurfaceStateBaseAddress(const uint64_t value) {
        TheStructure.Common.SurfaceStateBaseAddress = value >> SURFACESTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getSurfaceStateBaseAddress(void) const {
        return (TheStructure.Common.SurfaceStateBaseAddress << SURFACESTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setDynamicStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.DynamicStateBaseAddressModifyEnable = value;
    }
    inline bool getDynamicStateBaseAddressModifyEnable(void) const {
        return (TheStructure.Common.DynamicStateBaseAddressModifyEnable);
    }
    inline void setDynamicStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.DynamicStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getDynamicStateMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.DynamicStateMemoryObjectControlState_Reserved);
    }
    inline void setDynamicStateMemoryObjectControlStateIndexToMocsTables(const uint64_t value) {
        TheStructure.Common.DynamicStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getDynamicStateMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.DynamicStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagDYNAMICSTATEBASEADDRESS {
        DYNAMICSTATEBASEADDRESS_BIT_SHIFT = 0xc,
        DYNAMICSTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } DYNAMICSTATEBASEADDRESS;
    inline void setDynamicStateBaseAddress(const uint64_t value) {
        TheStructure.Common.DynamicStateBaseAddress = value >> DYNAMICSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getDynamicStateBaseAddress(void) const {
        return (TheStructure.Common.DynamicStateBaseAddress << DYNAMICSTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setIndirectObjectBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.IndirectObjectBaseAddressModifyEnable = value;
    }
    inline bool getIndirectObjectBaseAddressModifyEnable(void) const {
        return (TheStructure.Common.IndirectObjectBaseAddressModifyEnable);
    }
    inline void setIndirectObjectMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.IndirectObjectMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getIndirectObjectMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.IndirectObjectMemoryObjectControlState_Reserved);
    }
    inline void setIndirectObjectMemoryObjectControlStateIndexToMocsTables(const uint64_t value) {
        TheStructure.Common.IndirectObjectMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getIndirectObjectMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.IndirectObjectMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagINDIRECTOBJECTBASEADDRESS {
        INDIRECTOBJECTBASEADDRESS_BIT_SHIFT = 0xc,
        INDIRECTOBJECTBASEADDRESS_ALIGN_SIZE = 0x1000,
    } INDIRECTOBJECTBASEADDRESS;
    inline void setIndirectObjectBaseAddress(const uint64_t value) {
        TheStructure.Common.IndirectObjectBaseAddress = value >> INDIRECTOBJECTBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getIndirectObjectBaseAddress(void) const {
        return (TheStructure.Common.IndirectObjectBaseAddress << INDIRECTOBJECTBASEADDRESS_BIT_SHIFT);
    }
    inline void setInstructionBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.InstructionBaseAddressModifyEnable = value;
    }
    inline bool getInstructionBaseAddressModifyEnable(void) const {
        return (TheStructure.Common.InstructionBaseAddressModifyEnable);
    }
    inline void setInstructionMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.InstructionMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getInstructionMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.InstructionMemoryObjectControlState_Reserved);
    }
    inline void setInstructionMemoryObjectControlStateIndexToMocsTables(const uint64_t value) {
        TheStructure.Common.InstructionMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getInstructionMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.InstructionMemoryObjectControlState_IndexToMocsTables << 1);
    }
    inline void setInstructionMemoryObjectControlState(const uint32_t value) {
        uint64_t val = static_cast<uint64_t>(value);
        TheStructure.Common.InstructionMemoryObjectControlState_Reserved = val;
        TheStructure.Common.InstructionMemoryObjectControlState_IndexToMocsTables = (val >> 1);
    }
    inline uint32_t getInstructionMemoryObjectControlState(void) const {
        uint64_t mocs = TheStructure.Common.InstructionMemoryObjectControlState_Reserved;
        mocs |= (TheStructure.Common.InstructionMemoryObjectControlState_IndexToMocsTables << 1);
        return static_cast<uint32_t>(mocs);
    }
    typedef enum tagINSTRUCTIONBASEADDRESS {
        INSTRUCTIONBASEADDRESS_BIT_SHIFT = 0xc,
        INSTRUCTIONBASEADDRESS_ALIGN_SIZE = 0x1000,
    } INSTRUCTIONBASEADDRESS;
    inline void setInstructionBaseAddress(const uint64_t value) {
        TheStructure.Common.InstructionBaseAddress = value >> INSTRUCTIONBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getInstructionBaseAddress(void) const {
        return (TheStructure.Common.InstructionBaseAddress << INSTRUCTIONBASEADDRESS_BIT_SHIFT);
    }
    inline void setGeneralStateBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.GeneralStateBufferSizeModifyEnable = value;
    }
    inline bool getGeneralStateBufferSizeModifyEnable(void) const {
        return (TheStructure.Common.GeneralStateBufferSizeModifyEnable);
    }
    inline void setGeneralStateBufferSize(const uint32_t value) {
        TheStructure.Common.GeneralStateBufferSize = value;
    }
    inline uint32_t getGeneralStateBufferSize(void) const {
        return (TheStructure.Common.GeneralStateBufferSize);
    }
    inline void setDynamicStateBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.DynamicStateBufferSizeModifyEnable = value;
    }
    inline bool getDynamicStateBufferSizeModifyEnable(void) const {
        return (TheStructure.Common.DynamicStateBufferSizeModifyEnable);
    }
    inline void setDynamicStateBufferSize(const uint32_t value) {
        TheStructure.Common.DynamicStateBufferSize = value;
    }
    inline uint32_t getDynamicStateBufferSize(void) const {
        return (TheStructure.Common.DynamicStateBufferSize);
    }
    inline void setIndirectObjectBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.IndirectObjectBufferSizeModifyEnable = value;
    }
    inline bool getIndirectObjectBufferSizeModifyEnable(void) const {
        return (TheStructure.Common.IndirectObjectBufferSizeModifyEnable);
    }
    inline void setIndirectObjectBufferSize(const uint32_t value) {
        TheStructure.Common.IndirectObjectBufferSize = value;
    }
    inline uint32_t getIndirectObjectBufferSize(void) const {
        return (TheStructure.Common.IndirectObjectBufferSize);
    }
    inline void setInstructionBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.InstructionBufferSizeModifyEnable = value;
    }
    inline bool getInstructionBufferSizeModifyEnable(void) const {
        return (TheStructure.Common.InstructionBufferSizeModifyEnable);
    }
    inline void setInstructionBufferSize(const uint32_t value) {
        TheStructure.Common.InstructionBufferSize = value;
    }
    inline uint32_t getInstructionBufferSize(void) const {
        return (TheStructure.Common.InstructionBufferSize);
    }
    inline void setBindlessSurfaceStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.BindlessSurfaceStateBaseAddressModifyEnable = value;
    }
    inline bool getBindlessSurfaceStateBaseAddressModifyEnable(void) const {
        return (TheStructure.Common.BindlessSurfaceStateBaseAddressModifyEnable);
    }
    inline void setBindlessSurfaceStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.BindlessSurfaceStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getBindlessSurfaceStateMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.BindlessSurfaceStateMemoryObjectControlState_Reserved);
    }
    inline void setBindlessSurfaceStateMemoryObjectControlStateIndexToMocsTables(const uint64_t value) {
        TheStructure.Common.BindlessSurfaceStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getBindlessSurfaceStateMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.BindlessSurfaceStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagBINDLESSSURFACESTATEBASEADDRESS {
        BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT = 0xc,
        BINDLESSSURFACESTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } BINDLESSSURFACESTATEBASEADDRESS;
    inline void setBindlessSurfaceStateBaseAddress(const uint64_t value) {
        TheStructure.Common.BindlessSurfaceStateBaseAddress = value >> BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBindlessSurfaceStateBaseAddress(void) const {
        return (TheStructure.Common.BindlessSurfaceStateBaseAddress << BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setBindlessSurfaceStateSize(const uint32_t value) {
        TheStructure.Common.BindlessSurfaceStateSize = value;
    }
    inline uint32_t getBindlessSurfaceStateSize(void) const {
        return (TheStructure.Common.BindlessSurfaceStateSize);
    }
    inline void setBindlessSamplerStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.BindlessSamplerStateBaseAddressModifyEnable = value;
    }
    inline bool getBindlessSamplerStateBaseAddressModifyEnable(void) const {
        return (TheStructure.Common.BindlessSamplerStateBaseAddressModifyEnable);
    }
    inline void setBindlessSamplerStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.BindlessSamplerStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getBindlessSamplerStateMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.BindlessSamplerStateMemoryObjectControlState_Reserved);
    }
    inline void setBindlessSamplerStateMemoryObjectControlStateIndexToMocsTables(const uint64_t value) {
        TheStructure.Common.BindlessSamplerStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getBindlessSamplerStateMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.BindlessSamplerStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagBINDLESSSAMPLERSTATEBASEADDRESS {
        BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT = 0xc,
        BINDLESSSAMPLERSTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } BINDLESSSAMPLERSTATEBASEADDRESS;
    inline void setBindlessSamplerStateBaseAddress(const uint64_t value) {
        TheStructure.Common.BindlessSamplerStateBaseAddress = value >> BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBindlessSamplerStateBaseAddress(void) const {
        return (TheStructure.Common.BindlessSamplerStateBaseAddress << BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setBindlessSamplerStateBufferSize(const uint32_t value) {
        TheStructure.Common.BindlessSamplerStateBufferSize = value;
    }
    inline uint32_t getBindlessSamplerStateBufferSize(void) const {
        return (TheStructure.Common.BindlessSamplerStateBufferSize);
    }
} STATE_BASE_ADDRESS;
STATIC_ASSERT(88 == sizeof(STATE_BASE_ADDRESS));

typedef struct tagMI_REPORT_PERF_COUNT {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t UseGlobalGtt : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 3);
            uint64_t CoreModeEnable : BITFIELD_RANGE(4, 4);
            uint64_t Reserved_37 : BITFIELD_RANGE(5, 5);
            uint64_t MemoryAddress : BITFIELD_RANGE(6, 63);
            uint32_t ReportId;
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT = 0x28,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    typedef enum tagPATCH_CONSTANTS {
        MEMORYADDRESS_BYTEOFFSET = 0x4,
        MEMORYADDRESS_INDEX = 0x1,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_REPORT_PERF_COUNT sInit(void) {
        MI_REPORT_PERF_COUNT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt(void) const {
        return (TheStructure.Common.UseGlobalGtt);
    }
    inline void setCoreModeEnable(const uint64_t value) {
        TheStructure.Common.CoreModeEnable = value;
    }
    inline uint64_t getCoreModeEnable(void) const {
        return (TheStructure.Common.CoreModeEnable);
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x6,
        MEMORYADDRESS_ALIGN_SIZE = 0x40,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress(void) const {
        return (TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT);
    }
    inline void setReportId(const uint32_t value) {
        TheStructure.Common.ReportId = value;
    }
    inline uint32_t getReportId(void) const {
        return (TheStructure.Common.ReportId);
    }
} MI_REPORT_PERF_COUNT;
STATIC_ASSERT(16 == sizeof(MI_REPORT_PERF_COUNT));

typedef struct tagGPGPU_CSR_BASE_ADDRESS {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 11);
            uint64_t GpgpuCsrBaseAddress : BITFIELD_RANGE(12, 63);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_UNNAMED_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_GPGPU_CSR_BASE_ADDRESS = 0x4,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagPATCH_CONSTANTS {
        GPGPUCSRBASEADDRESS_BYTEOFFSET = 0x4,
        GPGPUCSRBASEADDRESS_INDEX = 0x1,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_UNNAMED_1;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_GPGPU_CSR_BASE_ADDRESS;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagGPGPU_CSR_BASE_ADDRESS sInit(void) {
        GPGPU_CSR_BASE_ADDRESS state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(uint32_t const index) {
        DEBUG_BREAK_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    typedef enum tagGPGPUCSRBASEADDRESS {
        GPGPUCSRBASEADDRESS_BIT_SHIFT = 0xC,
        GPGPUCSRBASEADDRESS_ALIGN_SIZE = 0x1000,
    } GPGPUCSRBASEADDRESS;
    inline uint64_t getGpgpuCsrBaseAddress() const {
        return (uint64_t)TheStructure.Common.GpgpuCsrBaseAddress << GPGPUCSRBASEADDRESS_BIT_SHIFT;
    }
    inline void setGpgpuCsrBaseAddress(uint64_t value) {
        TheStructure.Common.GpgpuCsrBaseAddress = value >> GPGPUCSRBASEADDRESS_BIT_SHIFT;
    }
} GPGPU_CSR_BASE_ADDRESS;
STATIC_ASSERT(12 == sizeof(GPGPU_CSR_BASE_ADDRESS));

typedef struct tagSTATE_SIP {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 3);
            uint64_t SystemInstructionPointer : BITFIELD_RANGE(4, 63);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x1,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_STATE_SIP = 0x2,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagPATCH_CONSTANTS {
        SYSTEMINSTRUCTIONPOINTER_BYTEOFFSET = 0x4,
        SYSTEMINSTRUCTIONPOINTER_INDEX = 0x1,
    } PATCH_CONSTANTS;
    void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_SIP;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagSTATE_SIP sInit() {
        STATE_SIP state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(uint32_t const index) {
        DEBUG_BREAK_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    typedef enum tagSYSTEMINSTRUCTIONPOINTER {
        SYSTEMINSTRUCTIONPOINTER_BIT_SHIFT = 0x4,
        SYSTEMINSTRUCTIONPOINTER_ALIGN_SIZE = 0x10,
    } SYSTEMINSTRUCTIONPOINTER;
    inline uint64_t getSystemInstructionPointer() const {
        return (uint64_t)TheStructure.Common.SystemInstructionPointer << SYSTEMINSTRUCTIONPOINTER_BIT_SHIFT;
    }
    inline void setSystemInstructionPointer(uint64_t value) {
        TheStructure.Common.SystemInstructionPointer = value >> SYSTEMINSTRUCTIONPOINTER_BIT_SHIFT;
    }
} STATE_SIP;
STATIC_ASSERT(12 == sizeof(STATE_SIP));

struct MI_USER_INTERRUPT {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 22);
            uint32_t MICommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    enum MI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_USER_INTERRUPT = 2,
    };
    enum COMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0,
    };
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MICommandOpcode = MI_COMMAND_OPCODE_MI_USER_INTERRUPT;
    }
    static MI_USER_INTERRUPT sInit(void) {
        MI_USER_INTERRUPT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        return TheStructure.RawData[index];
    }
};
STATIC_ASSERT(4 == sizeof(MI_USER_INTERRUPT));

typedef struct tagMI_FLUSH_DW {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 7);
            uint32_t NotifyEnable : BITFIELD_RANGE(8, 8);
            uint32_t FlushLlc : BITFIELD_RANGE(9, 9);
            uint32_t Reserved_10 : BITFIELD_RANGE(10, 13);
            uint32_t PostSyncOperation : BITFIELD_RANGE(14, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_17 : BITFIELD_RANGE(17, 17);
            uint32_t TlbInvalidate : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_19 : BITFIELD_RANGE(19, 20);
            uint32_t StoreDataIndex : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_22 : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint64_t DestinationAddressType : BITFIELD_RANGE(2, 2);
            uint64_t DestinationAddress : BITFIELD_RANGE(3, 47);
            uint64_t Reserved_80 : BITFIELD_RANGE(48, 63);
            // DWORD 3
            uint64_t ImmediateData;
        } Common;
        uint32_t RawData[5];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x3,
    } DWORD_LENGTH;
    typedef enum tagPOST_SYNC_OPERATION {
        POST_SYNC_OPERATION_NO_WRITE = 0x0,
        POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD = 0x1,
        POST_SYNC_OPERATION_WRITE_TIMESTAMP_REGISTER = 0x3,
    } POST_SYNC_OPERATION;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_FLUSH_DW = 0x26,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    typedef enum tagDESTINATION_ADDRESS_TYPE {
        DESTINATION_ADDRESS_TYPE_PPGTT = 0x0,
        DESTINATION_ADDRESS_TYPE_GGTT = 0x1,
    } DESTINATION_ADDRESS_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.PostSyncOperation = POST_SYNC_OPERATION_NO_WRITE;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_FLUSH_DW;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
        TheStructure.Common.DestinationAddressType = DESTINATION_ADDRESS_TYPE_PPGTT;
    }
    static tagMI_FLUSH_DW sInit(void) {
        MI_FLUSH_DW state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 5);
        return TheStructure.RawData[index];
    }
    inline void setNotifyEnable(const bool value) {
        TheStructure.Common.NotifyEnable = value;
    }
    inline bool getNotifyEnable(void) const {
        return TheStructure.Common.NotifyEnable;
    }
    inline void setFlushLlc(const bool value) {
        TheStructure.Common.FlushLlc = value;
    }
    inline bool getFlushLlc(void) const {
        return TheStructure.Common.FlushLlc;
    }
    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }
    inline POST_SYNC_OPERATION getPostSyncOperation(void) const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.PostSyncOperation);
    }
    inline void setTlbInvalidate(const bool value) {
        TheStructure.Common.TlbInvalidate = value;
    }
    inline bool getTlbInvalidate(void) const {
        return TheStructure.Common.TlbInvalidate;
    }
    inline void setStoreDataIndex(const bool value) {
        TheStructure.Common.StoreDataIndex = value;
    }
    inline bool getStoreDataIndex(void) const {
        return TheStructure.Common.StoreDataIndex;
    }
    inline void setDestinationAddressType(const DESTINATION_ADDRESS_TYPE value) {
        TheStructure.Common.DestinationAddressType = value;
    }
    inline DESTINATION_ADDRESS_TYPE getDestinationAddressType(void) const {
        return static_cast<DESTINATION_ADDRESS_TYPE>(TheStructure.Common.DestinationAddressType);
    }
    typedef enum tagDESTINATIONADDRESS {
        DESTINATIONADDRESS_BIT_SHIFT = 0x3,
        DESTINATIONADDRESS_ALIGN_SIZE = 0x8,
    } DESTINATIONADDRESS;
    inline void setDestinationAddress(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xfffffffffff8L);
        TheStructure.Common.DestinationAddress = value >> DESTINATIONADDRESS_BIT_SHIFT;
    }
    inline uint64_t getDestinationAddress(void) const {
        return TheStructure.Common.DestinationAddress << DESTINATIONADDRESS_BIT_SHIFT;
    }
    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }
    inline uint64_t getImmediateData(void) const {
        return TheStructure.Common.ImmediateData;
    }
} MI_FLUSH_DW;
STATIC_ASSERT(20 == sizeof(MI_FLUSH_DW));

typedef struct tagXY_SRC_COPY_BLT {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 10);
            uint32_t DestTilingEnable : BITFIELD_RANGE(11, 11);
            uint32_t Reserved_12 : BITFIELD_RANGE(12, 14);
            uint32_t SrcTilingEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 19);
            uint32_t _32BppByteMask : BITFIELD_RANGE(20, 21);
            uint32_t InstructionTarget_Opcode : BITFIELD_RANGE(22, 28);
            uint32_t Client : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t DestinationPitch : BITFIELD_RANGE(0, 15);
            uint32_t RasterOperation : BITFIELD_RANGE(16, 23);
            uint32_t ColorDepth : BITFIELD_RANGE(24, 25);
            uint32_t Reserved_58 : BITFIELD_RANGE(26, 29);
            uint32_t ClippingEnabled : BITFIELD_RANGE(30, 30);
            uint32_t Reserved_63 : BITFIELD_RANGE(31, 31);
            // DWORD 2
            uint32_t DestinationX1Coordinate_Left : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY1Coordinate_Top : BITFIELD_RANGE(16, 31);
            // DWORD 3
            uint32_t DestinationX2Coordinate_Right : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY2Coordinate_Bottom : BITFIELD_RANGE(16, 31);
            // DWORD 4
            uint64_t DestinationBaseAddress;
            // DWORD 6
            uint32_t SourceX1Coordinate_Left : BITFIELD_RANGE(0, 15);
            uint32_t SourceY1Coordinate_Top : BITFIELD_RANGE(16, 31);
            // DWORD 7
            uint32_t SourcePitch : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_240 : BITFIELD_RANGE(16, 31);
            // DWORD 8
            uint64_t SourceBaseAddress;
        } Common;
        uint32_t RawData[10];
    } TheStructure;
    typedef enum tagDEST_TILING_ENABLE {
        DEST_TILING_ENABLE_TILING_DISABLED_LINEAR_BLIT = 0x0,
        DEST_TILING_ENABLE_TILING_ENABLED = 0x1,
    } DEST_TILING_ENABLE;
    typedef enum tagSRC_TILING_ENABLE {
        SRC_TILING_ENABLE_TILING_DISABLED_LINEAR = 0x0,
        SRC_TILING_ENABLE_TILING_ENABLED = 0x1,
    } SRC_TILING_ENABLE;
    typedef enum tag_32BPP_BYTE_MASK {
        _32BPP_BYTE_MASK_WRITE_RGB_CHANNEL = 0x1,
        _32BPP_BYTE_MASK_WRITE_ALPHA_CHANNEL = 0x2,
    } _32BPP_BYTE_MASK;
    typedef enum tagCLIENT {
        CLIENT_2D_PROCESSOR = 0x2,
    } CLIENT;
    typedef enum tagCOLOR_DEPTH {
        COLOR_DEPTH_8_BIT_COLOR = 0x0,
        COLOR_DEPTH_16_BIT_COLOR565 = 0x1,
        COLOR_DEPTH_16_BIT_COLOR1555 = 0x2,
        COLOR_DEPTH_32_BIT_COLOR = 0x3,
    } COLOR_DEPTH;
    typedef enum tagCLIPPING_ENABLED {
        CLIPPING_ENABLED_DISABLED = 0x0,
        CLIPPING_ENABLED_ENABLED = 0x1,
    } CLIPPING_ENABLED;
    typedef enum tagINSTRUCTIONTARGET_OPCODE {
        INSTRUCTIONTARGET_OPCODE_OPCODE = 0x53,
    } INSTRUCTIONTARGET_OPCODE;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x8,
    } DWORD_LENGTH;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DestTilingEnable = DEST_TILING_ENABLE_TILING_DISABLED_LINEAR_BLIT;
        TheStructure.Common.SrcTilingEnable = SRC_TILING_ENABLE_TILING_DISABLED_LINEAR;
        TheStructure.Common.Client = CLIENT_2D_PROCESSOR;
        TheStructure.Common.ColorDepth = COLOR_DEPTH_8_BIT_COLOR;
        TheStructure.Common.ClippingEnabled = CLIPPING_ENABLED_DISABLED;
        TheStructure.Common.InstructionTarget_Opcode = INSTRUCTIONTARGET_OPCODE_OPCODE;
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
    }
    static tagXY_SRC_COPY_BLT sInit(void) {
        XY_SRC_COPY_BLT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 10);
        return TheStructure.RawData[index];
    }
    inline void setDestTilingEnable(const DEST_TILING_ENABLE value) {
        TheStructure.Common.DestTilingEnable = value;
    }
    inline DEST_TILING_ENABLE getDestTilingEnable(void) const {
        return static_cast<DEST_TILING_ENABLE>(TheStructure.Common.DestTilingEnable);
    }
    inline void setSrcTilingEnable(const SRC_TILING_ENABLE value) {
        TheStructure.Common.SrcTilingEnable = value;
    }
    inline SRC_TILING_ENABLE getSrcTilingEnable(void) const {
        return static_cast<SRC_TILING_ENABLE>(TheStructure.Common.SrcTilingEnable);
    }
    inline void set32BppByteMask(const _32BPP_BYTE_MASK value) {
        TheStructure.Common._32BppByteMask = value;
    }
    inline _32BPP_BYTE_MASK get32BppByteMask(void) const {
        return static_cast<_32BPP_BYTE_MASK>(TheStructure.Common._32BppByteMask);
    }
    inline void setInstructionTargetOpcode(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1fc00000);
        TheStructure.Common.InstructionTarget_Opcode = value;
    }
    inline uint32_t getInstructionTargetOpcode(void) const {
        return TheStructure.Common.InstructionTarget_Opcode;
    }
    inline void setClient(const CLIENT value) {
        TheStructure.Common.Client = value;
    }
    inline CLIENT getClient(void) const {
        return static_cast<CLIENT>(TheStructure.Common.Client);
    }
    inline void setDestinationPitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationPitch = value;
    }
    inline uint32_t getDestinationPitch(void) const {
        return TheStructure.Common.DestinationPitch;
    }
    inline void setRasterOperation(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xff0000);
        TheStructure.Common.RasterOperation = value;
    }
    inline uint32_t getRasterOperation(void) const {
        return TheStructure.Common.RasterOperation;
    }
    inline void setColorDepth(const COLOR_DEPTH value) {
        TheStructure.Common.ColorDepth = value;
    }
    inline COLOR_DEPTH getColorDepth(void) const {
        return static_cast<COLOR_DEPTH>(TheStructure.Common.ColorDepth);
    }
    inline void setClippingEnabled(const CLIPPING_ENABLED value) {
        TheStructure.Common.ClippingEnabled = value;
    }
    inline CLIPPING_ENABLED getClippingEnabled(void) const {
        return static_cast<CLIPPING_ENABLED>(TheStructure.Common.ClippingEnabled);
    }
    inline void setDestinationX1CoordinateLeft(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationX1Coordinate_Left = value;
    }
    inline uint32_t getDestinationX1CoordinateLeft(void) const {
        return TheStructure.Common.DestinationX1Coordinate_Left;
    }
    inline void setDestinationY1CoordinateTop(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff0000);
        TheStructure.Common.DestinationY1Coordinate_Top = value;
    }
    inline uint32_t getDestinationY1CoordinateTop(void) const {
        return TheStructure.Common.DestinationY1Coordinate_Top;
    }
    inline void setTransferWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationX2Coordinate_Right = value;
    }
    inline uint32_t getTransferWidth(void) const {
        return TheStructure.Common.DestinationX2Coordinate_Right;
    }
    inline void setTransferHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff0000);
        TheStructure.Common.DestinationY2Coordinate_Bottom = value;
    }
    inline uint32_t getTransferHeight(void) const {
        return TheStructure.Common.DestinationY2Coordinate_Bottom;
    }
    inline void setDestinationBaseAddress(const uint64_t value) {
        TheStructure.Common.DestinationBaseAddress = value;
    }
    inline uint64_t getDestinationBaseAddress(void) const {
        return TheStructure.Common.DestinationBaseAddress;
    }
    inline void setSourceX1CoordinateLeft(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.SourceX1Coordinate_Left = value;
    }
    inline uint32_t getSourceX1CoordinateLeft(void) const {
        return TheStructure.Common.SourceX1Coordinate_Left;
    }
    inline void setSourceY1CoordinateTop(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff0000);
        TheStructure.Common.SourceY1Coordinate_Top = value;
    }
    inline uint32_t getSourceY1CoordinateTop(void) const {
        return TheStructure.Common.SourceY1Coordinate_Top;
    }
    inline void setSourcePitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.SourcePitch = value;
    }
    inline uint32_t getSourcePitch(void) const {
        return TheStructure.Common.SourcePitch;
    }
    inline void setSourceBaseAddress(const uint64_t value) {
        TheStructure.Common.SourceBaseAddress = value;
    }
    inline uint64_t getSourceBaseAddress(void) const {
        return TheStructure.Common.SourceBaseAddress;
    }
} XY_SRC_COPY_BLT;
STATIC_ASSERT(40 == sizeof(XY_SRC_COPY_BLT));

typedef struct tagGRF {
    union tagTheStructure {
        float fRegs[8];
        uint32_t dwRegs[8];
        uint16_t wRegs[16];
        uint32_t RawData[8];
    } TheStructure;
} GRF;
STATIC_ASSERT(32 == sizeof(GRF));

typedef struct tagMI_BATCH_BUFFER_START {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t AddressSpaceIndicator : BITFIELD_RANGE(8, 8);
            uint32_t Reserved_9 : BITFIELD_RANGE(9, 9);
            uint32_t ResourceStreamerEnable : BITFIELD_RANGE(10, 10);
            uint32_t Reserved_11 : BITFIELD_RANGE(11, 14);
            uint32_t PredicationEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 19);
            uint32_t PoshEnable : BITFIELD_RANGE(20, 20);
            uint32_t PoshStart : BITFIELD_RANGE(21, 21);
            uint32_t SecondLevelBatchBuffer : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint64_t BatchBufferStartAddress_Graphicsaddress39_2 : BITFIELD_RANGE(2, 47);
            uint64_t BatchBufferStartAddress_Reserved : BITFIELD_RANGE(48, 63);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tagADDRESS_SPACE_INDICATOR {
        ADDRESS_SPACE_INDICATOR_GGTT = 0x0,
        ADDRESS_SPACE_INDICATOR_PPGTT = 0x1,
    } ADDRESS_SPACE_INDICATOR;
    typedef enum tagSECOND_LEVEL_BATCH_BUFFER {
        SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH = 0x0,
        SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH = 0x1,
    } SECOND_LEVEL_BATCH_BUFFER;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_BATCH_BUFFER_START = 0x31,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.AddressSpaceIndicator = ADDRESS_SPACE_INDICATOR_GGTT;
        TheStructure.Common.SecondLevelBatchBuffer = SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_BATCH_BUFFER_START;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_BATCH_BUFFER_START sInit(void) {
        MI_BATCH_BUFFER_START state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setAddressSpaceIndicator(const ADDRESS_SPACE_INDICATOR value) {
        TheStructure.Common.AddressSpaceIndicator = value;
    }
    inline ADDRESS_SPACE_INDICATOR getAddressSpaceIndicator(void) const {
        return static_cast<ADDRESS_SPACE_INDICATOR>(TheStructure.Common.AddressSpaceIndicator);
    }
    inline void setResourceStreamerEnable(const bool value) {
        TheStructure.Common.ResourceStreamerEnable = value;
    }
    inline bool getResourceStreamerEnable(void) const {
        return (TheStructure.Common.ResourceStreamerEnable);
    }
    inline void setPredicationEnable(const uint32_t value) {
        TheStructure.Common.PredicationEnable = value;
    }
    inline uint32_t getPredicationEnable(void) const {
        return (TheStructure.Common.PredicationEnable);
    }
    inline void setPoshEnable(const uint32_t value) {
        TheStructure.Common.PoshEnable = value;
    }
    inline uint32_t getPoshEnable(void) const {
        return (TheStructure.Common.PoshEnable);
    }
    inline void setPoshStart(const uint32_t value) {
        TheStructure.Common.PoshStart = value;
    }
    inline uint32_t getPoshStart(void) const {
        return (TheStructure.Common.PoshStart);
    }
    inline void setSecondLevelBatchBuffer(const SECOND_LEVEL_BATCH_BUFFER value) {
        TheStructure.Common.SecondLevelBatchBuffer = value;
    }
    inline SECOND_LEVEL_BATCH_BUFFER getSecondLevelBatchBuffer(void) const {
        return static_cast<SECOND_LEVEL_BATCH_BUFFER>(TheStructure.Common.SecondLevelBatchBuffer);
    }
    typedef enum tagBATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2 {
        BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT = 0x2,
        BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2_ALIGN_SIZE = 0x4,
    } BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2;
    inline void setBatchBufferStartAddressGraphicsaddress392(const uint64_t value) {
        TheStructure.Common.BatchBufferStartAddress_Graphicsaddress39_2 = value >> BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT;
    }
    inline uint64_t getBatchBufferStartAddressGraphicsaddress392(void) const {
        return (TheStructure.Common.BatchBufferStartAddress_Graphicsaddress39_2 << BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT);
    }
    inline void setBatchBufferStartAddressGraphicsaddress472(const uint64_t value) {
        setBatchBufferStartAddressGraphicsaddress392(value);
    }
    inline uint64_t getBatchBufferStartAddressGraphicsaddress472(void) const {
        return getBatchBufferStartAddressGraphicsaddress392();
    }
    typedef enum tagBATCHBUFFERSTARTADDRESS_RESERVED {
        BATCHBUFFERSTARTADDRESS_RESERVED_BIT_SHIFT = 0x2,
        BATCHBUFFERSTARTADDRESS_RESERVED_ALIGN_SIZE = 0x4,
    } BATCHBUFFERSTARTADDRESS_RESERVED;
    inline void setBatchBufferStartAddressReserved(const uint64_t value) {
        TheStructure.Common.BatchBufferStartAddress_Reserved = value >> BATCHBUFFERSTARTADDRESS_RESERVED_BIT_SHIFT;
    }
    inline uint64_t getBatchBufferStartAddressReserved(void) const {
        return (TheStructure.Common.BatchBufferStartAddress_Reserved << BATCHBUFFERSTARTADDRESS_RESERVED_BIT_SHIFT);
    }
} MI_BATCH_BUFFER_START;
STATIC_ASSERT(12 == sizeof(MI_BATCH_BUFFER_START));

typedef struct tagMEDIA_SURFACE_STATE {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 29);
            uint32_t Rotation : BITFIELD_RANGE(30, 31);
            uint32_t Cr_VCb_UPixelOffsetVDirection : BITFIELD_RANGE(0, 1);
            uint32_t PictureStructure : BITFIELD_RANGE(2, 3);
            uint32_t Width : BITFIELD_RANGE(4, 17);
            uint32_t Height : BITFIELD_RANGE(18, 31);
            uint32_t TileMode : BITFIELD_RANGE(0, 1);
            uint32_t HalfPitchForChroma : BITFIELD_RANGE(2, 2);
            uint32_t SurfacePitch : BITFIELD_RANGE(3, 20);
            uint32_t AddressControl : BITFIELD_RANGE(21, 21);
            uint32_t MemoryCompressionEnable : BITFIELD_RANGE(22, 22);
            uint32_t MemoryCompressionMode : BITFIELD_RANGE(23, 23);
            uint32_t Cr_VCb_UPixelOffsetVDirectionMsb : BITFIELD_RANGE(24, 24);
            uint32_t Cr_VCb_UPixelOffsetUDirection : BITFIELD_RANGE(25, 25);
            uint32_t InterleaveChroma : BITFIELD_RANGE(26, 26);
            uint32_t SurfaceFormat : BITFIELD_RANGE(27, 31);
            uint32_t YOffsetForU_Cb : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_110 : BITFIELD_RANGE(14, 15);
            uint32_t XOffsetForU_Cb : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_126 : BITFIELD_RANGE(30, 31);
            uint32_t Reserved_128;
            uint32_t SurfaceMemoryObjectControlState_Reserved : BITFIELD_RANGE(0, 0);
            uint32_t SurfaceMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(1, 6);
            uint32_t Reserved_167 : BITFIELD_RANGE(7, 17);
            uint32_t TiledResourceMode : BITFIELD_RANGE(18, 19);
            uint32_t Depth : BITFIELD_RANGE(20, 23);
            uint32_t Reserved_184 : BITFIELD_RANGE(24, 29);
            uint32_t VerticalLineStrideOffset : BITFIELD_RANGE(30, 30);
            uint32_t VerticalLineStride : BITFIELD_RANGE(31, 31);
            uint32_t SurfaceBaseAddressLow;
            uint32_t SurfaceBaseAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_240 : BITFIELD_RANGE(16, 31);
        } Common;
        struct tagSurfaceFormatIsNotOneOfPlanarFormats {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
        } SurfaceFormatIsNotOneOfPlanarFormats;
        struct tagSurfaceFormatIsOneOfPlanarFormats {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 15);
            uint32_t YOffset : BITFIELD_RANGE(16, 19);
            uint32_t XOffset : BITFIELD_RANGE(20, 26);
            uint32_t Reserved_27 : BITFIELD_RANGE(27, 31);
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
        } SurfaceFormatIsOneOfPlanarFormats;
        struct tag_SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0 {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t YOffsetForV_Cr : BITFIELD_RANGE(0, 14);
            uint32_t Reserved_143 : BITFIELD_RANGE(15, 15);
            uint32_t XOffsetForV_Cr : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_158 : BITFIELD_RANGE(30, 31);
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
        } _SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0;
        uint32_t RawData[8];
    } TheStructure;
    typedef enum tagROTATION {
        ROTATION_NO_ROTATION_OR_0_DEGREE = 0x0,
        ROTATION_90_DEGREE_ROTATION = 0x1,
        ROTATION_180_DEGREE_ROTATION = 0x2,
        ROTATION_270_DEGREE_ROTATION = 0x3,
    } ROTATION;
    typedef enum tagPICTURE_STRUCTURE {
        PICTURE_STRUCTURE_FRAME_PICTURE = 0x0,
        PICTURE_STRUCTURE_TOP_FIELD_PICTURE = 0x1,
        PICTURE_STRUCTURE_BOTTOM_FIELD_PICTURE = 0x2,
        PICTURE_STRUCTURE_INVALID_NOT_ALLOWED = 0x3,
    } PICTURE_STRUCTURE;
    typedef enum tagTILE_MODE {
        TILE_MODE_TILEMODE_LINEAR = 0x0,
        TILE_MODE_TILEMODE_XMAJOR = 0x2,
        TILE_MODE_TILEMODE_YMAJOR = 0x3,
    } TILE_MODE;
    typedef enum tagADDRESS_CONTROL {
        ADDRESS_CONTROL_CLAMP = 0x0,
        ADDRESS_CONTROL_MIRROR = 0x1,
    } ADDRESS_CONTROL;
    typedef enum tagMEMORY_COMPRESSION_MODE {
        MEMORY_COMPRESSION_MODE_HORIZONTAL_COMPRESSION_MODE = 0x0,
        MEMORY_COMPRESSION_MODE_VERTICAL_COMPRESSION_MODE = 0x1,
    } MEMORY_COMPRESSION_MODE;
    typedef enum tagSURFACE_FORMAT {
        SURFACE_FORMAT_YCRCB_NORMAL = 0x0,
        SURFACE_FORMAT_YCRCB_SWAPUVY = 0x1,
        SURFACE_FORMAT_YCRCB_SWAPUV = 0x2,
        SURFACE_FORMAT_YCRCB_SWAPY = 0x3,
        SURFACE_FORMAT_PLANAR_420_8 = 0x4,
        SURFACE_FORMAT_Y8_UNORM_VA = 0x5,
        SURFACE_FORMAT_Y16_SNORM = 0x6,
        SURFACE_FORMAT_Y16_UNORM_VA = 0x7,
        SURFACE_FORMAT_R10G10B10A2_UNORM = 0x8,
        SURFACE_FORMAT_R8G8B8A8_UNORM = 0x9,
        SURFACE_FORMAT_R8B8_UNORM_CRCB = 0xa,
        SURFACE_FORMAT_R8_UNORM_CR_CB = 0xb,
        SURFACE_FORMAT_Y8_UNORM = 0xc,
        SURFACE_FORMAT_A8Y8U8V8_UNORM = 0xd,
        SURFACE_FORMAT_B8G8R8A8_UNORM = 0xe,
        SURFACE_FORMAT_R16G16B16A16 = 0xf,
        SURFACE_FORMAT_Y1_UNORM = 0x10,
        SURFACE_FORMAT_Y32_UNORM = 0x11,
        SURFACE_FORMAT_PLANAR_422_8 = 0x12,
        SURFACE_FORMAT_FM_STRBUF_Y1 = 0x13,
        SURFACE_FORMAT_FM_STRBUF_Y8 = 0x14,
        SURFACE_FORMAT_FM_STRBUF_Y16 = 0x15,
        SURFACE_FORMAT_FM_STRBUF_Y32 = 0x16,
        SURFACE_FORMAT_PLANAR_420_16 = 0x17,
        SURFACE_FORMAT_R16B16_UNORM_CRCB = 0x18,
        SURFACE_FORMAT_R16_UNORM_CR_CB = 0x19,
        SURFACE_FORMAT_Y16_UNORM = 0x1a,
    } SURFACE_FORMAT;
    typedef enum tagSURFACE_MEMORY_OBJECT_CONTROL_STATE {
        SURFACE_MEMORY_OBJECT_CONTROL_STATE_DEFAULTVAUEDESC = 0x0,
    } SURFACE_MEMORY_OBJECT_CONTROL_STATE;
    typedef enum tagTILED_RESOURCE_MODE {
        TILED_RESOURCE_MODE_TRMODE_NONE = 0x0,
        TILED_RESOURCE_MODE_TRMODE_TILEYF = 0x1,
        TILED_RESOURCE_MODE_TRMODE_TILEYS = 0x2,
    } TILED_RESOURCE_MODE;
    typedef enum tagPATCH_CONSTANTS {
        SURFACEBASEADDRESS_BYTEOFFSET = 0x18,
        SURFACEBASEADDRESS_INDEX = 0x6,
        SURFACEBASEADDRESSHIGH_BYTEOFFSET = 0x1c,
        SURFACEBASEADDRESSHIGH_INDEX = 0x7,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.Rotation = ROTATION_NO_ROTATION_OR_0_DEGREE;
        TheStructure.Common.PictureStructure = PICTURE_STRUCTURE_FRAME_PICTURE;
        TheStructure.Common.TileMode = TILE_MODE_TILEMODE_LINEAR;
        TheStructure.Common.AddressControl = ADDRESS_CONTROL_CLAMP;
        TheStructure.Common.MemoryCompressionMode = MEMORY_COMPRESSION_MODE_HORIZONTAL_COMPRESSION_MODE;
        TheStructure.Common.SurfaceFormat = SURFACE_FORMAT_YCRCB_NORMAL;
        TheStructure.Common.TiledResourceMode = TILED_RESOURCE_MODE_TRMODE_NONE;
    }
    static tagMEDIA_SURFACE_STATE sInit(void) {
        MEDIA_SURFACE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 8);
        return TheStructure.RawData[index];
    }
    inline void setRotation(const ROTATION value) {
        TheStructure.Common.Rotation = value;
    }
    inline ROTATION getRotation(void) const {
        return static_cast<ROTATION>(TheStructure.Common.Rotation);
    }
    inline void setCrVCbUPixelOffsetVDirection(const uint32_t value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetVDirection = value;
    }
    inline uint32_t getCrVCbUPixelOffsetVDirection(void) const {
        return (TheStructure.Common.Cr_VCb_UPixelOffsetVDirection);
    }
    inline void setPictureStructure(const PICTURE_STRUCTURE value) {
        TheStructure.Common.PictureStructure = value;
    }
    inline PICTURE_STRUCTURE getPictureStructure(void) const {
        return static_cast<PICTURE_STRUCTURE>(TheStructure.Common.PictureStructure);
    }
    inline void setWidth(const uint32_t value) {
        TheStructure.Common.Width = value - 1;
    }
    inline uint32_t getWidth(void) const {
        return (TheStructure.Common.Width + 1);
    }
    inline void setHeight(const uint32_t value) {
        TheStructure.Common.Height = value - 1;
    }
    inline uint32_t getHeight(void) const {
        return (TheStructure.Common.Height + 1);
    }
    inline void setTileMode(const TILE_MODE value) {
        TheStructure.Common.TileMode = value;
    }
    inline TILE_MODE getTileMode(void) const {
        return static_cast<TILE_MODE>(TheStructure.Common.TileMode);
    }
    inline void setHalfPitchForChroma(const bool value) {
        TheStructure.Common.HalfPitchForChroma = value;
    }
    inline bool getHalfPitchForChroma(void) const {
        return (TheStructure.Common.HalfPitchForChroma);
    }
    inline void setSurfacePitch(const uint32_t value) {
        TheStructure.Common.SurfacePitch = value - 1;
    }
    inline uint32_t getSurfacePitch(void) const {
        return (TheStructure.Common.SurfacePitch + 1);
    }
    inline void setAddressControl(const ADDRESS_CONTROL value) {
        TheStructure.Common.AddressControl = value;
    }
    inline ADDRESS_CONTROL getAddressControl(void) const {
        return static_cast<ADDRESS_CONTROL>(TheStructure.Common.AddressControl);
    }
    inline void setMemoryCompressionEnable(const bool value) {
        TheStructure.Common.MemoryCompressionEnable = value;
    }
    inline bool getMemoryCompressionEnable(void) const {
        return (TheStructure.Common.MemoryCompressionEnable);
    }
    inline void setMemoryCompressionMode(const MEMORY_COMPRESSION_MODE value) {
        TheStructure.Common.MemoryCompressionMode = value;
    }
    inline MEMORY_COMPRESSION_MODE getMemoryCompressionMode(void) const {
        return static_cast<MEMORY_COMPRESSION_MODE>(TheStructure.Common.MemoryCompressionMode);
    }
    inline void setCrVCbUPixelOffsetVDirectionMsb(const uint32_t value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb = value;
    }
    inline uint32_t getCrVCbUPixelOffsetVDirectionMsb(void) const {
        return (TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb);
    }
    inline void setCrVCbUPixelOffsetUDirection(const uint32_t value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetUDirection = value;
    }
    inline uint32_t getCrVCbUPixelOffsetUDirection(void) const {
        return (TheStructure.Common.Cr_VCb_UPixelOffsetUDirection);
    }
    inline void setInterleaveChroma(const bool value) {
        TheStructure.Common.InterleaveChroma = value;
    }
    inline bool getInterleaveChroma(void) const {
        return (TheStructure.Common.InterleaveChroma);
    }
    inline void setSurfaceFormat(const SURFACE_FORMAT value) {
        TheStructure.Common.SurfaceFormat = value;
    }
    inline SURFACE_FORMAT getSurfaceFormat(void) const {
        return static_cast<SURFACE_FORMAT>(TheStructure.Common.SurfaceFormat);
    }
    inline void setYOffsetForUCb(const uint32_t value) {
        TheStructure.Common.YOffsetForU_Cb = value;
    }
    inline uint32_t getYOffsetForUCb(void) const {
        return (TheStructure.Common.YOffsetForU_Cb);
    }
    inline void setXOffsetForUCb(const uint32_t value) {
        TheStructure.Common.XOffsetForU_Cb = value;
    }
    inline uint32_t getXOffsetForUCb(void) const {
        return (TheStructure.Common.XOffsetForU_Cb);
    }
    inline void setSurfaceMemoryObjectControlStateReserved(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_Reserved = value;
    }
    inline uint32_t getSurfaceMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_Reserved);
    }
    inline void setSurfaceMemoryObjectControlStateIndexToMocsTables(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint32_t getSurfaceMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_IndexToMocsTables << 1);
    }
    inline void setTiledResourceMode(const TILED_RESOURCE_MODE value) {
        TheStructure.Common.TiledResourceMode = value;
    }
    inline TILED_RESOURCE_MODE getTiledResourceMode(void) const {
        return static_cast<TILED_RESOURCE_MODE>(TheStructure.Common.TiledResourceMode);
    }
    inline void setDepth(const uint32_t value) {
        TheStructure.Common.Depth = value;
    }
    inline uint32_t getDepth(void) const {
        return (TheStructure.Common.Depth);
    }
    inline void setVerticalLineStrideOffset(const uint32_t value) {
        TheStructure.Common.VerticalLineStrideOffset = value;
    }
    inline uint32_t getVerticalLineStrideOffset(void) const {
        return (TheStructure.Common.VerticalLineStrideOffset);
    }
    inline void setVerticalLineStride(const uint32_t value) {
        TheStructure.Common.VerticalLineStride = value;
    }
    inline uint32_t getVerticalLineStride(void) const {
        return (TheStructure.Common.VerticalLineStride);
    }
    inline void setSurfaceBaseAddress(const uint64_t value) {
        TheStructure.Common.SurfaceBaseAddressLow = static_cast<uint32_t>(value & 0xffffffff);
        TheStructure.Common.SurfaceBaseAddressHigh = (value >> 32) & 0xffffffff;
    }
    inline uint64_t getSurfaceBaseAddress(void) const {
        return (TheStructure.Common.SurfaceBaseAddressLow |
                static_cast<uint64_t>(TheStructure.Common.SurfaceBaseAddressHigh) << 32);
    }
    inline void setSurfaceBaseAddressHigh(const uint32_t value) {
        TheStructure.Common.SurfaceBaseAddressHigh = value;
    }
    inline uint32_t getSurfaceBaseAddressHigh(void) const {
        return (TheStructure.Common.SurfaceBaseAddressHigh);
    }
    typedef enum tagYOFFSET {
        YOFFSET_BIT_SHIFT = 0x2,
        YOFFSET_ALIGN_SIZE = 0x4,
    } YOFFSET;
    inline void setYOffset(const uint32_t value) {
        TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset = value >> YOFFSET_BIT_SHIFT;
    }
    inline uint32_t getYOffset(void) const {
        return (TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset << YOFFSET_BIT_SHIFT);
    }
    typedef enum tagXOFFSET {
        XOFFSET_BIT_SHIFT = 0x2,
        XOFFSET_ALIGN_SIZE = 0x4,
    } XOFFSET;
    inline void setXOffset(const uint32_t value) {
        TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset = value >> XOFFSET_BIT_SHIFT;
    }
    inline uint32_t getXOffset(void) const {
        return (TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset << XOFFSET_BIT_SHIFT);
    }
    inline void setYOffsetForVCr(const uint32_t value) {
        TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr = value;
    }
    inline uint32_t getYOffsetForVCr(void) const {
        return (TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr);
    }
    inline void setXOffsetForVCr(const uint32_t value) {
        TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr = value;
    }
    inline uint32_t getXOffsetForVCr(void) const {
        return (TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr);
    }
} MEDIA_SURFACE_STATE;
STATIC_ASSERT(32 == sizeof(MEDIA_SURFACE_STATE));

typedef struct tagMI_MATH {
    union _DW0 {
        struct _BitField {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved : BITFIELD_RANGE(8, 22);
            uint32_t InstructionOpcode : BITFIELD_RANGE(23, 28);
            uint32_t InstructionType : BITFIELD_RANGE(29, 31);
        } BitField;
        uint32_t Value;
    } DW0;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_MATH = 0x1A,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
} MI_MATH;
typedef struct tagMI_MATH_ALU_INST_INLINE {
    union _DW0 {
        struct _BitField {
            uint32_t Operand2 : BITFIELD_RANGE(0, 9);
            uint32_t Operand1 : BITFIELD_RANGE(10, 19);
            uint32_t ALUOpcode : BITFIELD_RANGE(20, 31);
        } BitField;
        uint32_t Value;
    } DW0;
} MI_MATH_ALU_INST_INLINE;
typedef struct tagMI_SEMAPHORE_WAIT {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 11);
            uint32_t CompareOperation : BITFIELD_RANGE(12, 14);
            uint32_t WaitMode : BITFIELD_RANGE(15, 15);
            uint32_t RegisterPollMode : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_17 : BITFIELD_RANGE(17, 21);
            uint32_t MemoryType : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t SemaphoreDataDword;
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint64_t SemaphoreAddress_Graphicsaddress : BITFIELD_RANGE(2, 63);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;
    typedef enum tagCOMPARE_OPERATION {
        COMPARE_OPERATION_SAD_GREATER_THAN_SDD = 0x0,
        COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD = 0x1,
        COMPARE_OPERATION_SAD_LESS_THAN_SDD = 0x2,
        COMPARE_OPERATION_SAD_LESS_THAN_OR_EQUAL_SDD = 0x3,
        COMPARE_OPERATION_SAD_EQUAL_SDD = 0x4,
        COMPARE_OPERATION_SAD_NOT_EQUAL_SDD = 0x5,
    } COMPARE_OPERATION;
    typedef enum tagWAIT_MODE {
        WAIT_MODE_SIGNAL_MODE = 0x0,
        WAIT_MODE_POLLING_MODE = 0x1,
    } WAIT_MODE;
    typedef enum tagREGISTER_POLL_MODE {
        REGISTER_POLL_MODE_MEMORY_POLL = 0x0,
        REGISTER_POLL_MODE_REGISTER_POLL = 0x1,
    } REGISTER_POLL_MODE;
    typedef enum tagMEMORY_TYPE {
        MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS = 0x0,
        MEMORY_TYPE_GLOBAL_GRAPHICS_ADDRESS = 0x1,
    } MEMORY_TYPE;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT = 0x1c,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.CompareOperation = COMPARE_OPERATION_SAD_GREATER_THAN_SDD;
        TheStructure.Common.WaitMode = WAIT_MODE_SIGNAL_MODE;
        TheStructure.Common.RegisterPollMode = REGISTER_POLL_MODE_MEMORY_POLL;
        TheStructure.Common.MemoryType = MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_SEMAPHORE_WAIT sInit(void) {
        MI_SEMAPHORE_WAIT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setCompareOperation(const COMPARE_OPERATION value) {
        TheStructure.Common.CompareOperation = value;
    }
    inline COMPARE_OPERATION getCompareOperation(void) const {
        return static_cast<COMPARE_OPERATION>(TheStructure.Common.CompareOperation);
    }
    inline void setWaitMode(const WAIT_MODE value) {
        TheStructure.Common.WaitMode = value;
    }
    inline WAIT_MODE getWaitMode(void) const {
        return static_cast<WAIT_MODE>(TheStructure.Common.WaitMode);
    }
    inline void setRegisterPollMode(const REGISTER_POLL_MODE value) {
        TheStructure.Common.RegisterPollMode = value;
    }
    inline REGISTER_POLL_MODE getRegisterPollMode(void) const {
        return static_cast<REGISTER_POLL_MODE>(TheStructure.Common.RegisterPollMode);
    }
    inline void setMemoryType(const MEMORY_TYPE value) {
        TheStructure.Common.MemoryType = value;
    }
    inline MEMORY_TYPE getMemoryType(void) const {
        return static_cast<MEMORY_TYPE>(TheStructure.Common.MemoryType);
    }
    inline void setSemaphoreDataDword(const uint32_t value) {
        TheStructure.Common.SemaphoreDataDword = value;
    }
    inline uint32_t getSemaphoreDataDword(void) const {
        return (TheStructure.Common.SemaphoreDataDword);
    }
    typedef enum tagSEMAPHOREADDRESS_GRAPHICSADDRESS {
        SEMAPHOREADDRESS_GRAPHICSADDRESS_BIT_SHIFT = 0x2,
        SEMAPHOREADDRESS_GRAPHICSADDRESS_ALIGN_SIZE = 0x4,
    } SEMAPHOREADDRESS_GRAPHICSADDRESS39_2;
    inline void setSemaphoreGraphicsAddress(const uint64_t value) {
        TheStructure.Common.SemaphoreAddress_Graphicsaddress = value >> SEMAPHOREADDRESS_GRAPHICSADDRESS_BIT_SHIFT;
    }
    inline uint64_t getSemaphoreGraphicsAddress(void) const {
        return (TheStructure.Common.SemaphoreAddress_Graphicsaddress << SEMAPHOREADDRESS_GRAPHICSADDRESS_BIT_SHIFT);
    }
    typedef enum tagSEMAPHOREADDRESS_RESERVED {
        SEMAPHOREADDRESS_RESERVED_BIT_SHIFT = 0x2,
        SEMAPHOREADDRESS_RESERVED_ALIGN_SIZE = 0x4,
    } SEMAPHOREADDRESS_RESERVED;
} MI_SEMAPHORE_WAIT;
STATIC_ASSERT(16 == sizeof(MI_SEMAPHORE_WAIT));

typedef struct tagMI_STORE_DATA_IMM {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 9);
            uint32_t Reserved_10 : BITFIELD_RANGE(10, 20);
            uint32_t StoreQword : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t CoreModeEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 1);
            uint64_t Address_Graphicsaddress39_2 : BITFIELD_RANGE(2, 39);
            uint64_t Address_Reserved : BITFIELD_RANGE(40, 63);
            uint32_t DataDword0;
            uint32_t DataDword1;
        } Common;
        uint32_t RawData[5];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_STORE_DWORD = 0x2,
        DWORD_LENGTH_STORE_QWORD = 0x3,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_STORE_DATA_IMM = 0x20,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_STORE_DWORD;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_STORE_DATA_IMM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_STORE_DATA_IMM sInit(void) {
        MI_STORE_DATA_IMM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 5);
        return TheStructure.RawData[index];
    }
    inline void setDwordLength(const DWORD_LENGTH value) {
        TheStructure.Common.DwordLength = value;
    }
    inline DWORD_LENGTH getDwordLength(void) const {
        return static_cast<DWORD_LENGTH>(TheStructure.Common.DwordLength);
    }
    inline void setStoreQword(const bool value) {
        TheStructure.Common.StoreQword = value;
    }
    inline bool getStoreQword(void) const {
        return (TheStructure.Common.StoreQword);
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt(void) const {
        return (TheStructure.Common.UseGlobalGtt);
    }
    inline void setCoreModeEnable(const uint64_t value) {
        TheStructure.Common.CoreModeEnable = value;
    }
    inline uint64_t getCoreModeEnable(void) const {
        return (TheStructure.Common.CoreModeEnable);
    }
    typedef enum tagADDRESS_GRAPHICSADDRESS39_2 {
        ADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT = 0x2,
        ADDRESS_GRAPHICSADDRESS39_2_ALIGN_SIZE = 0x4,
    } ADDRESS_GRAPHICSADDRESS39_2;
    inline void setAddress(const uint64_t value) {
        TheStructure.Common.Address_Graphicsaddress39_2 = value >> ADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT;
    }
    inline uint64_t getAddress(void) const {
        return (TheStructure.Common.Address_Graphicsaddress39_2 << ADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT);
    }
    typedef enum tagADDRESS_RESERVED {
        ADDRESS_RESERVED_BIT_SHIFT = 0x2,
        ADDRESS_RESERVED_ALIGN_SIZE = 0x4,
    } ADDRESS_RESERVED;
    inline void setAddressReserved(const uint64_t value) {
        TheStructure.Common.Address_Reserved = value >> ADDRESS_RESERVED_BIT_SHIFT;
    }
    inline uint64_t getAddressReserved(void) const {
        return (TheStructure.Common.Address_Reserved << ADDRESS_RESERVED_BIT_SHIFT);
    }
    inline void setDataDword0(const uint32_t value) {
        TheStructure.Common.DataDword0 = value;
    }
    inline uint32_t getDataDword0(void) const {
        return (TheStructure.Common.DataDword0);
    }
    inline void setDataDword1(const uint32_t value) {
        TheStructure.Common.DataDword1 = value;
    }
    inline uint32_t getDataDword1(void) const {
        return (TheStructure.Common.DataDword1);
    }
} MI_STORE_DATA_IMM;
STATIC_ASSERT(20 == sizeof(MI_STORE_DATA_IMM));

typedef struct tagPWR_CLK_STATE_REGISTER {
    typedef enum tagADDRESS {
        REG_ADDRESS = 0x20C8
    } ADDRESS;
    union tagTheStructure {
        struct tagCommon {
            uint32_t EUmin : BITFIELD_RANGE(0, 3);
            uint32_t EUmax : BITFIELD_RANGE(4, 7);
            uint32_t SScount : BITFIELD_RANGE(8, 10);
            uint32_t SSCountEn : BITFIELD_RANGE(11, 11);
            uint32_t SliceCountRequest : BITFIELD_RANGE(12, 17);
            uint32_t EnableSliceCountRequest : BITFIELD_RANGE(18, 18);
            uint32_t Reserved : BITFIELD_RANGE(19, 30);
            uint32_t EnablePwrClockGating : BITFIELD_RANGE(31, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    inline void init(void) {
        TheStructure.RawData[0] = 0;
        TheStructure.Common.EnablePwrClockGating = 1;
        TheStructure.Common.EUmin = 8;
        TheStructure.Common.EUmax = 8;
    }
    static tagPWR_CLK_STATE_REGISTER sInit(void) {
        PWR_CLK_STATE_REGISTER state;
        state.init();
        return state;
    }
} PWR_CLK_STATE_REGISTER;
STATIC_ASSERT(4 == sizeof(PWR_CLK_STATE_REGISTER));

typedef struct tagASYNC_SLICE_COUNT_SELECT_REGISTER {
    typedef enum tagADDRESS {
        REG_ADDRESS = 0xA204
    } ADDRESS;
    union tagTheStructure {
        struct tagCommon {
            uint32_t AsyncSliceCount : BITFIELD_RANGE(0, 3);
            uint32_t AsyncEuCount : BITFIELD_RANGE(4, 7);
            uint32_t AsyncSubSliceCount : BITFIELD_RANGE(8, 10);
            uint32_t Reserved : BITFIELD_RANGE(11, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    inline void init(void) {
        TheStructure.RawData[0] = 0;
        TheStructure.Common.AsyncSliceCount = 2;
        TheStructure.Common.AsyncSubSliceCount = 4;
        TheStructure.Common.AsyncEuCount = 8;
    }
    static tagASYNC_SLICE_COUNT_SELECT_REGISTER sInit(void) {
        ASYNC_SLICE_COUNT_SELECT_REGISTER state;
        state.init();
        return state;
    }
} ASYNC_SLICE_COUNT_SELECT_REGISTER;
STATIC_ASSERT(4 == sizeof(ASYNC_SLICE_COUNT_SELECT_REGISTER));

#pragma pack()
