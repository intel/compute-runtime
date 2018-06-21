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
#pragma pack(1)
typedef struct tagMEDIA_SURFACE_STATE {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0;
            uint32_t Cr_VCb_UPixelOffsetVDirection : BITFIELD_RANGE(0, 1);
            uint32_t PictureStructure : BITFIELD_RANGE(2, 3);
            uint32_t Width : BITFIELD_RANGE(4, 17);
            uint32_t Height : BITFIELD_RANGE(18, 31);
            uint32_t TileMode : BITFIELD_RANGE(0, 1);
            uint32_t HalfPitchForChroma : BITFIELD_RANGE(2, 2);
            uint32_t SurfacePitch : BITFIELD_RANGE(3, 20);
            uint32_t AddressControl : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_86 : BITFIELD_RANGE(22, 25);
            uint32_t InterleaveChroma : BITFIELD_RANGE(26, 26);
            uint32_t SurfaceFormat : BITFIELD_RANGE(27, 31);
            uint32_t YOffsetForU_Cb : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_110 : BITFIELD_RANGE(14, 15);
            uint32_t XOffsetForU_Cb : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_126 : BITFIELD_RANGE(30, 31);
            uint32_t Reserved_128;
            uint32_t SurfaceMemoryObjectControlState_AgeForQuadlru : BITFIELD_RANGE(0, 1);
            uint32_t SurfaceMemoryObjectControlState_Reserved : BITFIELD_RANGE(2, 2);
            uint32_t SurfaceMemoryObjectControlState_TargetCache : BITFIELD_RANGE(3, 4);
            uint32_t SurfaceMemoryObjectControlState_MemoryTypeLlcEllcCacheabilityControl : BITFIELD_RANGE(5, 6);
            uint32_t Reserved_167 : BITFIELD_RANGE(7, 29);
            uint32_t VerticalLineStrideOffset : BITFIELD_RANGE(30, 30);
            uint32_t VerticalLineStride : BITFIELD_RANGE(31, 31);
            uint32_t SurfaceBaseAddressLow;
            uint32_t SurfaceBaseAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_240 : BITFIELD_RANGE(16, 31);
        } Common;
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
    } SURFACE_FORMAT;
    typedef enum tagSURFACE_MEMORY_OBJECT_CONTROL_STATE {
        SURFACE_MEMORY_OBJECT_CONTROL_STATE_DEFAULTVAUEDESC = 0x0,
    } SURFACE_MEMORY_OBJECT_CONTROL_STATE;
    typedef enum tagPATCH_CONSTANTS {
        SURFACEBASEADDRESS_BYTEOFFSET = 0x18,
        SURFACEBASEADDRESS_INDEX = 0x6,
        SURFACEBASEADDRESSHIGH_BYTEOFFSET = 0x1c,
        SURFACEBASEADDRESSHIGH_INDEX = 0x7,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.PictureStructure = PICTURE_STRUCTURE_FRAME_PICTURE;
        TheStructure.Common.TileMode = TILE_MODE_TILEMODE_LINEAR;
        TheStructure.Common.AddressControl = ADDRESS_CONTROL_CLAMP;
        TheStructure.Common.SurfaceFormat = SURFACE_FORMAT_YCRCB_NORMAL;
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
    inline void setSurfaceMemoryObjectControlStateAgeForQuadlru(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_AgeForQuadlru = value;
    }
    inline uint32_t getSurfaceMemoryObjectControlStateAgeForQuadlru(void) const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_AgeForQuadlru);
    }
    inline void setSurfaceMemoryObjectControlStateReserved(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_Reserved = value;
    }
    inline uint32_t getSurfaceMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_Reserved);
    }
    inline void setSurfaceMemoryObjectControlStateTargetCache(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_TargetCache = value;
    }
    inline uint32_t getSurfaceMemoryObjectControlStateTargetCache(void) const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_TargetCache);
    }
    inline void setSurfaceMemoryObjectControlStateMemoryTypeLlcEllcCacheabilityControl(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_MemoryTypeLlcEllcCacheabilityControl = value;
    }
    inline uint32_t getSurfaceMemoryObjectControlStateMemoryTypeLlcEllcCacheabilityControl(void) const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_MemoryTypeLlcEllcCacheabilityControl);
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
        TheStructure.Common.SurfaceBaseAddressLow = static_cast<uint32_t>(value);
        TheStructure.Common.SurfaceBaseAddressHigh = value >> 32;
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
            uint32_t DwordLength : BITFIELD_RANGE(0, 5);
            uint32_t Reserved : BITFIELD_RANGE(6, 22);
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
#pragma pack()
