/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/helpers/hw_helper.h"

namespace OCLRT {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

struct GENX {
    static bool (*isSimulationFcn)(unsigned short);
    typedef struct tagINTERFACE_DESCRIPTOR_DATA {
        typedef enum tagDENORM_MODE {
            DENORM_MODE_FTZ = 0x0,
            DENORM_MODE_SETBYKERNEL = 0x1,
        } DENORM_MODE;
        typedef enum tagSAMPLERSTATEPOINTER {
            SAMPLERSTATEPOINTER_BIT_SHIFT = 0x5,
            SAMPLERSTATEPOINTER_ALIGN_SIZE = 0x20,
        } SAMPLERSTATEPOINTER;
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
        typedef enum tagBINDINGTABLEPOINTER {
            BINDINGTABLEPOINTER_BIT_SHIFT = 0x5,
            BINDINGTABLEPOINTER_ALIGN_SIZE = 0x20,
        } BINDINGTABLEPOINTER;
        static tagINTERFACE_DESCRIPTOR_DATA sInit(void) {
            INTERFACE_DESCRIPTOR_DATA state;
            return state;
        }
        inline void setKernelStartPointerHigh(const uint32_t value) {
        }
        inline void setKernelStartPointer(const uint64_t value) {
        }
        inline void setNumberOfThreadsInGpgpuThreadGroup(const uint32_t value) {
        }
        inline void setCrossThreadConstantDataReadLength(const uint32_t value) {
        }
        inline void setDenormMode(const DENORM_MODE value) {
        }
        inline void setConstantIndirectUrbEntryReadLength(const uint32_t value) {
        }
        inline void setBindingTablePointer(const uint64_t value) {
        }
        inline void setSamplerStatePointer(const uint64_t value) {
        }
        inline void setSamplerCount(const SAMPLER_COUNT value) {
        }
        inline void setSharedLocalMemorySize(const SHARED_LOCAL_MEMORY_SIZE value) {
        }
        inline void setBarrierEnable(const bool value) {
        }
        inline void setBindingTableEntryCount(const uint32_t value) {
        }
    } INTERFACE_DESCRIPTOR_DATA;

    typedef struct tagBINDING_TABLE_STATE {
        inline void init(void) {
        }
        inline uint32_t getSurfaceStatePointer(void) const {
            return 0u;
        }
        inline void setSurfaceStatePointer(const uint64_t value) {
        }
        inline uint32_t getRawData(const uint32_t index) {
            return 0;
        }
        typedef enum tagSURFACESTATEPOINTER {
            SURFACESTATEPOINTER_BIT_SHIFT = 0x6,
            SURFACESTATEPOINTER_ALIGN_SIZE = 0x40,
        } SURFACESTATEPOINTER;
    } BINDING_TABLE_STATE;

    typedef struct tagGPGPU_WALKER {
        typedef enum tagSIMD_SIZE {
            SIMD_SIZE_SIMD8 = 0x0,
            SIMD_SIZE_SIMD16 = 0x1,
            SIMD_SIZE_SIMD32 = 0x2,
        } SIMD_SIZE;
        typedef enum tagINDIRECTDATASTARTADDRESS {
            INDIRECTDATASTARTADDRESS_BIT_SHIFT = 0x6,
            INDIRECTDATASTARTADDRESS_ALIGN_SIZE = 0x40,
        } INDIRECTDATASTARTADDRESS;
        static tagGPGPU_WALKER sInit(void) {
            GPGPU_WALKER state;
            return state;
        }
        inline void setThreadWidthCounterMaximum(const uint32_t value) {
        }
        inline void setThreadGroupIdXDimension(const uint32_t value) {
        }
        inline void setThreadGroupIdYDimension(const uint32_t value) {
        }
        inline void setThreadGroupIdZDimension(const uint32_t value) {
        }
        inline void setRightExecutionMask(const uint32_t value) {
        }
        inline void setBottomExecutionMask(const uint32_t value) {
        }
        inline void setSimdSize(const SIMD_SIZE value) {
        }
        inline void setThreadGroupIdStartingX(const uint32_t value) {
        }
        inline void setThreadGroupIdStartingY(const uint32_t value) {
        }
        inline void setThreadGroupIdStartingResumeZ(const uint32_t value) {
        }
        inline void setIndirectDataStartAddress(const uint32_t value) {
        }
        inline void setInterfaceDescriptorOffset(const uint32_t value) {
        }
        inline void setIndirectDataLength(const uint32_t value) {
        }
    } GPGPU_WALKER;

    typedef struct tagPIPE_CONTROL {
        typedef enum tagPOST_SYNC_OPERATION {
            POST_SYNC_OPERATION_NO_WRITE = 0x0,
            POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA = 0x1,
            POST_SYNC_OPERATION_WRITE_PS_DEPTH_COUNT = 0x2,
            POST_SYNC_OPERATION_WRITE_TIMESTAMP = 0x3,
        } POST_SYNC_OPERATION;
        static tagPIPE_CONTROL sInit(void) {
            PIPE_CONTROL state;
            return state;
        }
        inline void setCommandStreamerStallEnable(const uint32_t value) {
        }
        inline void setDcFlushEnable(const bool value) {
        }
        inline void setStateCacheInvalidationEnable(const bool value) {
        }
        inline void setPipeControlFlushEnable(const bool value) {
        }
        inline void setTextureCacheInvalidationEnable(const bool value) {
        }
        inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        }
        inline void setAddress(const uint32_t value) {
        }
        inline void setAddressHigh(const uint32_t value) {
        }
        inline void setImmediateData(const uint64_t value) {
        }
        inline void setGenericMediaStateClear(const bool value) {
        }
    } PIPE_CONTROL;

    typedef struct tagMI_LOAD_REGISTER_IMM {
        static tagMI_LOAD_REGISTER_IMM sInit(void) {
            MI_LOAD_REGISTER_IMM state;
            return state;
        }
        inline void setRegisterOffset(const uint32_t value) {
        }
        inline void setDataDword(const uint32_t value) {
        }
    } MI_LOAD_REGISTER_IMM;

    typedef struct tagMI_LOAD_REGISTER_REG {
        static tagMI_LOAD_REGISTER_REG sInit(void) {
            MI_LOAD_REGISTER_REG state;
            return state;
        }
        inline void setSourceRegisterAddress(const uint32_t value) {
        }
        inline void setDestinationRegisterAddress(const uint32_t value) {
        }
    } MI_LOAD_REGISTER_REG;

    typedef struct tagMI_MATH {
        union _DW0 {
            struct _BitField {
                uint32_t DwordLength : BITFIELD_RANGE(0, 5);
                uint32_t Reserved : BITFIELD_RANGE(6, 22);
                uint32_t InstructionOpcode : BITFIELD_RANGE(23, 28);
                uint32_t InstructionType : BITFIELD_RANGE(29, 31);
            } BitField;
            uint32_t Value;
        } DW0;
        typedef enum tagMI_COMMAND_OPCODE {
            MI_COMMAND_OPCODE_MI_MATH = 0x0,
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

    typedef struct tagMI_COMMAND_OPCODE_MI_MATH {
    } MI_COMMAND_OPCODE_MI_MATH;

    typedef struct tagMI_STORE_REGISTER_MEM {
        static tagMI_STORE_REGISTER_MEM sInit(void) {
            MI_STORE_REGISTER_MEM state;
            return state;
        }
        inline void setRegisterAddress(const uint32_t value) {
        }
        inline void setMemoryAddress(const uint64_t value) {
        }
    } MI_STORE_REGISTER_MEM;

    typedef struct tagMI_REPORT_PERF_COUNT {
        static tagMI_REPORT_PERF_COUNT sInit(void) {
            MI_REPORT_PERF_COUNT state;
            return state;
        }
        inline void setReportId(const uint32_t value) {
        }
        inline void setMemoryAddress(const uint64_t value) {
        }
    } MI_REPORT_PERF_COUNT;

    typedef struct tagMI_BATCH_BUFFER_START {
        typedef enum tagSECOND_LEVEL_BATCH_BUFFER {
            SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH = 0x0,
            SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH = 0x1,
        } SECOND_LEVEL_BATCH_BUFFER;
        static tagMI_BATCH_BUFFER_START sInit(void) {
            MI_BATCH_BUFFER_START state;
            return state;
        }
        inline void setSecondLevelBatchBuffer(const SECOND_LEVEL_BATCH_BUFFER value) {
        }
        inline void setBatchBufferStartAddressGraphicsaddress472(const uint64_t value) {
        }
    } MI_BATCH_BUFFER_START;

    typedef struct tagMEDIA_STATE_FLUSH {
        static tagMEDIA_STATE_FLUSH sInit(void) {
            MEDIA_STATE_FLUSH state;
            return state;
        }
        inline void setInterfaceDescriptorOffset(const uint32_t value) {
        }
    } MEDIA_STATE_FLUSH;

    typedef struct tagMEDIA_INTERFACE_DESCRIPTOR_LOAD {
        static tagMEDIA_INTERFACE_DESCRIPTOR_LOAD sInit(void) {
            MEDIA_INTERFACE_DESCRIPTOR_LOAD state;
            return state;
        }
        inline void setInterfaceDescriptorDataStartAddress(const uint32_t value) {
        }
        inline void setInterfaceDescriptorTotalLength(const uint32_t value) {
        }
    } MEDIA_INTERFACE_DESCRIPTOR_LOAD;

    typedef struct tagMI_BATCH_BUFFER_END {
        static tagMI_BATCH_BUFFER_END sInit(void) {
            MI_BATCH_BUFFER_END state;
            return state;
        }
    } MI_BATCH_BUFFER_END;

    typedef struct tagRENDER_SURFACE_STATE {
    } RENDER_SURFACE_STATE;

    typedef struct tagMEDIA_VFE_STATE {
        static tagMEDIA_VFE_STATE sInit(void) {
            MEDIA_VFE_STATE state;
            return state;
        }
        inline void setMaximumNumberOfThreads(const uint32_t value) {
        }
        inline void setNumberOfUrbEntries(const uint32_t value) {
        }
        inline void setUrbEntryAllocationSize(const uint32_t value) {
        }
        inline void setPerThreadScratchSpace(const uint32_t value) {
        }
        inline void setStackSize(const uint32_t value) {
        }
        inline void setScratchSpaceBasePointer(const uint32_t value) {
        }
        inline void setScratchSpaceBasePointerHigh(const uint32_t value) {
        }
    } MEDIA_VFE_STATE;

    typedef struct tagSAMPLER_STATE {
        inline void setIndirectStatePointer(const uint32_t indirectStatePointerValue) {
        }
    } SAMPLER_STATE;

    typedef struct tagGPGPU_CSR_BASE_ADDRESS {
        inline void init(void) {
        }
        inline void setGpgpuCsrBaseAddress(uint64_t value) {
        }
    } GPGPU_CSR_BASE_ADDRESS;

    typedef struct tagSTATE_SIP {
        inline void init(void) {
        }
        inline void setSystemInstructionPointer(uint64_t value) {
        }
    } STATE_SIP;

    typedef GPGPU_WALKER WALKER_TYPE;
    static GPGPU_WALKER cmdInitGpgpuWalker;
    static INTERFACE_DESCRIPTOR_DATA cmdInitInterfaceDescriptorData;
    static MEDIA_STATE_FLUSH cmdInitMediaStateFlush;
    static MEDIA_INTERFACE_DESCRIPTOR_LOAD cmdInitMediaInterfaceDescriptorLoad;
};

} // namespace OCLRT
