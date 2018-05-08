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

#include "aub_header.h"
#include <cstdint>

#ifndef WIN32
#pragma pack(4)
#else
#pragma pack(push, 4)
#endif

struct CmdServicesMemTraceVersion {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint32_t memtraceFileVersion;
    struct {
        uint32_t metal : 3;
        uint32_t stepping : 5;
        uint32_t device : 8;
        uint32_t csxSwizzling : 2;
        uint32_t recordingMethod : 2;
        uint32_t pch : 8;
        uint32_t captureTool : 4;
    };
    uint32_t primaryVersion;
    uint32_t secondaryVersion;
    char commandLine[4];
    int32_t getCommandLineLength() const {
        return getPacketSize() - (5);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 4;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0xe)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0xe;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0xe; }
    struct CaptureToolValues {
        enum { GenKmdCapture = 1,
               Aubload = 0,
               Amber = 3,
               Ghal3DUlt = 2,
               AubDump = 4 };
    };
    struct DeviceValues {
        enum {
            Blc = 2,
            Il = 5,
            Skl = 12,
            Hsw = 9,
            Bxt = 14,
            Sbr = 6,
            Ivb = 7,
            Chv = 13,
            El = 4,
            Ctg = 3,
            Lrb2 = 8,
            Bwr = 0,
            Vlv = 10,
            Cln = 1,
            Kbl = 16,
            Bdw = 11
        };
    };
    struct RecordingMethodValues {
        enum { Phy = 1,
               Gfx = 0 };
    };
    struct CsxSwizzlingValues {
        enum { Disabled = 0,
               Enabled = 1 };
    };
    struct PchValues {
        enum { LynxPoint = 4,
               CougarPoint = 2,
               PantherPoint = 3,
               Default = 0,
               IbexPeak = 1 };
    };
    struct SteppingValues {
        enum {
            N = 13,
            O = 14,
            L = 11,
            M = 12,
            B = 1,
            C = 2,
            A = 0,
            F = 5,
            G = 6,
            D = 3,
            E = 4,
            Z = 25,
            X = 23,
            Y = 24,
            R = 17,
            S = 18,
            P = 15,
            Q = 16,
            V = 21,
            W = 22,
            T = 19,
            U = 20,
            J = 9,
            K = 10,
            H = 7,
            I = 8
        };
    };
};

struct CmdServicesMemTraceRegisterCompare {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint32_t registerOffset;
    struct {
        uint32_t noReadExpect : 1;
        uint32_t : 15;
        uint32_t registerSize : 4;
        uint32_t : 8;
        uint32_t registerSpace : 4;
    };
    uint32_t readMaskLow;
    uint32_t readMaskHigh;
    uint32_t data[1];
    int32_t getDataLength() const {
        return getPacketSize() - (5);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 4;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x1)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x1;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x1; }
    struct RegisterSpaceValues {
        enum { MchBar = 1,
               Mmio = 0,
               VtdBar = 5,
               PciConfig = 2,
               IO = 4,
               AzaliaBar = 3 };
    };
    struct RegisterSizeValues {
        enum { Qword = 3,
               Dword = 2,
               Word = 1,
               Byte = 0 };
    };
    struct NoReadExpectValues {
        enum { ReadExpect = 0,
               ReadWithoutExpect = 1 };
    };
};

struct CmdServicesMemTraceRegisterPoll {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint32_t registerOffset;
    struct {
        uint32_t : 1;
        uint32_t timeoutAction : 1;
        uint32_t pollNotEqual : 1;
        uint32_t : 1;
        uint32_t operationType : 4;
        uint32_t : 8;
        uint32_t registerSize : 4;
        uint32_t : 8;
        uint32_t registerSpace : 4;
    };
    uint32_t pollMaskLow;
    uint32_t pollMaskHigh;
    uint32_t data[1];
    int32_t getDataLength() const {
        return getPacketSize() - (5);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 4;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x2)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x2;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x2; }
    struct OperationTypeValues {
        enum { Normal = 0,
               InterlacedCrc = 1 };
    };
    struct RegisterSpaceValues {
        enum { MchBar = 1,
               Mmio = 0,
               VtdBar = 5,
               PciConfig = 2,
               IO = 4,
               AzaliaBar = 3 };
    };
    struct TimeoutActionValues {
        enum { Abort = 0,
               Ignore = 1 };
    };
    struct RegisterSizeValues {
        enum { Qword = 3,
               Dword = 2,
               Word = 1,
               Byte = 0 };
    };
};

struct CmdServicesMemTraceRegisterWrite {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint32_t registerOffset;
    struct {
        uint32_t : 4;
        uint32_t messageSourceId : 4;
        uint32_t : 8;
        uint32_t registerSize : 4;
        uint32_t : 8;
        uint32_t registerSpace : 4;
    };
    uint32_t writeMaskLow;
    uint32_t writeMaskHigh;
    uint32_t data[1];
    int32_t getDataLength() const {
        return getPacketSize() - (5);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 4;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x3)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x3;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x3; }
    struct MessageSourceIdValues {
        enum { Workaround = 4,
               Gt = 2,
               Ia = 0,
               Me = 1,
               Pch = 3 };
    };
    struct RegisterSpaceValues {
        enum { MchBar = 1,
               Mmio = 0,
               VtdBar = 5,
               PciConfig = 2,
               IO = 4,
               AzaliaBar = 3 };
    };
    struct RegisterSizeValues {
        enum { Qword = 3,
               Dword = 2,
               Word = 1,
               Byte = 0 };
    };
};

struct CmdServicesMemTraceMemoryCompare {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint32_t address;
    uint32_t addressHigh;
    struct {
        uint32_t noReadExpect : 1;
        uint32_t repeatMemory : 1;
        uint32_t tiling : 2;
        uint32_t : 2;
        uint32_t crcCompare : 1;
        uint32_t : 13;
        uint32_t dataTypeHint : 8;
        uint32_t addressSpace : 4;
    };
    uint32_t dataSizeInBytes;
    uint32_t data[1];
    int32_t getDataLength() const {
        return getPacketSize() - (5);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 4;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x4)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x4;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x4; }
    struct RepeatMemoryValues {
        enum { NoRepeat = 0,
               Repeat = 1 };
    };
    struct DataTypeHintValues {
        enum {
            TraceInterfaceDescriptor = 29,
            TraceCommandBufferPrimary = 39,
            TraceRemap = 37,
            TraceVertexShaderState = 16,
            TraceSfViewport = 23,
            TraceMediaObjectIndirectData = 36,
            Trace1DMap = 10,
            TraceVolumeMap = 9,
            TraceVldState = 30,
            TraceBatchBufferPrimary = 42,
            TraceSamplerDefaultColor = 28,
            TraceClipViewport = 22,
            TraceStripsFansState = 19,
            TraceNotype = 0,
            TraceAudioLinkTable = 46,
            TraceGeometryShaderState = 17,
            TraceConstantBuffer = 11,
            TraceBatchBufferBlt = 43,
            TraceBinBuffer = 2,
            TraceIndexBuffer = 13,
            Trace2DMap = 6,
            TraceCubeMap = 7,
            TraceVfeState = 31,
            TraceDepthStencilState = 33,
            TraceBatchBufferMfx = 44,
            TraceRenderSurfaceState = 35,
            TraceWindowerIzState = 20,
            TraceCommandBufferMfx = 41,
            TraceBatchBuffer = 1,
            TraceCcViewport = 24,
            TraceColorCalcState = 21,
            TraceCommandBuffer = 38,
            TraceAudioData = 47,
            TraceSlowStateBuffer = 4,
            TraceAudioCommandBuffer = 45,
            TraceCommandBufferBlt = 40,
            TraceKernelInstructions = 26,
            TraceConstantUrbEntry = 12,
            TraceBlendState = 32,
            TraceIndirectStateBuffer = 8,
            TraceClipperState = 18,
            TraceSamplerState = 25,
            TraceBindingTableState = 34,
            TraceBinPointerList = 3,
            TraceVertexBufferState = 5,
            TraceScratchSpace = 27
        };
    };
    struct TilingValues {
        enum { NoTiling = 0,
               WTiling = 3,
               YTiling = 2,
               XTiling = 1 };
    };
    struct CrcCompareValues {
        enum { Crc = 1,
               NoCrc = 0 };
    };
    struct NoReadExpectValues {
        enum { ReadExpect = 0,
               ReadWithoutExpect = 1 };
    };
    struct AddressSpaceValues {
        enum {
            TraceGttEntry = 4,
            TraceNonapetureGttGfx = 7,
            TraceLocal = 1,
            TracePml4Entry = 10,
            TraceGttGfx = 0,
            TraceNonlocal = 2,
            TraceGttPdEntry = 3,
            TracePpgttEntry = 6,
            TracePpgttGfx = 5,
            TracePpgttPdEntry = 9,
            TracePhysicalPdpEntry = 8
        };
    };
};

struct CmdServicesMemTraceMemoryPoll {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint32_t address;
    uint32_t addressHigh;
    struct {
        uint32_t pollNotEqual : 1;
        uint32_t : 1;
        uint32_t tiling : 2;
        uint32_t dataSize : 2;
        uint32_t : 2;
        uint32_t timeoutAction : 1;
        uint32_t : 11;
        uint32_t dataTypeHint : 8;
        uint32_t addressSpace : 4;
    };
    uint32_t pollMaskLow;
    uint32_t pollMaskHigh;
    uint32_t data[1];
    int32_t getDataLength() const {
        return getPacketSize() - (6);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 5;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x5)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x5;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x5; }
    struct DataTypeHintValues {
        enum {
            TraceInterfaceDescriptor = 29,
            TraceCommandBufferPrimary = 39,
            TraceRemap = 37,
            TraceVertexShaderState = 16,
            TraceSfViewport = 23,
            TraceMediaObjectIndirectData = 36,
            Trace1DMap = 10,
            TraceVolumeMap = 9,
            TraceVldState = 30,
            TraceBatchBufferPrimary = 42,
            TraceSamplerDefaultColor = 28,
            TraceClipViewport = 22,
            TraceStripsFansState = 19,
            TraceNotype = 0,
            TraceAudioLinkTable = 46,
            TraceGeometryShaderState = 17,
            TraceConstantBuffer = 11,
            TraceBatchBufferBlt = 43,
            TraceBinBuffer = 2,
            TraceIndexBuffer = 13,
            Trace2DMap = 6,
            TraceCubeMap = 7,
            TraceVfeState = 31,
            TraceDepthStencilState = 33,
            TraceBatchBufferMfx = 44,
            TraceRenderSurfaceState = 35,
            TraceWindowerIzState = 20,
            TraceCommandBufferMfx = 41,
            TraceBatchBuffer = 1,
            TraceCcViewport = 24,
            TraceColorCalcState = 21,
            TraceCommandBuffer = 38,
            TraceAudioData = 47,
            TraceSlowStateBuffer = 4,
            TraceAudioCommandBuffer = 45,
            TraceCommandBufferBlt = 40,
            TraceKernelInstructions = 26,
            TraceConstantUrbEntry = 12,
            TraceBlendState = 32,
            TraceIndirectStateBuffer = 8,
            TraceClipperState = 18,
            TraceSamplerState = 25,
            TraceBindingTableState = 34,
            TraceBinPointerList = 3,
            TraceVertexBufferState = 5,
            TraceScratchSpace = 27
        };
    };
    struct DataSizeValues {
        enum { Qword = 3,
               Dword = 2,
               Word = 1,
               Byte = 0 };
    };
    struct TilingValues {
        enum { NoTiling = 0,
               WTiling = 3,
               YTiling = 2,
               XTiling = 1 };
    };
    struct TimeoutActionValues {
        enum { Abort = 0,
               Ignore = 1 };
    };
    struct AddressSpaceValues {
        enum {
            TraceGttEntry = 4,
            TraceNonapetureGttGfx = 7,
            TraceLocal = 1,
            TracePml4Entry = 10,
            TraceGttGfx = 0,
            TraceNonlocal = 2,
            TraceGttPdEntry = 3,
            TracePpgttEntry = 6,
            TracePpgttGfx = 5,
            TracePpgttPdEntry = 9,
            TracePhysicalPdpEntry = 8
        };
    };
};

struct CmdServicesMemTraceMemoryWrite {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint64_t address;
    int32_t getAddressLength() const {
        return 2 - (1) + 1;
    }
    struct {
        uint32_t frontDoorAccess : 1;
        uint32_t repeatMemory : 1;
        uint32_t tiling : 2;
        uint32_t : 16;
        uint32_t dataTypeHint : 8;
        uint32_t addressSpace : 4;
    };
    uint32_t dataSizeInBytes;
    uint32_t data[1];
    int32_t getDataLength() const {
        return getPacketSize() - (5);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 4;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x6)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x6;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x6; }
    struct RepeatMemoryValues {
        enum { NoRepeat = 0,
               Repeat = 1 };
    };
    struct DataTypeHintValues {
        enum {
            TraceVertexBufferState = 5,
            TraceCommandBufferPrimary = 39,
            TraceVertexShaderState = 16,
            TraceExtendedRootTableEntry = 52,
            TraceClipViewport = 22,
            Trace1DMap = 10,
            TraceBatchBufferPrimary = 42,
            TraceClipperState = 18,
            TraceLogicalRingContextVecs = 51,
            TraceRingContextVcs = 57,
            TraceLri = 59,
            TraceBlendState = 32,
            TraceBinBuffer = 2,
            TraceSlowStateBuffer = 4,
            TraceRemap = 37,
            TraceDepthStencilState = 33,
            TraceAudioData = 47,
            TraceDummyGgttEntry = 62,
            TraceWindowerIzState = 20,
            Trace2DMap = 6,
            TraceBindingTableState = 34,
            TraceGucProcessDescriptor = 60,
            TraceIndirectStateBuffer = 8,
            TraceConstantBuffer = 11,
            TraceMediaObjectIndirectData = 36,
            TraceStripsFansState = 19,
            TraceBatchBuffer = 1,
            TraceLogicalRingContextVcs = 50,
            TraceSfViewport = 23,
            TraceCommandBufferBlt = 40,
            TraceRingContextBcs = 56,
            TraceCcViewport = 24,
            TraceIndexBuffer = 13,
            TraceScratchSpace = 27,
            TraceGucContextDescriptor = 61,
            TraceBatchBufferMfx = 44,
            TraceCommandBufferMfx = 41,
            TraceBatchBufferBlt = 43,
            TraceSamplerState = 25,
            TraceRingContextRcs = 55,
            TraceAudioLinkTable = 46,
            TraceRenderSurfaceState = 35,
            TraceSamplerDefaultColor = 28,
            TraceVldState = 30,
            TraceVfeState = 31,
            TraceExtendedContextTableEntry = 53,
            TraceLogicalRingContextRcs = 48,
            TraceInterfaceDescriptor = 29,
            TraceConstantUrbEntry = 12,
            TraceCommandBuffer = 38,
            TracePasidTableEntry = 54,
            TraceBinPointerList = 3,
            TraceRingContextVecs = 58,
            TraceNotype = 0,
            TraceGeometryShaderState = 17,
            TraceAudioCommandBuffer = 45,
            TraceColorCalcState = 21,
            TraceKernelInstructions = 26,
            TraceVolumeMap = 9,
            TraceCubeMap = 7,
            TraceLogicalRingContextBcs = 49
        };
    };
    struct TilingValues {
        enum { NoTiling = 0,
               WTiling = 3,
               YTiling = 2,
               XTiling = 1 };
    };
    struct AddressSpaceValues {
        enum {
            TraceGttEntry = 4,
            TraceNonapetureGttGfx = 7,
            TraceLocal = 1,
            TracePml4Entry = 10,
            TraceGttGfx = 0,
            TraceNonlocal = 2,
            TraceGttPdEntry = 3,
            TracePpgttEntry = 6,
            TracePpgttGfx = 5,
            TracePpgttPdEntry = 9,
            TracePowerContext = 11,
            TracePhysicalPdpEntry = 8
        };
    };
};

struct CmdServicesMemTraceMemoryWriteDiscontiguous {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    struct {
        uint32_t frontDoorAccess : 1;
        uint32_t repeatMemory : 1;
        uint32_t tiling : 2;
        uint32_t numberOfAddressDataPairs : 16;
        uint32_t dataTypeHint : 8;
        uint32_t addressSpace : 4;
    };
    struct {
        uint64_t address;
        uint32_t dataSizeInBytes;
    } Dword_2_To_190[63];
    int32_t getDword2To190Length() const {
        return 190 - (2) + 1;
    }
    uint32_t data[1];
    int32_t getDataLength() const {
        return getPacketSize() - (191);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 190;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0xb)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0xb;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0xb; }
    struct RepeatMemoryValues {
        enum { NoRepeat = 0,
               Repeat = 1 };
    };
    struct DataTypeHintValues {
        enum {
            TraceVertexBufferState = 5,
            TraceCommandBufferPrimary = 39,
            TraceRingContextBcs = 56,
            TraceExtendedRootTableEntry = 52,
            TraceClipViewport = 22,
            Trace1DMap = 10,
            TraceBatchBufferPrimary = 42,
            TraceClipperState = 18,
            TraceRingContextVcs = 57,
            TraceVolumeMap = 9,
            TraceBlendState = 32,
            TraceSlowStateBuffer = 4,
            TraceRemap = 37,
            TraceDepthStencilState = 33,
            TraceAudioData = 47,
            TraceColorCalcState = 21,
            TraceWindowerIzState = 20,
            Trace2DMap = 6,
            TraceBindingTableState = 34,
            TraceIndirectStateBuffer = 8,
            TraceConstantBuffer = 11,
            TraceMediaObjectIndirectData = 36,
            TraceStripsFansState = 19,
            TraceBatchBuffer = 1,
            TraceSfViewport = 23,
            TraceCommandBufferBlt = 40,
            TraceBinBuffer = 2,
            TraceCcViewport = 24,
            TraceIndexBuffer = 13,
            TraceScratchSpace = 27,
            TraceLogicalRingContextVecs = 51,
            TraceBatchBufferMfx = 44,
            TraceCommandBufferMfx = 41,
            TraceBatchBufferBlt = 43,
            TraceSamplerState = 25,
            TraceRingContextRcs = 55,
            TraceAudioLinkTable = 46,
            TraceRenderSurfaceState = 35,
            TraceSamplerDefaultColor = 28,
            TraceVldState = 30,
            TraceVfeState = 31,
            TraceExtendedContextTableEntry = 53,
            TraceLogicalRingContextRcs = 48,
            TraceInterfaceDescriptor = 29,
            TraceConstantUrbEntry = 12,
            TraceCommandBuffer = 38,
            TraceVertexShaderState = 16,
            TraceBinPointerList = 3,
            TraceRingContextVecs = 58,
            TraceNotype = 0,
            TraceGeometryShaderState = 17,
            TraceAudioCommandBuffer = 45,
            TraceLogicalRingContextVcs = 50,
            TraceKernelInstructions = 26,
            TracePasidTableEntry = 54,
            TraceCubeMap = 7,
            TraceLogicalRingContextBcs = 49
        };
    };
    struct TilingValues {
        enum { NoTiling = 0,
               WTiling = 3,
               YTiling = 2,
               XTiling = 1 };
    };
    struct AddressSpaceValues {
        enum {
            TraceGttEntry = 4,
            TraceNonapetureGttGfx = 7,
            TraceLocal = 1,
            TracePml4Entry = 10,
            TraceGttGfx = 0,
            TraceNonlocal = 2,
            TraceGttPdEntry = 3,
            TracePpgttEntry = 6,
            TracePpgttGfx = 5,
            TracePpgttPdEntry = 9,
            TracePowerContext = 11,
            TracePhysicalPdpEntry = 8
        };
    };
};

struct CmdServicesMemTraceFrameBegin {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    struct {
        uint32_t frameNumber : 16;
        uint32_t : 16;
    };
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 1;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x7)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x7;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x7; }
};

struct CmdServicesMemTraceComment {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    struct {
        uint32_t syncOnComment : 1;
        uint32_t syncOnSimulatorDisplay : 1;
        uint32_t : 30;
    };
    char comment[4];
    int32_t getCommentLength() const {
        return getPacketSize() - (2);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 1;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x8)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x8;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x8; }
};

struct CmdServicesMemTraceDelay {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint32_t time;
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 1;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x9)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x9;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x9; }
};

struct CmdServicesMemTraceMemoryDump {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint32_t physicalAddressDwordLow;
    uint32_t physicalAddressDwordHigh;
    uint32_t stride;
    uint32_t width;
    uint32_t height;
    struct {
        uint32_t addressSpace : 2;
        uint32_t : 2;
        uint32_t tiling : 2;
        uint32_t : 26;
    };
    char filename[4];
    int32_t getFilenameLength() const {
        return getPacketSize() - (7);
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 5;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0xa)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0xa;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0xa; }
    struct TilingValues {
        enum { NoTiling = 0,
               WTiling = 3,
               YTiling = 2,
               XTiling = 1 };
    };
    struct AddressSpaceValues {
        enum { TraceGttGfx = 0,
               TraceLocal = 1 };
    };
};

struct CmdServicesMemTraceTestPhaseMarker {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    struct {
        uint32_t toolSpecificSubPhase : 12;
        uint32_t beginTestPhase : 4;
        uint32_t : 16;
    };
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 1;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0xc)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0xc;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0xc; }
    struct BeginTestPhaseValues {
        enum { PollForTestCompletion = 8,
               SetupPhase = 2,
               DispatchPhase = 3,
               VerificationPhase = 10,
               MemoryInitializationPhase = 0,
               ExecutePhase = 4 };
    };
};

struct CmdServicesMemTraceMemoryContinuousRegion {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint64_t address;
    int32_t getAddressLength() const {
        return 2 - (1) + 1;
    }
    uint64_t regionSize;
    int32_t getRegionSizeLength() const {
        return 4 - (3) + 1;
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 4;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0xd)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0xd;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0xd; }
};

struct CmdServicesMemTracePredicate {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    struct {
        uint32_t predicateState : 1;
        uint32_t target : 4;
        uint32_t : 27;
    };
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 4;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0xf)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0xf;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0xf; }
    struct PredicateStateValues {
        enum { Disabled = 0,
               Enabled = 1 };
    };
    struct TargetValues {
        enum {
            Fpgarunlist = 8,
            Simulator = 0,
            Pipe = 1,
            Silicon = 4,
            Uncore = 6,
            Emulator = 3,
            Pipe2D = 7,
            Fpgamedia = 5,
            Pipegt = 2
        };
    };
};

struct CmdServicesMemTraceDumpCompress {
    union {
        AubCmdHdr Header;
        struct {
            uint32_t dwordCount : 16;
            uint32_t instructionSubOpcode : 7;
            uint32_t instructionOpcode : 6;
            uint32_t instructionType : 3;
        };
    };
    uint64_t surfaceAddress;
    uint64_t getSurfaceAddress() const {
        return getMisalignedUint64(&this->surfaceAddress);
    }
    void setSurfaceAddress(const uint64_t surfaceAddress) {
        setMisalignedUint64(&this->surfaceAddress, surfaceAddress);
    }
    int getSurfaceAddressLength() const {
        return 2 - (1) + 1;
    }
    uint32_t surfaceWidth;
    uint32_t surfaceHeight;
    uint32_t surfacePitch;
    struct {
        uint32_t surfaceFormat : 12;
        uint32_t dumpType : 3;
        uint32_t : 1;
        uint32_t surfaceTilingType : 3;
        uint32_t : 3;
        uint32_t surfaceType : 3;
        uint32_t : 3;
        uint32_t tiledResourceMode : 2;
        uint32_t : 1;
        uint32_t useClearValue : 1;
    };
    uint64_t auxSurfaceAddress;
    int getAuxSurfaceAddressLength() const {
        return 8 - (7) + 1;
    }
    uint32_t auxSurfaceWidth;
    uint32_t auxSurfaceHeight;
    uint32_t auxSurfacePitch;
    struct {
        uint32_t auxSurfaceQPitch : 17;
        uint32_t : 4;
        uint32_t auxSurfaceTilingType : 3;
        uint32_t : 8;
    };
    struct {
        uint32_t blockWidth : 8;
        uint32_t blockHeight : 8;
        uint32_t blockDepth : 8;
        uint32_t mode : 1;
        uint32_t algorithm : 3;
        uint32_t : 4;
    };
    uint32_t tileWidth;
    uint32_t tileHeight;
    uint32_t tileDepth;
    uint32_t clearColorRed;
    uint32_t clearColorGreen;
    uint32_t clearColorBlue;
    uint32_t clearColorAlpha;
    struct {
        uint32_t gttType : 2;
        uint32_t clearColorType : 1;
        uint32_t : 29;
    };
    uint32_t directoryHandle;
    uint64_t clearColorAddress;
    int getClearColorAddressLength() const {
        return 24 - (23) + 1;
    }
    int32_t getPacketSize() const {
        return dwordCount + 1;
    }
    int32_t getLengthBias() const {
        return 1;
    }
    uint32_t getBaseLength() const {
        return 19;
    }
    bool matchesHeader() const {
        if (instructionType != 0x7)
            return false;
        if (instructionOpcode != 0x2e)
            return false;
        if (instructionSubOpcode != 0x10)
            return false;
        return true;
    }
    void setHeader() {
        instructionType = 0x7;
        instructionOpcode = 0x2e;
        instructionSubOpcode = 0x10;
    }
    static uint32_t type() { return 0x7; }
    static uint32_t opcode() { return 0x2e; }
    static uint32_t subOpcode() { return 0x10; }
    struct GttTypeValues {
        enum { Ppgtt = 1,
               Ggtt = 0 };
    };
    struct SurfaceTilingTypeValues {
        enum { YmajorS = 4,
               Xmajor = 2,
               YmajorF = 5,
               Linear = 0,
               Wmajor = 1,
               Ymajor = 3 };
    };
    struct ModeValues {
        enum { Horizontal = 1,
               Vertical = 0 };
    };
    struct ClearColorTypeValues {
        enum { Immediate = 0,
               Address = 1 };
    };
    struct SurfaceTypeValues {
        enum {
            SurftypeCube = 3,
            SurftypeStrbuf = 5,
            SurftypeBuffer = 4,
            Surftype3D = 2,
            Surftype2D = 1,
            Surftype1D = 0,
            SurftypeNull = 6
        };
    };
    struct AlgorithmValues {
        enum { Uncompressed = 4,
               Astc = 1,
               Lossless = 2,
               Media = 0,
               Msaa = 3 };
    };
    struct AuxSurfaceTilingTypeValues {
        enum { YmajorS = 4,
               Xmajor = 2,
               YmajorF = 5,
               Linear = 0,
               Wmajor = 1,
               Ymajor = 3 };
    };
    struct DumpTypeValues {
        enum { Bin = 1,
               Png = 4,
               Bmp = 0,
               Bmp32 = 2,
               Tre = 3 };
    };
    struct TiledResourceModeValues {
        enum { TrmodeNone = 0,
               TrmodeYf = 1,
               TrmodeYs = 2 };
    };
};
#ifndef WIN32
#pragma pack()
#else
#pragma pack(pop)
#endif
