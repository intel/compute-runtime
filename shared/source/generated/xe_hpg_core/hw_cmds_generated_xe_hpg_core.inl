/*
 * Copyright (C) 2021-2025 Intel Corporation
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
    }
    static tagBINDING_TABLE_STATE sInit() {
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
    inline uint32_t getSurfaceStatePointer() const {
        return (TheStructure.Common.SurfaceStatePointer << SURFACESTATEPOINTER_BIT_SHIFT);
    }
} BINDING_TABLE_STATE;
STATIC_ASSERT(4 == sizeof(BINDING_TABLE_STATE));

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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.Rotation = ROTATION_NO_ROTATION_OR_0_DEGREE;
        TheStructure.Common.PictureStructure = PICTURE_STRUCTURE_FRAME_PICTURE;
        TheStructure.Common.TileMode = TILE_MODE_TILEMODE_LINEAR;
        TheStructure.Common.AddressControl = ADDRESS_CONTROL_CLAMP;
        TheStructure.Common.MemoryCompressionMode = MEMORY_COMPRESSION_MODE_HORIZONTAL_COMPRESSION_MODE;
        TheStructure.Common.SurfaceFormat = SURFACE_FORMAT_YCRCB_NORMAL;
        TheStructure.Common.TiledResourceMode = TILED_RESOURCE_MODE_TRMODE_NONE;
    }
    static tagMEDIA_SURFACE_STATE sInit() {
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
    inline ROTATION getRotation() const {
        return static_cast<ROTATION>(TheStructure.Common.Rotation);
    }
    inline void setCrVCbUPixelOffsetVDirection(const uint32_t value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetVDirection = value;
    }
    inline uint32_t getCrVCbUPixelOffsetVDirection() const {
        return (TheStructure.Common.Cr_VCb_UPixelOffsetVDirection);
    }
    inline void setPictureStructure(const PICTURE_STRUCTURE value) {
        TheStructure.Common.PictureStructure = value;
    }
    inline PICTURE_STRUCTURE getPictureStructure() const {
        return static_cast<PICTURE_STRUCTURE>(TheStructure.Common.PictureStructure);
    }
    inline void setWidth(const uint32_t value) {
        TheStructure.Common.Width = value - 1;
    }
    inline uint32_t getWidth() const {
        return (TheStructure.Common.Width + 1);
    }
    inline void setHeight(const uint32_t value) {
        TheStructure.Common.Height = value - 1;
    }
    inline uint32_t getHeight() const {
        return (TheStructure.Common.Height + 1);
    }
    inline void setTileMode(const TILE_MODE value) {
        TheStructure.Common.TileMode = value;
    }
    inline TILE_MODE getTileMode() const {
        return static_cast<TILE_MODE>(TheStructure.Common.TileMode);
    }
    inline void setHalfPitchForChroma(const bool value) {
        TheStructure.Common.HalfPitchForChroma = value;
    }
    inline bool getHalfPitchForChroma() const {
        return (TheStructure.Common.HalfPitchForChroma);
    }
    inline void setSurfacePitch(const uint32_t value) {
        TheStructure.Common.SurfacePitch = value - 1;
    }
    inline uint32_t getSurfacePitch() const {
        return (TheStructure.Common.SurfacePitch + 1);
    }
    inline void setAddressControl(const ADDRESS_CONTROL value) {
        TheStructure.Common.AddressControl = value;
    }
    inline ADDRESS_CONTROL getAddressControl() const {
        return static_cast<ADDRESS_CONTROL>(TheStructure.Common.AddressControl);
    }
    inline void setMemoryCompressionEnable(const bool value) {
        TheStructure.Common.MemoryCompressionEnable = value;
    }
    inline bool getMemoryCompressionEnable() const {
        return (TheStructure.Common.MemoryCompressionEnable);
    }
    inline void setMemoryCompressionMode(const MEMORY_COMPRESSION_MODE value) {
        TheStructure.Common.MemoryCompressionMode = value;
    }
    inline MEMORY_COMPRESSION_MODE getMemoryCompressionMode() const {
        return static_cast<MEMORY_COMPRESSION_MODE>(TheStructure.Common.MemoryCompressionMode);
    }
    inline void setCrVCbUPixelOffsetVDirectionMsb(const uint32_t value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb = value;
    }
    inline uint32_t getCrVCbUPixelOffsetVDirectionMsb() const {
        return (TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb);
    }
    inline void setCrVCbUPixelOffsetUDirection(const uint32_t value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetUDirection = value;
    }
    inline uint32_t getCrVCbUPixelOffsetUDirection() const {
        return (TheStructure.Common.Cr_VCb_UPixelOffsetUDirection);
    }
    inline void setInterleaveChroma(const bool value) {
        TheStructure.Common.InterleaveChroma = value;
    }
    inline bool getInterleaveChroma() const {
        return (TheStructure.Common.InterleaveChroma);
    }
    inline void setSurfaceFormat(const SURFACE_FORMAT value) {
        TheStructure.Common.SurfaceFormat = value;
    }
    inline SURFACE_FORMAT getSurfaceFormat() const {
        return static_cast<SURFACE_FORMAT>(TheStructure.Common.SurfaceFormat);
    }
    inline void setYOffsetForUCb(const uint32_t value) {
        TheStructure.Common.YOffsetForU_Cb = value;
    }
    inline uint32_t getYOffsetForUCb() const {
        return (TheStructure.Common.YOffsetForU_Cb);
    }
    inline void setXOffsetForUCb(const uint32_t value) {
        TheStructure.Common.XOffsetForU_Cb = value;
    }
    inline uint32_t getXOffsetForUCb() const {
        return (TheStructure.Common.XOffsetForU_Cb);
    }
    inline void setSurfaceMemoryObjectControlStateReserved(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_Reserved = value;
    }
    inline uint32_t getSurfaceMemoryObjectControlStateReserved() const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_Reserved);
    }
    inline void setSurfaceMemoryObjectControlState(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint32_t getSurfaceMemoryObjectControlState() const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_IndexToMocsTables << 1);
    }
    inline void setTiledResourceMode(const TILED_RESOURCE_MODE value) {
        TheStructure.Common.TiledResourceMode = value;
    }
    inline TILED_RESOURCE_MODE getTiledResourceMode() const {
        return static_cast<TILED_RESOURCE_MODE>(TheStructure.Common.TiledResourceMode);
    }
    inline void setDepth(const uint32_t value) {
        TheStructure.Common.Depth = value;
    }
    inline uint32_t getDepth() const {
        return (TheStructure.Common.Depth);
    }
    inline void setVerticalLineStrideOffset(const uint32_t value) {
        TheStructure.Common.VerticalLineStrideOffset = value;
    }
    inline uint32_t getVerticalLineStrideOffset() const {
        return (TheStructure.Common.VerticalLineStrideOffset);
    }
    inline void setVerticalLineStride(const uint32_t value) {
        TheStructure.Common.VerticalLineStride = value;
    }
    inline uint32_t getVerticalLineStride() const {
        return (TheStructure.Common.VerticalLineStride);
    }
    inline void setSurfaceBaseAddress(const uint64_t value) {
        TheStructure.Common.SurfaceBaseAddressLow = static_cast<uint32_t>(value & 0xffffffff);
        TheStructure.Common.SurfaceBaseAddressHigh = (value >> 32) & 0xffffffff;
    }
    inline uint64_t getSurfaceBaseAddress() const {
        return (TheStructure.Common.SurfaceBaseAddressLow |
                static_cast<uint64_t>(TheStructure.Common.SurfaceBaseAddressHigh) << 32);
    }
    inline void setSurfaceBaseAddressHigh(const uint32_t value) {
        TheStructure.Common.SurfaceBaseAddressHigh = value;
    }
    inline uint32_t getSurfaceBaseAddressHigh() const {
        return (TheStructure.Common.SurfaceBaseAddressHigh);
    }
    typedef enum tagYOFFSET {
        YOFFSET_BIT_SHIFT = 0x2,
        YOFFSET_ALIGN_SIZE = 0x4,
    } YOFFSET;
    inline void setYOffset(const uint32_t value) {
        TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset = value >> YOFFSET_BIT_SHIFT;
    }
    inline uint32_t getYOffset() const {
        return (TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset << YOFFSET_BIT_SHIFT);
    }
    typedef enum tagXOFFSET {
        XOFFSET_BIT_SHIFT = 0x2,
        XOFFSET_ALIGN_SIZE = 0x4,
    } XOFFSET;
    inline void setXOffset(const uint32_t value) {
        TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset = value >> XOFFSET_BIT_SHIFT;
    }
    inline uint32_t getXOffset() const {
        return (TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset << XOFFSET_BIT_SHIFT);
    }
    inline void setYOffsetForVCr(const uint32_t value) {
        TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr = value;
    }
    inline uint32_t getYOffsetForVCr() const {
        return (TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr);
    }
    inline void setXOffsetForVCr(const uint32_t value) {
        TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr = value;
    }
    inline uint32_t getXOffsetForVCr() const {
        return (TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr);
    }
} MEDIA_SURFACE_STATE;
STATIC_ASSERT(32 == sizeof(MEDIA_SURFACE_STATE));

typedef struct tagMI_MATH {
    union _DW0 {
        struct _BitField {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t MemoryObjectControlState : BITFIELD_RANGE(8, 14);
            uint32_t Reserved : BITFIELD_RANGE(15, 22);
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

typedef struct tagPIPE_CONTROL {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t PredicateEnable : BITFIELD_RANGE(8, 8);
            uint32_t HdcPipelineFlush : BITFIELD_RANGE(9, 9);
            uint32_t Reserved_10 : BITFIELD_RANGE(10, 10);
            uint32_t UnTypedDataPortCacheFlush : BITFIELD_RANGE(11, 11);
            uint32_t Reserved_12 : BITFIELD_RANGE(12, 12);
            uint32_t CompressionControlSurfaceCcsFlush : BITFIELD_RANGE(13, 13);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(14, 14);
            uint32_t Reserved_15 : BITFIELD_RANGE(15, 15);
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
            uint32_t PssStallSyncEnable : BITFIELD_RANGE(17, 17);
            uint32_t TlbInvalidate : BITFIELD_RANGE(18, 18);
            uint32_t DepthStallSyncEnable : BITFIELD_RANGE(19, 19);
            uint32_t CommandStreamerStallEnable : BITFIELD_RANGE(20, 20);
            uint32_t StoreDataIndex : BITFIELD_RANGE(21, 21);
            uint32_t ProtectedMemoryEnable : BITFIELD_RANGE(22, 22);
            uint32_t LriPostSyncOperation : BITFIELD_RANGE(23, 23);
            uint32_t DestinationAddressType : BITFIELD_RANGE(24, 24);
            uint32_t AmfsFlushEnable : BITFIELD_RANGE(25, 25);
            uint32_t FlushLlc : BITFIELD_RANGE(26, 26);
            uint32_t ProtectedMemoryDisable : BITFIELD_RANGE(27, 27);
            uint32_t TileCacheFlushEnable : BITFIELD_RANGE(28, 28);
            uint32_t Reserved_61 : BITFIELD_RANGE(29, 29);
            uint32_t L3FabricFlush : BITFIELD_RANGE(30, 30);
            uint32_t TbimrForceBatchClosure : BITFIELD_RANGE(31, 31);
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
    typedef enum tagUN_TYPED_DATA_PORT_CACHE_FLUSH {
        UN_TYPED_DATA_PORT_CACHE_FLUSH_DISABLED = 0x0,
        UN_TYPED_DATA_PORT_CACHE_FLUSH_ENABLED = 0x1,
    } UN_TYPED_DATA_PORT_CACHE_FLUSH;
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
    typedef enum tagLRI_POST_SYNC_OPERATION {
        LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION = 0x0,
        LRI_POST_SYNC_OPERATION_MMIO_WRITE_IMMEDIATE_DATA = 0x1,
    } LRI_POST_SYNC_OPERATION;
    typedef enum tagDESTINATION_ADDRESS_TYPE {
        DESTINATION_ADDRESS_TYPE_PPGTT = 0x0,
        DESTINATION_ADDRESS_TYPE_GGTT = 0x1,
    } DESTINATION_ADDRESS_TYPE;
    typedef enum tagTBIMR_FORCE_BATCH_CLOSURE {
        TBIMR_FORCE_BATCH_CLOSURE_NO_BATCH_CLOSURE = 0x0,
        TBIMR_FORCE_BATCH_CLOSURE_CLOSE_BATCH = 0x1,
    } TBIMR_FORCE_BATCH_CLOSURE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common.UnTypedDataPortCacheFlush = UN_TYPED_DATA_PORT_CACHE_FLUSH_DISABLED;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_PIPE_CONTROL;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_PIPE_CONTROL;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_3D;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.PostSyncOperation = POST_SYNC_OPERATION_NO_WRITE;
        TheStructure.Common.LriPostSyncOperation = LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION;
        TheStructure.Common.DestinationAddressType = DESTINATION_ADDRESS_TYPE_PPGTT;
        TheStructure.Common.TbimrForceBatchClosure = TBIMR_FORCE_BATCH_CLOSURE_NO_BATCH_CLOSURE;
        TheStructure.Common.CommandStreamerStallEnable = 1;
    }
    static tagPIPE_CONTROL sInit() {
        PIPE_CONTROL state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 6);
        return TheStructure.RawData[index];
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
    inline void setHdcPipelineFlush(const bool value) {
        TheStructure.Common.HdcPipelineFlush = value;
    }
    inline bool getHdcPipelineFlush() const {
        return TheStructure.Common.HdcPipelineFlush;
    }
    inline void setUnTypedDataPortCacheFlush(const bool value) {
        TheStructure.Common.UnTypedDataPortCacheFlush = value;
    }
    inline bool getUnTypedDataPortCacheFlush() const {
        return TheStructure.Common.UnTypedDataPortCacheFlush;
    }
    inline void setCompressionControlSurfaceCcsFlush(const bool value) {
        TheStructure.Common.CompressionControlSurfaceCcsFlush = value;
    }
    inline bool getCompressionControlSurfaceCcsFlush() const {
        return TheStructure.Common.CompressionControlSurfaceCcsFlush;
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    inline void setDepthCacheFlushEnable(const bool value) {
        TheStructure.Common.DepthCacheFlushEnable = value;
    }
    inline bool getDepthCacheFlushEnable() const {
        return TheStructure.Common.DepthCacheFlushEnable;
    }
    inline void setStallAtPixelScoreboard(const bool value) {
        TheStructure.Common.StallAtPixelScoreboard = value;
    }
    inline bool getStallAtPixelScoreboard() const {
        return TheStructure.Common.StallAtPixelScoreboard;
    }
    inline void setStateCacheInvalidationEnable(const bool value) {
        TheStructure.Common.StateCacheInvalidationEnable = value;
    }
    inline bool getStateCacheInvalidationEnable() const {
        return TheStructure.Common.StateCacheInvalidationEnable;
    }
    inline void setConstantCacheInvalidationEnable(const bool value) {
        TheStructure.Common.ConstantCacheInvalidationEnable = value;
    }
    inline bool getConstantCacheInvalidationEnable() const {
        return TheStructure.Common.ConstantCacheInvalidationEnable;
    }
    inline void setVfCacheInvalidationEnable(const bool value) {
        TheStructure.Common.VfCacheInvalidationEnable = value;
    }
    inline bool getVfCacheInvalidationEnable() const {
        return TheStructure.Common.VfCacheInvalidationEnable;
    }
    inline void setDcFlushEnable(const bool value) {
        TheStructure.Common.DcFlushEnable = value;
    }
    inline bool getDcFlushEnable() const {
        return TheStructure.Common.DcFlushEnable;
    }
    inline void setProtectedMemoryApplicationId(const bool value) {
        TheStructure.Common.ProtectedMemoryApplicationId = value;
    }
    inline bool getProtectedMemoryApplicationId() const {
        return TheStructure.Common.ProtectedMemoryApplicationId;
    }
    inline void setPipeControlFlushEnable(const bool value) {
        TheStructure.Common.PipeControlFlushEnable = value;
    }
    inline bool getPipeControlFlushEnable() const {
        return TheStructure.Common.PipeControlFlushEnable;
    }
    inline void setNotifyEnable(const bool value) {
        TheStructure.Common.NotifyEnable = value;
    }
    inline bool getNotifyEnable() const {
        return TheStructure.Common.NotifyEnable;
    }
    inline void setIndirectStatePointersDisable(const bool value) {
        TheStructure.Common.IndirectStatePointersDisable = value;
    }
    inline bool getIndirectStatePointersDisable() const {
        return TheStructure.Common.IndirectStatePointersDisable;
    }
    inline void setTextureCacheInvalidationEnable(const bool value) {
        TheStructure.Common.TextureCacheInvalidationEnable = value;
    }
    inline bool getTextureCacheInvalidationEnable() const {
        return TheStructure.Common.TextureCacheInvalidationEnable;
    }
    inline void setInstructionCacheInvalidateEnable(const bool value) {
        TheStructure.Common.InstructionCacheInvalidateEnable = value;
    }
    inline bool getInstructionCacheInvalidateEnable() const {
        return TheStructure.Common.InstructionCacheInvalidateEnable;
    }
    inline void setRenderTargetCacheFlushEnable(const bool value) {
        TheStructure.Common.RenderTargetCacheFlushEnable = value;
    }
    inline bool getRenderTargetCacheFlushEnable() const {
        return TheStructure.Common.RenderTargetCacheFlushEnable;
    }
    inline void setDepthStallEnable(const bool value) {
        TheStructure.Common.DepthStallEnable = value;
    }
    inline bool getDepthStallEnable() const {
        return TheStructure.Common.DepthStallEnable;
    }
    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }
    inline POST_SYNC_OPERATION getPostSyncOperation() const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.PostSyncOperation);
    }
    inline void setGenericMediaStateClear(const bool value) {
        TheStructure.Common.GenericMediaStateClear = value;
    }
    inline bool getGenericMediaStateClear() const {
        return TheStructure.Common.GenericMediaStateClear;
    }
    inline void setPssStallSyncEnable(const bool value) {
        TheStructure.Common.PssStallSyncEnable = value;
    }
    inline bool getPssStallSyncEnable() const {
        return TheStructure.Common.PssStallSyncEnable;
    }
    inline void setTlbInvalidate(const bool value) {
        TheStructure.Common.TlbInvalidate = value;
    }
    inline bool getTlbInvalidate() const {
        return TheStructure.Common.TlbInvalidate;
    }
    inline void setDepthStallSyncEnable(const bool value) {
        TheStructure.Common.DepthStallSyncEnable = value;
    }
    inline bool getDepthStallSyncEnable() const {
        return TheStructure.Common.DepthStallSyncEnable;
    }
    inline void setCommandStreamerStallEnable(const uint32_t value) {
        TheStructure.Common.CommandStreamerStallEnable = value;
    }
    inline uint32_t getCommandStreamerStallEnable() const {
        return TheStructure.Common.CommandStreamerStallEnable;
    }
    inline void setStoreDataIndex(const bool value) {
        TheStructure.Common.StoreDataIndex = value;
    }
    inline bool getStoreDataIndex() const {
        return TheStructure.Common.StoreDataIndex;
    }
    inline void setProtectedMemoryEnable(const bool value) {
        TheStructure.Common.ProtectedMemoryEnable = value;
    }
    inline bool getProtectedMemoryEnable() const {
        return TheStructure.Common.ProtectedMemoryEnable;
    }
    inline void setLriPostSyncOperation(const LRI_POST_SYNC_OPERATION value) {
        TheStructure.Common.LriPostSyncOperation = value;
    }
    inline LRI_POST_SYNC_OPERATION getLriPostSyncOperation() const {
        return static_cast<LRI_POST_SYNC_OPERATION>(TheStructure.Common.LriPostSyncOperation);
    }
    inline void setDestinationAddressType(const DESTINATION_ADDRESS_TYPE value) {
        TheStructure.Common.DestinationAddressType = value;
    }
    inline DESTINATION_ADDRESS_TYPE getDestinationAddressType() const {
        return static_cast<DESTINATION_ADDRESS_TYPE>(TheStructure.Common.DestinationAddressType);
    }
    inline void setAmfsFlushEnable(const bool value) {
        TheStructure.Common.AmfsFlushEnable = value;
    }
    inline bool getAmfsFlushEnable() const {
        return TheStructure.Common.AmfsFlushEnable;
    }
    inline void setFlushLlc(const bool value) {
        TheStructure.Common.FlushLlc = value;
    }
    inline bool getFlushLlc() const {
        return TheStructure.Common.FlushLlc;
    }
    inline void setProtectedMemoryDisable(const bool value) {
        TheStructure.Common.ProtectedMemoryDisable = value;
    }
    inline bool getProtectedMemoryDisable() const {
        return TheStructure.Common.ProtectedMemoryDisable;
    }
    inline void setTileCacheFlushEnable(const bool value) {
        TheStructure.Common.TileCacheFlushEnable = value;
    }
    inline bool getTileCacheFlushEnable() const {
        return TheStructure.Common.TileCacheFlushEnable;
    }
    inline void setL3FabricFlush(const bool value) {
        TheStructure.Common.L3FabricFlush = value;
    }
    inline bool getL3FabricFlush() const {
        return TheStructure.Common.L3FabricFlush;
    }
    inline void setTbimrForceBatchClosure(const TBIMR_FORCE_BATCH_CLOSURE value) {
        TheStructure.Common.TbimrForceBatchClosure = value;
    }
    inline TBIMR_FORCE_BATCH_CLOSURE getTbimrForceBatchClosure() const {
        return static_cast<TBIMR_FORCE_BATCH_CLOSURE>(TheStructure.Common.TbimrForceBatchClosure);
    }
    typedef enum tagADDRESS {
        ADDRESS_BIT_SHIFT = 0x2,
        ADDRESS_ALIGN_SIZE = 0x4,
    } ADDRESS;
    inline void setAddress(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfffffffc);
        TheStructure.Common.Address = value >> ADDRESS_BIT_SHIFT;
    }
    inline uint32_t getAddress() const {
        return TheStructure.Common.Address << ADDRESS_BIT_SHIFT;
    }
    inline void setAddressHigh(const uint32_t value) {
        TheStructure.Common.AddressHigh = value;
    }
    inline uint32_t getAddressHigh() const {
        return TheStructure.Common.AddressHigh;
    }
    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }
    inline uint64_t getImmediateData() const {
        return TheStructure.Common.ImmediateData;
    }
} PIPE_CONTROL;
STATIC_ASSERT(24 == sizeof(PIPE_CONTROL));

typedef struct tagMI_ATOMIC {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t AtomicOpcode : BITFIELD_RANGE(8, 15);
            uint32_t ReturnDataControl : BITFIELD_RANGE(16, 16);
            uint32_t CsStall : BITFIELD_RANGE(17, 17);
            uint32_t InlineData : BITFIELD_RANGE(18, 18);
            uint32_t DataSize : BITFIELD_RANGE(19, 20);
            uint32_t PostSyncOperation : BITFIELD_RANGE(21, 21);
            uint32_t MemoryType : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_33 : BITFIELD_RANGE(1, 1);
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
    typedef enum tagATOMIC_OPCODES {
        ATOMIC_4B_MOVE = 0x4,
        ATOMIC_4B_INCREMENT = 0x5,
        ATOMIC_4B_DECREMENT = 0x6,
        ATOMIC_8B_MOVE = 0x24,
        ATOMIC_8B_INCREMENT = 0x25,
        ATOMIC_8B_DECREMENT = 0x26,
        ATOMIC_8B_ADD = 0x27,
        ATOMIC_8B_CMP_WR = 0x2E,
    } ATOMIC_OPCODES;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_INLINE_DATA_0;
        TheStructure.Common.DataSize = DATA_SIZE_DWORD;
        TheStructure.Common.PostSyncOperation =
            POST_SYNC_OPERATION_NO_POST_SYNC_OPERATION;
        TheStructure.Common.MemoryType = MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_ATOMIC;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_ATOMIC sInit() {
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
    inline DWORD_LENGTH getDwordLength() const {
        return static_cast<DWORD_LENGTH>(TheStructure.Common.DwordLength);
    }
    inline void setAtomicOpcode(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0xff00);
        TheStructure.Common.AtomicOpcode = value;
    }
    inline uint32_t getAtomicOpcode() const {
        return TheStructure.Common.AtomicOpcode;
    }
    inline void setReturnDataControl(const uint32_t value) {
        TheStructure.Common.ReturnDataControl = value;
    }
    inline uint32_t getReturnDataControl() const {
        return TheStructure.Common.ReturnDataControl;
    }
    inline void setCsStall(const uint32_t value) {
        TheStructure.Common.CsStall = value;
    }
    inline uint32_t getCsStall() const { return TheStructure.Common.CsStall; }
    inline void setInlineData(const uint32_t value) {
        TheStructure.Common.InlineData = value;
    }
    inline uint32_t getInlineData() const {
        return TheStructure.Common.InlineData;
    }
    inline void setDataSize(const DATA_SIZE value) {
        TheStructure.Common.DataSize = value;
    }
    inline DATA_SIZE getDataSize() const {
        return static_cast<DATA_SIZE>(TheStructure.Common.DataSize);
    }
    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }
    inline POST_SYNC_OPERATION getPostSyncOperation() const {
        return static_cast<POST_SYNC_OPERATION>(
            TheStructure.Common.PostSyncOperation);
    }
    inline void setMemoryType(const MEMORY_TYPE value) {
        TheStructure.Common.MemoryType = value;
    }
    inline MEMORY_TYPE getMemoryType() const {
        return static_cast<MEMORY_TYPE>(TheStructure.Common.MemoryType);
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x2,
        MEMORYADDRESS_ALIGN_SIZE = 0x4,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0xfffffffcL);
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint32_t getMemoryAddress() const {
        return TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT;
    }
    inline void setMemoryAddressHigh(const uint32_t value) {
        TheStructure.Common.MemoryAddressHigh = value;
    }
    inline uint32_t getMemoryAddressHigh() const {
        return TheStructure.Common.MemoryAddressHigh;
    }
    inline void setOperand1DataDword0(const uint32_t value) {
        TheStructure.Common.Operand1DataDword0 = value;
    }
    inline uint32_t getOperand1DataDword0() const {
        return TheStructure.Common.Operand1DataDword0;
    }
    inline void setOperand2DataDword0(const uint32_t value) {
        TheStructure.Common.Operand2DataDword0 = value;
    }
    inline uint32_t getOperand2DataDword0() const {
        return TheStructure.Common.Operand2DataDword0;
    }
    inline void setOperand1DataDword1(const uint32_t value) {
        TheStructure.Common.Operand1DataDword1 = value;
    }
    inline uint32_t getOperand1DataDword1() const {
        return TheStructure.Common.Operand1DataDword1;
    }
    inline void setOperand2DataDword1(const uint32_t value) {
        TheStructure.Common.Operand2DataDword1 = value;
    }
    inline uint32_t getOperand2DataDword1() const {
        return TheStructure.Common.Operand2DataDword1;
    }
    inline void setOperand1DataDword2(const uint32_t value) {
        TheStructure.Common.Operand1DataDword2 = value;
    }
    inline uint32_t getOperand1DataDword2() const {
        return TheStructure.Common.Operand1DataDword2;
    }
    inline void setOperand2DataDword2(const uint32_t value) {
        TheStructure.Common.Operand2DataDword2 = value;
    }
    inline uint32_t getOperand2DataDword2() const {
        return TheStructure.Common.Operand2DataDword2;
    }
    inline void setOperand1DataDword3(const uint32_t value) {
        TheStructure.Common.Operand1DataDword3 = value;
    }
    inline uint32_t getOperand1DataDword3() const {
        return TheStructure.Common.Operand1DataDword3;
    }
    inline void setOperand2DataDword3(const uint32_t value) {
        TheStructure.Common.Operand2DataDword3 = value;
    }
    inline uint32_t getOperand2DataDword3() const {
        return TheStructure.Common.Operand2DataDword3;
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_BATCH_BUFFER_END;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_BATCH_BUFFER_END sInit() {
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
    inline bool getEndContext() const {
        return (TheStructure.Common.EndContext);
    }
} MI_BATCH_BUFFER_END;
STATIC_ASSERT(4 == sizeof(MI_BATCH_BUFFER_END));

typedef struct tagMI_LOAD_REGISTER_IMM {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t ByteWriteDisables : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_12 : BITFIELD_RANGE(12, 16);
            uint32_t MmioRemapEnable : BITFIELD_RANGE(17, 17);
            uint32_t Reserved_13 : BITFIELD_RANGE(18, 18);
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_LOAD_REGISTER_IMM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_LOAD_REGISTER_IMM sInit() {
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
    inline uint32_t getByteWriteDisables() const {
        return (TheStructure.Common.ByteWriteDisables);
    }
    inline void setAddCsMmioStartOffset(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffset = value;
    }
    inline uint32_t getAddCsMmioStartOffset() const {
        return (TheStructure.Common.AddCsMmioStartOffset);
    }
    typedef enum tagREGISTEROFFSET {
        REGISTEROFFSET_BIT_SHIFT = 0x2,
        REGISTEROFFSET_ALIGN_SIZE = 0x4,
    } REGISTEROFFSET;
    inline void setRegisterOffset(const uint32_t value) {
        TheStructure.Common.RegisterOffset = value >> REGISTEROFFSET_BIT_SHIFT;
    }
    inline uint32_t getRegisterOffset() const {
        return (TheStructure.Common.RegisterOffset << REGISTEROFFSET_BIT_SHIFT);
    }
    inline void setDataDword(const uint32_t value) {
        TheStructure.Common.DataDword = value;
    }
    inline uint32_t getDataDword() const {
        return (TheStructure.Common.DataDword);
    }
    inline void setMmioRemapEnable(const bool value) {
        TheStructure.Common.MmioRemapEnable = value;
    }
    inline bool getMmioRemapEnable() const {
        return TheStructure.Common.MmioRemapEnable;
    }
} MI_LOAD_REGISTER_IMM;
STATIC_ASSERT(12 == sizeof(MI_LOAD_REGISTER_IMM));

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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_NOOP;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_NOOP sInit() {
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
    inline uint32_t getIdentificationNumber() const {
        return (TheStructure.Common.IdentificationNumber);
    }
    inline void setIdentificationNumberRegisterWriteEnable(const bool value) {
        TheStructure.Common.IdentificationNumberRegisterWriteEnable = value;
    }
    inline bool getIdentificationNumberRegisterWriteEnable() const {
        return (TheStructure.Common.IdentificationNumberRegisterWriteEnable);
    }
} MI_NOOP;
STATIC_ASSERT(4 == sizeof(MI_NOOP));

typedef struct tagRENDER_SURFACE_STATE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t CubeFaceEnablePositiveZ : BITFIELD_RANGE(0, 0);
            uint32_t CubeFaceEnableNegativeZ : BITFIELD_RANGE(1, 1);
            uint32_t CubeFaceEnablePositiveY : BITFIELD_RANGE(2, 2);
            uint32_t CubeFaceEnableNegativeY : BITFIELD_RANGE(3, 3);
            uint32_t CubeFaceEnablePositiveX : BITFIELD_RANGE(4, 4);
            uint32_t CubeFaceEnableNegativeX : BITFIELD_RANGE(5, 5);
            uint32_t MediaBoundaryPixelMode : BITFIELD_RANGE(6, 7);
            uint32_t RenderCacheReadWriteMode : BITFIELD_RANGE(8, 8);
            uint32_t SamplerL2OutOfOrderModeDisable : BITFIELD_RANGE(9, 9);
            uint32_t VerticalLineStrideOffset : BITFIELD_RANGE(10, 10);
            uint32_t VerticalLineStride : BITFIELD_RANGE(11, 11);
            uint32_t TileMode : BITFIELD_RANGE(12, 13);
            uint32_t SurfaceHorizontalAlignment : BITFIELD_RANGE(14, 15);
            uint32_t SurfaceVerticalAlignment : BITFIELD_RANGE(16, 17);
            uint32_t SurfaceFormat : BITFIELD_RANGE(18, 26);
            uint32_t Reserved_27 : BITFIELD_RANGE(27, 27);
            uint32_t SurfaceArray : BITFIELD_RANGE(28, 28);
            uint32_t SurfaceType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t SurfaceQpitch : BITFIELD_RANGE(0, 14);
            uint32_t SampleTapDiscardDisable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_48 : BITFIELD_RANGE(16, 16);
            uint32_t DoubleFetchDisable : BITFIELD_RANGE(17, 17);
            uint32_t CornerTexelMode : BITFIELD_RANGE(18, 18);
            uint32_t BaseMipLevel : BITFIELD_RANGE(19, 23);
            uint32_t MemoryObjectControlStateEncryptedData : BITFIELD_RANGE(24, 24);
            uint32_t MemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(25, 30);
            uint32_t Reserved_63 : BITFIELD_RANGE(31, 31);
            // DWORD 2
            uint32_t Width : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_78 : BITFIELD_RANGE(14, 15);
            uint32_t Height : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_94 : BITFIELD_RANGE(30, 30);
            uint32_t DepthStencilResource : BITFIELD_RANGE(31, 31);
            // DWORD 3
            uint32_t SurfacePitch : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_114 : BITFIELD_RANGE(18, 20);
            uint32_t Depth : BITFIELD_RANGE(21, 31);
            // DWORD 4
            uint32_t MultisamplePositionPaletteIndex : BITFIELD_RANGE(0, 2);
            uint32_t NumberOfMultisamples : BITFIELD_RANGE(3, 5);
            uint32_t MultisampledSurfaceStorageFormat : BITFIELD_RANGE(6, 6);
            uint32_t RenderTargetViewExtent : BITFIELD_RANGE(7, 17);
            uint32_t MinimumArrayElement : BITFIELD_RANGE(18, 28);
            uint32_t RenderTargetAndSampleUnormRotation : BITFIELD_RANGE(29, 30);
            uint32_t DecompressInL3 : BITFIELD_RANGE(31, 31);
            // DWORD 5
            uint32_t MipCountLod : BITFIELD_RANGE(0, 3);
            uint32_t SurfaceMinLod : BITFIELD_RANGE(4, 7);
            uint32_t MipTailStartLod : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_172 : BITFIELD_RANGE(12, 13);
            uint32_t CoherencyType : BITFIELD_RANGE(14, 15);
            uint32_t L1CacheControlCachePolicy : BITFIELD_RANGE(16, 18);
            uint32_t Reserved_179 : BITFIELD_RANGE(19, 19);
            uint32_t EwaDisableForCube : BITFIELD_RANGE(20, 20);
            uint32_t YOffset : BITFIELD_RANGE(21, 23);
            uint32_t Reserved_184 : BITFIELD_RANGE(24, 24);
            uint32_t XOffset : BITFIELD_RANGE(25, 31);
            // DWORD 6
            uint32_t Reserved_192 : BITFIELD_RANGE(0, 14);
            uint32_t YuvInterpolationEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_208 : BITFIELD_RANGE(16, 31);
            // DWORD 7
            uint32_t ResourceMinLod : BITFIELD_RANGE(0, 11);
            uint32_t Reserved_236 : BITFIELD_RANGE(12, 15);
            uint32_t ShaderChannelSelectAlpha : BITFIELD_RANGE(16, 18);
            uint32_t ShaderChannelSelectBlue : BITFIELD_RANGE(19, 21);
            uint32_t ShaderChannelSelectGreen : BITFIELD_RANGE(22, 24);
            uint32_t ShaderChannelSelectRed : BITFIELD_RANGE(25, 27);
            uint32_t Reserved_252 : BITFIELD_RANGE(28, 29);
            uint32_t MemoryCompressionEnable : BITFIELD_RANGE(30, 30);
            uint32_t MemoryCompressionType : BITFIELD_RANGE(31, 31);
            // DWORD 8, 9
            uint64_t SurfaceBaseAddress;
            // DWORD 10, 11
            uint64_t QuiltWidth : BITFIELD_RANGE(0, 4);
            uint64_t QuiltHeight : BITFIELD_RANGE(5, 9);
            uint64_t ClearValueAddressEnable : BITFIELD_RANGE(10, 10);
            uint64_t ProceduralTexture : BITFIELD_RANGE(11, 11);
            uint64_t Reserved_332 : BITFIELD_RANGE(12, 63);
            // DWORD 12
            uint32_t CompressionFormat : BITFIELD_RANGE(0, 4);
            uint32_t Reserved_389 : BITFIELD_RANGE(5, 5);
            uint32_t ClearAddressLow : BITFIELD_RANGE(6, 31);
            // DWORD 13
            uint32_t ClearAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_432 : BITFIELD_RANGE(16, 30);
            uint32_t DisallowLowQualityFiltering : BITFIELD_RANGE(31, 31);
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } Common;
        struct tag_SurfaceFormatIsPlanar {
            // DWORD 0
            uint32_t Reserved_0;
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t YOffsetForUOrUvPlane : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t XOffsetForUOrUvPlane : BITFIELD_RANGE(16, 29);
            uint32_t HalfPitchForChroma : BITFIELD_RANGE(30, 30);
            uint32_t SeparateUvPlaneEnable : BITFIELD_RANGE(31, 31);
            // DWORD 7
            uint32_t Reserved_224;
            // DWORD 8, 9
            uint64_t Reserved_256;
            // DWORD 10
            uint64_t Reserved_320 : BITFIELD_RANGE(0, 31);
            // DWORD 11
            uint64_t YOffsetForVPlane : BITFIELD_RANGE(32, 45);
            uint64_t Reserved_366 : BITFIELD_RANGE(46, 47);
            uint64_t XOffsetForVPlane : BITFIELD_RANGE(48, 61);
            uint64_t Reserved_382 : BITFIELD_RANGE(62, 63);
            // DWORD 12
            uint32_t Reserved_384;
            // DWORD 13
            uint32_t Reserved_416;
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } _SurfaceFormatIsPlanar;
        struct tag_SurfaceFormatIsnotPlanar {
            // DWORD 0
            uint32_t Reserved_0;
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t AuxiliarySurfaceMode : BITFIELD_RANGE(0, 2);
            uint32_t AuxiliarySurfacePitch : BITFIELD_RANGE(3, 12);
            uint32_t Reserved_205 : BITFIELD_RANGE(13, 15);
            uint32_t AuxiliarySurfaceQpitch : BITFIELD_RANGE(16, 30);
            uint32_t Reserved_223 : BITFIELD_RANGE(31, 31);
            // DWORD 7
            uint32_t Reserved_224;
            // DWORD 8, 9
            uint64_t Reserved_256;
            // DWORD 10, 11
            uint64_t Reserved_320;
            // DWORD 12
            uint32_t Reserved_384;
            // DWORD 13
            uint32_t Reserved_416;
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } _SurfaceFormatIsnotPlanar;
        struct tag_SurfaceFormatIsnotPlanarAndMemoryCompressionEnableIs0 {
            // DWORD 0
            uint32_t Reserved_0;
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t Reserved_192;
            // DWORD 7
            uint32_t Reserved_224;
            // DWORD 8, 9
            uint64_t Reserved_256;
            // DWORD 10, 11
            uint64_t Reserved_320 : BITFIELD_RANGE(0, 11);
            uint64_t AuxiliarySurfaceBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 12
            uint32_t Reserved_384;
            // DWORD 13
            uint32_t Reserved_416;
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } _SurfaceFormatIsnotPlanarAndMemoryCompressionEnableIs0;
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
        TILE_MODE_TILE64 = 0x1,
        TILE_MODE_XMAJOR = 0x2,
        TILE_MODE_TILE4 = 0x3,
    } TILE_MODE;
    typedef enum tagSURFACE_HORIZONTAL_ALIGNMENT {
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_16 = 0x0,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_32 = 0x1,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_64 = 0x2,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_128 = 0x3,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT = SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_128,
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
        SURFACE_FORMAT_PLANAR_422_8_P208 = 0xc,
        SURFACE_FORMAT_PLANAR_420_8_SAMPLE_8X8 = 0xd,
        SURFACE_FORMAT_PLANAR_411_8 = 0xe,
        SURFACE_FORMAT_PLANAR_422_8 = 0xf,
        SURFACE_FORMAT_R8G8B8A8_UNORM_VDI = 0x10,
        SURFACE_FORMAT_YCRCB_NORMAL_SAMPLE_8X8 = 0x11,
        SURFACE_FORMAT_YCRCB_SWAPUVY_SAMPLE_8X8 = 0x12,
        SURFACE_FORMAT_YCRCB_SWAPUV_SAMPLE_8X8 = 0x13,
        SURFACE_FORMAT_YCRCB_SWAPY_SAMPLE_8X8 = 0x14,
        SURFACE_FORMAT_R32G32B32A32_FLOAT_LD = 0x15,
        SURFACE_FORMAT_PLANAR_420_16_SAMPLE_8X8 = 0x16,
        SURFACE_FORMAT_R16B16_UNORM_SAMPLE_8X8 = 0x17,
        SURFACE_FORMAT_Y16_UNORM_SAMPLE_8X8 = 0x18,
        SURFACE_FORMAT_PLANAR_Y32_UNORM = 0x19,
        SURFACE_FORMAT_R32G32B32A32_SFIXED = 0x20,
        SURFACE_FORMAT_R64G64_PASSTHRU = 0x21,
        SURFACE_FORMAT_R32G32B32_FLOAT = 0x40,
        SURFACE_FORMAT_R32G32B32_SINT = 0x41,
        SURFACE_FORMAT_R32G32B32_UINT = 0x42,
        SURFACE_FORMAT_R32G32B32_UNORM = 0x43,
        SURFACE_FORMAT_R32G32B32_SNORM = 0x44,
        SURFACE_FORMAT_R32G32B32_SSCALED = 0x45,
        SURFACE_FORMAT_R32G32B32_USCALED = 0x46,
        SURFACE_FORMAT_R32G32B32_FLOAT_LD = 0x47,
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
        SURFACE_FORMAT_R32G32_FLOAT_LD = 0x97,
        SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS_LD = 0x98,
        SURFACE_FORMAT_R32G32_SFIXED = 0xa0,
        SURFACE_FORMAT_R64_PASSTHRU = 0xa1,
        SURFACE_FORMAT_B8G8R8A8_UNORM = 0xc0,
        SURFACE_FORMAT_B8G8R8A8_UNORM_SRGB = 0xc1,
        SURFACE_FORMAT_R10G10B10A2_UNORM = 0xc2,
        SURFACE_FORMAT_R10G10B10A2_UNORM_SRGB = 0xc3,
        SURFACE_FORMAT_R10G10B10A2_UINT = 0xc4,
        SURFACE_FORMAT_R10G10B10_SNORM_A2_UNORM = 0xc5,
        SURFACE_FORMAT_R10G10B10A2_UNORM_SAMPLE_8X8 = 0xc6,
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
        SURFACE_FORMAT_R10G10B10_FLOAT_A2_UNORM = 0xd5,
        SURFACE_FORMAT_R32_SINT = 0xd6,
        SURFACE_FORMAT_R32_UINT = 0xd7,
        SURFACE_FORMAT_R32_FLOAT = 0xd8,
        SURFACE_FORMAT_R24_UNORM_X8_TYPELESS = 0xd9,
        SURFACE_FORMAT_X24_TYPELESS_G8_UINT = 0xda,
        SURFACE_FORMAT_R32_FLOAT_LD = 0xdb,
        SURFACE_FORMAT_R24_UNORM_X8_TYPELESS_LD = 0xdc,
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
        SURFACE_FORMAT_R8B8G8A8_UNORM = 0xfa,
        SURFACE_FORMAT_R8G8B8A8_SINT_NOA = 0xfb,
        SURFACE_FORMAT_R8G8B8A8_UINT_NOA = 0xfc,
        SURFACE_FORMAT_R8G8B8A8_UNORM_YUV = 0xfd,
        SURFACE_FORMAT_R8G8B8A8_UNORM_SNCK = 0xfe,
        SURFACE_FORMAT_R8G8B8A8_UNORM_NOA = 0xff,
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
        SURFACE_FORMAT_R8G8_SNORM_DX9 = 0x120,
        SURFACE_FORMAT_R16_FLOAT_DX9 = 0x121,
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
        SURFACE_FORMAT_R8B8_UNORM = 0x1a0,
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
        SURFACE_TYPE_SURFTYPE_SCRATCH = 0x6,
        SURFACE_TYPE_SURFTYPE_NULL = 0x7,
    } SURFACE_TYPE;
    typedef enum tagSAMPLE_TAP_DISCARD_DISABLE {
        SAMPLE_TAP_DISCARD_DISABLE_DISABLE = 0x0,
        SAMPLE_TAP_DISCARD_DISABLE_ENABLE = 0x1,
    } SAMPLE_TAP_DISCARD_DISABLE;
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
    typedef enum tagDECOMPRESS_IN_L3 {
        DECOMPRESS_IN_L3_DISABLE = 0x0,
        DECOMPRESS_IN_L3_ENABLE = 0x1,
    } DECOMPRESS_IN_L3;
    typedef enum tagCOHERENCY_TYPE {
        COHERENCY_TYPE_GPU_COHERENT = 0x0, // patched from COHERENCY_TYPE_SINGLE_GPU_COHERENT
        COHERENCY_TYPE_IA_COHERENT = 0x1,  // patched from COHERENCY_TYPE_SYSTEM_COHERENT
        COHERENCY_TYPE_MULTI_GPU_COHERENT = 0x2,
    } COHERENCY_TYPE;
    typedef enum tagL1_CACHE_CONTROL { // patched
        L1_CACHE_CONTROL_WBP = 0x0,
        L1_CACHE_CONTROL_UC = 0x1,
        L1_CACHE_CONTROL_WB = 0x2,
        L1_CACHE_CONTROL_WT = 0x3,
        L1_CACHE_CONTROL_WS = 0x4,
    } L1_CACHE_CONTROL;
    typedef enum tagAUXILIARY_SURFACE_MODE {
        AUXILIARY_SURFACE_MODE_AUX_NONE = 0x0,
        AUXILIARY_SURFACE_MODE_AUX_CCS_D = 0x1,
        AUXILIARY_SURFACE_MODE_AUX_APPEND = 0x2,
        AUXILIARY_SURFACE_MODE_AUX_MCS_LCE = 0x4,
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
    typedef enum tagMEMORY_COMPRESSION_TYPE {
        MEMORY_COMPRESSION_TYPE_MEDIA_COMPRESSION = 0x0,
        MEMORY_COMPRESSION_TYPE_3D_COMPRESSION = 0x1,
    } MEMORY_COMPRESSION_TYPE;
    typedef enum tagCOMPRESSION_FORMAT {
        COMPRESSION_FORMAT_RGBA16_FLOAT = 0x1,
        COMPRESSION_FORMAT_Y210 = 0x2,
        COMPRESSION_FORMAT_YUY2 = 0x3,
        COMPRESSION_FORMAT_Y410_1010102 = 0x4,
        COMPRESSION_FORMAT_Y216 = 0x5,
        COMPRESSION_FORMAT_Y416 = 0x6,
        COMPRESSION_FORMAT_P010 = 0x7,
        COMPRESSION_FORMAT_P016 = 0x8,
        COMPRESSION_FORMAT_AYUV = 0x9,
        COMPRESSION_FORMAT_ARGB_8B = 0xa,
        COMPRESSION_FORMAT_YCRCB_SWAPY = 0xb,
        COMPRESSION_FORMAT_YCRCB_SWAPUV = 0xc,
        COMPRESSION_FORMAT_YCRCB_SWAPUVY = 0xd,
        COMPRESSION_FORMAT_RGB_10B = 0xe,
        COMPRESSION_FORMAT_NV21NV12 = 0xf,
    } COMPRESSION_FORMAT;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MediaBoundaryPixelMode = MEDIA_BOUNDARY_PIXEL_MODE_NORMAL_MODE;
        TheStructure.Common.RenderCacheReadWriteMode = RENDER_CACHE_READ_WRITE_MODE_WRITE_ONLY_CACHE;
        TheStructure.Common.TileMode = TILE_MODE_LINEAR;
        TheStructure.Common.SurfaceHorizontalAlignment = SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_16;
        TheStructure.Common.SurfaceVerticalAlignment = SURFACE_VERTICAL_ALIGNMENT_VALIGN_4;
        TheStructure.Common.SurfaceType = SURFACE_TYPE_SURFTYPE_1D;
        TheStructure.Common.SampleTapDiscardDisable = SAMPLE_TAP_DISCARD_DISABLE_DISABLE;
        TheStructure.Common.NumberOfMultisamples = NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1;
        TheStructure.Common.MultisampledSurfaceStorageFormat = MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS;
        TheStructure.Common.RenderTargetAndSampleUnormRotation = RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_0DEG;
        TheStructure.Common.DecompressInL3 = DECOMPRESS_IN_L3_DISABLE;
        TheStructure.Common.CoherencyType = COHERENCY_TYPE_GPU_COHERENT;
        TheStructure.Common.L1CacheControlCachePolicy = L1_CACHE_CONTROL_WBP;
        TheStructure.Common.MemoryCompressionType = MEMORY_COMPRESSION_TYPE_MEDIA_COMPRESSION;
        TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma = HALF_PITCH_FOR_CHROMA_DISABLE;
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceMode = AUXILIARY_SURFACE_MODE_AUX_NONE;
        TheStructure.Common.DisallowLowQualityFiltering = false;
    }
    static tagRENDER_SURFACE_STATE sInit() {
        RENDER_SURFACE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 16);
        return TheStructure.RawData[index];
    }
    inline void setCubeFaceEnablePositiveZ(const bool value) {
        TheStructure.Common.CubeFaceEnablePositiveZ = value;
    }
    inline bool getCubeFaceEnablePositiveZ() const {
        return TheStructure.Common.CubeFaceEnablePositiveZ;
    }
    inline void setCubeFaceEnableNegativeZ(const bool value) {
        TheStructure.Common.CubeFaceEnableNegativeZ = value;
    }
    inline bool getCubeFaceEnableNegativeZ() const {
        return TheStructure.Common.CubeFaceEnableNegativeZ;
    }
    inline void setCubeFaceEnablePositiveY(const bool value) {
        TheStructure.Common.CubeFaceEnablePositiveY = value;
    }
    inline bool getCubeFaceEnablePositiveY() const {
        return TheStructure.Common.CubeFaceEnablePositiveY;
    }
    inline void setCubeFaceEnableNegativeY(const bool value) {
        TheStructure.Common.CubeFaceEnableNegativeY = value;
    }
    inline bool getCubeFaceEnableNegativeY() const {
        return TheStructure.Common.CubeFaceEnableNegativeY;
    }
    inline void setCubeFaceEnablePositiveX(const bool value) {
        TheStructure.Common.CubeFaceEnablePositiveX = value;
    }
    inline bool getCubeFaceEnablePositiveX() const {
        return TheStructure.Common.CubeFaceEnablePositiveX;
    }
    inline void setCubeFaceEnableNegativeX(const bool value) {
        TheStructure.Common.CubeFaceEnableNegativeX = value;
    }
    inline bool getCubeFaceEnableNegativeX() const {
        return TheStructure.Common.CubeFaceEnableNegativeX;
    }
    inline void setMediaBoundaryPixelMode(const MEDIA_BOUNDARY_PIXEL_MODE value) {
        TheStructure.Common.MediaBoundaryPixelMode = value;
    }
    inline MEDIA_BOUNDARY_PIXEL_MODE getMediaBoundaryPixelMode() const {
        return static_cast<MEDIA_BOUNDARY_PIXEL_MODE>(TheStructure.Common.MediaBoundaryPixelMode);
    }
    inline void setRenderCacheReadWriteMode(const RENDER_CACHE_READ_WRITE_MODE value) {
        TheStructure.Common.RenderCacheReadWriteMode = value;
    }
    inline RENDER_CACHE_READ_WRITE_MODE getRenderCacheReadWriteMode() const {
        return static_cast<RENDER_CACHE_READ_WRITE_MODE>(TheStructure.Common.RenderCacheReadWriteMode);
    }
    inline void setSamplerL2OutOfOrderModeDisable(const bool value) {
        TheStructure.Common.SamplerL2OutOfOrderModeDisable = value;
    }
    inline bool getSamplerL2OutOfOrderModeDisable() const {
        return TheStructure.Common.SamplerL2OutOfOrderModeDisable;
    }
    inline void setVerticalLineStrideOffset(const bool value) {
        TheStructure.Common.VerticalLineStrideOffset = value;
    }
    inline bool getVerticalLineStrideOffset() const {
        return TheStructure.Common.VerticalLineStrideOffset;
    }
    inline void setVerticalLineStride(const bool value) {
        TheStructure.Common.VerticalLineStride = value;
    }
    inline bool getVerticalLineStride() const {
        return TheStructure.Common.VerticalLineStride;
    }
    inline void setTileMode(const TILE_MODE value) {
        TheStructure.Common.TileMode = value;
    }
    inline TILE_MODE getTileMode() const {
        return static_cast<TILE_MODE>(TheStructure.Common.TileMode);
    }
    inline void setSurfaceHorizontalAlignment(const SURFACE_HORIZONTAL_ALIGNMENT value) {
        TheStructure.Common.SurfaceHorizontalAlignment = value;
    }
    inline SURFACE_HORIZONTAL_ALIGNMENT getSurfaceHorizontalAlignment() const {
        return static_cast<SURFACE_HORIZONTAL_ALIGNMENT>(TheStructure.Common.SurfaceHorizontalAlignment);
    }
    inline void setSurfaceVerticalAlignment(const SURFACE_VERTICAL_ALIGNMENT value) {
        TheStructure.Common.SurfaceVerticalAlignment = value;
    }
    inline SURFACE_VERTICAL_ALIGNMENT getSurfaceVerticalAlignment() const {
        return static_cast<SURFACE_VERTICAL_ALIGNMENT>(TheStructure.Common.SurfaceVerticalAlignment);
    }
    inline void setSurfaceFormat(const SURFACE_FORMAT value) {
        TheStructure.Common.SurfaceFormat = value;
    }
    inline SURFACE_FORMAT getSurfaceFormat() const {
        return static_cast<SURFACE_FORMAT>(TheStructure.Common.SurfaceFormat);
    }
    inline void setSurfaceArray(const bool value) {
        TheStructure.Common.SurfaceArray = value;
    }
    inline bool getSurfaceArray() const {
        return TheStructure.Common.SurfaceArray;
    }
    inline void setSurfaceType(const SURFACE_TYPE value) {
        TheStructure.Common.SurfaceType = value;
    }
    inline SURFACE_TYPE getSurfaceType() const {
        return static_cast<SURFACE_TYPE>(TheStructure.Common.SurfaceType);
    }
    typedef enum tagSURFACEQPITCH {
        SURFACEQPITCH_BIT_SHIFT = 0x2,
        SURFACEQPITCH_ALIGN_SIZE = 0x4,
    } SURFACEQPITCH;
    inline void setSurfaceQPitch(const uint32_t value) {
        UNRECOVERABLE_IF((value >> SURFACEQPITCH_BIT_SHIFT) > 0x1ffff);
        TheStructure.Common.SurfaceQpitch = value >> SURFACEQPITCH_BIT_SHIFT;
    }
    inline uint32_t getSurfaceQPitch() const {
        return TheStructure.Common.SurfaceQpitch << SURFACEQPITCH_BIT_SHIFT;
    }
    inline void setSampleTapDiscardDisable(const SAMPLE_TAP_DISCARD_DISABLE value) {
        TheStructure.Common.SampleTapDiscardDisable = value;
    }
    inline SAMPLE_TAP_DISCARD_DISABLE getSampleTapDiscardDisable() const {
        return static_cast<SAMPLE_TAP_DISCARD_DISABLE>(TheStructure.Common.SampleTapDiscardDisable);
    }
    inline void setDoubleFetchDisable(const bool value) {
        TheStructure.Common.DoubleFetchDisable = value;
    }
    inline bool getDoubleFetchDisable() const {
        return TheStructure.Common.DoubleFetchDisable;
    }
    inline void setCornerTexelMode(const bool value) {
        TheStructure.Common.CornerTexelMode = value;
    }
    inline bool getCornerTexelMode() const {
        return TheStructure.Common.CornerTexelMode;
    }
    inline void setBaseMipLevel(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1f);
        TheStructure.Common.BaseMipLevel = value;
    }
    inline uint32_t getBaseMipLevel() const {
        return TheStructure.Common.BaseMipLevel;
    }
    inline void setMemoryObjectControlState(const uint32_t value) { // patched
        TheStructure.Common.MemoryObjectControlStateEncryptedData = value;
        TheStructure.Common.MemoryObjectControlStateIndexToMocsTables = (value >> 1);
    }
    inline uint32_t getMemoryObjectControlState() const { // patched
        uint32_t mocs = TheStructure.Common.MemoryObjectControlStateEncryptedData;
        mocs |= (TheStructure.Common.MemoryObjectControlStateIndexToMocsTables << 1);
        return (mocs);
    }
    inline void setWidth(const uint32_t value) {
        UNRECOVERABLE_IF((value - 1) > 0x3fff);
        TheStructure.Common.Width = value - 1;
    }
    inline uint32_t getWidth() const {
        return TheStructure.Common.Width + 1;
    }
    inline void setHeight(const uint32_t value) {
        UNRECOVERABLE_IF((value - 1) > 0x3fff);
        TheStructure.Common.Height = value - 1;
    }
    inline uint32_t getHeight() const {
        return TheStructure.Common.Height + 1;
    }
    inline void setDepthStencilResource(const bool value) {
        TheStructure.Common.DepthStencilResource = value;
    }
    inline bool getDepthStencilResource() const {
        return TheStructure.Common.DepthStencilResource;
    }
    inline void setSurfacePitch(const uint32_t value) {
        TheStructure.Common.SurfacePitch = value - 1;
    }
    inline uint32_t getSurfacePitch() const {
        return TheStructure.Common.SurfacePitch + 1;
    }
    inline void setDepth(const uint32_t value) {
        UNRECOVERABLE_IF((value - 1) > 0x7ff);
        TheStructure.Common.Depth = value - 1;
    }
    inline uint32_t getDepth() const {
        return TheStructure.Common.Depth + 1;
    }
    inline void setMultisamplePositionPaletteIndex(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7);
        TheStructure.Common.MultisamplePositionPaletteIndex = value;
    }
    inline uint32_t getMultisamplePositionPaletteIndex() const {
        return TheStructure.Common.MultisamplePositionPaletteIndex;
    }
    inline void setNumberOfMultisamples(const NUMBER_OF_MULTISAMPLES value) {
        TheStructure.Common.NumberOfMultisamples = value;
    }
    inline NUMBER_OF_MULTISAMPLES getNumberOfMultisamples() const {
        return static_cast<NUMBER_OF_MULTISAMPLES>(TheStructure.Common.NumberOfMultisamples);
    }
    inline void setMultisampledSurfaceStorageFormat(const MULTISAMPLED_SURFACE_STORAGE_FORMAT value) {
        TheStructure.Common.MultisampledSurfaceStorageFormat = value;
    }
    inline MULTISAMPLED_SURFACE_STORAGE_FORMAT getMultisampledSurfaceStorageFormat() const {
        return static_cast<MULTISAMPLED_SURFACE_STORAGE_FORMAT>(TheStructure.Common.MultisampledSurfaceStorageFormat);
    }
    inline void setRenderTargetViewExtent(const uint32_t value) {
        UNRECOVERABLE_IF((value - 1) > 0x7ff);
        TheStructure.Common.RenderTargetViewExtent = value - 1;
    }
    inline uint32_t getRenderTargetViewExtent() const {
        return TheStructure.Common.RenderTargetViewExtent + 1;
    }
    inline void setMinimumArrayElement(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7ff);
        TheStructure.Common.MinimumArrayElement = value;
    }
    inline uint32_t getMinimumArrayElement() const {
        return TheStructure.Common.MinimumArrayElement;
    }
    inline void setRenderTargetAndSampleUnormRotation(const RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION value) {
        TheStructure.Common.RenderTargetAndSampleUnormRotation = value;
    }
    inline RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION getRenderTargetAndSampleUnormRotation() const {
        return static_cast<RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION>(TheStructure.Common.RenderTargetAndSampleUnormRotation);
    }
    inline void setDecompressInL3(const DECOMPRESS_IN_L3 value) {
        TheStructure.Common.DecompressInL3 = value;
    }
    inline DECOMPRESS_IN_L3 getDecompressInL3() const {
        return static_cast<DECOMPRESS_IN_L3>(TheStructure.Common.DecompressInL3);
    }
    inline void setMIPCountLOD(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.MipCountLod = value;
    }
    inline uint32_t getMIPCountLOD() const {
        return TheStructure.Common.MipCountLod;
    }
    inline void setSurfaceMinLOD(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.SurfaceMinLod = value;
    }
    inline uint32_t getSurfaceMinLOD() const {
        return TheStructure.Common.SurfaceMinLod;
    }
    inline void setMipTailStartLOD(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.MipTailStartLod = value;
    }
    inline uint32_t getMipTailStartLOD() const {
        return TheStructure.Common.MipTailStartLod;
    }
    inline void setCoherencyType(const COHERENCY_TYPE value) {
        TheStructure.Common.CoherencyType = value;
    }
    inline COHERENCY_TYPE getCoherencyType() const {
        return static_cast<COHERENCY_TYPE>(TheStructure.Common.CoherencyType);
    }
    inline void setL1CacheControlCachePolicy(const L1_CACHE_CONTROL value) { // patched
        TheStructure.Common.L1CacheControlCachePolicy = value;
    }
    inline L1_CACHE_CONTROL getL1CacheControlCachePolicy() const { // patched
        return static_cast<L1_CACHE_CONTROL>(TheStructure.Common.L1CacheControlCachePolicy);
    }
    inline void setEwaDisableForCube(const bool value) {
        TheStructure.Common.EwaDisableForCube = value;
    }
    inline bool getEwaDisableForCube() const {
        return TheStructure.Common.EwaDisableForCube;
    }
    typedef enum tagYOFFSET {
        YOFFSET_BIT_SHIFT = 0x2,
        YOFFSET_ALIGN_SIZE = 0x4,
    } YOFFSET;
    inline void setYOffset(const uint32_t value) {
        UNRECOVERABLE_IF((value >> YOFFSET_BIT_SHIFT) > 0x1f);
        TheStructure.Common.YOffset = value >> YOFFSET_BIT_SHIFT;
    }
    inline uint32_t getYOffset() const {
        return TheStructure.Common.YOffset << YOFFSET_BIT_SHIFT;
    }
    typedef enum tagXOFFSET {
        XOFFSET_BIT_SHIFT = 0x2,
        XOFFSET_ALIGN_SIZE = 0x4,
    } XOFFSET;
    inline void setXOffset(const uint32_t value) {
        UNRECOVERABLE_IF((value >> XOFFSET_BIT_SHIFT) > 0x1ff);
        TheStructure.Common.XOffset = value >> XOFFSET_BIT_SHIFT;
    }
    inline uint32_t getXOffset() const {
        return TheStructure.Common.XOffset << XOFFSET_BIT_SHIFT;
    }
    inline void setYuvInterpolationEnable(const bool value) {
        TheStructure.Common.YuvInterpolationEnable = value;
    }
    inline bool getYuvInterpolationEnable() const {
        return TheStructure.Common.YuvInterpolationEnable;
    }
    inline void setResourceMinLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfff);
        TheStructure.Common.ResourceMinLod = value;
    }
    inline uint32_t getResourceMinLod() const {
        return TheStructure.Common.ResourceMinLod;
    }
    inline void setShaderChannelSelectAlpha(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectAlpha = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectAlpha() const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectAlpha);
    }
    inline void setShaderChannelSelectBlue(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectBlue = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectBlue() const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectBlue);
    }
    inline void setShaderChannelSelectGreen(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectGreen = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectGreen() const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectGreen);
    }
    inline void setShaderChannelSelectRed(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectRed = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectRed() const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectRed);
    }
    inline void setMemoryCompressionEnable(const bool value) {
        TheStructure.Common.MemoryCompressionEnable = value;
    }
    inline bool getMemoryCompressionEnable() const {
        return TheStructure.Common.MemoryCompressionEnable;
    }
    inline void setMemoryCompressionType(const MEMORY_COMPRESSION_TYPE value) {
        TheStructure.Common.MemoryCompressionType = value;
    }
    inline MEMORY_COMPRESSION_TYPE getMemoryCompressionType() const {
        return static_cast<MEMORY_COMPRESSION_TYPE>(TheStructure.Common.MemoryCompressionType);
    }
    inline void setSurfaceBaseAddress(const uint64_t value) {
        TheStructure.Common.SurfaceBaseAddress = value;
    }
    inline uint64_t getSurfaceBaseAddress() const {
        return TheStructure.Common.SurfaceBaseAddress;
    }
    inline void setQuiltWidth(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x1fL);
        TheStructure.Common.QuiltWidth = value;
    }
    inline uint64_t getQuiltWidth() const {
        return TheStructure.Common.QuiltWidth;
    }
    inline void setQuiltHeight(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x1fL);
        TheStructure.Common.QuiltHeight = value;
    }
    inline uint64_t getQuiltHeight() const {
        return TheStructure.Common.QuiltHeight;
    }
    inline void setClearValueAddressEnable(const bool value) {
        TheStructure.Common.ClearValueAddressEnable = value;
    }
    inline bool getClearValueAddressEnable() const {
        return TheStructure.Common.ClearValueAddressEnable;
    }
    inline void setProceduralTexture(const bool value) {
        TheStructure.Common.ProceduralTexture = value;
    }
    inline bool getProceduralTexture() const {
        return TheStructure.Common.ProceduralTexture;
    }
    inline void setCompressionFormat(const uint32_t value) {
        TheStructure.Common.CompressionFormat = value;
    }
    inline uint32_t getCompressionFormat() const {
        return TheStructure.Common.CompressionFormat;
    }
    typedef enum tagCLEARADDRESSLOW {
        CLEARADDRESSLOW_BIT_SHIFT = 0x6,
        CLEARADDRESSLOW_ALIGN_SIZE = 0x40,
    } CLEARADDRESSLOW;
    inline void setClearColorAddress(const uint32_t value) { // patched
        TheStructure.Common.ClearAddressLow = value >> CLEARADDRESSLOW_BIT_SHIFT;
    }
    inline uint32_t getClearColorAddress() const { // patched
        return TheStructure.Common.ClearAddressLow << CLEARADDRESSLOW_BIT_SHIFT;
    }
    inline void setClearColorAddressHigh(const uint32_t value) { // patched
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.ClearAddressHigh = value;
    }
    inline uint32_t getClearColorAddressHigh() const { // patched
        return TheStructure.Common.ClearAddressHigh;
    }
    inline void setDisallowLowQualityFiltering(const bool value) { // patched
        UNRECOVERABLE_IF(true);
        TheStructure.Common.DisallowLowQualityFiltering = value;
    }
    inline bool getDisallowLowQualityFiltering() const {
        return TheStructure.Common.DisallowLowQualityFiltering;
    }
    inline void setYOffsetForUOrUvPlane(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure._SurfaceFormatIsPlanar.YOffsetForUOrUvPlane = value;
    }
    inline uint32_t getYOffsetForUOrUvPlane() const {
        return TheStructure._SurfaceFormatIsPlanar.YOffsetForUOrUvPlane;
    }
    inline void setXOffsetForUOrUvPlane(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure._SurfaceFormatIsPlanar.XOffsetForUOrUvPlane = value;
    }
    inline uint32_t getXOffsetForUOrUvPlane() const {
        return TheStructure._SurfaceFormatIsPlanar.XOffsetForUOrUvPlane;
    }
    inline void setHalfPitchForChroma(const HALF_PITCH_FOR_CHROMA value) {
        TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma = value;
    }
    inline HALF_PITCH_FOR_CHROMA getHalfPitchForChroma() const {
        return static_cast<HALF_PITCH_FOR_CHROMA>(TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma);
    }
    inline void setSeparateUvPlaneEnable(const bool value) {
        TheStructure._SurfaceFormatIsPlanar.SeparateUvPlaneEnable = value;
    }
    inline bool getSeparateUvPlaneEnable() const {
        return TheStructure._SurfaceFormatIsPlanar.SeparateUvPlaneEnable;
    }
    inline void setYOffsetForVPlane(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x3fffL);
        TheStructure._SurfaceFormatIsPlanar.YOffsetForVPlane = value;
    }
    inline uint64_t getYOffsetForVPlane() const {
        return TheStructure._SurfaceFormatIsPlanar.YOffsetForVPlane;
    }
    inline void setXOffsetForVPlane(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x3fffL);
        TheStructure._SurfaceFormatIsPlanar.XOffsetForVPlane = value;
    }
    inline uint64_t getXOffsetForVPlane() const {
        return TheStructure._SurfaceFormatIsPlanar.XOffsetForVPlane;
    }
    inline void setAuxiliarySurfaceMode(const AUXILIARY_SURFACE_MODE value) {
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceMode = value;
    }
    inline AUXILIARY_SURFACE_MODE getAuxiliarySurfaceMode() const {
        return static_cast<AUXILIARY_SURFACE_MODE>(TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceMode);
    }
    inline void setAuxiliarySurfacePitch(const uint32_t value) {
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfacePitch = value - 1;
    }
    inline uint32_t getAuxiliarySurfacePitch() const {
        return TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfacePitch + 1;
    }
    typedef enum tagAUXILIARYSURFACEQPITCH {
        AUXILIARYSURFACEQPITCH_BIT_SHIFT = 0x2,
        AUXILIARYSURFACEQPITCH_ALIGN_SIZE = 0x4,
    } AUXILIARYSURFACEQPITCH;
    inline void setAuxiliarySurfaceQPitch(const uint32_t value) {
        UNRECOVERABLE_IF((value >> AUXILIARYSURFACEQPITCH_BIT_SHIFT) > 0x1ffff);
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceQpitch = value >> AUXILIARYSURFACEQPITCH_BIT_SHIFT;
    }
    inline uint32_t getAuxiliarySurfaceQPitch() const {
        return TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceQpitch << AUXILIARYSURFACEQPITCH_BIT_SHIFT;
    }
    typedef enum tagAUXILIARYSURFACEBASEADDRESS {
        AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT = 0xc,
        AUXILIARYSURFACEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } AUXILIARYSURFACEBASEADDRESS;
    inline void setAuxiliarySurfaceBaseAddress(const uint64_t value) {
        TheStructure._SurfaceFormatIsnotPlanarAndMemoryCompressionEnableIs0.AuxiliarySurfaceBaseAddress = value >> AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getAuxiliarySurfaceBaseAddress() const {
        return TheStructure._SurfaceFormatIsnotPlanarAndMemoryCompressionEnableIs0.AuxiliarySurfaceBaseAddress << AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT;
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
            uint32_t ExtendedIndirectStatePointer : BITFIELD_RANGE(24, 31);
            uint32_t TczAddressControlMode : BITFIELD_RANGE(0, 2);
            uint32_t TcyAddressControlMode : BITFIELD_RANGE(3, 5);
            uint32_t TcxAddressControlMode : BITFIELD_RANGE(6, 8);
            uint32_t ReductionTypeEnable : BITFIELD_RANGE(9, 9);
            uint32_t NonNormalizedCoordinateEnable : BITFIELD_RANGE(10, 10);
            uint32_t TrilinearFilterQuality : BITFIELD_RANGE(11, 12);
            uint32_t RAddressMinFilterRoundingEnable : BITFIELD_RANGE(13, 13);
            uint32_t RAddressMagFilterRoundingEnable : BITFIELD_RANGE(14, 14);
            uint32_t VAddressMinFilterRoundingEnable : BITFIELD_RANGE(15, 15);
            uint32_t VAddressMagFilterRoundingEnable : BITFIELD_RANGE(16, 16);
            uint32_t UAddressMinFilterRoundingEnable : BITFIELD_RANGE(17, 17);
            uint32_t UAddressMagFilterRoundingEnable : BITFIELD_RANGE(18, 18);
            uint32_t MaximumAnisotropy : BITFIELD_RANGE(19, 21);
            uint32_t ReductionType : BITFIELD_RANGE(22, 23);
            uint32_t AllowLowQualityLodCalculation : BITFIELD_RANGE(24, 24);
            uint32_t Reserved_121 : BITFIELD_RANGE(25, 25);
            uint32_t LowQualityFilter : BITFIELD_RANGE(26, 26);
            uint32_t Reserved_123 : BITFIELD_RANGE(27, 31);
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
        TEXTURE_BORDER_COLOR_MODE_OGL = 0x0,
        TEXTURE_BORDER_COLOR_MODE_8BIT = 0x1,
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
        TRILINEAR_FILTER_QUALITY_TRIQUAL_HIGHMAG_CLAMP_MIPFILTER = 0x1,
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
    typedef enum tagLOW_QUALITY_FILTER {
        LOW_QUALITY_FILTER_DISABLE = 0x0,
        LOW_QUALITY_FILTER_ENABLE = 0x1,
    } LOW_QUALITY_FILTER;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.LodAlgorithm = LOD_ALGORITHM_LEGACY;
        TheStructure.Common.MinModeFilter = MIN_MODE_FILTER_NEAREST;
        TheStructure.Common.MagModeFilter = MAG_MODE_FILTER_NEAREST;
        TheStructure.Common.MipModeFilter = MIP_MODE_FILTER_NONE;
        TheStructure.Common.CoarseLodQualityMode = COARSE_LOD_QUALITY_MODE_DISABLED;
        TheStructure.Common.LodPreclampMode = LOD_PRECLAMP_MODE_NONE;
        TheStructure.Common.TextureBorderColorMode = TEXTURE_BORDER_COLOR_MODE_OGL;
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
        TheStructure.Common.LowQualityFilter = LOW_QUALITY_FILTER_DISABLE;
    }
    static tagSAMPLER_STATE sInit() {
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
    inline LOD_ALGORITHM getLodAlgorithm() const {
        return static_cast<LOD_ALGORITHM>(TheStructure.Common.LodAlgorithm);
    }
    inline void setTextureLodBias(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0x3ffe);
        TheStructure.Common.TextureLodBias = value;
    }
    inline uint32_t getTextureLodBias() const {
        return TheStructure.Common.TextureLodBias;
    }
    inline void setMinModeFilter(const MIN_MODE_FILTER value) {
        TheStructure.Common.MinModeFilter = value;
    }
    inline MIN_MODE_FILTER getMinModeFilter() const {
        return static_cast<MIN_MODE_FILTER>(TheStructure.Common.MinModeFilter);
    }
    inline void setMagModeFilter(const MAG_MODE_FILTER value) {
        TheStructure.Common.MagModeFilter = value;
    }
    inline MAG_MODE_FILTER getMagModeFilter() const {
        return static_cast<MAG_MODE_FILTER>(TheStructure.Common.MagModeFilter);
    }
    inline void setMipModeFilter(const MIP_MODE_FILTER value) {
        TheStructure.Common.MipModeFilter = value;
    }
    inline MIP_MODE_FILTER getMipModeFilter() const {
        return static_cast<MIP_MODE_FILTER>(TheStructure.Common.MipModeFilter);
    }
    inline void setCoarseLodQualityMode(const COARSE_LOD_QUALITY_MODE value) {
        TheStructure.Common.CoarseLodQualityMode = value;
    }
    inline COARSE_LOD_QUALITY_MODE getCoarseLodQualityMode() const {
        return static_cast<COARSE_LOD_QUALITY_MODE>(TheStructure.Common.CoarseLodQualityMode);
    }
    inline void setLodPreclampMode(const LOD_PRECLAMP_MODE value) {
        TheStructure.Common.LodPreclampMode = value;
    }
    inline LOD_PRECLAMP_MODE getLodPreclampMode() const {
        return static_cast<LOD_PRECLAMP_MODE>(TheStructure.Common.LodPreclampMode);
    }
    inline void setTextureBorderColorMode(const TEXTURE_BORDER_COLOR_MODE value) {
        TheStructure.Common.TextureBorderColorMode = value;
    }
    inline TEXTURE_BORDER_COLOR_MODE getTextureBorderColorMode() const {
        return static_cast<TEXTURE_BORDER_COLOR_MODE>(TheStructure.Common.TextureBorderColorMode);
    }
    inline void setCpsLodCompensationEnable(const bool value) {
        TheStructure.Common.CpsLodCompensationEnable = value;
    }
    inline bool getCpsLodCompensationEnable() const {
        return TheStructure.Common.CpsLodCompensationEnable;
    }
    inline void setSamplerDisable(const bool value) {
        TheStructure.Common.SamplerDisable = value;
    }
    inline bool getSamplerDisable() const {
        return TheStructure.Common.SamplerDisable;
    }
    inline void setCubeSurfaceControlMode(const CUBE_SURFACE_CONTROL_MODE value) {
        TheStructure.Common.CubeSurfaceControlMode = value;
    }
    inline CUBE_SURFACE_CONTROL_MODE getCubeSurfaceControlMode() const {
        return static_cast<CUBE_SURFACE_CONTROL_MODE>(TheStructure.Common.CubeSurfaceControlMode);
    }
    inline void setShadowFunction(const SHADOW_FUNCTION value) {
        TheStructure.Common.ShadowFunction = value;
    }
    inline SHADOW_FUNCTION getShadowFunction() const {
        return static_cast<SHADOW_FUNCTION>(TheStructure.Common.ShadowFunction);
    }
    inline void setChromakeyMode(const CHROMAKEY_MODE value) {
        TheStructure.Common.ChromakeyMode = value;
    }
    inline CHROMAKEY_MODE getChromakeyMode() const {
        return static_cast<CHROMAKEY_MODE>(TheStructure.Common.ChromakeyMode);
    }
    inline void setChromakeyIndex(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0x60);
        TheStructure.Common.ChromakeyIndex = value;
    }
    inline uint32_t getChromakeyIndex() const {
        return TheStructure.Common.ChromakeyIndex;
    }
    inline void setChromakeyEnable(const bool value) {
        TheStructure.Common.ChromakeyEnable = value;
    }
    inline bool getChromakeyEnable() const {
        return TheStructure.Common.ChromakeyEnable;
    }
    inline void setMaxLod(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0xfff00);
        TheStructure.Common.MaxLod = value;
    }
    inline uint32_t getMaxLod() const {
        return TheStructure.Common.MaxLod;
    }
    inline void setMinLod(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0xfff00000L);
        TheStructure.Common.MinLod = value;
    }
    inline uint32_t getMinLod() const {
        return TheStructure.Common.MinLod;
    }
    inline void setLodClampMagnificationMode(const LOD_CLAMP_MAGNIFICATION_MODE value) {
        TheStructure.Common.LodClampMagnificationMode = value;
    }
    inline LOD_CLAMP_MAGNIFICATION_MODE getLodClampMagnificationMode() const {
        return static_cast<LOD_CLAMP_MAGNIFICATION_MODE>(TheStructure.Common.LodClampMagnificationMode);
    }
    inline void setSrgbDecode(const SRGB_DECODE value) {
        TheStructure.Common.SrgbDecode = value;
    }
    inline SRGB_DECODE getSrgbDecode() const {
        return static_cast<SRGB_DECODE>(TheStructure.Common.SrgbDecode);
    }
    inline void setReturnFilterWeightForNullTexels(const RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS value) {
        TheStructure.Common.ReturnFilterWeightForNullTexels = value;
    }
    inline RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS getReturnFilterWeightForNullTexels() const {
        return static_cast<RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS>(TheStructure.Common.ReturnFilterWeightForNullTexels);
    }
    inline void setReturnFilterWeightForBorderTexels(const RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS value) {
        TheStructure.Common.ReturnFilterWeightForBorderTexels = value;
    }
    inline RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS getReturnFilterWeightForBorderTexels() const {
        return static_cast<RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS>(TheStructure.Common.ReturnFilterWeightForBorderTexels);
    }
    typedef enum tagINDIRECTSTATEPOINTER {
        INDIRECTSTATEPOINTER_BIT_SHIFT = 0x6,
        INDIRECTSTATEPOINTER_ALIGN_SIZE = 0x40,
    } INDIRECTSTATEPOINTER;
    inline void setIndirectStatePointer(const uint32_t value) {
        UNRECOVERABLE_IF(value & 0x3f);
        TheStructure.Common.IndirectStatePointer = static_cast<uint32_t>(value & 0xffffc0) >> INDIRECTSTATEPOINTER_BIT_SHIFT;
        setExtendedIndirectStatePointer(value);
    }
    inline void setExtendedIndirectStatePointer(const uint32_t value) {
        TheStructure.Common.ExtendedIndirectStatePointer = static_cast<uint32_t>(value & 0xff000000) >> 24;
    }
    inline uint64_t getIndirectStatePointer() const {
        return (TheStructure.Common.ExtendedIndirectStatePointer << 24) | (TheStructure.Common.IndirectStatePointer << INDIRECTSTATEPOINTER_BIT_SHIFT);
    }
    inline void setTczAddressControlMode(const TEXTURE_COORDINATE_MODE value) {
        TheStructure.Common.TczAddressControlMode = value;
    }
    inline TEXTURE_COORDINATE_MODE getTczAddressControlMode() const {
        return static_cast<TEXTURE_COORDINATE_MODE>(TheStructure.Common.TczAddressControlMode);
    }
    inline void setTcyAddressControlMode(const TEXTURE_COORDINATE_MODE value) {
        TheStructure.Common.TcyAddressControlMode = value;
    }
    inline TEXTURE_COORDINATE_MODE getTcyAddressControlMode() const {
        return static_cast<TEXTURE_COORDINATE_MODE>(TheStructure.Common.TcyAddressControlMode);
    }
    inline void setTcxAddressControlMode(const TEXTURE_COORDINATE_MODE value) {
        TheStructure.Common.TcxAddressControlMode = value;
    }
    inline TEXTURE_COORDINATE_MODE getTcxAddressControlMode() const {
        return static_cast<TEXTURE_COORDINATE_MODE>(TheStructure.Common.TcxAddressControlMode);
    }
    inline void setReductionTypeEnable(const bool value) {
        TheStructure.Common.ReductionTypeEnable = value;
    }
    inline bool getReductionTypeEnable() const {
        return TheStructure.Common.ReductionTypeEnable;
    }
    inline void setNonNormalizedCoordinateEnable(const bool value) {
        TheStructure.Common.NonNormalizedCoordinateEnable = value;
    }
    inline bool getNonNormalizedCoordinateEnable() const {
        return TheStructure.Common.NonNormalizedCoordinateEnable;
    }
    inline void setTrilinearFilterQuality(const TRILINEAR_FILTER_QUALITY value) {
        TheStructure.Common.TrilinearFilterQuality = value;
    }
    inline TRILINEAR_FILTER_QUALITY getTrilinearFilterQuality() const {
        return static_cast<TRILINEAR_FILTER_QUALITY>(TheStructure.Common.TrilinearFilterQuality);
    }
    inline void setRAddressMinFilterRoundingEnable(const bool value) {
        TheStructure.Common.RAddressMinFilterRoundingEnable = value;
    }
    inline bool getRAddressMinFilterRoundingEnable() const {
        return TheStructure.Common.RAddressMinFilterRoundingEnable;
    }
    inline void setRAddressMagFilterRoundingEnable(const bool value) {
        TheStructure.Common.RAddressMagFilterRoundingEnable = value;
    }
    inline bool getRAddressMagFilterRoundingEnable() const {
        return TheStructure.Common.RAddressMagFilterRoundingEnable;
    }
    inline void setVAddressMinFilterRoundingEnable(const bool value) {
        TheStructure.Common.VAddressMinFilterRoundingEnable = value;
    }
    inline bool getVAddressMinFilterRoundingEnable() const {
        return TheStructure.Common.VAddressMinFilterRoundingEnable;
    }
    inline void setVAddressMagFilterRoundingEnable(const bool value) {
        TheStructure.Common.VAddressMagFilterRoundingEnable = value;
    }
    inline bool getVAddressMagFilterRoundingEnable() const {
        return TheStructure.Common.VAddressMagFilterRoundingEnable;
    }
    inline void setUAddressMinFilterRoundingEnable(const bool value) {
        TheStructure.Common.UAddressMinFilterRoundingEnable = value;
    }
    inline bool getUAddressMinFilterRoundingEnable() const {
        return TheStructure.Common.UAddressMinFilterRoundingEnable;
    }
    inline void setUAddressMagFilterRoundingEnable(const bool value) {
        TheStructure.Common.UAddressMagFilterRoundingEnable = value;
    }
    inline bool getUAddressMagFilterRoundingEnable() const {
        return TheStructure.Common.UAddressMagFilterRoundingEnable;
    }
    inline void setMaximumAnisotropy(const MAXIMUM_ANISOTROPY value) {
        TheStructure.Common.MaximumAnisotropy = value;
    }
    inline MAXIMUM_ANISOTROPY getMaximumAnisotropy() const {
        return static_cast<MAXIMUM_ANISOTROPY>(TheStructure.Common.MaximumAnisotropy);
    }
    inline void setReductionType(const REDUCTION_TYPE value) {
        TheStructure.Common.ReductionType = value;
    }
    inline REDUCTION_TYPE getReductionType() const {
        return static_cast<REDUCTION_TYPE>(TheStructure.Common.ReductionType);
    }
    inline void setLowQualityFilter(const LOW_QUALITY_FILTER value) {
        TheStructure.Common.LowQualityFilter = value;
    }
    inline LOW_QUALITY_FILTER getLowQualityFilter() const {
        return static_cast<LOW_QUALITY_FILTER>(TheStructure.Common.LowQualityFilter);
    }
    inline void setAllowLowQualityLodCalculation(const bool value) {
        TheStructure.Common.AllowLowQualityLodCalculation = value;
    }
    inline bool getAllowLowQualityLodCalculation() const {
        return TheStructure.Common.AllowLowQualityLodCalculation;
    }
} SAMPLER_STATE;
STATIC_ASSERT(16 == sizeof(SAMPLER_STATE));

typedef struct tagSTATE_BASE_ADDRESS {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1-2
            uint64_t GeneralStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 3);
            uint64_t GeneralStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t GeneralStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_43 : BITFIELD_RANGE(11, 11);
            uint64_t GeneralStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 3
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 12);
            uint32_t EnableMemoryCompressionForAllStatelessAccesses : BITFIELD_RANGE(13, 13);
            uint32_t Reserved_110 : BITFIELD_RANGE(14, 15);
            uint32_t StatelessDataPortAccessMemoryObjectControlState_Reserved : BITFIELD_RANGE(16, 16);
            uint32_t StatelessDataPortAccessMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(17, 22);
            uint32_t L1CachePolicyL1CacheControl : BITFIELD_RANGE(23, 25);
            uint32_t Reserved_119 : BITFIELD_RANGE(26, 31);
            // DWORD 4-5
            uint64_t SurfaceStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_129 : BITFIELD_RANGE(1, 3);
            uint64_t SurfaceStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t SurfaceStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_139 : BITFIELD_RANGE(11, 11);
            uint64_t SurfaceStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 6-7
            uint64_t DynamicStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_193 : BITFIELD_RANGE(1, 3);
            uint64_t DynamicStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t DynamicStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_203 : BITFIELD_RANGE(11, 11);
            uint64_t DynamicStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 8-9
            uint64_t Reserved8 : BITFIELD_RANGE(0, 63);
            // DWORD 10-11
            uint64_t InstructionBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_321 : BITFIELD_RANGE(1, 3);
            uint64_t InstructionMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t InstructionMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_331 : BITFIELD_RANGE(11, 11);
            uint64_t InstructionBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 12
            uint32_t GeneralStateBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_385 : BITFIELD_RANGE(1, 11);
            uint32_t GeneralStateBufferSize : BITFIELD_RANGE(12, 31);
            // DWORD 13
            uint32_t DynamicStateBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_417 : BITFIELD_RANGE(1, 11);
            uint32_t DynamicStateBufferSize : BITFIELD_RANGE(12, 31);
            // DWORD 14
            uint32_t IndirectObjectBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_449 : BITFIELD_RANGE(1, 11);
            uint32_t IndirectObjectBufferSize : BITFIELD_RANGE(12, 31);
            // DWORD 15
            uint32_t InstructionBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_481 : BITFIELD_RANGE(1, 11);
            uint32_t InstructionBufferSize : BITFIELD_RANGE(12, 31);
            // DWORD 16-17
            uint64_t BindlessSurfaceStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_513 : BITFIELD_RANGE(1, 3);
            uint64_t BindlessSurfaceStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t BindlessSurfaceStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_523 : BITFIELD_RANGE(11, 11);
            uint64_t BindlessSurfaceStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 18
            uint32_t BindlessSurfaceStateSize;
            // DWORD 19-20
            uint64_t BindlessSamplerStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_609 : BITFIELD_RANGE(1, 3);
            uint64_t BindlessSamplerStateMemoryObjectControlState_Reserved : BITFIELD_RANGE(4, 4);
            uint64_t BindlessSamplerStateMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_619 : BITFIELD_RANGE(11, 11);
            uint64_t BindlessSamplerStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 21
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
    typedef enum tagENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES {
        ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED = 0x0,
        ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_ENABLED = 0x1,
    } ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES;
    typedef enum tagL1_CACHE_CONTROL {
        L1_CACHE_CONTROL_WBP = 0x0,
        L1_CACHE_CONTROL_UC = 0x1,
        L1_CACHE_CONTROL_WB = 0x2,
        L1_CACHE_CONTROL_WT = 0x3,
        L1_CACHE_CONTROL_WS = 0x4,
    } L1_CACHE_CONTROL;
    typedef enum tagPATCH_CONSTANTS {
        GENERALSTATEBASEADDRESS_BYTEOFFSET = 0x4,
        GENERALSTATEBASEADDRESS_INDEX = 0x1,
        SURFACESTATEBASEADDRESS_BYTEOFFSET = 0x10,
        SURFACESTATEBASEADDRESS_INDEX = 0x4,
        DYNAMICSTATEBASEADDRESS_BYTEOFFSET = 0x18,
        DYNAMICSTATEBASEADDRESS_INDEX = 0x6,
        INSTRUCTIONBASEADDRESS_BYTEOFFSET = 0x28,
        INSTRUCTIONBASEADDRESS_INDEX = 0xa,
        BINDLESSSURFACESTATEBASEADDRESS_BYTEOFFSET = 0x40,
        BINDLESSSURFACESTATEBASEADDRESS_INDEX = 0x10,
        BINDLESSSAMPLERSTATEBASEADDRESS_BYTEOFFSET = 0x4c,
        BINDLESSSAMPLERSTATEBASEADDRESS_INDEX = 0x13,
    } PATCH_CONSTANTS;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_BASE_ADDRESS;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.EnableMemoryCompressionForAllStatelessAccesses = ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED;
        TheStructure.Common.L1CachePolicyL1CacheControl = L1_CACHE_CONTROL_WBP;
    }
    static tagSTATE_BASE_ADDRESS sInit() {
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
    inline bool getGeneralStateBaseAddressModifyEnable() const {
        return (TheStructure.Common.GeneralStateBaseAddressModifyEnable);
    }
    inline void setGeneralStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.GeneralStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getGeneralStateMemoryObjectControlStateReserved() const {
        return (TheStructure.Common.GeneralStateMemoryObjectControlState_Reserved);
    }
    inline void setGeneralStateMemoryObjectControlState(const uint64_t value) {
        TheStructure.Common.GeneralStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getGeneralStateMemoryObjectControlState() const {
        return (TheStructure.Common.GeneralStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagGENERALSTATEBASEADDRESS {
        GENERALSTATEBASEADDRESS_BIT_SHIFT = 0xc,
        GENERALSTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } GENERALSTATEBASEADDRESS;
    inline void setGeneralStateBaseAddress(const uint64_t value) {
        TheStructure.Common.GeneralStateBaseAddress = value >> GENERALSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getGeneralStateBaseAddress() const {
        return (TheStructure.Common.GeneralStateBaseAddress << GENERALSTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setEnableMemoryCompressionForAllStatelessAccesses(const ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES value) {
        TheStructure.Common.EnableMemoryCompressionForAllStatelessAccesses = value;
    }
    inline ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES getEnableMemoryCompressionForAllStatelessAccesses() const {
        return static_cast<ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES>(TheStructure.Common.EnableMemoryCompressionForAllStatelessAccesses);
    }
    inline void setStatelessDataPortAccessMemoryObjectControlStateReserved(const uint32_t value) {
        TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_Reserved = value;
    }
    inline uint32_t getStatelessDataPortAccessMemoryObjectControlStateReserved() const {
        return (TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_Reserved);
    }
    inline void setL1CacheControlCachePolicy(const L1_CACHE_CONTROL value) {
        TheStructure.Common.L1CachePolicyL1CacheControl = value;
    }
    inline L1_CACHE_CONTROL getL1CacheControlCachePolicy() const {
        return static_cast<L1_CACHE_CONTROL>(TheStructure.Common.L1CachePolicyL1CacheControl);
    }
    inline void setStatelessDataPortAccessMemoryObjectControlState(const uint32_t value) {
        TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_Reserved = value;
        TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_IndexToMocsTables = (value >> 1);
    }
    inline uint32_t getStatelessDataPortAccessMemoryObjectControlState() const {
        uint32_t mocs = TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_Reserved;
        mocs |= (TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_IndexToMocsTables << 1);
        return (mocs);
    }
    inline void setSurfaceStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.SurfaceStateBaseAddressModifyEnable = value;
    }
    inline bool getSurfaceStateBaseAddressModifyEnable() const {
        return (TheStructure.Common.SurfaceStateBaseAddressModifyEnable);
    }
    inline void setSurfaceStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.SurfaceStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getSurfaceStateMemoryObjectControlStateReserved() const {
        return (TheStructure.Common.SurfaceStateMemoryObjectControlState_Reserved);
    }
    inline void setSurfaceStateMemoryObjectControlState(const uint64_t value) {
        TheStructure.Common.SurfaceStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getSurfaceStateMemoryObjectControlState() const {
        return (TheStructure.Common.SurfaceStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagSURFACESTATEBASEADDRESS {
        SURFACESTATEBASEADDRESS_BIT_SHIFT = 0xc,
        SURFACESTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } SURFACESTATEBASEADDRESS;
    inline void setSurfaceStateBaseAddress(const uint64_t value) {
        TheStructure.Common.SurfaceStateBaseAddress = value >> SURFACESTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getSurfaceStateBaseAddress() const {
        return (TheStructure.Common.SurfaceStateBaseAddress << SURFACESTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setDynamicStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.DynamicStateBaseAddressModifyEnable = value;
    }
    inline bool getDynamicStateBaseAddressModifyEnable() const {
        return (TheStructure.Common.DynamicStateBaseAddressModifyEnable);
    }
    inline void setDynamicStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.DynamicStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getDynamicStateMemoryObjectControlStateReserved() const {
        return (TheStructure.Common.DynamicStateMemoryObjectControlState_Reserved);
    }
    inline void setDynamicStateMemoryObjectControlState(const uint64_t value) {
        TheStructure.Common.DynamicStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getDynamicStateMemoryObjectControlState() const {
        return (TheStructure.Common.DynamicStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagDYNAMICSTATEBASEADDRESS {
        DYNAMICSTATEBASEADDRESS_BIT_SHIFT = 0xc,
        DYNAMICSTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } DYNAMICSTATEBASEADDRESS;
    inline void setDynamicStateBaseAddress(const uint64_t value) {
        TheStructure.Common.DynamicStateBaseAddress = value >> DYNAMICSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getDynamicStateBaseAddress() const {
        return (TheStructure.Common.DynamicStateBaseAddress << DYNAMICSTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setInstructionBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.InstructionBaseAddressModifyEnable = value;
    }
    inline bool getInstructionBaseAddressModifyEnable() const {
        return (TheStructure.Common.InstructionBaseAddressModifyEnable);
    }
    inline void setInstructionMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.InstructionMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getInstructionMemoryObjectControlStateReserved() const {
        return (TheStructure.Common.InstructionMemoryObjectControlState_Reserved);
    }
    inline void setInstructionMemoryObjectControlState(const uint32_t value) {
        uint64_t val = static_cast<uint64_t>(value);
        TheStructure.Common.InstructionMemoryObjectControlState_Reserved = val;
        TheStructure.Common.InstructionMemoryObjectControlState_IndexToMocsTables = (val >> 1);
    }
    inline uint32_t getInstructionMemoryObjectControlState() const {
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
    inline uint64_t getInstructionBaseAddress() const {
        return (TheStructure.Common.InstructionBaseAddress << INSTRUCTIONBASEADDRESS_BIT_SHIFT);
    }
    inline void setGeneralStateBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.GeneralStateBufferSizeModifyEnable = value;
    }
    inline bool getGeneralStateBufferSizeModifyEnable() const {
        return (TheStructure.Common.GeneralStateBufferSizeModifyEnable);
    }
    inline void setGeneralStateBufferSize(const uint32_t value) {
        TheStructure.Common.GeneralStateBufferSize = value;
    }
    inline uint32_t getGeneralStateBufferSize() const {
        return (TheStructure.Common.GeneralStateBufferSize);
    }
    inline void setDynamicStateBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.DynamicStateBufferSizeModifyEnable = value;
    }
    inline bool getDynamicStateBufferSizeModifyEnable() const {
        return (TheStructure.Common.DynamicStateBufferSizeModifyEnable);
    }
    inline void setDynamicStateBufferSize(const uint32_t value) {
        TheStructure.Common.DynamicStateBufferSize = value;
    }
    inline uint32_t getDynamicStateBufferSize() const {
        return (TheStructure.Common.DynamicStateBufferSize);
    }
    inline void setIndirectObjectBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.IndirectObjectBufferSizeModifyEnable = value;
    }
    inline bool getIndirectObjectBufferSizeModifyEnable() const {
        return (TheStructure.Common.IndirectObjectBufferSizeModifyEnable);
    }
    inline void setIndirectObjectBufferSize(const uint32_t value) {
        TheStructure.Common.IndirectObjectBufferSize = value;
    }
    inline uint32_t getIndirectObjectBufferSize() const {
        return (TheStructure.Common.IndirectObjectBufferSize);
    }
    inline void setInstructionBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.InstructionBufferSizeModifyEnable = value;
    }
    inline bool getInstructionBufferSizeModifyEnable() const {
        return (TheStructure.Common.InstructionBufferSizeModifyEnable);
    }
    inline void setInstructionBufferSize(const uint32_t value) {
        TheStructure.Common.InstructionBufferSize = value;
    }
    inline uint32_t getInstructionBufferSize() const {
        return (TheStructure.Common.InstructionBufferSize);
    }
    inline void setBindlessSurfaceStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.BindlessSurfaceStateBaseAddressModifyEnable = value;
    }
    inline bool getBindlessSurfaceStateBaseAddressModifyEnable() const {
        return (TheStructure.Common.BindlessSurfaceStateBaseAddressModifyEnable);
    }
    inline void setBindlessSurfaceStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.BindlessSurfaceStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getBindlessSurfaceStateMemoryObjectControlStateReserved() const {
        return (TheStructure.Common.BindlessSurfaceStateMemoryObjectControlState_Reserved);
    }
    inline void setBindlessSurfaceStateMemoryObjectControlState(const uint64_t value) {
        TheStructure.Common.BindlessSurfaceStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getBindlessSurfaceStateMemoryObjectControlState() const {
        return (TheStructure.Common.BindlessSurfaceStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagBINDLESSSURFACESTATEBASEADDRESS {
        BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT = 0xc,
        BINDLESSSURFACESTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } BINDLESSSURFACESTATEBASEADDRESS;
    inline void setBindlessSurfaceStateBaseAddress(const uint64_t value) {
        TheStructure.Common.BindlessSurfaceStateBaseAddress = value >> BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBindlessSurfaceStateBaseAddress() const {
        return (TheStructure.Common.BindlessSurfaceStateBaseAddress << BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setBindlessSurfaceStateSize(const uint32_t value) {
        TheStructure.Common.BindlessSurfaceStateSize = value;
    }
    inline uint32_t getBindlessSurfaceStateSize() const {
        return TheStructure.Common.BindlessSurfaceStateSize;
    }
    inline void setBindlessSamplerStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.BindlessSamplerStateBaseAddressModifyEnable = value;
    }
    inline bool getBindlessSamplerStateBaseAddressModifyEnable() const {
        return (TheStructure.Common.BindlessSamplerStateBaseAddressModifyEnable);
    }
    inline void setBindlessSamplerStateMemoryObjectControlStateReserved(const uint64_t value) {
        TheStructure.Common.BindlessSamplerStateMemoryObjectControlState_Reserved = value;
    }
    inline uint64_t getBindlessSamplerStateMemoryObjectControlStateReserved() const {
        return (TheStructure.Common.BindlessSamplerStateMemoryObjectControlState_Reserved);
    }
    inline void setBindlessSamplerStateMemoryObjectControlState(const uint64_t value) {
        TheStructure.Common.BindlessSamplerStateMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint64_t getBindlessSamplerStateMemoryObjectControlState() const {
        return (TheStructure.Common.BindlessSamplerStateMemoryObjectControlState_IndexToMocsTables << 1);
    }
    typedef enum tagBINDLESSSAMPLERSTATEBASEADDRESS {
        BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT = 0xc,
        BINDLESSSAMPLERSTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } BINDLESSSAMPLERSTATEBASEADDRESS;
    inline void setBindlessSamplerStateBaseAddress(const uint64_t value) {
        TheStructure.Common.BindlessSamplerStateBaseAddress = value >> BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBindlessSamplerStateBaseAddress() const {
        return (TheStructure.Common.BindlessSamplerStateBaseAddress << BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT);
    }
    inline void setBindlessSamplerStateBufferSize(const uint32_t value) {
        TheStructure.Common.BindlessSamplerStateBufferSize = value;
    }
    inline uint32_t getBindlessSamplerStateBufferSize() const {
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_REPORT_PERF_COUNT sInit() {
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
    inline bool getUseGlobalGtt() const {
        return (TheStructure.Common.UseGlobalGtt);
    }
    inline void setCoreModeEnable(const uint64_t value) {
        TheStructure.Common.CoreModeEnable = value;
    }
    inline uint64_t getCoreModeEnable() const {
        return (TheStructure.Common.CoreModeEnable);
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x6,
        MEMORYADDRESS_ALIGN_SIZE = 0x40,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress() const {
        return (TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT);
    }
    inline void setReportId(const uint32_t value) {
        TheStructure.Common.ReportId = value;
    }
    inline uint32_t getReportId() const {
        return (TheStructure.Common.ReportId);
    }
} MI_REPORT_PERF_COUNT;
STATIC_ASSERT(16 == sizeof(MI_REPORT_PERF_COUNT));

struct MI_USER_INTERRUPT {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_USER_INTERRUPT;
    }
    static MI_USER_INTERRUPT sInit() {
        MI_USER_INTERRUPT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        return TheStructure.RawData[index];
    }
};
STATIC_ASSERT(4 == sizeof(MI_USER_INTERRUPT));

typedef struct tagMI_SET_PREDICATE {
    union tagTheStructure {
        struct tagCommon {
            uint32_t PredicateEnable : BITFIELD_RANGE(0, 3);
            uint32_t PredicateEnableWparid : BITFIELD_RANGE(4, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagPREDICATE_ENABLE {
        PREDICATE_ENABLE_PREDICATE_DISABLE = 0x0,
        PREDICATE_ENABLE_PREDICATE_ON_CLEAR = 0x1,
        PREDICATE_ENABLE_PREDICATE_ON_SET = 0x2,
        PREDICATE_ENABLE_NOOP_ALWAYS = 0xf,
    } PREDICATE_ENABLE;
    typedef enum tagPREDICATE_ENABLE_WPARID {
        PREDICATE_ENABLE_WPARID_NOOP_NEVER = 0x0,
        PREDICATE_ENABLE_WPARID_NOOP_ON_ZERO_VALUE = 0x1,
        PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE = 0x2,
    } PREDICATE_ENABLE_WPARID;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_SET_PREDICATE = 0x1,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.PredicateEnable = PREDICATE_ENABLE_PREDICATE_DISABLE;
        TheStructure.Common.PredicateEnableWparid = PREDICATE_ENABLE_WPARID_NOOP_NEVER;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_SET_PREDICATE;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_SET_PREDICATE sInit() {
        MI_SET_PREDICATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setPredicateEnable(const PREDICATE_ENABLE value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline PREDICATE_ENABLE getPredicateEnable() const {
        return static_cast<PREDICATE_ENABLE>(TheStructure.Common.PredicateEnable);
    }
    inline void setPredicateEnableWparid(const PREDICATE_ENABLE_WPARID value) {
        TheStructure.Common.PredicateEnableWparid = value;
    }
    inline PREDICATE_ENABLE_WPARID getPredicateEnableWparid() const {
        return static_cast<PREDICATE_ENABLE_WPARID>(TheStructure.Common.PredicateEnableWparid);
    }
} MI_SET_PREDICATE;
STATIC_ASSERT(4 == sizeof(MI_SET_PREDICATE));

typedef struct tagMI_CONDITIONAL_BATCH_BUFFER_END {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 11);
            uint32_t CompareOperation : BITFIELD_RANGE(12, 14);
            uint32_t PredicateEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 17);
            uint32_t EndCurrentBatchBufferLevel : BITFIELD_RANGE(18, 18);
            uint32_t CompareMaskMode : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 20);
            uint32_t CompareSemaphore : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t CompareDataDword;
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 2);
            uint64_t CompareAddress : BITFIELD_RANGE(3, 47);
            uint64_t CompareAddressReserved_112 : BITFIELD_RANGE(48, 63);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;
    typedef enum tagCOMPARE_OPERATION {
        COMPARE_OPERATION_MAD_GREATER_THAN_IDD = 0x0,
        COMPARE_OPERATION_MAD_GREATER_THAN_OR_EQUAL_IDD = 0x1,
        COMPARE_OPERATION_MAD_LESS_THAN_IDD = 0x2,
        COMPARE_OPERATION_MAD_LESS_THAN_OR_EQUAL_IDD = 0x3,
        COMPARE_OPERATION_MAD_EQUAL_IDD = 0x4,
        COMPARE_OPERATION_MAD_NOT_EQUAL_IDD = 0x5,
    } COMPARE_OPERATION;
    typedef enum tagCOMPARE_MASK_MODE {
        COMPARE_MASK_MODE_COMPARE_MASK_MODE_DISABLED = 0x0,
        COMPARE_MASK_MODE_COMPARE_MASK_MODE_ENABLED = 0x1,
    } COMPARE_MASK_MODE;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_CONDITIONAL_BATCH_BUFFER_END = 0x36,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.CompareOperation = COMPARE_OPERATION_MAD_GREATER_THAN_IDD;
        TheStructure.Common.CompareMaskMode = COMPARE_MASK_MODE_COMPARE_MASK_MODE_DISABLED;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_CONDITIONAL_BATCH_BUFFER_END;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_CONDITIONAL_BATCH_BUFFER_END sInit() {
        MI_CONDITIONAL_BATCH_BUFFER_END state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setCompareOperation(const COMPARE_OPERATION value) {
        TheStructure.Common.CompareOperation = value;
    }
    inline COMPARE_OPERATION getCompareOperation() const {
        return static_cast<COMPARE_OPERATION>(TheStructure.Common.CompareOperation);
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
    inline void setEndCurrentBatchBufferLevel(const bool value) {
        TheStructure.Common.EndCurrentBatchBufferLevel = value;
    }
    inline bool getEndCurrentBatchBufferLevel() const {
        return TheStructure.Common.EndCurrentBatchBufferLevel;
    }
    inline void setCompareMaskMode(const COMPARE_MASK_MODE value) {
        TheStructure.Common.CompareMaskMode = value;
    }
    inline COMPARE_MASK_MODE getCompareMaskMode() const {
        return static_cast<COMPARE_MASK_MODE>(TheStructure.Common.CompareMaskMode);
    }
    inline void setCompareSemaphore(const bool value) {
        TheStructure.Common.CompareSemaphore = value;
    }
    inline bool getCompareSemaphore() const {
        return TheStructure.Common.CompareSemaphore;
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt() const {
        return TheStructure.Common.UseGlobalGtt;
    }
    inline void setCompareDataDword(const uint32_t value) {
        TheStructure.Common.CompareDataDword = value;
    }
    inline uint32_t getCompareDataDword() const {
        return TheStructure.Common.CompareDataDword;
    }
    typedef enum tagCOMPAREADDRESS {
        COMPAREADDRESS_BIT_SHIFT = 0x3,
        COMPAREADDRESS_ALIGN_SIZE = 0x8,
    } COMPAREADDRESS;
    inline void setCompareAddress(const uint64_t value) {
        TheStructure.Common.CompareAddress = value >> COMPAREADDRESS_BIT_SHIFT;
    }
    inline uint64_t getCompareAddress() const {
        return TheStructure.Common.CompareAddress << COMPAREADDRESS_BIT_SHIFT;
    }
} MI_CONDITIONAL_BATCH_BUFFER_END;
STATIC_ASSERT(16 == sizeof(MI_CONDITIONAL_BATCH_BUFFER_END));

struct XY_BLOCK_COPY_BLT {
    union tagTheStructure {
        struct tagCommon {
            /// DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t NumberofMultisamples : BITFIELD_RANGE(9, 11);
            uint32_t SpecialModeofOperation : BITFIELD_RANGE(12, 13);
            uint32_t Reserved_14 : BITFIELD_RANGE(14, 18);
            uint32_t ColorDepth : BITFIELD_RANGE(19, 21);
            uint32_t InstructionTarget_Opcode : BITFIELD_RANGE(22, 28);
            uint32_t Client : BITFIELD_RANGE(29, 31);

            /// DWORD 1
            uint32_t DestinationPitch : BITFIELD_RANGE(0, 17);
            uint32_t DestinationAuxiliarysurfacemode : BITFIELD_RANGE(18, 20);
            uint32_t DestinationMOCS : BITFIELD_RANGE(21, 27);
            uint32_t DestinationControlSurfaceType : BITFIELD_RANGE(28, 28);
            uint32_t DestinationCompressionEnable : BITFIELD_RANGE(29, 29);
            uint32_t DestinationTiling : BITFIELD_RANGE(30, 31);

            /// DWORD 2
            uint32_t DestinationX1Coordinate_Left : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY1Coordinate_Top : BITFIELD_RANGE(16, 31);

            /// DWORD 3
            uint32_t DestinationX2Coordinate_Right : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY2Coordinate_Bottom : BITFIELD_RANGE(16, 31);

            /// DWORD 4..5
            uint64_t DestinationBaseAddress;

            /// DWORD 6
            uint32_t DestinationXoffset : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t DestinationYoffset : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_222 : BITFIELD_RANGE(30, 30);
            uint32_t DestinationTargetMemory : BITFIELD_RANGE(31, 31);

            /// DWORD 7
            uint32_t SourceX1Coordinate_Left : BITFIELD_RANGE(0, 15);
            uint32_t SourceY1Coordinate_Top : BITFIELD_RANGE(16, 31);

            /// DWORD 8
            uint32_t SourcePitch : BITFIELD_RANGE(0, 17);
            uint32_t SourceAuxiliarysurfacemode : BITFIELD_RANGE(18, 20);
            uint32_t SourceMOCS : BITFIELD_RANGE(21, 27);
            uint32_t SourceControlSurfaceType : BITFIELD_RANGE(28, 28);
            uint32_t SourceCompressionEnable : BITFIELD_RANGE(29, 29);
            uint32_t SourceTiling : BITFIELD_RANGE(30, 31);

            /// DWORD 9..10
            uint64_t SourceBaseAddress;

            /// DWORD 11
            uint32_t SourceXoffset : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_366 : BITFIELD_RANGE(14, 15);
            uint32_t SourceYoffset : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_382 : BITFIELD_RANGE(30, 30);
            uint32_t SourceTargetMemory : BITFIELD_RANGE(31, 31);

            /// DWORD 12
            uint32_t SourceCompressionFormat : BITFIELD_RANGE(0, 4);
            uint32_t SourceClearValueEnable : BITFIELD_RANGE(5, 5);
            uint32_t SourceClearAddressLow : BITFIELD_RANGE(6, 31);

            /// DWORD 13
            uint32_t SourceClearAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_432 : BITFIELD_RANGE(16, 31);

            /// DWORD 14
            uint32_t DestinationCompressionFormat : BITFIELD_RANGE(0, 4);
            uint32_t DestinationClearValueEnable : BITFIELD_RANGE(5, 5);
            uint32_t DestinationClearAddressLow : BITFIELD_RANGE(6, 31);

            /// DWORD 15
            uint32_t DestinationClearAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_496 : BITFIELD_RANGE(16, 31);

            /// DWORD 16
            uint32_t DestinationSurfaceHeight : BITFIELD_RANGE(0, 13);
            uint32_t DestinationSurfaceWidth : BITFIELD_RANGE(14, 27);
            uint32_t Reserved_540 : BITFIELD_RANGE(28, 28);
            uint32_t DestinationSurfaceType : BITFIELD_RANGE(29, 31);

            /// DWORD 17
            uint32_t DestinationLOD : BITFIELD_RANGE(0, 3);
            uint32_t DestinationSurfaceQpitch : BITFIELD_RANGE(4, 20);
            uint32_t DestinationSurfaceDepth : BITFIELD_RANGE(21, 31);

            /// DWORD 18
            uint32_t DestinationHorizontalAlign : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_578 : BITFIELD_RANGE(2, 2);
            uint32_t DestinationVerticalAlign : BITFIELD_RANGE(3, 4);
            uint32_t DestinationSSID : BITFIELD_RANGE(5, 7);
            uint32_t DestinationMipTailStartLOD : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_588 : BITFIELD_RANGE(12, 17);
            uint32_t DestinationDepthStencilResource : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_595 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationArrayIndex : BITFIELD_RANGE(21, 31);

            /// DWORD 19
            uint32_t SourceSurfaceHeight : BITFIELD_RANGE(0, 13);
            uint32_t SourceSurfaceWidth : BITFIELD_RANGE(14, 27);
            uint32_t Reserved_636 : BITFIELD_RANGE(28, 28);
            uint32_t SourceSurfaceType : BITFIELD_RANGE(29, 31);

            /// DWORD 20
            uint32_t SourceLOD : BITFIELD_RANGE(0, 3);
            uint32_t SourceSurfaceQpitch : BITFIELD_RANGE(4, 20);
            uint32_t SourceSurfaceDepth : BITFIELD_RANGE(21, 31);

            /// DWORD 21
            uint32_t SourceHorizontalAlign : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_674 : BITFIELD_RANGE(2, 2);
            uint32_t SourceVerticalAlign : BITFIELD_RANGE(3, 4);
            uint32_t SourceSSID : BITFIELD_RANGE(5, 7);
            uint32_t SourceMipTailStartLOD : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_684 : BITFIELD_RANGE(12, 17);
            uint32_t SourceDepthStencilResource : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_691 : BITFIELD_RANGE(19, 20);
            uint32_t SourceArrayIndex : BITFIELD_RANGE(21, 31);

        } Common;
        uint32_t RawData[22];
    } TheStructure;

    enum DWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 20,
    };

    enum NUMBER_OF_MULTISAMPLES {
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1 = 0,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2 = 1,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4 = 2,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8 = 3,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16 = 4,
    };

    enum SPECIAL_MODE_OF_OPERATION {
        SPECIAL_MODE_OF_OPERATION_NONE = 0,
        SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE = 1,
        SPECIAL_MODE_OF_OPERATION_PARTIAL_RESOLVE = 2,
    };

    enum COLOR_DEPTH {
        COLOR_DEPTH_8_BIT_COLOR = 0,
        COLOR_DEPTH_16_BIT_COLOR = 1,
        COLOR_DEPTH_32_BIT_COLOR = 2,
        COLOR_DEPTH_64_BIT_COLOR = 3,
        COLOR_DEPTH_96_BIT_COLOR_ONLY_LINEAR_CASE_IS_SUPPORTED = 4,
        COLOR_DEPTH_128_BIT_COLOR = 5,
    };

    enum CLIENT {
        CLIENT_2D_PROCESSOR = 2,
    };

    enum AUXILIARY_SURFACE_MODE {
        AUXILIARY_SURFACE_MODE_AUX_NONE = 0,
        AUXILIARY_SURFACE_MODE_AUX_CCS_E = 5,
    };

    enum CONTROL_SURFACE_TYPE {
        CONTROL_SURFACE_TYPE_3D = 0,
        CONTROL_SURFACE_TYPE_MEDIA = 1,
    };

    enum COMPRESSION_ENABLE {
        COMPRESSION_ENABLE_COMPRESSION_DISABLE = 0,
        COMPRESSION_ENABLE_COMPRESSION_ENABLE = 1,
    };

    enum TILING {
        TILING_LINEAR = 0,
        TILING_TILE4 = 2,
        TILING_TILE64 = 3,
    };

    enum TARGET_MEMORY {
        TARGET_MEMORY_LOCAL_MEM = 0,
        TARGET_MEMORY_SYSTEM_MEM = 1,
    };

    enum CLEAR_VALUE_ENABLE {
        CLEAR_VALUE_ENABLE_DISABLE = 0,
        CLEAR_VALUE_ENABLE_ENABLE = 1,
    };

    enum SURFACE_TYPE {
        SURFACE_TYPE_SURFTYPE_1D = 0,
        SURFACE_TYPE_SURFTYPE_2D = 1,
        SURFACE_TYPE_SURFTYPE_3D = 2,
        SURFACE_TYPE_SURFTYPE_CUBE = 3,
        SURFACE_TYPE_SURFTYPE_BUFFER = 4,
        SURFACE_TYPE_SURFTYPE_STRBUF = 5,
        SURFACE_TYPE_SURFTYPE_NULL = 7,
    };

    enum INSTRUCTIONTARGET_OPCODE {
        INSTRUCTIONTARGET_OPCODE_OPCODE = 0x41,
    };

    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH::DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.InstructionTarget_Opcode = INSTRUCTIONTARGET_OPCODE_OPCODE;
        TheStructure.Common.Client = CLIENT::CLIENT_2D_PROCESSOR;
    }

    static XY_BLOCK_COPY_BLT sInit() {
        XY_BLOCK_COPY_BLT state;
        state.init();
        return state;
    }

    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index < 22);
        return TheStructure.RawData[index];
    }

    inline void setNumberofMultisamples(const NUMBER_OF_MULTISAMPLES value) {
        TheStructure.Common.NumberofMultisamples = value;
    }

    inline NUMBER_OF_MULTISAMPLES getNumberofMultisamples() const {
        return static_cast<NUMBER_OF_MULTISAMPLES>(TheStructure.Common.NumberofMultisamples);
    }

    inline void setSpecialModeofOperation(const SPECIAL_MODE_OF_OPERATION value) {
        TheStructure.Common.SpecialModeofOperation = value;
    }

    inline SPECIAL_MODE_OF_OPERATION getSpecialModeOfOperation() const {
        return static_cast<SPECIAL_MODE_OF_OPERATION>(TheStructure.Common.SpecialModeofOperation);
    }

    inline void setColorDepth(const COLOR_DEPTH value) {
        TheStructure.Common.ColorDepth = value;
    }

    inline COLOR_DEPTH getColorDepth() const {
        return static_cast<COLOR_DEPTH>(TheStructure.Common.ColorDepth);
    }

    inline void setInstructionTargetOpcode(const uint32_t value) {
        TheStructure.Common.InstructionTarget_Opcode = value;
    }

    inline uint32_t getInstructionTargetOpcode() const {
        return (TheStructure.Common.InstructionTarget_Opcode);
    }

    inline void setClient(const CLIENT value) {
        TheStructure.Common.Client = value;
    }

    inline CLIENT getClient() const {
        return static_cast<CLIENT>(TheStructure.Common.Client);
    }

    inline void setDestinationPitch(const uint32_t value) {
        TheStructure.Common.DestinationPitch = value - 1;
    }

    inline uint32_t getDestinationPitch() const {
        return (TheStructure.Common.DestinationPitch + 1);
    }

    inline void setDestinationAuxiliarysurfacemode(const AUXILIARY_SURFACE_MODE value) {
        TheStructure.Common.DestinationAuxiliarysurfacemode = value;
    }

    inline AUXILIARY_SURFACE_MODE getDestinationAuxiliarysurfacemode() const {
        return static_cast<AUXILIARY_SURFACE_MODE>(TheStructure.Common.DestinationAuxiliarysurfacemode);
    }

    inline void setDestinationMOCS(const uint32_t value) {
        TheStructure.Common.DestinationMOCS = value;
    }

    inline uint32_t getDestinationMOCS() const {
        return (TheStructure.Common.DestinationMOCS);
    }

    inline void setDestinationControlSurfaceType(const CONTROL_SURFACE_TYPE value) {
        TheStructure.Common.DestinationControlSurfaceType = value;
    }

    inline CONTROL_SURFACE_TYPE getDestinationControlSurfaceType() const {
        return static_cast<CONTROL_SURFACE_TYPE>(TheStructure.Common.DestinationControlSurfaceType);
    }

    inline void setDestinationCompressionEnable(const COMPRESSION_ENABLE value) {
        TheStructure.Common.DestinationCompressionEnable = value;
    }

    inline COMPRESSION_ENABLE getDestinationCompressionEnable() const {
        return static_cast<COMPRESSION_ENABLE>(TheStructure.Common.DestinationCompressionEnable);
    }

    inline void setDestinationTiling(const TILING value) {
        TheStructure.Common.DestinationTiling = value;
    }

    inline TILING getDestinationTiling() const {
        return static_cast<TILING>(TheStructure.Common.DestinationTiling);
    }

    inline void setDestinationX1CoordinateLeft(const uint32_t value) {
        TheStructure.Common.DestinationX1Coordinate_Left = value;
    }

    inline uint32_t getDestinationX1CoordinateLeft() const {
        return (TheStructure.Common.DestinationX1Coordinate_Left);
    }

    inline void setDestinationY1CoordinateTop(const uint32_t value) {
        TheStructure.Common.DestinationY1Coordinate_Top = value;
    }

    inline uint32_t getDestinationY1CoordinateTop() const {
        return (TheStructure.Common.DestinationY1Coordinate_Top);
    }

    inline void setDestinationX2CoordinateRight(const uint32_t value) {
        TheStructure.Common.DestinationX2Coordinate_Right = value;
    }

    inline uint32_t getDestinationX2CoordinateRight() const {
        return (TheStructure.Common.DestinationX2Coordinate_Right);
    }

    inline void setDestinationY2CoordinateBottom(const uint32_t value) {
        TheStructure.Common.DestinationY2Coordinate_Bottom = value;
    }

    inline uint32_t getDestinationY2CoordinateBottom() const {
        return (TheStructure.Common.DestinationY2Coordinate_Bottom);
    }

    inline void setDestinationBaseAddress(const uint64_t value) {
        TheStructure.Common.DestinationBaseAddress = value;
    }

    inline uint64_t getDestinationBaseAddress() const {
        return (TheStructure.Common.DestinationBaseAddress);
    }

    inline void setDestinationXOffset(const uint32_t value) {
        TheStructure.Common.DestinationXoffset = value;
    }

    inline uint32_t getDestinationXOffset() const {
        return (TheStructure.Common.DestinationXoffset);
    }

    inline void setDestinationYOffset(const uint32_t value) {
        TheStructure.Common.DestinationYoffset = value;
    }

    inline uint32_t getDestinationYOffset() const {
        return (TheStructure.Common.DestinationYoffset);
    }

    inline void setDestinationTargetMemory(const TARGET_MEMORY value) {
        TheStructure.Common.DestinationTargetMemory = value;
    }

    inline TARGET_MEMORY getDestinationTargetMemory() const {
        return static_cast<TARGET_MEMORY>(TheStructure.Common.DestinationTargetMemory);
    }

    inline void setSourceX1CoordinateLeft(const uint32_t value) {
        TheStructure.Common.SourceX1Coordinate_Left = value;
    }

    inline uint32_t getSourceX1CoordinateLeft() const {
        return (TheStructure.Common.SourceX1Coordinate_Left);
    }

    inline void setSourceY1CoordinateTop(const uint32_t value) {
        TheStructure.Common.SourceY1Coordinate_Top = value;
    }

    inline uint32_t getSourceY1CoordinateTop() const {
        return (TheStructure.Common.SourceY1Coordinate_Top);
    }

    inline void setSourcePitch(const uint32_t value) {
        TheStructure.Common.SourcePitch = value - 1;
    }

    inline uint32_t getSourcePitch() const {
        return (TheStructure.Common.SourcePitch + 1);
    }

    inline void setSourceAuxiliarysurfacemode(const AUXILIARY_SURFACE_MODE value) {
        TheStructure.Common.SourceAuxiliarysurfacemode = value;
    }

    inline AUXILIARY_SURFACE_MODE getSourceAuxiliarysurfacemode() const {
        return static_cast<AUXILIARY_SURFACE_MODE>(TheStructure.Common.SourceAuxiliarysurfacemode);
    }

    inline void setSourceMOCS(const uint32_t value) {
        TheStructure.Common.SourceMOCS = value;
    }

    inline uint32_t getSourceMOCS() const {
        return (TheStructure.Common.SourceMOCS);
    }

    inline void setSourceControlSurfaceType(const CONTROL_SURFACE_TYPE value) {
        TheStructure.Common.SourceControlSurfaceType = value;
    }

    inline CONTROL_SURFACE_TYPE getSourceControlSurfaceType() const {
        return static_cast<CONTROL_SURFACE_TYPE>(TheStructure.Common.SourceControlSurfaceType);
    }

    inline void setSourceCompressionEnable(const COMPRESSION_ENABLE value) {
        TheStructure.Common.SourceCompressionEnable = value;
    }

    inline COMPRESSION_ENABLE getSourceCompressionEnable() const {
        return static_cast<COMPRESSION_ENABLE>(TheStructure.Common.SourceCompressionEnable);
    }

    inline void setSourceTiling(const TILING value) {
        TheStructure.Common.SourceTiling = value;
    }

    inline TILING getSourceTiling() const {
        return static_cast<TILING>(TheStructure.Common.SourceTiling);
    }

    inline void setSourceBaseAddress(const uint64_t value) {
        TheStructure.Common.SourceBaseAddress = value;
    }

    inline uint64_t getSourceBaseAddress() const {
        return (TheStructure.Common.SourceBaseAddress);
    }

    inline void setSourceXOffset(const uint32_t value) {
        TheStructure.Common.SourceXoffset = value;
    }

    inline uint32_t getSourceXOffset() const {
        return (TheStructure.Common.SourceXoffset);
    }

    inline void setSourceYOffset(const uint32_t value) {
        TheStructure.Common.SourceYoffset = value;
    }

    inline uint32_t getSourceYOffset() const {
        return (TheStructure.Common.SourceYoffset);
    }

    inline void setSourceTargetMemory(const TARGET_MEMORY value) {
        TheStructure.Common.SourceTargetMemory = value;
    }

    inline TARGET_MEMORY getSourceTargetMemory() const {
        return static_cast<TARGET_MEMORY>(TheStructure.Common.SourceTargetMemory);
    }

    inline void setSourceCompressionFormat(const uint32_t value) {
        TheStructure.Common.SourceCompressionFormat = value;
    }

    inline uint32_t getSourceCompressionFormat() const {
        return (TheStructure.Common.SourceCompressionFormat);
    }

    inline void setSourceClearValueEnable(const CLEAR_VALUE_ENABLE value) {
        TheStructure.Common.SourceClearValueEnable = value;
    }

    inline CLEAR_VALUE_ENABLE getSourceClearValueEnable() const {
        return static_cast<CLEAR_VALUE_ENABLE>(TheStructure.Common.SourceClearValueEnable);
    }

    typedef enum tagCLEARADDRESSLOW {
        CLEARADDRESSLOW_BIT_SHIFT = 6,
        CLEARADDRESSLOW_ALIGN_SIZE = 64,
    } CLEARADDRESSLOW;

    inline void setSourceClearAddressLow(const uint32_t value) {
        TheStructure.Common.SourceClearAddressLow = value >> CLEARADDRESSLOW_BIT_SHIFT;
    }

    inline uint32_t getSourceClearAddressLow() const {
        return (TheStructure.Common.SourceClearAddressLow << CLEARADDRESSLOW_BIT_SHIFT);
    }

    inline void setSourceClearAddressHigh(const uint32_t value) {
        TheStructure.Common.SourceClearAddressHigh = value;
    }

    inline uint32_t getSourceClearAddressHigh() const {
        return (TheStructure.Common.SourceClearAddressHigh);
    }

    inline void setDestinationCompressionFormat(const uint32_t value) {
        TheStructure.Common.DestinationCompressionFormat = value;
    }

    inline uint32_t getDestinationCompressionFormat() const {
        return (TheStructure.Common.DestinationCompressionFormat);
    }

    inline void setDestinationClearValueEnable(const CLEAR_VALUE_ENABLE value) {
        TheStructure.Common.DestinationClearValueEnable = value;
    }

    inline CLEAR_VALUE_ENABLE getDestinationClearValueEnable() const {
        return static_cast<CLEAR_VALUE_ENABLE>(TheStructure.Common.DestinationClearValueEnable);
    }

    inline void setDestinationClearAddressLow(const uint32_t value) {
        TheStructure.Common.DestinationClearAddressLow = value >> CLEARADDRESSLOW_BIT_SHIFT;
    }

    inline uint32_t getDestinationClearAddressLow() const {
        return (TheStructure.Common.DestinationClearAddressLow << CLEARADDRESSLOW_BIT_SHIFT);
    }

    inline void setDestinationClearAddressHigh(const uint32_t value) {
        TheStructure.Common.DestinationClearAddressHigh = value;
    }

    inline uint32_t getDestinationClearAddressHigh() const {
        return (TheStructure.Common.DestinationClearAddressHigh);
    }

    inline void setDestinationSurfaceHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.DestinationSurfaceHeight = value - 1;
    }

    inline uint32_t getDestinationSurfaceHeight() const {
        return (TheStructure.Common.DestinationSurfaceHeight + 1);
    }

    inline void setDestinationSurfaceWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.DestinationSurfaceWidth = value - 1;
    }

    inline uint32_t getDestinationSurfaceWidth() const {
        return (TheStructure.Common.DestinationSurfaceWidth + 1);
    }

    inline void setDestinationSurfaceType(const SURFACE_TYPE value) {
        TheStructure.Common.DestinationSurfaceType = value;
    }

    inline SURFACE_TYPE getDestinationSurfaceType() const {
        return static_cast<SURFACE_TYPE>(TheStructure.Common.DestinationSurfaceType);
    }

    inline void setDestinationLOD(const uint32_t value) {
        TheStructure.Common.DestinationLOD = value;
    }

    inline uint32_t getDestinationLOD() const {
        return (TheStructure.Common.DestinationLOD);
    }

    inline void setDestinationSurfaceQpitch(const uint32_t value) {
        TheStructure.Common.DestinationSurfaceQpitch = value;
    }

    inline uint32_t getDestinationSurfaceQpitch() const {
        return (TheStructure.Common.DestinationSurfaceQpitch);
    }

    inline void setDestinationSurfaceDepth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x7ff);
        TheStructure.Common.DestinationSurfaceDepth = value - 1;
    }

    inline uint32_t getDestinationSurfaceDepth() const {
        return (TheStructure.Common.DestinationSurfaceDepth + 1);
    }

    inline void setDestinationHorizontalAlign(const uint32_t value) {
        TheStructure.Common.DestinationHorizontalAlign = value;
    }

    inline uint32_t getDestinationHorizontalAlign() const {
        return (TheStructure.Common.DestinationHorizontalAlign);
    }

    inline void setDestinationVerticalAlign(const uint32_t value) {
        TheStructure.Common.DestinationVerticalAlign = value;
    }

    inline uint32_t getDestinationVerticalAlign() const {
        return (TheStructure.Common.DestinationVerticalAlign);
    }

    inline void setDestinationSSID(const uint32_t value) {
        TheStructure.Common.DestinationSSID = value;
    }

    inline uint32_t getDestinationSSID() const {
        return (TheStructure.Common.DestinationSSID);
    }

    inline void setDestinationMipTailStartLOD(const uint32_t value) {
        TheStructure.Common.DestinationMipTailStartLOD = value;
    }

    inline uint32_t getDestinationMipTailStartLOD() const {
        return (TheStructure.Common.DestinationMipTailStartLOD);
    }

    inline void setDestinationDepthStencilResource(const uint32_t value) {
        TheStructure.Common.DestinationDepthStencilResource = value;
    }

    inline uint32_t getDestinationDepthStencilResource() const {
        return (TheStructure.Common.DestinationDepthStencilResource);
    }

    inline void setDestinationArrayIndex(const uint32_t value) {
        TheStructure.Common.DestinationArrayIndex = value - 1;
    }

    inline uint32_t getDestinationArrayIndex() const {
        return (TheStructure.Common.DestinationArrayIndex + 1);
    }

    inline void setSourceSurfaceHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.SourceSurfaceHeight = value - 1;
    }

    inline uint32_t getSourceSurfaceHeight() const {
        return (TheStructure.Common.SourceSurfaceHeight + 1);
    }

    inline void setSourceSurfaceWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.SourceSurfaceWidth = value - 1;
    }

    inline uint32_t getSourceSurfaceWidth() const {
        return (TheStructure.Common.SourceSurfaceWidth + 1);
    }

    inline void setSourceSurfaceType(const SURFACE_TYPE value) {
        TheStructure.Common.SourceSurfaceType = value;
    }

    inline SURFACE_TYPE getSourceSurfaceType() const {
        return static_cast<SURFACE_TYPE>(TheStructure.Common.SourceSurfaceType);
    }

    inline void setSourceLOD(const uint32_t value) {
        TheStructure.Common.SourceLOD = value;
    }

    inline uint32_t getSourceLOD() const {
        return (TheStructure.Common.SourceLOD);
    }

    inline void setSourceSurfaceQpitch(const uint32_t value) {
        TheStructure.Common.SourceSurfaceQpitch = value;
    }

    inline uint32_t getSourceSurfaceQpitch() const {
        return (TheStructure.Common.SourceSurfaceQpitch);
    }

    inline void setSourceSurfaceDepth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x7ff);
        TheStructure.Common.SourceSurfaceDepth = value - 1;
    }

    inline uint32_t getSourceSurfaceDepth() const {
        return (TheStructure.Common.SourceSurfaceDepth + 1);
    }

    inline void setSourceHorizontalAlign(const uint32_t value) {
        TheStructure.Common.SourceHorizontalAlign = value;
    }

    inline uint32_t getSourceHorizontalAlign() const {
        return (TheStructure.Common.SourceHorizontalAlign);
    }

    inline void setSourceVerticalAlign(const uint32_t value) {
        TheStructure.Common.SourceVerticalAlign = value;
    }

    inline uint32_t getSourceVerticalAlign() const {
        return (TheStructure.Common.SourceVerticalAlign);
    }

    inline void setSourceSSID(const uint32_t value) {
        TheStructure.Common.SourceSSID = value;
    }

    inline uint32_t getSourceSSID() const {
        return (TheStructure.Common.SourceSSID);
    }

    inline void setSourceMipTailStartLOD(const uint32_t value) {
        TheStructure.Common.SourceMipTailStartLOD = value;
    }

    inline uint32_t getSourceMipTailStartLOD() const {
        return (TheStructure.Common.SourceMipTailStartLOD);
    }

    inline void setSourceDepthStencilResource(const uint32_t value) {
        TheStructure.Common.SourceDepthStencilResource = value;
    }

    inline uint32_t getSourceDepthStencilResource() const {
        return (TheStructure.Common.SourceDepthStencilResource);
    }

    inline void setSourceArrayIndex(const uint32_t value) {
        TheStructure.Common.SourceArrayIndex = value - 1;
    }

    inline uint32_t getSourceArrayIndex() const {
        return (TheStructure.Common.SourceArrayIndex + 1);
    }
};
STATIC_ASSERT(88 == sizeof(XY_BLOCK_COPY_BLT));

struct XY_FAST_COLOR_BLT {
    union tagTheStructure {
        struct tagCommon {
            /// DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t NumberofMultisamples : BITFIELD_RANGE(9, 11);
            uint32_t SpecialModeofOperation : BITFIELD_RANGE(12, 13);
            uint32_t Reserved_14 : BITFIELD_RANGE(14, 18);
            uint32_t ColorDepth : BITFIELD_RANGE(19, 21);
            uint32_t InstructionTarget_Opcode : BITFIELD_RANGE(22, 28);
            uint32_t Client : BITFIELD_RANGE(29, 31);

            /// DWORD 1
            uint32_t DestinationPitch : BITFIELD_RANGE(0, 17);
            uint32_t DestinationAuxiliarysurfacemode : BITFIELD_RANGE(18, 20);
            uint32_t DestinationMOCS : BITFIELD_RANGE(21, 27);
            uint32_t DestinationControlSurfaceType : BITFIELD_RANGE(28, 28);
            uint32_t DestinationCompressionEnable : BITFIELD_RANGE(29, 29);
            uint32_t DestinationTiling : BITFIELD_RANGE(30, 31);

            /// DWORD 2
            uint32_t DestinationX1Coordinate_Left : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY1Coordinate_Top : BITFIELD_RANGE(16, 31);

            /// DWORD 3
            uint32_t DestinationX2Coordinate_Right : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY2Coordinate_Bottom : BITFIELD_RANGE(16, 31);

            /// DWORD 4..5
            uint64_t DestinationBaseAddress;

            /// DWORD 6
            uint32_t DestinationXoffset : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t DestinationYoffset : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_222 : BITFIELD_RANGE(30, 30);
            uint32_t DestinationTargetMemory : BITFIELD_RANGE(31, 31);

            /// DWORD 7 - 10
            uint32_t FillColor[4];

            // DWORD 11
            uint32_t DestinationCompressionFormat : BITFIELD_RANGE(0, 4);
            uint32_t DestinationClearValueEnable : BITFIELD_RANGE(5, 5);
            uint32_t DestinationClearAddressLow : BITFIELD_RANGE(6, 31);

            // DWORD 12
            uint32_t DestinationClearAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved1 : BITFIELD_RANGE(16, 31);

            // DWORD 13
            uint32_t DestinationSurfaceHeight : BITFIELD_RANGE(0, 13);
            uint32_t DestinationSurfaceWidth : BITFIELD_RANGE(14, 27);
            uint32_t Reserved2 : BITFIELD_RANGE(28, 28);
            uint32_t DestinationSurfaceType : BITFIELD_RANGE(29, 31);

            // DWORD 14
            uint32_t DestinationLOD : BITFIELD_RANGE(0, 3);
            uint32_t DestinationSurfaceQpitch : BITFIELD_RANGE(4, 18);
            uint32_t Reserved3 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationSurfaceDepth : BITFIELD_RANGE(21, 31);

            // DWORD 15
            uint32_t DestinationHorizontalAlign : BITFIELD_RANGE(0, 1);
            uint32_t Reserved4 : BITFIELD_RANGE(2, 2);
            uint32_t DestinationVerticalAlign : BITFIELD_RANGE(3, 4);
            uint32_t Reserved5 : BITFIELD_RANGE(5, 7);
            uint32_t DestinationMipTailStartLOD : BITFIELD_RANGE(8, 11);
            uint32_t Reserved6 : BITFIELD_RANGE(12, 17);
            uint32_t DestinationDepthStencilResource : BITFIELD_RANGE(18, 18);
            uint32_t Reserved7 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationArrayIndex : BITFIELD_RANGE(21, 31);
        } Common;
        uint32_t RawData[15];
    } TheStructure;

    enum DWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0xE,
    };

    enum NUMBER_OF_MULTISAMPLES {
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1 = 0,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2 = 1,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4 = 2,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8 = 3,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16 = 4,
    };

    enum SPECIAL_MODE_OF_OPERATION {
        SPECIAL_MODE_OF_OPERATION_NONE = 0,
        SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE = 1,
        SPECIAL_MODE_OF_OPERATION_PARTIAL_RESOLVE = 2,
    };

    enum COLOR_DEPTH {
        COLOR_DEPTH_8_BIT_COLOR = 0,
        COLOR_DEPTH_16_BIT_COLOR = 1,
        COLOR_DEPTH_32_BIT_COLOR = 2,
        COLOR_DEPTH_64_BIT_COLOR = 3,
        COLOR_DEPTH_96_BIT_COLOR_ONLY_LINEAR_CASE_IS_SUPPORTED = 4,
        COLOR_DEPTH_128_BIT_COLOR = 5,
    };

    enum CLIENT {
        CLIENT_2D_PROCESSOR = 2,
    };

    enum DESTINATION_AUXILIARY_SURFACE_MODE {
        DESTINATION_AUXILIARY_SURFACE_MODE_AUX_NONE = 0,
        DESTINATION_AUXILIARY_SURFACE_MODE_AUX_CCS_E = 5,
    };

    enum DESTINATION_CLEAR_VALUE_ENABLE {
        DESTINATION_CLEAR_VALUE_ENABLE_DISABLE = 0,
        DESTINATION_CLEAR_VALUE_ENABLE_ENABLE = 1,
    };

    enum DESTINATION_CONTROL_SURFACE_TYPE {
        DESTINATION_CONTROL_SURFACE_TYPE_3D = 0,
        DESTINATION_CONTROL_SURFACE_TYPE_MEDIA = 1,
    };

    enum DESTINATION_COMPRESSION_ENABLE {
        DESTINATION_COMPRESSION_ENABLE_COMPRESSION_DISABLE = 0,
        DESTINATION_COMPRESSION_ENABLE_COMPRESSION_ENABLE = 1,
    };

    enum DESTINATION_TILING {
        DESTINATION_TILING_LINEAR = 0,
        DESTINATION_TILING_TILE4 = 2,
        DESTINATION_TILING_TILE64 = 3,
    };

    enum DESTINATION_TARGET_MEMORY {
        DESTINATION_TARGET_MEMORY_LOCAL_MEM = 0,
        DESTINATION_TARGET_MEMORY_SYSTEM_MEM = 1,
    };

    enum DESTINATION_SURFACE_TYPE {
        DESTINATION_SURFACE_TYPE_SURFTYPE_1D = 0,
        DESTINATION_SURFACE_TYPE_SURFTYPE_2D = 1,
        DESTINATION_SURFACE_TYPE_SURFTYPE_3D = 2,
        DESTINATION_SURFACE_TYPE_SURFTYPE_CUBE = 3,
    };
    enum INSTRUCTIONTARGET_OPCODE {
        INSTRUCTIONTARGET_OPCODE_OPCODE = 0x44,
    };

    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH::DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.InstructionTarget_Opcode = INSTRUCTIONTARGET_OPCODE::INSTRUCTIONTARGET_OPCODE_OPCODE;
        TheStructure.Common.Client = CLIENT::CLIENT_2D_PROCESSOR;
    }

    static XY_FAST_COLOR_BLT sInit() {
        XY_FAST_COLOR_BLT state;
        state.init();
        return state;
    }

    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index < 22);
        return TheStructure.RawData[index];
    }

    inline void setNumberofMultisamples(const NUMBER_OF_MULTISAMPLES value) {
        TheStructure.Common.NumberofMultisamples = value;
    }

    inline NUMBER_OF_MULTISAMPLES getNumberofMultisamples() const {
        return static_cast<NUMBER_OF_MULTISAMPLES>(TheStructure.Common.NumberofMultisamples);
    }

    inline void setSpecialModeofOperation(const SPECIAL_MODE_OF_OPERATION value) {
        UNRECOVERABLE_IF(true); // patched
        TheStructure.Common.SpecialModeofOperation = value;
    }

    inline SPECIAL_MODE_OF_OPERATION getSpecialModeOfOperation() const {
        return static_cast<SPECIAL_MODE_OF_OPERATION>(TheStructure.Common.SpecialModeofOperation);
    }

    inline void setColorDepth(const COLOR_DEPTH value) {
        TheStructure.Common.ColorDepth = value;
    }

    inline COLOR_DEPTH getColorDepth() const {
        return static_cast<COLOR_DEPTH>(TheStructure.Common.ColorDepth);
    }

    inline void setInstructionTargetOpcode(const uint32_t value) {
        TheStructure.Common.InstructionTarget_Opcode = value;
    }

    inline uint32_t getInstructionTargetOpcode() const {
        return (TheStructure.Common.InstructionTarget_Opcode);
    }

    inline void setClient(const CLIENT value) {
        TheStructure.Common.Client = value;
    }

    inline CLIENT getClient() const {
        return static_cast<CLIENT>(TheStructure.Common.Client);
    }

    inline void setDestinationPitch(const uint32_t value) {
        TheStructure.Common.DestinationPitch = value - 1;
    }

    inline uint32_t getDestinationPitch() const {
        return (TheStructure.Common.DestinationPitch + 1);
    }

    inline void setDestinationAuxiliarysurfacemode(const DESTINATION_AUXILIARY_SURFACE_MODE value) {
        TheStructure.Common.DestinationAuxiliarysurfacemode = value;
    }

    inline DESTINATION_AUXILIARY_SURFACE_MODE getDestinationAuxiliarysurfacemode() const {
        return static_cast<DESTINATION_AUXILIARY_SURFACE_MODE>(TheStructure.Common.DestinationAuxiliarysurfacemode);
    }

    inline void setDestinationMOCS(const uint32_t value) {
        TheStructure.Common.DestinationMOCS = value;
    }

    inline uint32_t getDestinationMOCS() const {
        return (TheStructure.Common.DestinationMOCS);
    }

    inline void setDestinationControlSurfaceType(const DESTINATION_CONTROL_SURFACE_TYPE value) {
        TheStructure.Common.DestinationControlSurfaceType = value;
    }

    inline DESTINATION_CONTROL_SURFACE_TYPE getDestinationControlSurfaceType() const {
        return static_cast<DESTINATION_CONTROL_SURFACE_TYPE>(TheStructure.Common.DestinationControlSurfaceType);
    }

    inline void setDestinationCompressionEnable(const DESTINATION_COMPRESSION_ENABLE value) {
        TheStructure.Common.DestinationCompressionEnable = value;
    }

    inline DESTINATION_COMPRESSION_ENABLE getDestinationCompressionEnable() const {
        return static_cast<DESTINATION_COMPRESSION_ENABLE>(TheStructure.Common.DestinationCompressionEnable);
    }

    inline void setDestinationTiling(const DESTINATION_TILING value) {
        TheStructure.Common.DestinationTiling = value;
    }

    inline DESTINATION_TILING getDestinationTiling() const {
        return static_cast<DESTINATION_TILING>(TheStructure.Common.DestinationTiling);
    }

    inline void setDestinationX1CoordinateLeft(const uint32_t value) {
        TheStructure.Common.DestinationX1Coordinate_Left = value;
    }

    inline uint32_t getDestinationX1CoordinateLeft() const {
        return (TheStructure.Common.DestinationX1Coordinate_Left);
    }

    inline void setDestinationY1CoordinateTop(const uint32_t value) {
        TheStructure.Common.DestinationY1Coordinate_Top = value;
    }

    inline uint32_t getDestinationY1CoordinateTop() const {
        return (TheStructure.Common.DestinationY1Coordinate_Top);
    }

    inline void setDestinationX2CoordinateRight(const uint32_t value) {
        TheStructure.Common.DestinationX2Coordinate_Right = value;
    }

    inline uint32_t getDestinationX2CoordinateRight() const {
        return (TheStructure.Common.DestinationX2Coordinate_Right);
    }

    inline void setDestinationY2CoordinateBottom(const uint32_t value) {
        TheStructure.Common.DestinationY2Coordinate_Bottom = value;
    }

    inline uint32_t getDestinationY2CoordinateBottom() const {
        return (TheStructure.Common.DestinationY2Coordinate_Bottom);
    }

    inline void setDestinationBaseAddress(const uint64_t value) {
        TheStructure.Common.DestinationBaseAddress = value;
    }

    inline uint64_t getDestinationBaseAddress() const {
        return (TheStructure.Common.DestinationBaseAddress);
    }

    inline void setDestinationXoffset(const uint32_t value) {
        TheStructure.Common.DestinationXoffset = value;
    }

    inline uint32_t getDestinationXoffset() const {
        return (TheStructure.Common.DestinationXoffset);
    }

    inline void setDestinationYoffset(const uint32_t value) {
        TheStructure.Common.DestinationYoffset = value;
    }

    inline uint32_t getDestinationYoffset() const {
        return (TheStructure.Common.DestinationYoffset);
    }

    inline void setDestinationTargetMemory(const DESTINATION_TARGET_MEMORY value) {
        TheStructure.Common.DestinationTargetMemory = value;
    }

    inline DESTINATION_TARGET_MEMORY getDestinationTargetMemory() const {
        return static_cast<DESTINATION_TARGET_MEMORY>(TheStructure.Common.DestinationTargetMemory);
    }

    inline void setFillColor(const uint32_t *value) {
        TheStructure.Common.FillColor[0] = value[0];
        TheStructure.Common.FillColor[1] = value[1];
        TheStructure.Common.FillColor[2] = value[2];
        TheStructure.Common.FillColor[3] = value[3];
    }

    inline void setDestinationCompressionFormat(const uint32_t value) {
        TheStructure.Common.DestinationCompressionFormat = value;
    }

    inline uint32_t getDestinationCompressionFormat() const {
        return (TheStructure.Common.DestinationCompressionFormat);
    }

    inline void setDestinationClearValueEnable(const DESTINATION_CLEAR_VALUE_ENABLE value) {
        TheStructure.Common.DestinationClearValueEnable = value;
    }

    inline DESTINATION_CLEAR_VALUE_ENABLE getDestinationClearValueEnable() const {
        return static_cast<DESTINATION_CLEAR_VALUE_ENABLE>(TheStructure.Common.DestinationClearValueEnable);
    }

    typedef enum tagDESTINATIONCLEARADDRESSLOW {
        DESTINATIONCLEARADDRESSLOW_BIT_SHIFT = 6,
        DESTINATIONCLEARADDRESSLOW_ALIGN_SIZE = 64,
    } DESTINATIONCLEARADDRESSLOW;

    inline void setDestinationClearAddressLow(const uint32_t value) {
        TheStructure.Common.DestinationClearAddressLow = value >> DESTINATIONCLEARADDRESSLOW_BIT_SHIFT;
    }

    inline uint32_t getDestinationClearAddressLow() const {
        return (TheStructure.Common.DestinationClearAddressLow << DESTINATIONCLEARADDRESSLOW_BIT_SHIFT);
    }

    inline void setDestinationClearAddressHigh(const uint32_t value) {
        TheStructure.Common.DestinationClearAddressHigh = value;
    }

    inline uint32_t getDestinationClearAddressHigh() const {
        return (TheStructure.Common.DestinationClearAddressHigh);
    }

    inline void setDestinationSurfaceHeight(const uint32_t value) {
        TheStructure.Common.DestinationSurfaceHeight = value - 1;
    }

    inline uint32_t getDestinationSurfaceHeight() const {
        return (TheStructure.Common.DestinationSurfaceHeight + 1);
    }

    inline void setDestinationSurfaceWidth(const uint32_t value) {
        TheStructure.Common.DestinationSurfaceWidth = value - 1;
    }

    inline uint32_t getDestinationSurfaceWidth() const {
        return (TheStructure.Common.DestinationSurfaceWidth + 1);
    }

    inline void setDestinationSurfaceType(const DESTINATION_SURFACE_TYPE value) {
        TheStructure.Common.DestinationSurfaceType = value;
    }

    inline DESTINATION_SURFACE_TYPE getDestinationSurfaceType() const {
        return static_cast<DESTINATION_SURFACE_TYPE>(TheStructure.Common.DestinationSurfaceType);
    }

    inline void setDestinationLOD(const uint32_t value) {
        TheStructure.Common.DestinationLOD = value;
    }

    inline uint32_t getDestinationLOD() const {
        return (TheStructure.Common.DestinationLOD);
    }

    inline void setDestinationSurfaceQpitch(const uint32_t value) {
        TheStructure.Common.DestinationSurfaceQpitch = value;
    }

    inline uint32_t getDestinationSurfaceQpitch() const {
        return (TheStructure.Common.DestinationSurfaceQpitch);
    }

    inline void setDestinationSurfaceDepth(const uint32_t value) {
        TheStructure.Common.DestinationSurfaceDepth = value;
    }

    inline uint32_t getDestinationSurfaceDepth() const {
        return (TheStructure.Common.DestinationSurfaceDepth);
    }

    inline void setDestinationHorizontalAlign(const uint32_t value) {
        TheStructure.Common.DestinationHorizontalAlign = value;
    }

    inline uint32_t getDestinationHorizontalAlign() const {
        return (TheStructure.Common.DestinationHorizontalAlign);
    }

    inline void setDestinationVerticalAlign(const uint32_t value) {
        TheStructure.Common.DestinationVerticalAlign = value;
    }

    inline uint32_t getDestinationVerticalAlign() const {
        return (TheStructure.Common.DestinationVerticalAlign);
    }

    inline void setDestinationMipTailStartLOD(const uint32_t value) {
        TheStructure.Common.DestinationMipTailStartLOD = value;
    }

    inline uint32_t getDestinationMipTailStartLOD() const {
        return (TheStructure.Common.DestinationMipTailStartLOD);
    }

    inline void setDestinationDepthStencilResource(const uint32_t value) {
        TheStructure.Common.DestinationDepthStencilResource = value;
    }

    inline uint32_t getDestinationDepthStencilResource() const {
        return (TheStructure.Common.DestinationDepthStencilResource);
    }

    inline void setDestinationArrayIndex(const uint32_t value) {
        TheStructure.Common.DestinationArrayIndex = value - 1;
    }

    inline uint32_t getDestinationArrayIndex() const {
        return (TheStructure.Common.DestinationArrayIndex + 1);
    }
};

STATIC_ASSERT(64 == sizeof(XY_FAST_COLOR_BLT));

struct MI_FLUSH_DW {
    union tagData {
        struct tagCommon {
            /// DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 7);
            uint32_t NotifyEnable : BITFIELD_RANGE(8, 8);
            uint32_t FlushLlc : BITFIELD_RANGE(9, 9);
            uint32_t Reserved_10 : BITFIELD_RANGE(10, 13);
            uint32_t PostSyncOperation : BITFIELD_RANGE(14, 15);
            uint32_t FlushCcs : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_17 : BITFIELD_RANGE(17, 17);
            uint32_t TlbInvalidate : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_19 : BITFIELD_RANGE(19, 20);
            uint32_t StoreDataIndex : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_22 : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);

            /// DWORD 1..2
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint64_t DestinationAddress : BITFIELD_RANGE(2, 47);
            uint64_t Reserved_80 : BITFIELD_RANGE(48, 63);

            /// DWORD 3..4
            uint64_t ImmediateData;

        } Common;
        uint32_t RawData[5];
    } TheStructure;

    enum DWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 3,
    };

    enum POST_SYNC_OPERATION {
        POST_SYNC_OPERATION_NO_WRITE = 0,
        POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD = 1,
        POST_SYNC_OPERATION_WRITE_TIMESTAMP_REGISTER = 3,
    };

    enum MI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_FLUSH_DW = 38,
    };

    enum COMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0,
    };

    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH::DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE::MI_COMMAND_OPCODE_MI_FLUSH_DW;
    }

    static MI_FLUSH_DW sInit() {
        MI_FLUSH_DW state;
        state.init();
        return state;
    }

    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index < 5);
        return TheStructure.RawData[index];
    }

    inline void setNotifyEnable(const uint32_t value) {
        TheStructure.Common.NotifyEnable = value;
    }

    inline uint32_t getNotifyEnable() const {
        return (TheStructure.Common.NotifyEnable);
    }

    inline void setFlushLlc(const uint32_t value) {
        TheStructure.Common.FlushLlc = value;
    }

    inline uint32_t getFlushLlc() const {
        return (TheStructure.Common.FlushLlc);
    }

    inline void setFlushCcs(const uint32_t value) {
        TheStructure.Common.FlushCcs = value;
    }

    inline uint32_t getFlushCcs() const {
        return (TheStructure.Common.FlushCcs);
    }

    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }

    inline POST_SYNC_OPERATION getPostSyncOperation() const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.PostSyncOperation);
    }

    inline void setTlbInvalidate(const uint32_t value) {
        TheStructure.Common.TlbInvalidate = value;
    }

    inline uint32_t getTlbInvalidate() const {
        return (TheStructure.Common.TlbInvalidate);
    }

    inline void setStoreDataIndex(const uint32_t value) {
        TheStructure.Common.StoreDataIndex = value;
    }

    inline uint32_t getStoreDataIndex() const {
        return (TheStructure.Common.StoreDataIndex);
    }

    typedef enum tagDESTINATIONADDRESS {
        DESTINATIONADDRESS_BIT_SHIFT = 2,
        DESTINATIONADDRESS_ALIGN_SIZE = 4,
    } DESTINATIONADDRESS;

    inline void setDestinationAddress(const uint64_t value) {
        TheStructure.Common.DestinationAddress = value >> DESTINATIONADDRESS_BIT_SHIFT;
    }

    inline uint64_t getDestinationAddress() const {
        return (TheStructure.Common.DestinationAddress << DESTINATIONADDRESS_BIT_SHIFT);
    }

    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }

    inline uint64_t getImmediateData() const {
        return (TheStructure.Common.ImmediateData);
    }
};
STATIC_ASSERT(20 == sizeof(MI_FLUSH_DW));

typedef struct tag_3DSTATE_BTD {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t DispatchTimeoutCounter : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_2 : BITFIELD_RANGE(2, 2);
            uint32_t AmfsMode : BITFIELD_RANGE(3, 4);
            uint32_t Reserved_5 : BITFIELD_RANGE(5, 31);
            // DWORD 2
            uint64_t PerDssMemoryBackedBufferSize : BITFIELD_RANGE(0, 2);
            uint64_t Reserved_35 : BITFIELD_RANGE(3, 9);
            uint64_t MemoryBackedBufferBasePointer : BITFIELD_RANGE(10, 63);
            // DWORD 3
            uint64_t PerThreadScratchSpace : BITFIELD_RANGE(0, 3);
            uint64_t Reserved_100 : BITFIELD_RANGE(4, 9);
            uint64_t BtdScratchSpaceBasePointer : BITFIELD_RANGE(10, 31);
            // DWORD 4
            uint64_t Reserved_128 : BITFIELD_RANGE(32, 63);
        } Common;
        uint32_t RawData[6];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x4,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_3DSTATE_BTD = 0x6,
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
    typedef enum tagAMFS_MODE {
        AMFS_MODE_NORMAL_MODE = 0x0,
        AMFS_MODE_TOUCH_MODE = 0x1,
        AMFS_MODE_BACKFILL_MODE = 0x2,
        AMFS_MODE_FALLBACK_MODE = 0x3,
    } AMFS_MODE;
    typedef enum tagPER_DSS_MEMORY_BACKED_BUFFER_SIZE {
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_2KB = 0x0,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_4KB = 0x1,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_8KB = 0x2,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_16KB = 0x3,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_32KB = 0x4,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_64KB = 0x5,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_128KB = 0x6,
    } PER_DSS_MEMORY_BACKED_BUFFER_SIZE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_3DSTATE_BTD;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.AmfsMode = AMFS_MODE_NORMAL_MODE;
        TheStructure.Common.PerDssMemoryBackedBufferSize = PER_DSS_MEMORY_BACKED_BUFFER_SIZE_128KB;
    }
    static tag_3DSTATE_BTD sInit() {
        _3DSTATE_BTD state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 6);
        return TheStructure.RawData[index];
    }
    inline void setDispatchTimeoutCounter(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3);
        TheStructure.Common.DispatchTimeoutCounter = value;
    }
    inline uint32_t getDispatchTimeoutCounter() const {
        return TheStructure.Common.DispatchTimeoutCounter;
    }
    inline void setAmfsMode(const AMFS_MODE value) {
        TheStructure.Common.AmfsMode = value;
    }
    inline AMFS_MODE getAmfsMode() const {
        return static_cast<AMFS_MODE>(TheStructure.Common.AmfsMode);
    }
    inline void setPerDssMemoryBackedBufferSize(const PER_DSS_MEMORY_BACKED_BUFFER_SIZE value) {
        TheStructure.Common.PerDssMemoryBackedBufferSize = value;
    }
    inline PER_DSS_MEMORY_BACKED_BUFFER_SIZE getPerDssMemoryBackedBufferSize() const {
        return static_cast<PER_DSS_MEMORY_BACKED_BUFFER_SIZE>(TheStructure.Common.PerDssMemoryBackedBufferSize);
    }
    typedef enum tagMEMORYBACKEDBUFFERBASEPOINTER {
        MEMORYBACKEDBUFFERBASEPOINTER_BIT_SHIFT = 0xa,
        MEMORYBACKEDBUFFERBASEPOINTER_ALIGN_SIZE = 0x400,
    } MEMORYBACKEDBUFFERBASEPOINTER;
    inline void setMemoryBackedBufferBasePointer(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xffffffffffffffffL);
        TheStructure.Common.MemoryBackedBufferBasePointer = value >> MEMORYBACKEDBUFFERBASEPOINTER_BIT_SHIFT;
    }
    inline uint64_t getMemoryBackedBufferBasePointer() const {
        return TheStructure.Common.MemoryBackedBufferBasePointer << MEMORYBACKEDBUFFERBASEPOINTER_BIT_SHIFT;
    }
    inline void setPerThreadScratchSpace(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xfL);
        TheStructure.Common.PerThreadScratchSpace = value;
    }
    inline uint64_t getPerThreadScratchSpace() const {
        return TheStructure.Common.PerThreadScratchSpace;
    }
    typedef enum tagBTDSCRATCHSPACEBASEPOINTER {
        BTDSCRATCHSPACEBASEPOINTER_BIT_SHIFT = 0xa,
        BTDSCRATCHSPACEBASEPOINTER_ALIGN_SIZE = 0x400,
    } BTDSCRATCHSPACEBASEPOINTER;
    inline void setBtdScratchSpaceBasePointer(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xffffffffL);
        TheStructure.Common.BtdScratchSpaceBasePointer = value >> BTDSCRATCHSPACEBASEPOINTER_BIT_SHIFT;
    }
    inline uint64_t getBtdScratchSpaceBasePointer() const {
        return TheStructure.Common.BtdScratchSpaceBasePointer << BTDSCRATCHSPACEBASEPOINTER_BIT_SHIFT;
    }
} _3DSTATE_BTD;
STATIC_ASSERT(24 == sizeof(_3DSTATE_BTD));
STATIC_ASSERT(NEO::TypeTraits::isPodV<_3DSTATE_BTD>);

typedef struct tagGRF {
    union tagTheStructure {
        float fRegs[8];
        uint32_t dwRegs[8];
        uint16_t wRegs[16];
        uint32_t RawData[8];
    } TheStructure;
} GRF;
STATIC_ASSERT(32 == sizeof(GRF));

typedef struct tagPOSTSYNC_DATA {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t Operation : BITFIELD_RANGE(0, 1);
            uint32_t DataportPipelineFlush : BITFIELD_RANGE(2, 2);
            uint32_t Reserved_3 : BITFIELD_RANGE(3, 3);
            uint32_t MocsReserved_4 : BITFIELD_RANGE(4, 4);
            uint32_t MocsIndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint32_t SystemMemoryFenceRequest : BITFIELD_RANGE(11, 11);
            uint32_t DataportSubsliceCacheFlush : BITFIELD_RANGE(12, 12);
            uint32_t Reserved_13 : BITFIELD_RANGE(13, 31);
            // DWORD 1
            uint64_t DestinationAddress;
            // DWORD 3
            uint64_t ImmediateData;
        } Common;
        uint32_t RawData[5];
    } TheStructure;
    typedef enum tagOPERATION {
        OPERATION_NO_WRITE = 0x0,
        OPERATION_WRITE_IMMEDIATE_DATA = 0x1,
        OPERATION_WRITE_TIMESTAMP = 0x3,
    } OPERATION;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.Operation = OPERATION_NO_WRITE;
    }
    static tagPOSTSYNC_DATA sInit() {
        POSTSYNC_DATA state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 5);
        return TheStructure.RawData[index];
    }
    inline void setOperation(const OPERATION value) {
        TheStructure.Common.Operation = value;
    }
    inline OPERATION getOperation() const {
        return static_cast<OPERATION>(TheStructure.Common.Operation);
    }
    inline void setDataportPipelineFlush(const bool value) {
        TheStructure.Common.DataportPipelineFlush = value;
    }
    inline bool getDataportPipelineFlush() const {
        return TheStructure.Common.DataportPipelineFlush;
    }
    inline void setMocs(const uint32_t value) { // patched
        UNRECOVERABLE_IF(value > 0x7f);
        TheStructure.Common.MocsReserved_4 = value;
        TheStructure.Common.MocsIndexToMocsTables = value >> 1;
    }
    inline uint32_t getMocs() const { // patched
        return (TheStructure.Common.MocsIndexToMocsTables << 1) | TheStructure.Common.MocsReserved_4;
    }
    inline void setSystemMemoryFenceRequest(const bool value) {
        TheStructure.Common.SystemMemoryFenceRequest = value;
    }
    inline bool getSystemMemoryFenceRequest() const {
        return TheStructure.Common.SystemMemoryFenceRequest;
    }
    inline void setDataportSubsliceCacheFlush(const bool value) {
        TheStructure.Common.DataportSubsliceCacheFlush = value;
    }
    inline bool getDataportSubsliceCacheFlush() const {
        return TheStructure.Common.DataportSubsliceCacheFlush;
    }
    inline void setDestinationAddress(const uint64_t value) {
        TheStructure.Common.DestinationAddress = value;
    }
    inline uint64_t getDestinationAddress() const {
        return TheStructure.Common.DestinationAddress;
    }
    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }
    inline uint64_t getImmediateData() const {
        return TheStructure.Common.ImmediateData;
    }
} POSTSYNC_DATA;
STATIC_ASSERT(20 == sizeof(POSTSYNC_DATA));

typedef struct tagINTERFACE_DESCRIPTOR_DATA {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint64_t Reserved_0 : BITFIELD_RANGE(0, 5);
            uint64_t KernelStartPointer : BITFIELD_RANGE(6, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(32, 63);
            // DWORD 2
            uint32_t Reserved_64 : BITFIELD_RANGE(0, 6);
            uint32_t SoftwareExceptionEnable : BITFIELD_RANGE(7, 7);
            uint32_t Reserved_72 : BITFIELD_RANGE(8, 10);
            uint32_t MaskStackExceptionEnable : BITFIELD_RANGE(11, 11);
            uint32_t Reserved_76 : BITFIELD_RANGE(12, 12);
            uint32_t IllegalOpcodeExceptionEnable : BITFIELD_RANGE(13, 13);
            uint32_t Reserved_78 : BITFIELD_RANGE(14, 15);
            uint32_t FloatingPointMode : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_81 : BITFIELD_RANGE(17, 17);
            uint32_t SingleProgramFlow : BITFIELD_RANGE(18, 18);
            uint32_t DenormMode : BITFIELD_RANGE(19, 19);
            uint32_t ThreadPreemptionDisable : BITFIELD_RANGE(20, 20);
            uint32_t Reserved_85 : BITFIELD_RANGE(21, 31);
            // DWORD 3
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 1);
            uint32_t SamplerCount : BITFIELD_RANGE(2, 4);
            uint32_t SamplerStatePointer : BITFIELD_RANGE(5, 31);
            // DWORD 4
            uint32_t BindingTableEntryCount : BITFIELD_RANGE(0, 4);
            uint32_t BindingTablePointer : BITFIELD_RANGE(5, 20);
            uint32_t Reserved_149 : BITFIELD_RANGE(21, 31);
            // DWORD 5
            uint32_t NumberOfThreadsInGpgpuThreadGroup : BITFIELD_RANGE(0, 9);
            uint32_t Reserved_170 : BITFIELD_RANGE(10, 15);
            uint32_t SharedLocalMemorySize : BITFIELD_RANGE(16, 20);
            uint32_t Reserved_181 : BITFIELD_RANGE(21, 21);
            uint32_t RoundingMode : BITFIELD_RANGE(22, 23);
            uint32_t Reserved_184 : BITFIELD_RANGE(24, 25);
            uint32_t ThreadGroupDispatchSize : BITFIELD_RANGE(26, 27);
            uint32_t NumberOfBarriers : BITFIELD_RANGE(28, 30);
            uint32_t BtdMode : BITFIELD_RANGE(31, 31);
            // DWORD 6
            uint32_t PreferredSlmAllocationSize : BITFIELD_RANGE(0, 3);
            uint32_t Reserved_196 : BITFIELD_RANGE(4, 31);
            // DWORD 7
            uint32_t Reserved_224;
        } Common;
        uint32_t RawData[8];
    } TheStructure;
    typedef enum tagFLOATING_POINT_MODE {
        FLOATING_POINT_MODE_IEEE_754 = 0x0,
        FLOATING_POINT_MODE_ALTERNATE = 0x1,
    } FLOATING_POINT_MODE;
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
    typedef enum tagBINDING_TABLE_ENTRY_COUNT {
        BINDING_TABLE_ENTRY_COUNT_PREFETCH_DISABLED = 0x0,
        BINDING_TABLE_ENTRY_COUNT_PREFETCH_COUNT_MIN = 0x1,
        BINDING_TABLE_ENTRY_COUNT_PREFETCH_COUNT_MAX = 0x1f,
    } BINDING_TABLE_ENTRY_COUNT;
    typedef enum tagSHARED_LOCAL_MEMORY_SIZE { // patched
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K = 0x0,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K = 0x1,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K = 0x2,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K = 0x3,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K = 0x4,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K = 0x5,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K = 0x6,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K = 0x7,
    } SHARED_LOCAL_MEMORY_SIZE;
    typedef enum tagROUNDING_MODE {
        ROUNDING_MODE_RTNE = 0x0,
        ROUNDING_MODE_RU = 0x1,
        ROUNDING_MODE_RD = 0x2,
        ROUNDING_MODE_RTZ = 0x3,
    } ROUNDING_MODE;
    typedef enum tagTHREAD_GROUP_DISPATCH_SIZE {
        THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8 = 0x0,
        THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4 = 0x1,
        THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2 = 0x2,
        THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1 = 0x3,
    } THREAD_GROUP_DISPATCH_SIZE;
    typedef enum tagNUMBER_OF_BARRIERS {
        NUMBER_OF_BARRIERS_NONE = 0x0,
        NUMBER_OF_BARRIERS_B1 = 0x1,
    } NUMBER_OF_BARRIERS;
    typedef enum tagBTD_MODE {
        BTD_MODE_DISABLE = 0x0,
        BTD_MODE_ENABLE = 0x1,
    } BTD_MODE;
    typedef enum tagPREFERRED_SLM_ALLOCATION_SIZE {
        PREFERRED_SLM_ALLOCATION_SIZE_MAX = 0x0,
        PREFERRED_SLM_ALLOCATION_SIZE_0KB = 0x8,
        PREFERRED_SLM_ALLOCATION_SIZE_16KB = 0x9,
        PREFERRED_SLM_ALLOCATION_SIZE_32KB = 0xa,
        PREFERRED_SLM_ALLOCATION_SIZE_64KB = 0xb,
        PREFERRED_SLM_ALLOCATION_SIZE_96KB = 0xc,
        PREFERRED_SLM_ALLOCATION_SIZE_128KB = 0xd,
    } PREFERRED_SLM_ALLOCATION_SIZE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.FloatingPointMode = FLOATING_POINT_MODE_IEEE_754;
        TheStructure.Common.SingleProgramFlow = SINGLE_PROGRAM_FLOW_MULTIPLE;
        TheStructure.Common.DenormMode = DENORM_MODE_FTZ;
        TheStructure.Common.ThreadPreemptionDisable = THREAD_PREEMPTION_DISABLE_DISABLE;
        TheStructure.Common.SamplerCount = SAMPLER_COUNT_NO_SAMPLERS_USED;
        TheStructure.Common.BindingTableEntryCount = BINDING_TABLE_ENTRY_COUNT_PREFETCH_DISABLED;
        TheStructure.Common.SharedLocalMemorySize = SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K; // patched
        TheStructure.Common.RoundingMode = ROUNDING_MODE_RTNE;
        TheStructure.Common.ThreadGroupDispatchSize = THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8;
        TheStructure.Common.NumberOfBarriers = NUMBER_OF_BARRIERS_NONE;
        TheStructure.Common.BtdMode = BTD_MODE_DISABLE;
        TheStructure.Common.PreferredSlmAllocationSize = PREFERRED_SLM_ALLOCATION_SIZE_MAX;
    }
    static tagINTERFACE_DESCRIPTOR_DATA sInit() {
        INTERFACE_DESCRIPTOR_DATA state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 8);
        return TheStructure.RawData[index];
    }
    typedef enum tagKERNELSTARTPOINTER {
        KERNELSTARTPOINTER_BIT_SHIFT = 0x6,
        KERNELSTARTPOINTER_ALIGN_SIZE = 0x40,
    } KERNELSTARTPOINTER;
    inline void setKernelStartPointer(const uint64_t value) {
        TheStructure.Common.KernelStartPointer = static_cast<uint32_t>(value) >> KERNELSTARTPOINTER_BIT_SHIFT;
    }
    inline uint64_t getKernelStartPointer() const {
        return static_cast<uint64_t>(TheStructure.Common.KernelStartPointer) << KERNELSTARTPOINTER_BIT_SHIFT; // patched
    }
    inline void setSoftwareExceptionEnable(const bool value) {
        TheStructure.Common.SoftwareExceptionEnable = value;
    }
    inline bool getSoftwareExceptionEnable() const {
        return TheStructure.Common.SoftwareExceptionEnable;
    }
    inline void setMaskStackExceptionEnable(const bool value) {
        TheStructure.Common.MaskStackExceptionEnable = value;
    }
    inline bool getMaskStackExceptionEnable() const {
        return TheStructure.Common.MaskStackExceptionEnable;
    }
    inline void setIllegalOpcodeExceptionEnable(const bool value) {
        TheStructure.Common.IllegalOpcodeExceptionEnable = value;
    }
    inline bool getIllegalOpcodeExceptionEnable() const {
        return TheStructure.Common.IllegalOpcodeExceptionEnable;
    }
    inline void setFloatingPointMode(const FLOATING_POINT_MODE value) {
        TheStructure.Common.FloatingPointMode = value;
    }
    inline FLOATING_POINT_MODE getFloatingPointMode() const {
        return static_cast<FLOATING_POINT_MODE>(TheStructure.Common.FloatingPointMode);
    }
    inline void setSingleProgramFlow(const SINGLE_PROGRAM_FLOW value) {
        TheStructure.Common.SingleProgramFlow = value;
    }
    inline SINGLE_PROGRAM_FLOW getSingleProgramFlow() const {
        return static_cast<SINGLE_PROGRAM_FLOW>(TheStructure.Common.SingleProgramFlow);
    }
    inline void setDenormMode(const DENORM_MODE value) {
        TheStructure.Common.DenormMode = value;
    }
    inline DENORM_MODE getDenormMode() const {
        return static_cast<DENORM_MODE>(TheStructure.Common.DenormMode);
    }
    inline void setThreadPreemptionDisable(const THREAD_PREEMPTION_DISABLE value) {
        UNRECOVERABLE_IF(true); // patched
        TheStructure.Common.ThreadPreemptionDisable = value;
    }
    inline THREAD_PREEMPTION_DISABLE getThreadPreemptionDisable() const {
        UNRECOVERABLE_IF(true); // patched
        return static_cast<THREAD_PREEMPTION_DISABLE>(TheStructure.Common.ThreadPreemptionDisable);
    }
    inline void setSamplerCount(const SAMPLER_COUNT value) {
        TheStructure.Common.SamplerCount = value;
    }
    inline SAMPLER_COUNT getSamplerCount() const {
        return static_cast<SAMPLER_COUNT>(TheStructure.Common.SamplerCount);
    }
    typedef enum tagSAMPLERSTATEPOINTER {
        SAMPLERSTATEPOINTER_BIT_SHIFT = 0x5,
        SAMPLERSTATEPOINTER_ALIGN_SIZE = 0x20,
    } SAMPLERSTATEPOINTER;
    inline void setSamplerStatePointer(const uint32_t value) {
        TheStructure.Common.SamplerStatePointer = static_cast<uint32_t>(value) >> SAMPLERSTATEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getSamplerStatePointer() const {
        return TheStructure.Common.SamplerStatePointer << SAMPLERSTATEPOINTER_BIT_SHIFT;
    }
    inline void setBindingTableEntryCount(const uint32_t value) {
        TheStructure.Common.BindingTableEntryCount = value;
    }
    inline uint32_t getBindingTableEntryCount() const {
        return TheStructure.Common.BindingTableEntryCount;
    }
    typedef enum tagBINDINGTABLEPOINTER {
        BINDINGTABLEPOINTER_BIT_SHIFT = 0x5,
        BINDINGTABLEPOINTER_ALIGN_SIZE = 0x20,
    } BINDINGTABLEPOINTER;
    inline void setBindingTablePointer(const uint32_t value) {
        TheStructure.Common.BindingTablePointer = static_cast<uint32_t>(value) >> BINDINGTABLEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getBindingTablePointer() const {
        return TheStructure.Common.BindingTablePointer << BINDINGTABLEPOINTER_BIT_SHIFT;
    }
    inline void setNumberOfThreadsInGpgpuThreadGroup(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff);
        TheStructure.Common.NumberOfThreadsInGpgpuThreadGroup = value;
    }
    inline uint32_t getNumberOfThreadsInGpgpuThreadGroup() const {
        return TheStructure.Common.NumberOfThreadsInGpgpuThreadGroup;
    }
    inline void setSharedLocalMemorySize(const uint32_t value) { // patched
        TheStructure.Common.SharedLocalMemorySize = value;
    }
    inline uint32_t getSharedLocalMemorySize() const { // patched
        return static_cast<uint32_t>(TheStructure.Common.SharedLocalMemorySize);
    }
    inline void setRoundingMode(const ROUNDING_MODE value) {
        TheStructure.Common.RoundingMode = value;
    }
    inline ROUNDING_MODE getRoundingMode() const {
        return static_cast<ROUNDING_MODE>(TheStructure.Common.RoundingMode);
    }
    inline void setThreadGroupDispatchSize(const THREAD_GROUP_DISPATCH_SIZE value) {
        TheStructure.Common.ThreadGroupDispatchSize = value;
    }
    inline THREAD_GROUP_DISPATCH_SIZE getThreadGroupDispatchSize() const {
        return static_cast<THREAD_GROUP_DISPATCH_SIZE>(TheStructure.Common.ThreadGroupDispatchSize);
    }
    inline void setNumberOfBarriers(const NUMBER_OF_BARRIERS value) {
        TheStructure.Common.NumberOfBarriers = value;
    }
    inline NUMBER_OF_BARRIERS getNumberOfBarriers() const {
        return static_cast<NUMBER_OF_BARRIERS>(TheStructure.Common.NumberOfBarriers);
    }
    inline void setBtdMode(const BTD_MODE value) {
        TheStructure.Common.BtdMode = value;
    }
    inline BTD_MODE getBtdMode() const {
        return static_cast<BTD_MODE>(TheStructure.Common.BtdMode);
    }
    inline void setPreferredSlmAllocationSize(const PREFERRED_SLM_ALLOCATION_SIZE value) {
        TheStructure.Common.PreferredSlmAllocationSize = value;
    }
    inline PREFERRED_SLM_ALLOCATION_SIZE getPreferredSlmAllocationSize() const {
        return static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(TheStructure.Common.PreferredSlmAllocationSize);
    }
} INTERFACE_DESCRIPTOR_DATA;
STATIC_ASSERT(32 == sizeof(INTERFACE_DESCRIPTOR_DATA));

typedef struct tagCOMPUTE_WALKER {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t PredicateEnable : BITFIELD_RANGE(8, 8);
            uint32_t WorkloadPartitionEnable : BITFIELD_RANGE(9, 9);
            uint32_t IndirectParameterEnable : BITFIELD_RANGE(10, 10);
            uint32_t Reserved_11 : BITFIELD_RANGE(11, 13);
            uint32_t SystolicModeEnable : BITFIELD_RANGE(14, 14);
            uint32_t Reserved_15 : BITFIELD_RANGE(15, 15);
            uint32_t CfeSubopcodeVariant : BITFIELD_RANGE(16, 17);
            uint32_t CfeSubopcode : BITFIELD_RANGE(18, 23);
            uint32_t ComputeCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t Pipeline : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 7);
            uint32_t DebugObjectId : BITFIELD_RANGE(8, 31);
            // DWORD 2
            uint32_t IndirectDataLength : BITFIELD_RANGE(0, 16);
            uint32_t L3PrefetchDisable : BITFIELD_RANGE(17, 17);
            uint32_t PartitionDispatchParameter : BITFIELD_RANGE(18, 29);
            uint32_t PartitionType : BITFIELD_RANGE(30, 31);
            // DWORD 3
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 5);
            uint32_t IndirectDataStartAddress : BITFIELD_RANGE(6, 31);
            // DWORD 4
            uint32_t Reserved_128 : BITFIELD_RANGE(0, 4);
            uint32_t DispatchWalkOrder : BITFIELD_RANGE(5, 6);
            uint32_t Reserved_135 : BITFIELD_RANGE(7, 16);
            uint32_t MessageSimd : BITFIELD_RANGE(17, 18);
            uint32_t TileLayout : BITFIELD_RANGE(19, 21);
            uint32_t WalkOrder : BITFIELD_RANGE(22, 24);
            uint32_t EmitInlineParameter : BITFIELD_RANGE(25, 25);
            uint32_t EmitLocal : BITFIELD_RANGE(26, 28);
            uint32_t GenerateLocalId : BITFIELD_RANGE(29, 29);
            uint32_t SimdSize : BITFIELD_RANGE(30, 31);
            // DWORD 5
            uint32_t ExecutionMask;
            // DWORD 6
            uint32_t LocalXMaximum : BITFIELD_RANGE(0, 9);
            uint32_t LocalYMaximum : BITFIELD_RANGE(10, 19);
            uint32_t LocalZMaximum : BITFIELD_RANGE(20, 29);
            uint32_t Reserved_222 : BITFIELD_RANGE(30, 31);
            // DWORD 7
            uint32_t ThreadGroupIdXDimension;
            // DWORD 8
            uint32_t ThreadGroupIdYDimension;
            // DWORD 9
            uint32_t ThreadGroupIdZDimension;
            // DWORD 10
            uint32_t ThreadGroupIdStartingX;
            // DWORD 11
            uint32_t ThreadGroupIdStartingY;
            // DWORD 12
            uint32_t ThreadGroupIdStartingZ;
            // DWORD 13
            uint64_t PartitionId : BITFIELD_RANGE(0, 31);
            // DWORD 14
            uint64_t PartitionSize : BITFIELD_RANGE(32, 63);
            // DWORD 15
            uint32_t PreemptX;
            // DWORD 16
            uint32_t PreemptY;
            // DWORD 17
            uint32_t PreemptZ;
            // DWORD 18
            INTERFACE_DESCRIPTOR_DATA InterfaceDescriptor;
            // DWORD 26
            POSTSYNC_DATA PostSync;
            // DWORD 31
            uint32_t InlineData[8];
        } Common;
        uint32_t RawData[39];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_FIXED_SIZE = 0x25,
    } DWORD_LENGTH;
    typedef enum tagCFE_SUBOPCODE_VARIANT {
        CFE_SUBOPCODE_VARIANT_STANDARD = 0x0,
        CFE_SUBOPCODE_VARIANT_TG_RESUME = 0x1,
    } CFE_SUBOPCODE_VARIANT;
    typedef enum tagCFE_SUBOPCODE {
        CFE_SUBOPCODE_COMPUTE_WALKER = 0x2,
    } CFE_SUBOPCODE;
    typedef enum tagCOMPUTE_COMMAND_OPCODE {
        COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND = 0x2,
    } COMPUTE_COMMAND_OPCODE;
    typedef enum tagPIPELINE {
        PIPELINE_COMPUTE = 0x2,
    } PIPELINE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagPARTITION_TYPE {
        PARTITION_TYPE_DISABLED = 0x0,
        PARTITION_TYPE_X = 0x1,
        PARTITION_TYPE_Y = 0x2,
        PARTITION_TYPE_Z = 0x3,
    } PARTITION_TYPE;
    typedef enum tagMESSAGE_SIMD {
        MESSAGE_SIMD_SIMD8 = 0x0,
        MESSAGE_SIMD_SIMD16 = 0x1,
        MESSAGE_SIMD_SIMD32 = 0x2,
    } MESSAGE_SIMD;
    typedef enum tagTILE_LAYOUT {
        TILE_LAYOUT_LINEAR = 0x0,
        TILE_LAYOUT_TILEY_32BPE = 0x1,
        TILE_LAYOUT_TILEY_64BPE = 0x2,
        TILE_LAYOUT_TILEY_128BPE = 0x3,
    } TILE_LAYOUT;
    typedef enum tagWALK_ORDER {
        WALK_ORDER_WALK_012 = 0x0,
        WALK_ORDER_WALK_021 = 0x1,
        WALK_ORDER_WALK_102 = 0x2,
        WALK_ORDER_WALK_120 = 0x3,
        WALK_ORDER_WALK_201 = 0x4,
        WALK_ORDER_WALK_210 = 0x5,
    } WALK_ORDER;
    typedef enum tagEMIT_LOCAL {
        EMIT_LOCAL_EMIT_NONE = 0x0,
        EMIT_LOCAL_EMIT_X = 0x1,
        EMIT_LOCAL_EMIT_XY = 0x3,
        EMIT_LOCAL_EMIT_XYZ = 0x7,
    } EMIT_LOCAL;
    typedef enum tagSIMD_SIZE {
        SIMD_SIZE_SIMD8 = 0x0,
        SIMD_SIZE_SIMD16 = 0x1,
        SIMD_SIZE_SIMD32 = 0x2,
    } SIMD_SIZE;
    typedef enum tagPARTITION_ID {
        PARTITION_ID_SUPPORTED_MIN = 0x0,
        PARTITION_ID_SUPPORTED_MAX = 0xf,
    } PARTITION_ID;
    typedef enum tagDISPATCH_WALK_ORDER { // patched
        DISPATCH_WALK_ORDER_LINEAR_WALK = 0x0,
        DISPATCH_WALK_ORDER_Y_ORDER_WALK = 0x1,
    } DISPATCH_WALK_ORDER;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_FIXED_SIZE;
        TheStructure.Common.CfeSubopcodeVariant = CFE_SUBOPCODE_VARIANT_STANDARD;
        TheStructure.Common.CfeSubopcode = CFE_SUBOPCODE_COMPUTE_WALKER;
        TheStructure.Common.ComputeCommandOpcode = COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND;
        TheStructure.Common.Pipeline = PIPELINE_COMPUTE;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.PartitionType = PARTITION_TYPE_DISABLED;
        TheStructure.Common.MessageSimd = MESSAGE_SIMD_SIMD8;
        TheStructure.Common.TileLayout = TILE_LAYOUT_LINEAR;
        TheStructure.Common.WalkOrder = WALK_ORDER_WALK_012;
        TheStructure.Common.EmitLocal = EMIT_LOCAL_EMIT_NONE;
        TheStructure.Common.SimdSize = SIMD_SIZE_SIMD8;
        TheStructure.Common.InterfaceDescriptor.init();
        TheStructure.Common.PostSync.init();
        for (uint32_t i = 0; i < 8; i++) {
            TheStructure.Common.InlineData[i] = 0;
        }
    }
    static tagCOMPUTE_WALKER sInit() {
        COMPUTE_WALKER state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 39);
        return TheStructure.RawData[index];
    }
    inline void setDwordLength(const DWORD_LENGTH value) { // patched
        TheStructure.Common.DwordLength = value;
    }
    inline DWORD_LENGTH getDwordLength() const { // patched
        return static_cast<DWORD_LENGTH>(TheStructure.Common.DwordLength);
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
    inline void setWorkloadPartitionEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionEnable = value;
    }
    inline bool getWorkloadPartitionEnable() const {
        return TheStructure.Common.WorkloadPartitionEnable;
    }
    inline void setIndirectParameterEnable(const bool value) {
        TheStructure.Common.IndirectParameterEnable = value;
    }
    inline bool getIndirectParameterEnable() const {
        return TheStructure.Common.IndirectParameterEnable;
    }
    inline void setSystolicModeEnable(const bool value) {
        TheStructure.Common.SystolicModeEnable = value;
    }
    inline bool getSystolicModeEnable() const {
        return TheStructure.Common.SystolicModeEnable;
    }
    inline void setCfeSubopcodeVariant(const CFE_SUBOPCODE_VARIANT value) {
        TheStructure.Common.CfeSubopcodeVariant = value;
    }
    inline CFE_SUBOPCODE_VARIANT getCfeSubopcodeVariant() const {
        return static_cast<CFE_SUBOPCODE_VARIANT>(TheStructure.Common.CfeSubopcodeVariant);
    }
    inline void setCfeSubopcode(const CFE_SUBOPCODE value) {
        TheStructure.Common.CfeSubopcode = value;
    }
    inline CFE_SUBOPCODE getCfeSubopcode() const {
        return static_cast<CFE_SUBOPCODE>(TheStructure.Common.CfeSubopcode);
    }
    inline void setComputeCommandOpcode(const COMPUTE_COMMAND_OPCODE value) {
        TheStructure.Common.ComputeCommandOpcode = value;
    }
    inline COMPUTE_COMMAND_OPCODE getComputeCommandOpcode() const {
        return static_cast<COMPUTE_COMMAND_OPCODE>(TheStructure.Common.ComputeCommandOpcode);
    }
    inline void setDebugObjectId(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffff);
        TheStructure.Common.DebugObjectId = value;
    }
    inline uint32_t getDebugObjectId() const {
        return TheStructure.Common.DebugObjectId;
    }
    inline void setIndirectDataLength(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1ffff);
        TheStructure.Common.IndirectDataLength = value;
    }
    inline uint32_t getIndirectDataLength() const {
        return TheStructure.Common.IndirectDataLength;
    }
    inline void setL3PrefetchDisable(const bool value) {
        TheStructure.Common.L3PrefetchDisable = value;
    }
    inline bool getL3PrefetchDisable() const {
        return TheStructure.Common.L3PrefetchDisable;
    }
    inline void setPartitionDispatchParameter(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfff);
        TheStructure.Common.PartitionDispatchParameter = value;
    }
    inline uint32_t getPartitionDispatchParameter() const {
        return TheStructure.Common.PartitionDispatchParameter;
    }
    inline void setPartitionType(const PARTITION_TYPE value) {
        TheStructure.Common.PartitionType = value;
    }
    inline PARTITION_TYPE getPartitionType() const {
        return static_cast<PARTITION_TYPE>(TheStructure.Common.PartitionType);
    }
    inline void setDispatchWalkOrder(const DISPATCH_WALK_ORDER value) { // patched
        TheStructure.Common.DispatchWalkOrder = value;
    }
    inline DISPATCH_WALK_ORDER getDispatchWalkOrder() const { // patched
        return static_cast<DISPATCH_WALK_ORDER>(TheStructure.Common.DispatchWalkOrder);
    }
    typedef enum tagINDIRECTDATASTARTADDRESS {
        INDIRECTDATASTARTADDRESS_BIT_SHIFT = 0x6,
        INDIRECTDATASTARTADDRESS_ALIGN_SIZE = 0x40,
    } INDIRECTDATASTARTADDRESS;
    inline void setIndirectDataStartAddress(const uint32_t value) {
        TheStructure.Common.IndirectDataStartAddress = value >> INDIRECTDATASTARTADDRESS_BIT_SHIFT;
    }
    inline uint32_t getIndirectDataStartAddress() const {
        return TheStructure.Common.IndirectDataStartAddress << INDIRECTDATASTARTADDRESS_BIT_SHIFT;
    }
    inline void setMessageSimd(const uint32_t value) { // patched
        TheStructure.Common.MessageSimd = value;
    }
    inline uint32_t getMessageSimd() const { // patched
        return (TheStructure.Common.MessageSimd);
    }
    inline void setTileLayout(const uint32_t value) { // patched
        TheStructure.Common.TileLayout = value;
    }
    inline uint32_t getTileLayout() const { // patched
        return (TheStructure.Common.TileLayout);
    }
    inline void setWalkOrder(const uint32_t value) { // patched
        TheStructure.Common.WalkOrder = value;
    }
    inline uint32_t getWalkOrder() const { // patched
        return (TheStructure.Common.WalkOrder);
    }
    inline void setEmitInlineParameter(const bool value) {
        TheStructure.Common.EmitInlineParameter = value;
    }
    inline bool getEmitInlineParameter() const {
        return TheStructure.Common.EmitInlineParameter;
    }
    inline void setEmitLocalId(const uint32_t value) { // patched
        TheStructure.Common.EmitLocal = value;
    }
    inline uint32_t getEmitLocalId() const { // patched
        return TheStructure.Common.EmitLocal;
    }
    inline void setGenerateLocalId(const bool value) {
        TheStructure.Common.GenerateLocalId = value;
    }
    inline bool getGenerateLocalId() const {
        return TheStructure.Common.GenerateLocalId;
    }
    inline void setSimdSize(const SIMD_SIZE value) {
        TheStructure.Common.SimdSize = value;
    }
    inline SIMD_SIZE getSimdSize() const {
        return static_cast<SIMD_SIZE>(TheStructure.Common.SimdSize);
    }
    inline void setExecutionMask(const uint32_t value) {
        TheStructure.Common.ExecutionMask = value;
    }
    inline uint32_t getExecutionMask() const {
        return TheStructure.Common.ExecutionMask;
    }
    inline void setLocalXMaximum(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff);
        TheStructure.Common.LocalXMaximum = value;
    }
    inline uint32_t getLocalXMaximum() const {
        return TheStructure.Common.LocalXMaximum;
    }
    inline void setLocalYMaximum(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff);
        TheStructure.Common.LocalYMaximum = value;
    }
    inline uint32_t getLocalYMaximum() const {
        return TheStructure.Common.LocalYMaximum;
    }
    inline void setLocalZMaximum(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff);
        TheStructure.Common.LocalZMaximum = value;
    }
    inline uint32_t getLocalZMaximum() const {
        return TheStructure.Common.LocalZMaximum;
    }
    inline void setThreadGroupIdXDimension(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdXDimension = value;
    }
    inline uint32_t getThreadGroupIdXDimension() const {
        return TheStructure.Common.ThreadGroupIdXDimension;
    }
    inline void setThreadGroupIdYDimension(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdYDimension = value;
    }
    inline uint32_t getThreadGroupIdYDimension() const {
        return TheStructure.Common.ThreadGroupIdYDimension;
    }
    inline void setThreadGroupIdZDimension(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdZDimension = value;
    }
    inline uint32_t getThreadGroupIdZDimension() const {
        return TheStructure.Common.ThreadGroupIdZDimension;
    }
    inline void setThreadGroupIdStartingX(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdStartingX = value;
    }
    inline uint32_t getThreadGroupIdStartingX() const {
        return TheStructure.Common.ThreadGroupIdStartingX;
    }
    inline void setThreadGroupIdStartingY(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdStartingY = value;
    }
    inline uint32_t getThreadGroupIdStartingY() const {
        return TheStructure.Common.ThreadGroupIdStartingY;
    }
    inline void setThreadGroupIdStartingZ(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdStartingZ = value;
    }
    inline uint32_t getThreadGroupIdStartingZ() const {
        return TheStructure.Common.ThreadGroupIdStartingZ;
    }
    inline void setPartitionId(const uint64_t value) {
        TheStructure.Common.PartitionId = value;
    }
    inline uint64_t getPartitionId() const {
        return TheStructure.Common.PartitionId;
    }
    inline void setPartitionSize(const uint64_t value) {
        TheStructure.Common.PartitionSize = value;
    }
    inline uint64_t getPartitionSize() const {
        return TheStructure.Common.PartitionSize;
    }
    inline void setPreemptX(const uint32_t value) {
        TheStructure.Common.PreemptX = value;
    }
    inline uint32_t getPreemptX() const {
        return TheStructure.Common.PreemptX;
    }
    inline void setPreemptY(const uint32_t value) {
        TheStructure.Common.PreemptY = value;
    }
    inline uint32_t getPreemptY() const {
        return TheStructure.Common.PreemptY;
    }
    inline void setPreemptZ(const uint32_t value) {
        TheStructure.Common.PreemptZ = value;
    }
    inline uint32_t getPreemptZ() const {
        return TheStructure.Common.PreemptZ;
    }
    inline void setInterfaceDescriptor(const INTERFACE_DESCRIPTOR_DATA &value) {
        TheStructure.Common.InterfaceDescriptor = value;
    }
    inline INTERFACE_DESCRIPTOR_DATA &getInterfaceDescriptor() {
        return TheStructure.Common.InterfaceDescriptor;
    }
    inline void setPostSync(const POSTSYNC_DATA &value) {
        TheStructure.Common.PostSync = value;
    }
    inline POSTSYNC_DATA &getPostSync() {
        return TheStructure.Common.PostSync;
    }
    inline uint32_t *getInlineDataPointer() {
        return reinterpret_cast<uint32_t *>(&TheStructure.Common.InlineData);
    }
    static constexpr uint32_t getInlineDataSize() {
        return sizeof(TheStructure.Common.InlineData);
    }
    using InterfaceDescriptorType = std::decay_t<decltype(TheStructure.Common.InterfaceDescriptor)>; // patched
} COMPUTE_WALKER;
STATIC_ASSERT(156 == sizeof(COMPUTE_WALKER));

typedef struct tagCFE_STATE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t CfeSubopcodeVariant : BITFIELD_RANGE(16, 17);
            uint32_t CfeSubopcode : BITFIELD_RANGE(18, 23);
            uint32_t ComputeCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t Pipeline : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 7);
            uint64_t Reserved_40 : BITFIELD_RANGE(8, 9);
            uint64_t ScratchSpaceBuffer : BITFIELD_RANGE(10, 31);
            // DWORD 2
            uint64_t Reserved_64 : BITFIELD_RANGE(32, 63);
            // DWORD 3
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 2);
            uint32_t NumberOfWalkers : BITFIELD_RANGE(3, 5);
            uint32_t FusedEuDispatch : BITFIELD_RANGE(6, 6);
            uint32_t Reserved_103 : BITFIELD_RANGE(7, 9);
            uint32_t LargeGRFThreadAdjustDisable : BITFIELD_RANGE(10, 10);
            uint32_t ComputeOverdispatchDisable : BITFIELD_RANGE(11, 11);
            uint32_t Reserved_108 : BITFIELD_RANGE(12, 12);
            uint32_t SingleSliceDispatchCcsMode : BITFIELD_RANGE(13, 13);
            uint32_t OverDispatchControl : BITFIELD_RANGE(14, 15);
            uint32_t MaximumNumberOfThreads : BITFIELD_RANGE(16, 31);
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160 : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_161 : BITFIELD_RANGE(1, 10);
            uint32_t Reserved_171 : BITFIELD_RANGE(11, 31);
        } Common;
        uint32_t RawData[6];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x4,
    } DWORD_LENGTH;
    typedef enum tagCFE_SUBOPCODE_VARIANT {
        CFE_SUBOPCODE_VARIANT_STANDARD = 0x0,
    } CFE_SUBOPCODE_VARIANT;
    typedef enum tagCFE_SUBOPCODE {
        CFE_SUBOPCODE_CFE_STATE = 0x0,
    } CFE_SUBOPCODE;
    typedef enum tagCOMPUTE_COMMAND_OPCODE {
        COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND = 0x2,
    } COMPUTE_COMMAND_OPCODE;
    typedef enum tagPIPELINE {
        PIPELINE_COMPUTE = 0x2,
    } PIPELINE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagOVER_DISPATCH_CONTROL {
        OVER_DISPATCH_CONTROL_NONE = 0x0,
        OVER_DISPATCH_CONTROL_LOW = 0x1,
        OVER_DISPATCH_CONTROL_NORMAL = 0x2,
        OVER_DISPATCH_CONTROL_HIGH = 0x3,
    } OVER_DISPATCH_CONTROL;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common.CfeSubopcodeVariant = CFE_SUBOPCODE_VARIANT_STANDARD;
        TheStructure.Common.CfeSubopcode = CFE_SUBOPCODE_CFE_STATE;
        TheStructure.Common.ComputeCommandOpcode = COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND;
        TheStructure.Common.Pipeline = PIPELINE_COMPUTE;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.OverDispatchControl = OVER_DISPATCH_CONTROL_NORMAL;
    }
    static tagCFE_STATE sInit() {
        CFE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 6);
        return TheStructure.RawData[index];
    }
    inline void setCfeSubopcodeVariant(const CFE_SUBOPCODE_VARIANT value) {
        TheStructure.Common.CfeSubopcodeVariant = value;
    }
    inline CFE_SUBOPCODE_VARIANT getCfeSubopcodeVariant() const {
        return static_cast<CFE_SUBOPCODE_VARIANT>(TheStructure.Common.CfeSubopcodeVariant);
    }
    inline void setCfeSubopcode(const CFE_SUBOPCODE value) {
        TheStructure.Common.CfeSubopcode = value;
    }
    inline CFE_SUBOPCODE getCfeSubopcode() const {
        return static_cast<CFE_SUBOPCODE>(TheStructure.Common.CfeSubopcode);
    }
    inline void setComputeCommandOpcode(const COMPUTE_COMMAND_OPCODE value) {
        TheStructure.Common.ComputeCommandOpcode = value;
    }
    inline COMPUTE_COMMAND_OPCODE getComputeCommandOpcode() const {
        return static_cast<COMPUTE_COMMAND_OPCODE>(TheStructure.Common.ComputeCommandOpcode);
    }
    typedef enum tagSCRATCHSPACEBUFFER {
        SCRATCHSPACEBUFFER_BIT_SHIFT = 0x6,
        SCRATCHSPACEBUFFER_ALIGN_SIZE = 0x40,
    } SCRATCHSPACEBUFFER;
    inline void setScratchSpaceBuffer(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xfffffc00L);
        TheStructure.Common.ScratchSpaceBuffer = static_cast<uint32_t>(value) >> SCRATCHSPACEBUFFER_BIT_SHIFT;
    }
    inline uint64_t getScratchSpaceBuffer() const {
        return TheStructure.Common.ScratchSpaceBuffer << SCRATCHSPACEBUFFER_BIT_SHIFT;
    }
    inline void setNumberOfWalkers(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x38);
        TheStructure.Common.NumberOfWalkers = value - 1;
    }
    inline uint32_t getNumberOfWalkers() const {
        return TheStructure.Common.NumberOfWalkers + 1;
    }
    inline void setFusedEuDispatch(const bool value) {
        TheStructure.Common.FusedEuDispatch = value;
    }
    inline bool getFusedEuDispatch() const {
        return TheStructure.Common.FusedEuDispatch;
    }
    inline void setLargeGRFThreadAdjustDisable(const bool value) {
        TheStructure.Common.LargeGRFThreadAdjustDisable = value;
    }
    inline bool getLargeGRFThreadAdjustDisable() const {
        return TheStructure.Common.LargeGRFThreadAdjustDisable;
    }
    inline void setComputeOverdispatchDisable(const bool value) {
        TheStructure.Common.ComputeOverdispatchDisable = value;
    }
    inline bool getComputeOverdispatchDisable() const {
        return TheStructure.Common.ComputeOverdispatchDisable;
    }
    inline void setSingleSliceDispatchCcsMode(const bool value) {
        TheStructure.Common.SingleSliceDispatchCcsMode = value;
    }
    inline bool getSingleSliceDispatchCcsMode() const {
        return TheStructure.Common.SingleSliceDispatchCcsMode;
    }
    inline void setOverDispatchControl(const OVER_DISPATCH_CONTROL value) {
        TheStructure.Common.OverDispatchControl = value;
    }
    inline OVER_DISPATCH_CONTROL getOverDispatchControl() const {
        return static_cast<OVER_DISPATCH_CONTROL>(TheStructure.Common.OverDispatchControl);
    }
    inline void setMaximumNumberOfThreads(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff0000);
        TheStructure.Common.MaximumNumberOfThreads = value;
    }
    inline uint32_t getMaximumNumberOfThreads() const {
        return TheStructure.Common.MaximumNumberOfThreads;
    }
} CFE_STATE;
STATIC_ASSERT(24 == sizeof(CFE_STATE));

typedef struct tagMI_ARB_CHECK {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Pre_FetchDisable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_1 : BITFIELD_RANGE(1, 7);
            uint32_t MaskBits : BITFIELD_RANGE(8, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_ARB_CHECK = 0x5,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_ARB_CHECK;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_ARB_CHECK sInit() {
        MI_ARB_CHECK state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setPreFetchDisable(const uint32_t value) {
        TheStructure.Common.Pre_FetchDisable = value;
        TheStructure.Common.MaskBits = 1 << 0; // PreFetchDisable is at bit0, so set bit0 of mask to 1
    }
    inline uint32_t getPreFetchDisable() const {
        return TheStructure.Common.Pre_FetchDisable;
    }

    // patched for easier templates usage
    inline void setPreParserDisable(const uint32_t value) {
        setPreFetchDisable(value);
    }

    // patched for easier templates usage
    inline uint32_t getPreParserDisable() const {
        return getPreFetchDisable();
    }

    inline void setMaskBits(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0xff00);
        TheStructure.Common.MaskBits = value;
    }
    inline uint32_t getMaskBits() const {
        return TheStructure.Common.MaskBits;
    }
} MI_ARB_CHECK;
STATIC_ASSERT(4 == sizeof(MI_ARB_CHECK));

typedef struct tagMI_BATCH_BUFFER_START {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t AddressSpaceIndicator : BITFIELD_RANGE(8, 8);
            uint32_t Reserved_9 : BITFIELD_RANGE(9, 9);
            uint32_t ResourceStreamerEnable : BITFIELD_RANGE(10, 10);
            uint32_t Reserved_11 : BITFIELD_RANGE(11, 14);
            uint32_t PredicationEnable : BITFIELD_RANGE(15, 15);
            uint32_t AddOffsetEnable : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_17 : BITFIELD_RANGE(17, 18);
            uint32_t EnableCommandCache : BITFIELD_RANGE(19, 19);
            uint32_t PoshEnable : BITFIELD_RANGE(20, 20);
            uint32_t PoshStart : BITFIELD_RANGE(21, 21);
            uint32_t SecondLevelBatchBuffer : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint64_t BatchBufferStartAddress : BITFIELD_RANGE(2, 47);
            uint64_t BatchBufferStartAddress_Reserved_80 : BITFIELD_RANGE(48, 63);
        } Common;
        struct tagMi_Mode_Nestedbatchbufferenableis0 {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 21);
            uint32_t SecondLevelBatchBuffer : BITFIELD_RANGE(22, 22);
            uint32_t Reserved_23 : BITFIELD_RANGE(23, 31);
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 47);
            uint64_t Reserved_80 : BITFIELD_RANGE(48, 63);
        } Mi_Mode_Nestedbatchbufferenableis0;
        struct tagMi_Mode_Nestedbatchbufferenableis1 {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 21);
            uint32_t NestedLevelBatchBuffer : BITFIELD_RANGE(22, 22);
            uint32_t Reserved_23 : BITFIELD_RANGE(23, 31);
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 47);
            uint64_t Reserved_80 : BITFIELD_RANGE(48, 63);
        } Mi_Mode_Nestedbatchbufferenableis1;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tagADDRESS_SPACE_INDICATOR {
        ADDRESS_SPACE_INDICATOR_GGTT = 0x0,
        ADDRESS_SPACE_INDICATOR_PPGTT = 0x1,
    } ADDRESS_SPACE_INDICATOR;
    typedef enum tagNESTED_LEVEL_BATCH_BUFFER {
        NESTED_LEVEL_BATCH_BUFFER_CHAIN = 0x0,
        NESTED_LEVEL_BATCH_BUFFER_NESTED = 0x1,
    } NESTED_LEVEL_BATCH_BUFFER;
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.AddressSpaceIndicator = ADDRESS_SPACE_INDICATOR_PPGTT;
        TheStructure.Common.SecondLevelBatchBuffer = SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_BATCH_BUFFER_START;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
        TheStructure.Mi_Mode_Nestedbatchbufferenableis1.NestedLevelBatchBuffer = NESTED_LEVEL_BATCH_BUFFER_CHAIN;
    }
    static tagMI_BATCH_BUFFER_START sInit() {
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
    inline ADDRESS_SPACE_INDICATOR getAddressSpaceIndicator() const {
        return static_cast<ADDRESS_SPACE_INDICATOR>(TheStructure.Common.AddressSpaceIndicator);
    }
    inline void setResourceStreamerEnable(const bool value) {
        TheStructure.Common.ResourceStreamerEnable = value;
    }
    inline bool getResourceStreamerEnable() const {
        return TheStructure.Common.ResourceStreamerEnable;
    }
    inline void setPredicationEnable(const uint32_t value) {
        TheStructure.Common.PredicationEnable = value;
    }
    inline uint32_t getPredicationEnable() const {
        return TheStructure.Common.PredicationEnable;
    }
    inline void setAddOffsetEnable(const bool value) {
        TheStructure.Common.AddOffsetEnable = value;
    }
    inline bool getAddOffsetEnable() const {
        return TheStructure.Common.AddOffsetEnable;
    }
    inline void setEnableCommandCache(const uint32_t value) {
        TheStructure.Common.EnableCommandCache = value;
    }
    inline uint32_t getEnableCommandCache() const {
        return TheStructure.Common.EnableCommandCache;
    }
    inline void setPoshEnable(const uint32_t value) {
        TheStructure.Common.PoshEnable = value;
    }
    inline uint32_t getPoshEnable() const {
        return TheStructure.Common.PoshEnable;
    }
    inline void setPoshStart(const uint32_t value) {
        TheStructure.Common.PoshStart = value;
    }
    inline uint32_t getPoshStart() const {
        return TheStructure.Common.PoshStart;
    }
    inline void setSecondLevelBatchBuffer(const SECOND_LEVEL_BATCH_BUFFER value) {
        TheStructure.Common.SecondLevelBatchBuffer = value;
    }
    inline SECOND_LEVEL_BATCH_BUFFER getSecondLevelBatchBuffer() const {
        return static_cast<SECOND_LEVEL_BATCH_BUFFER>(TheStructure.Common.SecondLevelBatchBuffer);
    }
    typedef enum tagBATCHBUFFERSTARTADDRESS {
        BATCHBUFFERSTARTADDRESS_BIT_SHIFT = 0x2,
        BATCHBUFFERSTARTADDRESS_ALIGN_SIZE = 0x4,
    } BATCHBUFFERSTARTADDRESS;
    inline void setBatchBufferStartAddress(const uint64_t value) {
        TheStructure.Common.BatchBufferStartAddress = value >> BATCHBUFFERSTARTADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBatchBufferStartAddress() const {
        return TheStructure.Common.BatchBufferStartAddress << BATCHBUFFERSTARTADDRESS_BIT_SHIFT;
    }
    inline void setNestedLevelBatchBuffer(const NESTED_LEVEL_BATCH_BUFFER value) {
        TheStructure.Mi_Mode_Nestedbatchbufferenableis1.NestedLevelBatchBuffer = value;
    }
    inline NESTED_LEVEL_BATCH_BUFFER getNestedLevelBatchBuffer() const {
        return static_cast<NESTED_LEVEL_BATCH_BUFFER>(TheStructure.Mi_Mode_Nestedbatchbufferenableis1.NestedLevelBatchBuffer);
    }
} MI_BATCH_BUFFER_START;
STATIC_ASSERT(12 == sizeof(MI_BATCH_BUFFER_START));

typedef struct tagMI_LOAD_REGISTER_MEM {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t MemoryObjectControlStateReserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t MemoryObjectControlStateIndexToMocsTables
                : BITFIELD_RANGE(9, 14);
            uint32_t MemoryObjectControlStateEnable : BITFIELD_RANGE(15, 15);
            uint32_t VirtualEngineIdOffsetEnable : BITFIELD_RANGE(16, 16);
            uint32_t MmioRemapEnable : BITFIELD_RANGE(17, 17);
            uint32_t Reserved_18 : BITFIELD_RANGE(18, 18);
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode =
            MI_COMMAND_OPCODE_MI_LOAD_REGISTER_MEM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_LOAD_REGISTER_MEM sInit() {
        MI_LOAD_REGISTER_MEM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void
    setMemoryObjectControlState(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0x7e00);
        TheStructure.Common.MemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint32_t getMemoryObjectControlState() const {
        return (TheStructure.Common.MemoryObjectControlStateIndexToMocsTables << 1);
    }
    inline void setMemoryObjectControlStateEnable(const bool value) {
        TheStructure.Common.MemoryObjectControlStateEnable = value;
    }
    inline bool getMemoryObjectControlStateEnable() const {
        return TheStructure.Common.MemoryObjectControlStateEnable;
    }
    inline void setVirtualEngineIdOffsetEnable(const bool value) {
        TheStructure.Common.VirtualEngineIdOffsetEnable = value;
    }
    inline bool getVirtualEngineIdOffsetEnable() const {
        return TheStructure.Common.VirtualEngineIdOffsetEnable;
    }
    inline void setMmioRemapEnable(const bool value) {
        TheStructure.Common.MmioRemapEnable = value;
    }
    inline bool getMmioRemapEnable() const {
        return TheStructure.Common.MmioRemapEnable;
    }
    inline void setAddCsMmioStartOffset(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffset = value;
    }
    inline uint32_t getAddCsMmioStartOffset() const {
        return TheStructure.Common.AddCsMmioStartOffset;
    }
    inline void setAsyncModeEnable(const bool value) {
        TheStructure.Common.AsyncModeEnable = value;
    }
    inline bool getAsyncModeEnable() const {
        return TheStructure.Common.AsyncModeEnable;
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt() const {
        return TheStructure.Common.UseGlobalGtt;
    }
    typedef enum tagREGISTERADDRESS {
        REGISTERADDRESS_BIT_SHIFT = 0x2,
        REGISTERADDRESS_ALIGN_SIZE = 0x4,
    } REGISTERADDRESS;
    inline void setRegisterAddress(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0x7ffffc);
        TheStructure.Common.RegisterAddress = value >> REGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getRegisterAddress() const {
        return TheStructure.Common.RegisterAddress << REGISTERADDRESS_BIT_SHIFT;
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x2,
        MEMORYADDRESS_ALIGN_SIZE = 0x4,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        DEBUG_BREAK_IF(value > 0xfffffffffffffffcL);
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress() const {
        return TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT;
    }
} MI_LOAD_REGISTER_MEM;
STATIC_ASSERT(16 == sizeof(MI_LOAD_REGISTER_MEM));

typedef struct tagMI_LOAD_REGISTER_REG {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t MmioRemapEnableSource : BITFIELD_RANGE(16, 16);
            uint32_t MmioRemapEnableDestination : BITFIELD_RANGE(17, 17);
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_LOAD_REGISTER_REG;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_LOAD_REGISTER_REG sInit() {
        MI_LOAD_REGISTER_REG state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setMmioRemapEnableSource(const bool value) {
        TheStructure.Common.MmioRemapEnableSource = value;
    }
    inline bool getMmioRemapEnableSource() const {
        return TheStructure.Common.MmioRemapEnableSource;
    }
    inline void setMmioRemapEnableDestination(const bool value) {
        TheStructure.Common.MmioRemapEnableDestination = value;
    }
    inline bool getMmioRemapEnableDestination() const {
        return TheStructure.Common.MmioRemapEnableDestination;
    }
    inline void setAddCsMmioStartOffsetSource(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffsetSource = value;
    }
    inline uint32_t getAddCsMmioStartOffsetSource() const {
        return TheStructure.Common.AddCsMmioStartOffsetSource;
    }
    inline void setAddCsMmioStartOffsetDestination(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffsetDestination = value;
    }
    inline uint32_t getAddCsMmioStartOffsetDestination() const {
        return TheStructure.Common.AddCsMmioStartOffsetDestination;
    }
    typedef enum tagSOURCEREGISTERADDRESS {
        SOURCEREGISTERADDRESS_BIT_SHIFT = 0x2,
        SOURCEREGISTERADDRESS_ALIGN_SIZE = 0x4,
    } SOURCEREGISTERADDRESS;
    inline void setSourceRegisterAddress(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0x7ffffc);
        TheStructure.Common.SourceRegisterAddress = value >> SOURCEREGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getSourceRegisterAddress() const {
        return TheStructure.Common.SourceRegisterAddress << SOURCEREGISTERADDRESS_BIT_SHIFT;
    }
    typedef enum tagDESTINATIONREGISTERADDRESS {
        DESTINATIONREGISTERADDRESS_BIT_SHIFT = 0x2,
        DESTINATIONREGISTERADDRESS_ALIGN_SIZE = 0x4,
    } DESTINATIONREGISTERADDRESS;
    inline void setDestinationRegisterAddress(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0x7ffffc);
        TheStructure.Common.DestinationRegisterAddress = value >> DESTINATIONREGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getDestinationRegisterAddress() const {
        return TheStructure.Common.DestinationRegisterAddress << DESTINATIONREGISTERADDRESS_BIT_SHIFT;
    }
} MI_LOAD_REGISTER_REG;
STATIC_ASSERT(12 == sizeof(MI_LOAD_REGISTER_REG));

typedef struct tagMI_SEMAPHORE_WAIT {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 11);
            uint32_t CompareOperation : BITFIELD_RANGE(12, 14);
            uint32_t WaitMode : BITFIELD_RANGE(15, 15);
            uint32_t RegisterPollMode : BITFIELD_RANGE(16, 16);
            uint32_t IndirectSemaphoreDataDword : BITFIELD_RANGE(17, 17);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_19 : BITFIELD_RANGE(19, 21);
            uint32_t MemoryType : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t SemaphoreDataDword;
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint64_t SemaphoreAddress : BITFIELD_RANGE(2, 63);
            uint32_t Reserved_192 : BITFIELD_RANGE(0, 4);
            uint32_t WaitTokenNumber : BITFIELD_RANGE(5, 9);
            uint32_t Reserved_202 : BITFIELD_RANGE(10, 31);
        } Common;
        uint32_t RawData[5];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x3,
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.CompareOperation =
            COMPARE_OPERATION_SAD_GREATER_THAN_SDD;
        TheStructure.Common.WaitMode = WAIT_MODE_SIGNAL_MODE;
        TheStructure.Common.RegisterPollMode = REGISTER_POLL_MODE_MEMORY_POLL;
        TheStructure.Common.MemoryType = MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_SEMAPHORE_WAIT sInit() {
        MI_SEMAPHORE_WAIT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 6);
        return TheStructure.RawData[index];
    }
    inline void setCompareOperation(const COMPARE_OPERATION value) {
        TheStructure.Common.CompareOperation = value;
    }
    inline COMPARE_OPERATION getCompareOperation() const {
        return static_cast<COMPARE_OPERATION>(TheStructure.Common.CompareOperation);
    }
    inline void setWaitMode(const WAIT_MODE value) {
        TheStructure.Common.WaitMode = value;
    }
    inline WAIT_MODE getWaitMode() const {
        return static_cast<WAIT_MODE>(TheStructure.Common.WaitMode);
    }
    inline void setRegisterPollMode(const REGISTER_POLL_MODE value) {
        TheStructure.Common.RegisterPollMode = value;
    }
    inline REGISTER_POLL_MODE getRegisterPollMode() const {
        return static_cast<REGISTER_POLL_MODE>(
            TheStructure.Common.RegisterPollMode);
    }
    inline void setIndirectSemaphoreDataDword(const bool value) {
        TheStructure.Common.IndirectSemaphoreDataDword = value;
    }
    inline bool getIndirectSemaphoreDataDword() const {
        return TheStructure.Common.IndirectSemaphoreDataDword;
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    inline void setMemoryType(const MEMORY_TYPE value) {
        TheStructure.Common.MemoryType = value;
    }
    inline MEMORY_TYPE getMemoryType() const {
        return static_cast<MEMORY_TYPE>(TheStructure.Common.MemoryType);
    }
    inline void setSemaphoreDataDword(const uint32_t value) {
        TheStructure.Common.SemaphoreDataDword = value;
    }
    inline uint32_t getSemaphoreDataDword() const {
        return TheStructure.Common.SemaphoreDataDword;
    }
    typedef enum tagSEMAPHOREADDRESS {
        SEMAPHOREADDRESS_BIT_SHIFT = 0x2,
        SEMAPHOREADDRESS_ALIGN_SIZE = 0x4,
    } SEMAPHOREADDRESS;
    inline void setSemaphoreGraphicsAddress(const uint64_t value) {
        TheStructure.Common.SemaphoreAddress = value >> SEMAPHOREADDRESS_BIT_SHIFT;
    }
    inline uint64_t getSemaphoreGraphicsAddress() const {
        return TheStructure.Common.SemaphoreAddress << SEMAPHOREADDRESS_BIT_SHIFT;
    }
    inline void setWaitTokenNumber(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0x3e0);
        TheStructure.Common.WaitTokenNumber = value;
    }
    inline uint32_t getWaitTokenNumber() const {
        return TheStructure.Common.WaitTokenNumber;
    }
} MI_SEMAPHORE_WAIT;
STATIC_ASSERT(20 == sizeof(MI_SEMAPHORE_WAIT));

typedef struct tagMI_STORE_DATA_IMM {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 9);
            uint32_t ForceWriteCompletionCheck : BITFIELD_RANGE(10, 10);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(11, 11);
            uint32_t Reserved_12 : BITFIELD_RANGE(12, 13);
            uint32_t MemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(14, 19);
            uint32_t MemoryObjectControlStateEnable : BITFIELD_RANGE(20, 20);
            uint32_t StoreQword : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t CoreModeEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 1);
            uint64_t Address : BITFIELD_RANGE(2, 47);
            uint64_t AddressReserved_80 : BITFIELD_RANGE(48, 63);
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_STORE_DWORD;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_STORE_DATA_IMM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_STORE_DATA_IMM sInit() {
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
    inline DWORD_LENGTH getDwordLength() const {
        return static_cast<DWORD_LENGTH>(TheStructure.Common.DwordLength);
    }
    inline void setForceWriteCompletionCheck(const bool value) {
        TheStructure.Common.ForceWriteCompletionCheck = value;
    }
    inline bool getForceWriteCompletionCheck() const {
        return TheStructure.Common.ForceWriteCompletionCheck;
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    inline void
    setMemoryObjectControlState(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0xfc000);
        TheStructure.Common.MemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint32_t getMemoryObjectControlState() const {
        return (TheStructure.Common.MemoryObjectControlStateIndexToMocsTables << 1);
    }
    inline void setMemoryObjectControlStateEnable(const bool value) {
        TheStructure.Common.MemoryObjectControlStateEnable = value;
    }
    inline bool getMemoryObjectControlStateEnable() const {
        return TheStructure.Common.MemoryObjectControlStateEnable;
    }
    inline void setStoreQword(const bool value) {
        TheStructure.Common.StoreQword = value;
    }
    inline bool getStoreQword() const {
        return TheStructure.Common.StoreQword;
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt() const {
        return TheStructure.Common.UseGlobalGtt;
    }
    inline void setCoreModeEnable(const uint64_t value) {
        TheStructure.Common.CoreModeEnable = value;
    }
    inline uint64_t getCoreModeEnable() const {
        return TheStructure.Common.CoreModeEnable;
    }
    typedef enum tagADDRESS {
        ADDRESS_BIT_SHIFT = 0x2,
        ADDRESS_ALIGN_SIZE = 0x4,
    } ADDRESS;
    inline void setAddress(const uint64_t value) {
        TheStructure.Common.Address = value >> ADDRESS_BIT_SHIFT;
    }
    inline uint64_t getAddress() const {
        return TheStructure.Common.Address << ADDRESS_BIT_SHIFT;
    }
    inline void setDataDword0(const uint32_t value) {
        TheStructure.Common.DataDword0 = value;
    }
    inline uint32_t getDataDword0() const {
        return TheStructure.Common.DataDword0;
    }
    inline void setDataDword1(const uint32_t value) {
        TheStructure.Common.DataDword1 = value;
    }
    inline uint32_t getDataDword1() const {
        return TheStructure.Common.DataDword1;
    }
} MI_STORE_DATA_IMM;
STATIC_ASSERT(20 == sizeof(MI_STORE_DATA_IMM));

typedef struct tagMI_STORE_REGISTER_MEM {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t MemoryObjectControlStateReserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t MemoryObjectControlStateIndexToMocsTables
                : BITFIELD_RANGE(9, 14);
            uint32_t MemoryObjectControlStateEnable : BITFIELD_RANGE(15, 15);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(16, 16);
            uint32_t MmioRemapEnable : BITFIELD_RANGE(17, 17);
            uint32_t Reserved_18 : BITFIELD_RANGE(18, 18);
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode =
            MI_COMMAND_OPCODE_MI_STORE_REGISTER_MEM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_STORE_REGISTER_MEM sInit() {
        MI_STORE_REGISTER_MEM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void
    setMemoryObjectControlState(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0x7e00);
        TheStructure.Common.MemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint32_t getMemoryObjectControlState() const {
        return (TheStructure.Common.MemoryObjectControlStateIndexToMocsTables << 1);
    }
    inline void setMemoryObjectControlStateEnable(const bool value) {
        TheStructure.Common.MemoryObjectControlStateEnable = value;
    }
    inline bool getMemoryObjectControlStateEnable() const {
        return TheStructure.Common.MemoryObjectControlStateEnable;
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    inline void setMmioRemapEnable(const bool value) {
        TheStructure.Common.MmioRemapEnable = value;
    }
    inline bool getMmioRemapEnable() const {
        return TheStructure.Common.MmioRemapEnable;
    }
    inline void setAddCsMmioStartOffset(const uint32_t value) {
        TheStructure.Common.AddCsMmioStartOffset = value;
    }
    inline uint32_t getAddCsMmioStartOffset() const {
        return TheStructure.Common.AddCsMmioStartOffset;
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt() const {
        return TheStructure.Common.UseGlobalGtt;
    }
    typedef enum tagREGISTERADDRESS {
        REGISTERADDRESS_BIT_SHIFT = 0x2,
        REGISTERADDRESS_ALIGN_SIZE = 0x4,
    } REGISTERADDRESS;
    inline void setRegisterAddress(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0x7ffffc);
        TheStructure.Common.RegisterAddress = value >> REGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getRegisterAddress() const {
        return TheStructure.Common.RegisterAddress << REGISTERADDRESS_BIT_SHIFT;
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x2,
        MEMORYADDRESS_ALIGN_SIZE = 0x4,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        DEBUG_BREAK_IF(value > 0xfffffffffffffffcL);
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress() const {
        return TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT;
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
            uint32_t Reserved_5 : BITFIELD_RANGE(5, 5);
            uint32_t MediaSamplerPowerClockGateDisable : BITFIELD_RANGE(6, 6);
            uint32_t SystolicModeEnable : BITFIELD_RANGE(7, 7);
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
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.PipelineSelection = PIPELINE_SELECTION_3D;
        TheStructure.Common._3DCommandSubOpcode =
            _3D_COMMAND_SUB_OPCODE_PIPELINE_SELECT;
        TheStructure.Common._3DCommandOpcode =
            _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_SINGLE_DW;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagPIPELINE_SELECT sInit() {
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
    inline PIPELINE_SELECTION getPipelineSelection() const {
        return static_cast<PIPELINE_SELECTION>(
            TheStructure.Common.PipelineSelection);
    }
    inline void setRenderSliceCommonPowerGateEnable(const bool value) {
        TheStructure.Common.RenderSliceCommonPowerGateEnable = value;
    }
    inline bool getRenderSliceCommonPowerGateEnable() const {
        return TheStructure.Common.RenderSliceCommonPowerGateEnable;
    }
    inline void setRenderSamplerPowerGateEnable(const bool value) {
        TheStructure.Common.RenderSamplerPowerGateEnable = value;
    }
    inline bool getRenderSamplerPowerGateEnable() const {
        return TheStructure.Common.RenderSamplerPowerGateEnable;
    }
    inline void setMediaSamplerDopClockGateEnable(const bool value) {
        TheStructure.Common.MediaSamplerDopClockGateEnable = value;
    }
    inline bool getMediaSamplerDopClockGateEnable() const {
        return TheStructure.Common.MediaSamplerDopClockGateEnable;
    }
    inline void setMediaSamplerPowerClockGateDisable(const bool value) {
        TheStructure.Common.MediaSamplerPowerClockGateDisable = value;
    }
    inline bool getMediaSamplerPowerClockGateDisable() const {
        return TheStructure.Common.MediaSamplerPowerClockGateDisable;
    }
    inline void setSystolicModeEnable(const bool value) {
        TheStructure.Common.SystolicModeEnable = value;
    }
    inline bool getSystolicModeEnable() const {
        return TheStructure.Common.SystolicModeEnable;
    }
    inline void setMaskBits(const uint32_t value) {
        DEBUG_BREAK_IF(value > 0xff00);
        TheStructure.Common.MaskBits = value;
    }
    inline uint32_t getMaskBits() const {
        return TheStructure.Common.MaskBits;
    }
} PIPELINE_SELECT;
STATIC_ASSERT(4 == sizeof(PIPELINE_SELECT));

typedef struct tagSTATE_COMPUTE_MODE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t ZPassAsyncComputeThreadLimit : BITFIELD_RANGE(0, 2);
            uint32_t ForceNonCoherent : BITFIELD_RANGE(3, 4);
            uint32_t FastClearDisabledOnCompressedSurface : BITFIELD_RANGE(5, 5);
            uint32_t DisableSlmReadMergeOptimization : BITFIELD_RANGE(6, 6);
            uint32_t PixelAsyncComputeThreadLimit : BITFIELD_RANGE(7, 9);
            uint32_t Reserved_42 : BITFIELD_RANGE(10, 10);
            uint32_t DisableAtomicOnClearData : BITFIELD_RANGE(11, 11);
            uint32_t Reserved_44 : BITFIELD_RANGE(12, 12);
            uint32_t DisableL1InvalidateForNonL1CacheableWrites : BITFIELD_RANGE(13, 13);
            uint32_t Reserved_46 : BITFIELD_RANGE(14, 14);
            uint32_t LargeGrfMode : BITFIELD_RANGE(15, 15);
            uint32_t MaskBits : BITFIELD_RANGE(16, 31);
        } Common;
        uint32_t RawData[2];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x0,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_STATE_COMPUTE_MODE = 0x5,
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
    typedef enum tagZ_PASS_ASYNC_COMPUTE_THREAD_LIMIT {
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60 = 0x0,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64 = 0x1,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_56 = 0x2,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_48 = 0x3,
    } Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT;
    typedef enum tagFORCE_NON_COHERENT {
        FORCE_NON_COHERENT_FORCE_DISABLED = 0x0,
        FORCE_NON_COHERENT_FORCE_CPU_NON_COHERENT = 0x1,
        FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT = 0x2,
    } FORCE_NON_COHERENT;
    typedef enum tagFAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE {
        FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE_ENABLED = 0x0,
        FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE_DISABLED = 0x1,
    } FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE;
    typedef enum tagDISABLE_SLM_READ_MERGE_OPTIMIZATION {
        DISABLE_SLM_READ_MERGE_OPTIMIZATION_ENABLED = 0x0,
        DISABLE_SLM_READ_MERGE_OPTIMIZATION_DISABLED = 0x1,
    } DISABLE_SLM_READ_MERGE_OPTIMIZATION;
    typedef enum tagPIXEL_ASYNC_COMPUTE_THREAD_LIMIT {
        PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED = 0x0,
        PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2 = 0x1,
        PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_8 = 0x2,
        PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_16 = 0x3,
        PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_24 = 0x4,
        PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_32 = 0x5,
        PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_40 = 0x6,
        PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_48 = 0x7,
    } PIXEL_ASYNC_COMPUTE_THREAD_LIMIT;
    typedef enum tagDISABLE_ATOMIC_ON_CLEAR_DATA {
        DISABLE_ATOMIC_ON_CLEAR_DATA_ENABLE = 0x0,
        DISABLE_ATOMIC_ON_CLEAR_DATA_DISABLE = 0x1,
    } DISABLE_ATOMIC_ON_CLEAR_DATA;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_COMPUTE_MODE;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.ZPassAsyncComputeThreadLimit = Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60;
        TheStructure.Common.ForceNonCoherent = FORCE_NON_COHERENT_FORCE_DISABLED;
        TheStructure.Common.FastClearDisabledOnCompressedSurface = FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE_ENABLED;
        TheStructure.Common.DisableSlmReadMergeOptimization = DISABLE_SLM_READ_MERGE_OPTIMIZATION_ENABLED;
        TheStructure.Common.PixelAsyncComputeThreadLimit = PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED;
        TheStructure.Common.DisableAtomicOnClearData = DISABLE_ATOMIC_ON_CLEAR_DATA_ENABLE;
    }
    static tagSTATE_COMPUTE_MODE sInit() {
        STATE_COMPUTE_MODE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 2);
        return TheStructure.RawData[index];
    }
    inline void setZPassAsyncComputeThreadLimit(const Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT value) {
        TheStructure.Common.ZPassAsyncComputeThreadLimit = value;
    }
    inline Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT getZPassAsyncComputeThreadLimit() const {
        return static_cast<Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT>(TheStructure.Common.ZPassAsyncComputeThreadLimit);
    }
    inline void setForceNonCoherent(const FORCE_NON_COHERENT value) {
        TheStructure.Common.ForceNonCoherent = value;
    }
    inline FORCE_NON_COHERENT getForceNonCoherent() const {
        return static_cast<FORCE_NON_COHERENT>(TheStructure.Common.ForceNonCoherent);
    }
    inline void setFastClearDisabledOnCompressedSurface(const FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE value) {
        TheStructure.Common.FastClearDisabledOnCompressedSurface = value;
    }
    inline FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE getFastClearDisabledOnCompressedSurface() const {
        return static_cast<FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE>(TheStructure.Common.FastClearDisabledOnCompressedSurface);
    }
    inline void setDisableSlmReadMergeOptimization(const DISABLE_SLM_READ_MERGE_OPTIMIZATION value) {
        TheStructure.Common.DisableSlmReadMergeOptimization = value;
    }
    inline DISABLE_SLM_READ_MERGE_OPTIMIZATION getDisableSlmReadMergeOptimization() const {
        return static_cast<DISABLE_SLM_READ_MERGE_OPTIMIZATION>(TheStructure.Common.DisableSlmReadMergeOptimization);
    }
    inline void setPixelAsyncComputeThreadLimit(const PIXEL_ASYNC_COMPUTE_THREAD_LIMIT value) {
        TheStructure.Common.PixelAsyncComputeThreadLimit = value;
    }
    inline PIXEL_ASYNC_COMPUTE_THREAD_LIMIT getPixelAsyncComputeThreadLimit() const {
        return static_cast<PIXEL_ASYNC_COMPUTE_THREAD_LIMIT>(TheStructure.Common.PixelAsyncComputeThreadLimit);
    }
    inline void setDisableAtomicOnClearData(const DISABLE_ATOMIC_ON_CLEAR_DATA value) {
        TheStructure.Common.DisableAtomicOnClearData = value;
    }
    inline DISABLE_ATOMIC_ON_CLEAR_DATA getDisableAtomicOnClearData() const {
        return static_cast<DISABLE_ATOMIC_ON_CLEAR_DATA>(TheStructure.Common.DisableAtomicOnClearData);
    }
    inline void setDisableL1InvalidateForNonL1CacheableWrites(const bool value) {
        TheStructure.Common.DisableL1InvalidateForNonL1CacheableWrites = value;
    }
    inline bool getDisableL1InvalidateForNonL1CacheableWrites() const {
        return TheStructure.Common.DisableL1InvalidateForNonL1CacheableWrites;
    }
    inline void setLargeGrfMode(const bool value) {
        TheStructure.Common.LargeGrfMode = value;
    }
    inline bool getLargeGrfMode() const {
        return TheStructure.Common.LargeGrfMode;
    }
    inline void setMaskBits(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.MaskBits = value;
    }
    inline uint32_t getMaskBits() const {
        return TheStructure.Common.MaskBits;
    }
} STATE_COMPUTE_MODE;
STATIC_ASSERT(8 == sizeof(STATE_COMPUTE_MODE));

typedef struct tag_3DSTATE_BINDING_TABLE_POOL_ALLOC {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t SurfaceObjectControlStateEncryptedData : BITFIELD_RANGE(0, 0);
            uint64_t SurfaceObjectControlStateIndexToMocsTables : BITFIELD_RANGE(1, 6);
            uint64_t Reserved_39 : BITFIELD_RANGE(7, 9);
            uint64_t Reserved_42 : BITFIELD_RANGE(10, 10);
            uint64_t Reserved_43 : BITFIELD_RANGE(11, 11);
            uint64_t BindingTablePoolBaseAddress : BITFIELD_RANGE(12, 47);
            uint64_t BindingTablePoolBaseAddressReserved_80 : BITFIELD_RANGE(48, 63);
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 11);
            uint32_t BindingTablePoolBufferSize : BITFIELD_RANGE(12, 31);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x2,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_3DSTATE_BINDING_TABLE_POOL_ALLOC = 0x19,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_3DSTATE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_3D = 0x3,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;

    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_3DSTATE_BINDING_TABLE_POOL_ALLOC;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_3DSTATE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_3D;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.BindingTablePoolBufferSize = 0;
    }
    static tag_3DSTATE_BINDING_TABLE_POOL_ALLOC sInit() {
        _3DSTATE_BINDING_TABLE_POOL_ALLOC state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setSurfaceObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.SurfaceObjectControlStateEncryptedData = value;
    }
    inline bool getSurfaceObjectControlStateEncryptedData() const {
        return TheStructure.Common.SurfaceObjectControlStateEncryptedData;
    }
    inline void setSurfaceObjectControlStateIndexToMocsTables(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x7eL);
        TheStructure.Common.SurfaceObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint64_t getSurfaceObjectControlStateIndexToMocsTables() const {
        return (TheStructure.Common.SurfaceObjectControlStateIndexToMocsTables << 1);
    }
    typedef enum tagBINDINGTABLEPOOLBASEADDRESS {
        BINDINGTABLEPOOLBASEADDRESS_BIT_SHIFT = 0xc,
        BINDINGTABLEPOOLBASEADDRESS_ALIGN_SIZE = 0x1000,
    } BINDINGTABLEPOOLBASEADDRESS;
    inline void setBindingTablePoolBaseAddress(const uint64_t value) {
        TheStructure.Common.BindingTablePoolBaseAddress = value >> BINDINGTABLEPOOLBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBindingTablePoolBaseAddress() const {
        return TheStructure.Common.BindingTablePoolBaseAddress << BINDINGTABLEPOOLBASEADDRESS_BIT_SHIFT;
    }
    inline void setBindingTablePoolBufferSize(const uint32_t value) { TheStructure.Common.BindingTablePoolBufferSize = value; }
    inline uint32_t getBindingTablePoolBufferSize() const { return TheStructure.Common.BindingTablePoolBufferSize; }
} _3DSTATE_BINDING_TABLE_POOL_ALLOC;
STATIC_ASSERT(16 == sizeof(_3DSTATE_BINDING_TABLE_POOL_ALLOC));

typedef struct tagL3_FLUSH_ADDRESS_RANGE {
    union tagTheStructure {
        struct tagCommon {
            uint64_t Reserved_0 : BITFIELD_RANGE(0, 2);
            uint64_t AddressMask : BITFIELD_RANGE(3, 8);
            uint64_t Reserved_9 : BITFIELD_RANGE(9, 11);
            uint64_t AddressLow : BITFIELD_RANGE(12, 31);
            uint64_t AddressHigh : BITFIELD_RANGE(32, 47);
            uint64_t Reserved_48 : BITFIELD_RANGE(48, 59);
            uint64_t L3FlushEvictionPolicy : BITFIELD_RANGE(60, 61);
            uint64_t Reserved_62 : BITFIELD_RANGE(62, 63);
        } Common;
        uint32_t RawData[2];
    } TheStructure;
    typedef enum tagL3_FLUSH_EVICTION_POLICY {
        L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION = 0x0,
        L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_OUT_EVICTION = 0x1,
        L3_FLUSH_EVICTION_POLICY_DISCARD = 0x2,
    } L3_FLUSH_EVICTION_POLICY;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.L3FlushEvictionPolicy = L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION;
    }
    static tagL3_FLUSH_ADDRESS_RANGE sInit() {
        L3_FLUSH_ADDRESS_RANGE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 2);
        return TheStructure.RawData[index];
    }

    typedef enum tagADDRESSLOW {
        ADDRESSLOW_BIT_SHIFT = 0xC,
        ADDRESSLOW_ALIGN_SIZE = 0x1000,
    } ADDRESSLOW;

    inline void setAddressLow(const uint64_t value) {
        TheStructure.Common.AddressLow = value >> ADDRESSLOW_BIT_SHIFT;
    }

    inline uint64_t getAddressLow() const {
        return (TheStructure.Common.AddressLow << ADDRESSLOW_BIT_SHIFT);
    }

    inline void setAddressHigh(const uint64_t value) {
        TheStructure.Common.AddressHigh = value;
    }

    inline uint64_t getAddressHigh() const {
        return (TheStructure.Common.AddressHigh);
    }

    inline void setAddress(const uint64_t value) {
        setAddressLow(static_cast<uint32_t>(value));
        setAddressHigh(static_cast<uint32_t>(value >> 32));
    }

    inline uint64_t getAddress() const {
        return static_cast<uint64_t>(getAddressLow()) | (static_cast<uint64_t>(getAddressHigh()) << 32);
    }

    inline void setL3FlushEvictionPolicy(const L3_FLUSH_EVICTION_POLICY value) {
        TheStructure.Common.L3FlushEvictionPolicy = value;
    }
    inline L3_FLUSH_EVICTION_POLICY getL3FlushEvictionPolicy() const {
        return static_cast<L3_FLUSH_EVICTION_POLICY>(TheStructure.Common.L3FlushEvictionPolicy);
    }
    inline void setAddressMask(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x1f8);
        TheStructure.Common.AddressMask = value;
    }
    inline uint32_t getAddressMask() const {
        return TheStructure.Common.AddressMask;
    }
} L3_FLUSH_ADDRESS_RANGE;
STATIC_ASSERT(8 == sizeof(L3_FLUSH_ADDRESS_RANGE));

struct L3_CONTROL_POST_SYNC_DATA {
    union tagTheStructure {
        struct tagCommon {
            uint64_t Reserved_96 : BITFIELD_RANGE(0, 2);
            uint64_t Address : BITFIELD_RANGE(3, 47);
            uint64_t Reserved_144 : BITFIELD_RANGE(48, 63);
            uint64_t ImmediateData;
        } Common;
        uint32_t RawData[4];
    } TheStructure;

    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
    }

    typedef enum tagADDRESS {
        ADDRESS_BIT_SHIFT = 0x3,
        ADDRESS_ALIGN_SIZE = 0x8,
    } ADDRESS;
    inline void setAddress(const uint64_t value) {
        TheStructure.Common.Address = value >> ADDRESS_BIT_SHIFT;
    }
    inline uint64_t getAddress() const {
        return TheStructure.Common.Address << ADDRESS_BIT_SHIFT;
    }
    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }
    inline uint64_t getImmediateData() const {
        return TheStructure.Common.ImmediateData;
    }
};

struct L3_CONTROL {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Length : BITFIELD_RANGE(0, 7);
            uint32_t DepthCacheFlush : BITFIELD_RANGE(8, 8);
            uint32_t RenderTargetCacheFlushEnable : BITFIELD_RANGE(9, 9);
            uint32_t HdcPipelineFlush : BITFIELD_RANGE(10, 10);
            uint32_t Reserved_11 : BITFIELD_RANGE(11, 12);
            uint32_t UnTypedDataPortCacheFlush : BITFIELD_RANGE(13, 13);
            uint32_t PostSyncOperation : BITFIELD_RANGE(14, 14);
            uint32_t PostSyncOperationL3CacheabilityControl : BITFIELD_RANGE(15, 15); // removed on DG1
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 19);
            uint32_t CommandStreamerStallEnable : BITFIELD_RANGE(20, 20);
            uint32_t DestinationAddressType : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_22 : BITFIELD_RANGE(22, 22);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(23, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t Type : BITFIELD_RANGE(29, 31);
            L3_CONTROL_POST_SYNC_DATA PostSyncData;
        } Common;
        uint32_t RawData[5];
    } TheStructure;

    typedef enum tagRENDER_TARGET_CACHE_FLUSH_ENABLE {
        RENDER_TARGET_CACHE_FLUSH_DISABLED = 0x0,
        RENDER_TARGET_CACHE_FLUSH_ENABLED = 0x1,
    } RENDER_TARGET_CACHE_FLUSH_ENABLE;
    typedef enum tagUN_TYPED_DATA_PORT_CACHE_FLUSH {
        UN_TYPED_DATA_PORT_CACHE_FLUSH_DISABLED = 0x0,
        UN_TYPED_DATA_PORT_CACHE_FLUSH_ENABLED = 0x1,
    } UN_TYPED_DATA_PORT_CACHE_FLUSH;
    typedef enum tagPOST_SYNC_OPERATION {
        POST_SYNC_OPERATION_NO_WRITE = 0x0,
        POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA = 0x1,
    } POST_SYNC_OPERATION;
    typedef enum tagPOST_SYNC_OPERATION_L3_CACHEABILITY_CONTROL {
        POST_SYNC_OPERATION_L3_CACHEABILITY_CONTROL_DEFAULT_MOCS = 0x0,
        POST_SYNC_OPERATION_L3_CACHEABILITY_CONTROL_CACHEABLE_MOCS = 0x1,
    } POST_SYNC_OPERATION_L3_CACHEABILITY_CONTROL;
    typedef enum tagDESTINATION_ADDRESS_TYPE {
        DESTINATION_ADDRESS_TYPE_PPGTT = 0x0,
        DESTINATION_ADDRESS_TYPE_GGTT = 0x1,
    } DESTINATION_ADDRESS_TYPE;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_L3_CONTROL = 0x1,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_L3_CONTROL = 0x5,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_3D = 0x3,
    } COMMAND_SUBTYPE;
    typedef enum tagTYPE {
        TYPE_GFXPIPE = 0x3,
    } TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.RenderTargetCacheFlushEnable = RENDER_TARGET_CACHE_FLUSH_ENABLED;
        TheStructure.Common.UnTypedDataPortCacheFlush = UN_TYPED_DATA_PORT_CACHE_FLUSH_DISABLED;
        TheStructure.Common.PostSyncOperation = POST_SYNC_OPERATION_NO_WRITE;
        TheStructure.Common.PostSyncOperationL3CacheabilityControl = POST_SYNC_OPERATION_L3_CACHEABILITY_CONTROL_DEFAULT_MOCS;
        TheStructure.Common.DestinationAddressType = DESTINATION_ADDRESS_TYPE_PPGTT;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_L3_CONTROL;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_L3_CONTROL;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_3D;
        TheStructure.Common.Type = TYPE_GFXPIPE;
        TheStructure.Common.PostSyncData.init();
    }
    static L3_CONTROL sInit() {
        L3_CONTROL state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 7);
        return TheStructure.RawData[index];
    }
    inline void setLength(const uint32_t value) {
        TheStructure.Common.Length = value;
    }
    inline uint32_t getLength() const {
        return TheStructure.Common.Length;
    }
    inline void setDepthCacheFlush(const bool value) {
        TheStructure.Common.DepthCacheFlush = value;
    }
    inline bool getDepthCacheFlush() const {
        return TheStructure.Common.DepthCacheFlush;
    }
    inline void setRenderTargetCacheFlushEnable(const bool value) {
        TheStructure.Common.RenderTargetCacheFlushEnable = value;
    }
    inline bool getRenderTargetCacheFlushEnable() const {
        return TheStructure.Common.RenderTargetCacheFlushEnable;
    }
    inline void setHdcPipelineFlush(const bool value) {
        TheStructure.Common.HdcPipelineFlush = value;
    }
    inline bool getHdcPipelineFlush() const {
        return TheStructure.Common.HdcPipelineFlush;
    }
    inline void setUnTypedDataPortCacheFlush(const bool value) {
        TheStructure.Common.UnTypedDataPortCacheFlush = value;
    }
    inline bool getUnTypedDataPortCacheFlush() const {
        return TheStructure.Common.UnTypedDataPortCacheFlush;
    }
    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }
    inline POST_SYNC_OPERATION getPostSyncOperation() const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.PostSyncOperation);
    }
    inline void setPostSyncOperationL3CacheabilityControl(const POST_SYNC_OPERATION_L3_CACHEABILITY_CONTROL value) {
        TheStructure.Common.PostSyncOperationL3CacheabilityControl = value;
    }
    inline POST_SYNC_OPERATION_L3_CACHEABILITY_CONTROL getPostSyncOperationL3CacheabilityControl() const {
        return static_cast<POST_SYNC_OPERATION_L3_CACHEABILITY_CONTROL>(TheStructure.Common.PostSyncOperationL3CacheabilityControl);
    }
    inline void setCommandStreamerStallEnable(const bool value) {
        TheStructure.Common.CommandStreamerStallEnable = value;
    }
    inline bool getCommandStreamerStallEnable() const {
        return TheStructure.Common.CommandStreamerStallEnable;
    }
    inline void setDestinationAddressType(const DESTINATION_ADDRESS_TYPE value) {
        TheStructure.Common.DestinationAddressType = value;
    }
    inline DESTINATION_ADDRESS_TYPE getDestinationAddressType() const {
        return static_cast<DESTINATION_ADDRESS_TYPE>(TheStructure.Common.DestinationAddressType);
    }
    inline void setType(const TYPE value) {
        TheStructure.Common.Type = value;
    }
    inline TYPE getType() const {
        return static_cast<TYPE>(TheStructure.Common.Type);
    }
    L3_CONTROL_POST_SYNC_DATA &getPostSyncData() {
        return TheStructure.Common.PostSyncData;
    }

    const L3_CONTROL_POST_SYNC_DATA &getPostSyncData() const {
        return TheStructure.Common.PostSyncData;
    }

    inline void setPostSyncAddress(const uint64_t value) {
        getPostSyncData().setAddress(value);
    }

    inline uint64_t getPostSyncAddress() const {
        return getPostSyncData().getAddress();
    }

    inline void setPostSyncImmediateData(const uint64_t value) {
        getPostSyncData().setImmediateData(value);
    }

    inline uint64_t getPostSyncImmediateData() const {
        return getPostSyncData().getImmediateData();
    }
};
STATIC_ASSERT(20 == sizeof(L3_CONTROL));
STATIC_ASSERT(NEO::TypeTraits::isPodV<L3_CONTROL>);

typedef struct tagSTATE_SIP {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
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
    inline void init() {
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
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    typedef enum tagSYSTEMINSTRUCTIONPOINTER {
        SYSTEMINSTRUCTIONPOINTER_BIT_SHIFT = 0x4,
        SYSTEMINSTRUCTIONPOINTER_ALIGN_SIZE = 0x10,
    } SYSTEMINSTRUCTIONPOINTER;
    inline void setSystemInstructionPointer(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xffffffffffffffffL);
        TheStructure.Common.SystemInstructionPointer = value >> SYSTEMINSTRUCTIONPOINTER_BIT_SHIFT;
    }
    inline uint64_t getSystemInstructionPointer() const {
        return TheStructure.Common.SystemInstructionPointer << SYSTEMINSTRUCTIONPOINTER_BIT_SHIFT;
    }
} STATE_SIP;
STATIC_ASSERT(12 == sizeof(STATE_SIP));

typedef struct tagSAMPLER_BORDER_COLOR_STATE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            float BorderColorRed;
            // DWORD 1
            float BorderColorGreen;
            // DWORD 2
            float BorderColorBlue;
            // DWORD 3
            float BorderColorAlpha;
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.BorderColorRed = 0.0;
        TheStructure.Common.BorderColorGreen = 0.0;
        TheStructure.Common.BorderColorBlue = 0.0;
        TheStructure.Common.BorderColorAlpha = 0.0;
    }
    static tagSAMPLER_BORDER_COLOR_STATE sInit() {
        SAMPLER_BORDER_COLOR_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setBorderColorRed(const float value) {
        TheStructure.Common.BorderColorRed = value;
    }
    inline float getBorderColorRed() const {
        return TheStructure.Common.BorderColorRed;
    }
    inline void setBorderColorGreen(const float value) {
        TheStructure.Common.BorderColorGreen = value;
    }
    inline float getBorderColorGreen() const {
        return TheStructure.Common.BorderColorGreen;
    }
    inline void setBorderColorBlue(const float value) {
        TheStructure.Common.BorderColorBlue = value;
    }
    inline float getBorderColorBlue() const {
        return TheStructure.Common.BorderColorBlue;
    }
    inline void setBorderColorAlpha(const float value) {
        TheStructure.Common.BorderColorAlpha = value;
    }
    inline float getBorderColorAlpha() const {
        return TheStructure.Common.BorderColorAlpha;
    }
} SAMPLER_BORDER_COLOR_STATE;
STATIC_ASSERT(16 == sizeof(SAMPLER_BORDER_COLOR_STATE));

#pragma pack()
