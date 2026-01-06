/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/source/xe_hpc_core/hw_info_xe_hpc_core.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using CommandsXeHpcCoreTest = ::testing::Test;

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMediaSurfaceStateWhenInitCalledThenFieldsSetToDefault) {
    using MEDIA_SURFACE_STATE = NEO::XeHpcCoreFamily::MEDIA_SURFACE_STATE;
    MEDIA_SURFACE_STATE cmd;
    cmd.init();

    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CompressionFormat);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ROTATION_NO_ROTATION_OR_0_DEGREE, cmd.TheStructure.Common.Rotation);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Cr_VCb_UPixelOffsetVDirection);
    EXPECT_EQ(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_FRAME_PICTURE, cmd.TheStructure.Common.PictureStructure);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Width);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Height);
    EXPECT_EQ(MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_LINEAR, cmd.TheStructure.Common.TileMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfacePitch);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ADDRESS_CONTROL_CLAMP, cmd.TheStructure.Common.AddressControl);
    EXPECT_EQ(MEDIA_SURFACE_STATE::MEMORY_COMPRESSION_TYPE_MEDIA_COMPRESSION, cmd.TheStructure.Common.MemoryCompressionType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Cr_VCb_UPixelOffsetUDirection);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_NORMAL, cmd.TheStructure.Common.SurfaceFormat);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.YOffsetForU_Cb);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.XOffsetForU_Cb);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.VerticalLineStrideOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.VerticalLineStride);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceBaseAddressLow); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceBaseAddressHigh);
    EXPECT_EQ(0x0u, cmd.TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMediaSurfaceStateWhenSetterUsedThenGetterReturnsValidValue) {
    using MEDIA_SURFACE_STATE = NEO::XeHpcCoreFamily::MEDIA_SURFACE_STATE;
    MEDIA_SURFACE_STATE cmd;
    cmd.init();

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_RGBA16_FLOAT);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_RGBA16_FLOAT, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_Y210);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_Y210, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_YUY2);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_YUY2, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_Y410_1010102);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_Y410_1010102, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_Y216);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_Y216, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_Y416);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_Y416, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_P010);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_P010, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_P016);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_P016, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_AYUV);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_AYUV, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_ARGB_8B);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_ARGB_8B, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPY);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPY, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPUV);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPUV, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPUVY);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPUVY, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_RGB_10B);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_RGB_10B, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_NV21NV12);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_NV21NV12, cmd.getCompressionFormat());

    cmd.setRotation(MEDIA_SURFACE_STATE::ROTATION_NO_ROTATION_OR_0_DEGREE);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ROTATION_NO_ROTATION_OR_0_DEGREE, cmd.getRotation());

    cmd.setRotation(MEDIA_SURFACE_STATE::ROTATION_90_DEGREE_ROTATION);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ROTATION_90_DEGREE_ROTATION, cmd.getRotation());

    cmd.setRotation(MEDIA_SURFACE_STATE::ROTATION_180_DEGREE_ROTATION);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ROTATION_180_DEGREE_ROTATION, cmd.getRotation());

    cmd.setRotation(MEDIA_SURFACE_STATE::ROTATION_270_DEGREE_ROTATION);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ROTATION_270_DEGREE_ROTATION, cmd.getRotation());

    cmd.setCrVCbUPixelOffsetVDirection(0x0u);
    EXPECT_EQ(0x0u, cmd.getCrVCbUPixelOffsetVDirection());
    cmd.setCrVCbUPixelOffsetVDirection(0x3u);
    EXPECT_EQ(0x3u, cmd.getCrVCbUPixelOffsetVDirection());

    cmd.setPictureStructure(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_FRAME_PICTURE);
    EXPECT_EQ(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_FRAME_PICTURE, cmd.getPictureStructure());

    cmd.setPictureStructure(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_TOP_FIELD_PICTURE);
    EXPECT_EQ(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_TOP_FIELD_PICTURE, cmd.getPictureStructure());

    cmd.setPictureStructure(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_BOTTOM_FIELD_PICTURE);
    EXPECT_EQ(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_BOTTOM_FIELD_PICTURE, cmd.getPictureStructure());

    cmd.setPictureStructure(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_INVALID_NOT_ALLOWED);
    EXPECT_EQ(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_INVALID_NOT_ALLOWED, cmd.getPictureStructure());

    cmd.setWidth(0x1u);
    EXPECT_EQ(0x1u, cmd.getWidth());
    cmd.setWidth(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getWidth());

    cmd.setHeight(0x1u);
    EXPECT_EQ(0x1u, cmd.getHeight());
    cmd.setHeight(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getHeight());

    cmd.setTileMode(MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_LINEAR);
    EXPECT_EQ(MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_LINEAR, cmd.getTileMode());

    cmd.setTileMode(MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_XMAJOR);
    EXPECT_EQ(MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_XMAJOR, cmd.getTileMode());

    cmd.setTileMode(MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_YMAJOR);              // patched
    EXPECT_EQ(MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_YMAJOR, cmd.getTileMode()); // patched

    cmd.setSurfacePitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getSurfacePitch());
    cmd.setSurfacePitch(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getSurfacePitch());

    cmd.setAddressControl(MEDIA_SURFACE_STATE::ADDRESS_CONTROL_CLAMP);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ADDRESS_CONTROL_CLAMP, cmd.getAddressControl());

    cmd.setAddressControl(MEDIA_SURFACE_STATE::ADDRESS_CONTROL_MIRROR);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ADDRESS_CONTROL_MIRROR, cmd.getAddressControl());

    cmd.setMemoryCompressionType(MEDIA_SURFACE_STATE::MEMORY_COMPRESSION_TYPE_MEDIA_COMPRESSION);
    EXPECT_EQ(MEDIA_SURFACE_STATE::MEMORY_COMPRESSION_TYPE_MEDIA_COMPRESSION, cmd.getMemoryCompressionType());

    cmd.setMemoryCompressionType(MEDIA_SURFACE_STATE::MEMORY_COMPRESSION_TYPE_RENDER_COMPRESSION);
    EXPECT_EQ(MEDIA_SURFACE_STATE::MEMORY_COMPRESSION_TYPE_RENDER_COMPRESSION, cmd.getMemoryCompressionType());

    cmd.setCrVCbUPixelOffsetVDirectionMsb(0x0u);
    EXPECT_EQ(0x0u, cmd.getCrVCbUPixelOffsetVDirectionMsb());
    cmd.setCrVCbUPixelOffsetVDirectionMsb(0x1u);
    EXPECT_EQ(0x1u, cmd.getCrVCbUPixelOffsetVDirectionMsb());

    cmd.setCrVCbUPixelOffsetUDirection(0x0u);
    EXPECT_EQ(0x0u, cmd.getCrVCbUPixelOffsetUDirection());
    cmd.setCrVCbUPixelOffsetUDirection(0x1u);
    EXPECT_EQ(0x1u, cmd.getCrVCbUPixelOffsetUDirection());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_NORMAL);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_NORMAL, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUVY);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUVY, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUV);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUV, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPY);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPY, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_8);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UNORM);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R8B8_UNORM_CRCB);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R8B8_UNORM_CRCB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R8_UNORM_CRCB);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R8_UNORM_CRCB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_Y8_UNORM);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_Y8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_A8Y8U8V8_UNORM);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_A8Y8U8V8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_B8G8R8A8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_PLANAR_422_8);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_PLANAR_422_8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_16);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_16, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R16B16_UNORM_CRCB);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R16B16_UNORM_CRCB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R16_UNORM_CRCB);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_R16_UNORM_CRCB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(MEDIA_SURFACE_STATE::SURFACE_FORMAT_Y16_UNORM);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_Y16_UNORM, cmd.getSurfaceFormat());

    cmd.setYOffsetForUCb(0x0u);
    EXPECT_EQ(0x0u, cmd.getYOffsetForUCb());
    cmd.setYOffsetForUCb(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getYOffsetForUCb());

    cmd.setXOffsetForUCb(0x0u);
    EXPECT_EQ(0x0u, cmd.getXOffsetForUCb());
    cmd.setXOffsetForUCb(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getXOffsetForUCb());

    cmd.setSurfaceMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getSurfaceMemoryObjectControlState());  // patched
    cmd.setSurfaceMemoryObjectControlState(0x7eu);              // patched
    EXPECT_EQ(0x7eu, cmd.getSurfaceMemoryObjectControlState()); // patched

    cmd.setVerticalLineStrideOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getVerticalLineStrideOffset());
    cmd.setVerticalLineStrideOffset(0x1u);
    EXPECT_EQ(0x1u, cmd.getVerticalLineStrideOffset());

    cmd.setVerticalLineStride(0x0u);
    EXPECT_EQ(0x0u, cmd.getVerticalLineStride());
    cmd.setVerticalLineStride(0x1u);
    EXPECT_EQ(0x1u, cmd.getVerticalLineStride());

    cmd.setSurfaceBaseAddress(0x0u);
    EXPECT_EQ(0x0u, cmd.getSurfaceBaseAddress());
    cmd.setSurfaceBaseAddress(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getSurfaceBaseAddress());

    cmd.setSurfaceBaseAddressHigh(0x0u);
    EXPECT_EQ(0x0u, cmd.getSurfaceBaseAddressHigh());
    cmd.setSurfaceBaseAddressHigh(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getSurfaceBaseAddressHigh());

    cmd.setYOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getYOffset());
    cmd.setYOffset(0xfu);
    EXPECT_EQ(0xfu, cmd.getYOffset());

    cmd.setXOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getXOffset());
    cmd.setXOffset(0x7fu);
    EXPECT_EQ(0x7fu, cmd.getXOffset());

    cmd.setYOffsetForVCr(0x0u);
    EXPECT_EQ(0x0u, cmd.getYOffsetForVCr());
    cmd.setYOffsetForVCr(0x7fffu);
    EXPECT_EQ(0x7fffu, cmd.getYOffsetForVCr());

    cmd.setXOffsetForVCr(0x0u);
    EXPECT_EQ(0x0u, cmd.getXOffsetForVCr());
    cmd.setXOffsetForVCr(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getXOffsetForVCr());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenPipeControlWhenInitCalledThenFieldsSetToDefault) { // patched
    using PIPE_CONTROL = NEO::XeHpcCoreFamily::PIPE_CONTROL;
    PIPE_CONTROL cmd;
    cmd.init();

    EXPECT_EQ(PIPE_CONTROL::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PredicateEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.HdcPipelineFlush);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.UnTypedDataPortCacheFlush); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CompressionControlSurface_CcsFlush);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WorkloadPartitionIdOffsetEnable);
    EXPECT_EQ(PIPE_CONTROL::_3D_COMMAND_SUB_OPCODE_PIPE_CONTROL, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(PIPE_CONTROL::_3D_COMMAND_OPCODE_PIPE_CONTROL, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(PIPE_CONTROL::COMMAND_SUBTYPE_GFXPIPE_3D, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(PIPE_CONTROL::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_NO_WRITE, cmd.TheStructure.Common.PostSyncOperation);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TlbInvalidate);
    EXPECT_EQ(0x1u, cmd.TheStructure.Common.CommandStreamerStallEnable); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.StoreDataIndex);
    EXPECT_EQ(PIPE_CONTROL::LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION, cmd.TheStructure.Common.LriPostSyncOperation);
    EXPECT_EQ(PIPE_CONTROL::DESTINATION_ADDRESS_TYPE_PPGTT, cmd.TheStructure.Common.DestinationAddressType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ProtectedMemoryDisable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.L3FabricFlush);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Address);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.AddressHigh);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.ImmediateData);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenPipeControlWhenSetterUsedThenGetterReturnsValidValue) { // patched
    using PIPE_CONTROL = NEO::XeHpcCoreFamily::PIPE_CONTROL;
    PIPE_CONTROL cmd;
    cmd.init();

    cmd.setPredicateEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getPredicateEnable());
    cmd.setPredicateEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getPredicateEnable());

    cmd.setHdcPipelineFlush(0x0u);
    EXPECT_EQ(0x0u, cmd.getHdcPipelineFlush());
    cmd.setHdcPipelineFlush(0x1u);
    EXPECT_EQ(0x1u, cmd.getHdcPipelineFlush());

    cmd.setUnTypedDataPortCacheFlush(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getUnTypedDataPortCacheFlush()); // patched
    cmd.setUnTypedDataPortCacheFlush(0x1u);              // patched
    EXPECT_EQ(0x1u, cmd.getUnTypedDataPortCacheFlush()); // patched

    cmd.setCompressionControlSurfaceCcsFlush(0x0u);
    EXPECT_EQ(0x0u, cmd.getCompressionControlSurfaceCcsFlush());
    cmd.setCompressionControlSurfaceCcsFlush(0x1u);
    EXPECT_EQ(0x1u, cmd.getCompressionControlSurfaceCcsFlush());

    cmd.setWorkloadPartitionIdOffsetEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getWorkloadPartitionIdOffsetEnable());
    cmd.setWorkloadPartitionIdOffsetEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getWorkloadPartitionIdOffsetEnable());

    cmd.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_NO_WRITE);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_NO_WRITE, cmd.getPostSyncOperation());

    cmd.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, cmd.getPostSyncOperation());

    cmd.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, cmd.getPostSyncOperation());

    cmd.setTlbInvalidate(0x0u);
    EXPECT_EQ(0x0u, cmd.getTlbInvalidate());
    cmd.setTlbInvalidate(0x1u);
    EXPECT_EQ(0x1u, cmd.getTlbInvalidate());

    cmd.setCommandStreamerStallEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getCommandStreamerStallEnable());
    cmd.setCommandStreamerStallEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getCommandStreamerStallEnable());

    cmd.setStoreDataIndex(0x0u);
    EXPECT_EQ(0x0u, cmd.getStoreDataIndex());
    cmd.setStoreDataIndex(0x1u);
    EXPECT_EQ(0x1u, cmd.getStoreDataIndex());

    cmd.setLriPostSyncOperation(PIPE_CONTROL::LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION);
    EXPECT_EQ(PIPE_CONTROL::LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION, cmd.getLriPostSyncOperation());

    cmd.setLriPostSyncOperation(PIPE_CONTROL::LRI_POST_SYNC_OPERATION_MMIO_WRITE_IMMEDIATE_DATA);
    EXPECT_EQ(PIPE_CONTROL::LRI_POST_SYNC_OPERATION_MMIO_WRITE_IMMEDIATE_DATA, cmd.getLriPostSyncOperation());

    cmd.setDestinationAddressType(PIPE_CONTROL::DESTINATION_ADDRESS_TYPE_PPGTT);
    EXPECT_EQ(PIPE_CONTROL::DESTINATION_ADDRESS_TYPE_PPGTT, cmd.getDestinationAddressType());

    cmd.setDestinationAddressType(PIPE_CONTROL::DESTINATION_ADDRESS_TYPE_GGTT);
    EXPECT_EQ(PIPE_CONTROL::DESTINATION_ADDRESS_TYPE_GGTT, cmd.getDestinationAddressType());

    cmd.setProtectedMemoryDisable(0x0u);
    EXPECT_EQ(0x0u, cmd.getProtectedMemoryDisable());
    cmd.setProtectedMemoryDisable(0x1u);
    EXPECT_EQ(0x1u, cmd.getProtectedMemoryDisable());

    cmd.setL3FabricFlush(0x0u);
    EXPECT_EQ(0x0u, cmd.getL3FabricFlush());
    cmd.setL3FabricFlush(0x1u);
    EXPECT_EQ(0x1u, cmd.getL3FabricFlush());

    cmd.setAddress(0x0u);
    EXPECT_EQ(0x0u, cmd.getAddress());
    cmd.setAddress(0xfffffffcu);
    EXPECT_EQ(0xfffffffcu, cmd.getAddress());

    cmd.setAddressHigh(0x0u);
    EXPECT_EQ(0x0u, cmd.getAddressHigh());
    cmd.setAddressHigh(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getAddressHigh());

    cmd.setImmediateData(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getImmediateData());
    cmd.setImmediateData(0xffffffffffffffffull);
    EXPECT_EQ(0xffffffffffffffffull, cmd.getImmediateData());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiAtomicWhenInitCalledThenFieldsSetToDefault) {
    using MI_ATOMIC = NEO::XeHpcCoreFamily::MI_ATOMIC;
    MI_ATOMIC cmd;
    cmd.init();

    EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH_INLINE_DATA_0, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.AtomicOpcode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ReturnDataControl);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CsStall);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.InlineData);
    EXPECT_EQ(MI_ATOMIC::DATA_SIZE_DWORD, cmd.TheStructure.Common.DataSize);
    EXPECT_EQ(MI_ATOMIC::POST_SYNC_OPERATION_NO_POST_SYNC_OPERATION, cmd.TheStructure.Common.PostSyncOperation);
    EXPECT_EQ(MI_ATOMIC::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS, cmd.TheStructure.Common.MemoryType);
    EXPECT_EQ(MI_ATOMIC::MI_COMMAND_OPCODE_MI_ATOMIC, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_ATOMIC::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WorkloadPartitionIdOffsetEnable);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.MemoryAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Operand1DataDword0);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Operand2DataDword0);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Operand1DataDword1);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Operand2DataDword1);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Operand1DataDword2);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Operand2DataDword2);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Operand1DataDword3);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Operand2DataDword3);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiAtomicWhenSetterUsedThenGetterReturnsValidValue) { // patched
    using MI_ATOMIC = NEO::XeHpcCoreFamily::MI_ATOMIC;
    MI_ATOMIC cmd;
    cmd.init();

    cmd.setDwordLength(MI_ATOMIC::DWORD_LENGTH_INLINE_DATA_0);
    EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH_INLINE_DATA_0, cmd.getDwordLength());

    cmd.setDwordLength(MI_ATOMIC::DWORD_LENGTH_INLINE_DATA_1);
    EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH_INLINE_DATA_1, cmd.getDwordLength());

    cmd.setAtomicOpcode(MI_ATOMIC::MI_COMMAND_OPCODE_MI_ATOMIC);              // patched
    EXPECT_EQ(MI_ATOMIC::MI_COMMAND_OPCODE_MI_ATOMIC, cmd.getAtomicOpcode()); // patched

    cmd.setReturnDataControl(0x0u);
    EXPECT_EQ(0x0u, cmd.getReturnDataControl());
    cmd.setReturnDataControl(0x1u);
    EXPECT_EQ(0x1u, cmd.getReturnDataControl());

    cmd.setCsStall(0x0u);
    EXPECT_EQ(0x0u, cmd.getCsStall());
    cmd.setCsStall(0x1u);
    EXPECT_EQ(0x1u, cmd.getCsStall());

    cmd.setInlineData(0x0u);
    EXPECT_EQ(0x0u, cmd.getInlineData());
    cmd.setInlineData(0x1u);
    EXPECT_EQ(0x1u, cmd.getInlineData());

    cmd.setDataSize(MI_ATOMIC::DATA_SIZE_DWORD);
    EXPECT_EQ(MI_ATOMIC::DATA_SIZE_DWORD, cmd.getDataSize());

    cmd.setDataSize(MI_ATOMIC::DATA_SIZE_QWORD);
    EXPECT_EQ(MI_ATOMIC::DATA_SIZE_QWORD, cmd.getDataSize());

    cmd.setDataSize(MI_ATOMIC::DATA_SIZE_OCTWORD);
    EXPECT_EQ(MI_ATOMIC::DATA_SIZE_OCTWORD, cmd.getDataSize());

    cmd.setPostSyncOperation(MI_ATOMIC::POST_SYNC_OPERATION_NO_POST_SYNC_OPERATION);
    EXPECT_EQ(MI_ATOMIC::POST_SYNC_OPERATION_NO_POST_SYNC_OPERATION, cmd.getPostSyncOperation());

    cmd.setPostSyncOperation(MI_ATOMIC::POST_SYNC_OPERATION_POST_SYNC_OPERATION);
    EXPECT_EQ(MI_ATOMIC::POST_SYNC_OPERATION_POST_SYNC_OPERATION, cmd.getPostSyncOperation());

    cmd.setMemoryType(MI_ATOMIC::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS);
    EXPECT_EQ(MI_ATOMIC::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS, cmd.getMemoryType());

    cmd.setMemoryType(MI_ATOMIC::MEMORY_TYPE_GLOBAL_GRAPHICS_ADDRESS);
    EXPECT_EQ(MI_ATOMIC::MEMORY_TYPE_GLOBAL_GRAPHICS_ADDRESS, cmd.getMemoryType());

    cmd.setWorkloadPartitionIdOffsetEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getWorkloadPartitionIdOffsetEnable());
    cmd.setWorkloadPartitionIdOffsetEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getWorkloadPartitionIdOffsetEnable());

    cmd.setMemoryAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getMemoryAddress());
    cmd.setMemoryAddress(0xfffffffffffffffcull);
    EXPECT_EQ(0xfffffffffffffffcull, cmd.getMemoryAddress());

    cmd.setOperand1DataDword0(0x0u);
    EXPECT_EQ(0x0u, cmd.getOperand1DataDword0());
    cmd.setOperand1DataDword0(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getOperand1DataDword0());

    cmd.setOperand2DataDword0(0x0u);
    EXPECT_EQ(0x0u, cmd.getOperand2DataDword0());
    cmd.setOperand2DataDword0(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getOperand2DataDword0());

    cmd.setOperand1DataDword1(0x0u);
    EXPECT_EQ(0x0u, cmd.getOperand1DataDword1());
    cmd.setOperand1DataDword1(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getOperand1DataDword1());

    cmd.setOperand2DataDword1(0x0u);
    EXPECT_EQ(0x0u, cmd.getOperand2DataDword1());
    cmd.setOperand2DataDword1(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getOperand2DataDword1());

    cmd.setOperand1DataDword2(0x0u);
    EXPECT_EQ(0x0u, cmd.getOperand1DataDword2());
    cmd.setOperand1DataDword2(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getOperand1DataDword2());

    cmd.setOperand2DataDword2(0x0u);
    EXPECT_EQ(0x0u, cmd.getOperand2DataDword2());
    cmd.setOperand2DataDword2(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getOperand2DataDword2());

    cmd.setOperand1DataDword3(0x0u);
    EXPECT_EQ(0x0u, cmd.getOperand1DataDword3());
    cmd.setOperand1DataDword3(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getOperand1DataDword3());

    cmd.setOperand2DataDword3(0x0u);
    EXPECT_EQ(0x0u, cmd.getOperand2DataDword3());
    cmd.setOperand2DataDword3(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getOperand2DataDword3());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiBatchBufferEndWhenInitCalledThenFieldsSetToDefault) {
    using MI_BATCH_BUFFER_END = NEO::XeHpcCoreFamily::MI_BATCH_BUFFER_END;
    MI_BATCH_BUFFER_END cmd;
    cmd.init();

    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PredicateEnable);
    EXPECT_EQ(MI_BATCH_BUFFER_END::MI_COMMAND_OPCODE_MI_BATCH_BUFFER_END, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_BATCH_BUFFER_END::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiBatchBufferEndWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_BATCH_BUFFER_END = NEO::XeHpcCoreFamily::MI_BATCH_BUFFER_END;
    MI_BATCH_BUFFER_END cmd;
    cmd.init();

    cmd.setPredicateEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getPredicateEnable());
    cmd.setPredicateEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getPredicateEnable());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiLoadRegisterImmWhenInitCalledThenFieldsSetToDefault) {
    using MI_LOAD_REGISTER_IMM = NEO::XeHpcCoreFamily::MI_LOAD_REGISTER_IMM;
    MI_LOAD_REGISTER_IMM cmd;
    cmd.init();

    EXPECT_EQ(MI_LOAD_REGISTER_IMM::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ByteWriteDisables);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MmioRemapEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.AddCsMmioStartOffset);
    EXPECT_EQ(MI_LOAD_REGISTER_IMM::MI_COMMAND_OPCODE_MI_LOAD_REGISTER_IMM, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_LOAD_REGISTER_IMM::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.RegisterOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DataDword);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiLoadRegisterImmWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_LOAD_REGISTER_IMM = NEO::XeHpcCoreFamily::MI_LOAD_REGISTER_IMM;
    MI_LOAD_REGISTER_IMM cmd;
    cmd.init();

    cmd.setByteWriteDisables(0x0u);
    EXPECT_EQ(0x0u, cmd.getByteWriteDisables());
    cmd.setByteWriteDisables(0xfu);
    EXPECT_EQ(0xfu, cmd.getByteWriteDisables());

    cmd.setMmioRemapEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getMmioRemapEnable());
    cmd.setMmioRemapEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getMmioRemapEnable());

    cmd.setAddCsMmioStartOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getAddCsMmioStartOffset());
    cmd.setAddCsMmioStartOffset(0x1u);
    EXPECT_EQ(0x1u, cmd.getAddCsMmioStartOffset());

    cmd.setRegisterOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getRegisterOffset());
    cmd.setRegisterOffset(0x7ffffcu);
    EXPECT_EQ(0x7ffffcu, cmd.getRegisterOffset());

    cmd.setDataDword(0x0u);
    EXPECT_EQ(0x0u, cmd.getDataDword());
    cmd.setDataDword(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getDataDword());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiNoopWhenInitCalledThenFieldsSetToDefault) {
    using MI_NOOP = NEO::XeHpcCoreFamily::MI_NOOP;
    MI_NOOP cmd;
    cmd.init();

    EXPECT_EQ(0x0u, cmd.TheStructure.Common.IdentificationNumber);
    EXPECT_EQ(MI_NOOP::MI_COMMAND_OPCODE_MI_NOOP, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_NOOP::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiNoopWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_NOOP = NEO::XeHpcCoreFamily::MI_NOOP;
    MI_NOOP cmd;
    cmd.init();

    cmd.setIdentificationNumber(0x0u);
    EXPECT_EQ(0x0u, cmd.getIdentificationNumber());
    cmd.setIdentificationNumber(0x3fffffu);
    EXPECT_EQ(0x3fffffu, cmd.getIdentificationNumber());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenRenderSurfaceStateWhenInitCalledThenFieldsSetToDefault) {
    using RENDER_SURFACE_STATE = NEO::XeHpcCoreFamily::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE cmd;
    cmd.init();

    EXPECT_EQ(RENDER_SURFACE_STATE::MEDIA_BOUNDARY_PIXEL_MODE_NORMAL_MODE, cmd.TheStructure.Common.MediaBoundaryPixelMode);
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_CACHE_READ_WRITE_MODE_WRITE_ONLY_CACHE, cmd.TheStructure.Common.RenderCacheReadWriteMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.VerticalLineStrideOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.VerticalLineStride);
    EXPECT_EQ(RENDER_SURFACE_STATE::TILE_MODE_LINEAR, cmd.TheStructure.Common.TileMode);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_16, cmd.TheStructure.Common.SurfaceHorizontalAlignment);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, cmd.TheStructure.Common.SurfaceVerticalAlignment);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceFormat);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D, cmd.TheStructure.Common.SurfaceType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceQpitch);
    EXPECT_EQ(RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_DISABLE, cmd.TheStructure.Common.SampleTapDiscardDisable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.BaseMipLevel);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Width);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Height);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DepthStencilResource);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfacePitch);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Depth);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MultisamplePositionPaletteIndex);
    EXPECT_EQ(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1, cmd.TheStructure.Common.NumberOfMultisamples);
    EXPECT_EQ(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS, cmd.TheStructure.Common.MultisampledSurfaceStorageFormat);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.RenderTargetViewExtent);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MinimumArrayElement);
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_0DEG, cmd.TheStructure.Common.RenderTargetAndSampleUnormRotation);
    EXPECT_EQ(RENDER_SURFACE_STATE::DECOMPRESS_IN_L3_DISABLE, cmd.TheStructure.Common.DecompressInL3);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MipCountLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceMinLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MipTailStartLod);
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, cmd.TheStructure.Common.CoherencyType);      // patched
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP, cmd.TheStructure.Common.L1CacheControlCachePolicy); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.YOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.XOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ResourceMinLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ShaderChannelSelectAlpha);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ShaderChannelSelectBlue);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ShaderChannelSelectGreen);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ShaderChannelSelectRed);
    EXPECT_EQ(RENDER_SURFACE_STATE::MEMORY_COMPRESSION_MODE_HORIZONTAL, cmd.TheStructure.Common.MemoryCompressionMode);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.SurfaceBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.QuiltWidth);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.QuiltHeight);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ProceduralTexture);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CompressionFormat);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ClearAddressLow);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ClearAddressHigh);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsPlanar.YOffsetForUOrUvPlane);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsPlanar.XOffsetForUOrUvPlane);
    EXPECT_EQ(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_DISABLE, cmd.TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsPlanar.YOffsetForVPlane);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsPlanar.XOffsetForVPlane);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_NONE, cmd.TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceMode);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfacePitch);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceQpitch);
    EXPECT_EQ(0x0ull, cmd.TheStructure._SurfaceFormatIsnotPlanarAndMemoryCompressionEnableIs0.AuxiliarySurfaceBaseAddress);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenRenderSurfaceStateWhenSetterUsedThenGetterReturnsValidValue) { // patched
    using RENDER_SURFACE_STATE = NEO::XeHpcCoreFamily::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE cmd;
    cmd.init();

    cmd.setMediaBoundaryPixelMode(RENDER_SURFACE_STATE::MEDIA_BOUNDARY_PIXEL_MODE_NORMAL_MODE);
    EXPECT_EQ(RENDER_SURFACE_STATE::MEDIA_BOUNDARY_PIXEL_MODE_NORMAL_MODE, cmd.getMediaBoundaryPixelMode());

    cmd.setMediaBoundaryPixelMode(RENDER_SURFACE_STATE::MEDIA_BOUNDARY_PIXEL_MODE_PROGRESSIVE_FRAME);
    EXPECT_EQ(RENDER_SURFACE_STATE::MEDIA_BOUNDARY_PIXEL_MODE_PROGRESSIVE_FRAME, cmd.getMediaBoundaryPixelMode());

    cmd.setMediaBoundaryPixelMode(RENDER_SURFACE_STATE::MEDIA_BOUNDARY_PIXEL_MODE_INTERLACED_FRAME);
    EXPECT_EQ(RENDER_SURFACE_STATE::MEDIA_BOUNDARY_PIXEL_MODE_INTERLACED_FRAME, cmd.getMediaBoundaryPixelMode());

    cmd.setRenderCacheReadWriteMode(RENDER_SURFACE_STATE::RENDER_CACHE_READ_WRITE_MODE_WRITE_ONLY_CACHE);
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_CACHE_READ_WRITE_MODE_WRITE_ONLY_CACHE, cmd.getRenderCacheReadWriteMode());

    cmd.setRenderCacheReadWriteMode(RENDER_SURFACE_STATE::RENDER_CACHE_READ_WRITE_MODE_READ_WRITE_CACHE);
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_CACHE_READ_WRITE_MODE_READ_WRITE_CACHE, cmd.getRenderCacheReadWriteMode());

    cmd.setVerticalLineStrideOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getVerticalLineStrideOffset());
    cmd.setVerticalLineStrideOffset(0x1u);
    EXPECT_EQ(0x1u, cmd.getVerticalLineStrideOffset());

    cmd.setVerticalLineStride(0x0u);
    EXPECT_EQ(0x0u, cmd.getVerticalLineStride());
    cmd.setVerticalLineStride(0x1u);
    EXPECT_EQ(0x1u, cmd.getVerticalLineStride());

    cmd.setTileMode(RENDER_SURFACE_STATE::TILE_MODE_LINEAR);
    EXPECT_EQ(RENDER_SURFACE_STATE::TILE_MODE_LINEAR, cmd.getTileMode());

    cmd.setTileMode(RENDER_SURFACE_STATE::TILE_MODE_TILE64);
    EXPECT_EQ(RENDER_SURFACE_STATE::TILE_MODE_TILE64, cmd.getTileMode());

    cmd.setTileMode(RENDER_SURFACE_STATE::TILE_MODE_XMAJOR);
    EXPECT_EQ(RENDER_SURFACE_STATE::TILE_MODE_XMAJOR, cmd.getTileMode());

    cmd.setTileMode(RENDER_SURFACE_STATE::TILE_MODE_TILE4);
    EXPECT_EQ(RENDER_SURFACE_STATE::TILE_MODE_TILE4, cmd.getTileMode());

    cmd.setSurfaceHorizontalAlignment(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_16);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_16, cmd.getSurfaceHorizontalAlignment());

    cmd.setSurfaceHorizontalAlignment(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_32);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_32, cmd.getSurfaceHorizontalAlignment());

    cmd.setSurfaceHorizontalAlignment(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_64);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_64, cmd.getSurfaceHorizontalAlignment());

    cmd.setSurfaceHorizontalAlignment(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_128);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_128, cmd.getSurfaceHorizontalAlignment());

    cmd.setSurfaceVerticalAlignment(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, cmd.getSurfaceVerticalAlignment());

    cmd.setSurfaceVerticalAlignment(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_8, cmd.getSurfaceVerticalAlignment());

    cmd.setSurfaceVerticalAlignment(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_16);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_16, cmd.getSurfaceVerticalAlignment());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32X32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32X32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_422_8_P208);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_422_8_P208, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_8_SAMPLE_8X8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_8_SAMPLE_8X8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_411_8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_411_8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_422_8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_422_8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_VDI);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_VDI, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_NORMAL_SAMPLE_8X8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_NORMAL_SAMPLE_8X8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUVY_SAMPLE_8X8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUVY_SAMPLE_8X8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUV_SAMPLE_8X8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUV_SAMPLE_8X8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPY_SAMPLE_8X8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPY_SAMPLE_8X8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_FLOAT_LD);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_FLOAT_LD, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_16_SAMPLE_8X8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_16_SAMPLE_8X8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16B16_UNORM_SAMPLE_8X8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16B16_UNORM_SAMPLE_8X8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_Y16_UNORM_SAMPLE_8X8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_Y16_UNORM_SAMPLE_8X8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_Y32_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_Y32_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_SFIXED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32A32_SFIXED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64_PASSTHRU);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64_PASSTHRU, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_FLOAT_LD);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_FLOAT_LD, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_SFIXED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32B32_SFIXED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_X32_TYPELESS_G8X24_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_X32_TYPELESS_G8X24_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L32A32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L32A32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16X16_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16X16_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16X16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16X16_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A32X32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A32X32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L32X32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L32X32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_I32X32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_I32X32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16A16_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_FLOAT_LD);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_FLOAT_LD, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS_LD);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS_LD, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_SFIXED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32G32_SFIXED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64_PASSTHRU);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64_PASSTHRU, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8G8R8A8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8G8R8A8_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8G8R8A8_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10_SNORM_A2_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10_SNORM_A2_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UNORM_SAMPLE_8X8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_UNORM_SAMPLE_8X8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R11G11B10_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R11G11B10_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10_FLOAT_A2_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10_FLOAT_A2_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R24_UNORM_X8_TYPELESS);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R24_UNORM_X8_TYPELESS, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_X24_TYPELESS_G8_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_X24_TYPELESS_G8_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_FLOAT_LD);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_FLOAT_LD, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R24_UNORM_X8_TYPELESS_LD);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R24_UNORM_X8_TYPELESS_LD, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L32_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L32_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A32_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A32_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L16A16_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L16A16_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_I24X8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_I24X8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L24X8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L24X8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A24X8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A24X8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_I32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_I32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A32_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A32_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_X8B8_UNORM_G8R8_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_X8B8_UNORM_G8R8_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A8X8_UNORM_G8R8_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A8X8_UNORM_G8R8_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8X8_UNORM_G8R8_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8X8_UNORM_G8R8_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8G8R8X8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8G8R8X8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8G8R8X8_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B8G8R8X8_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8X8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8X8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8X8_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8X8_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R9G9B9E5_SHAREDEXP);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R9G9B9E5_SHAREDEXP, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10X2_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10X2_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L16A16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L16A16_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10X2_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10X2_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8B8G8A8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8B8G8A8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_SINT_NOA);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_SINT_NOA, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UINT_NOA);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UINT_NOA, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_YUV);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_YUV, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_SNCK);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_SNCK, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_NOA);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8A8_UNORM_NOA, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G6R5_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G6R5_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G6R5_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G6R5_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G5R5A1_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G5R5A1_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G5R5A1_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G5R5A1_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B4G4R4A4_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B4G4R4A4_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B4G4R4A4_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B4G4R4A4_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A8P8_UNORM_PALETTE0);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A8P8_UNORM_PALETTE0, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A8P8_UNORM_PALETTE1);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A8P8_UNORM_PALETTE1, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_I16_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_I16_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L16_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L16_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A16_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A16_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8A8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8A8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_I16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_I16_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L16_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A16_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8A8_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8A8_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R5G5_SNORM_B6_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R5G5_SNORM_B6_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G5R5X1_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G5R5X1_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G5R5X1_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B5G5R5X1_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_SNORM_DX9);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8_SNORM_DX9, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_FLOAT_DX9);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16_FLOAT_DX9, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_P8A8_UNORM_PALETTE0);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_P8A8_UNORM_PALETTE0, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_P8A8_UNORM_PALETTE1);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_P8A8_UNORM_PALETTE1, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A1B5G5R5_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A1B5G5R5_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A4B4G4R4_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A4B4G4R4_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8A8_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8A8_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8A8_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8A8_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_I8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_I8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_P4A4_UNORM_PALETTE0);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_P4A4_UNORM_PALETTE0, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A4P4_UNORM_PALETTE0);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A4P4_UNORM_PALETTE0, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_P8_UNORM_PALETTE0);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_P8_UNORM_PALETTE0, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_P8_UNORM_PALETTE1);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_P8_UNORM_PALETTE1, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_P4A4_UNORM_PALETTE1);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_P4A4_UNORM_PALETTE1, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_A4P4_UNORM_PALETTE1);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_A4P4_UNORM_PALETTE1, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_Y8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_Y8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_L8_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_I8_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_I8_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_I8_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_I8_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_DXT1_RGB_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_DXT1_RGB_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_NORMAL);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_NORMAL, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUVY);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUVY, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_P2_UNORM_PALETTE0);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_P2_UNORM_PALETTE0, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_P2_UNORM_PALETTE1);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_P2_UNORM_PALETTE1, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC1_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC1_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC2_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC2_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC3_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC3_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC4_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC4_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC5_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC5_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC1_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC1_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC2_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC2_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC3_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC3_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_MONO8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_MONO8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUV);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPUV, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPY);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_YCRCB_SWAPY, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_DXT1_RGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_DXT1_RGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64B64A64_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64B64A64_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64B64_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64B64_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC4_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC4_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC5_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC5_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_FLOAT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8B8_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8B8_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC6H_SF16);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC6H_SF16, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC7_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC7_UNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC7_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC7_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC6H_UF16);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_BC6H_UF16, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_16);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_PLANAR_420_16, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_UNORM_SRGB);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_UNORM_SRGB, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC1_RGB8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC1_RGB8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_RGB8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_RGB8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_EAC_R11);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_EAC_R11, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_EAC_RG11);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_EAC_RG11, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_EAC_SIGNED_R11);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_EAC_SIGNED_R11, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_EAC_SIGNED_RG11);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_EAC_SIGNED_RG11, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_SRGB8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_SRGB8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R16G16B16_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_SFIXED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R32_SFIXED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R10G10B10A2_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_SNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_SNORM, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_USCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_USCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_SSCALED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_SSCALED, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_B10G10R10A2_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64B64A64_PASSTHRU);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64B64A64_PASSTHRU, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64B64_PASSTHRU);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R64G64B64_PASSTHRU, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_RGB8_PTA);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_RGB8_PTA, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_SRGB8_PTA);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_SRGB8_PTA, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_EAC_RGBA8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_EAC_RGBA8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_EAC_SRGB8_A8);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_ETC2_EAC_SRGB8_A8, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_UINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_UINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_SINT);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R8G8B8_SINT, cmd.getSurfaceFormat());

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_RAW);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_RAW, cmd.getSurfaceFormat());

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D, cmd.getSurfaceType());

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D, cmd.getSurfaceType());

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D, cmd.getSurfaceType());

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_CUBE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_CUBE, cmd.getSurfaceType());

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, cmd.getSurfaceType());

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, cmd.getSurfaceType());

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, cmd.getSurfaceType());

    cmd.setSurfaceQPitch(0x0u);                  // patched
    EXPECT_EQ(0x0u, cmd.getSurfaceQPitch());     // patched
    cmd.setSurfaceQPitch(0x1fffcu);              // patched
    EXPECT_EQ(0x1fffcu, cmd.getSurfaceQPitch()); // patched

    cmd.setSampleTapDiscardDisable(RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_DISABLE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_DISABLE, cmd.getSampleTapDiscardDisable());

    cmd.setSampleTapDiscardDisable(RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_ENABLE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_ENABLE, cmd.getSampleTapDiscardDisable());

    cmd.setBaseMipLevel(0x0u);
    EXPECT_EQ(0x0u, cmd.getBaseMipLevel());
    cmd.setBaseMipLevel(0x1fu);
    EXPECT_EQ(0x1fu, cmd.getBaseMipLevel());

    cmd.setMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getMemoryObjectControlState());  // patched
    cmd.setMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getMemoryObjectControlState()); // patched

    cmd.setWidth(0x1u);
    EXPECT_EQ(0x1u, cmd.getWidth());
    cmd.setWidth(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getWidth());

    cmd.setHeight(0x1u);
    EXPECT_EQ(0x1u, cmd.getHeight());
    cmd.setHeight(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getHeight());

    cmd.setDepthStencilResource(0x0u);
    EXPECT_EQ(0x0u, cmd.getDepthStencilResource());
    cmd.setDepthStencilResource(0x1u);
    EXPECT_EQ(0x1u, cmd.getDepthStencilResource());

    cmd.setSurfacePitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getSurfacePitch());
    cmd.setSurfacePitch(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getSurfacePitch());

    cmd.setDepth(0x1u);
    EXPECT_EQ(0x1u, cmd.getDepth());
    cmd.setDepth(0x7ffu);
    EXPECT_EQ(0x7ffu, cmd.getDepth());

    cmd.setMultisamplePositionPaletteIndex(0x0u);
    EXPECT_EQ(0x0u, cmd.getMultisamplePositionPaletteIndex());
    cmd.setMultisamplePositionPaletteIndex(0x7u);
    EXPECT_EQ(0x7u, cmd.getMultisamplePositionPaletteIndex());

    cmd.setNumberOfMultisamples(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1);
    EXPECT_EQ(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2);
    EXPECT_EQ(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4);
    EXPECT_EQ(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8);
    EXPECT_EQ(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16);
    EXPECT_EQ(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16, cmd.getNumberOfMultisamples());

    cmd.setMultisampledSurfaceStorageFormat(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);
    EXPECT_EQ(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS, cmd.getMultisampledSurfaceStorageFormat());

    cmd.setMultisampledSurfaceStorageFormat(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
    EXPECT_EQ(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL, cmd.getMultisampledSurfaceStorageFormat());

    cmd.setRenderTargetViewExtent(0x1u);
    EXPECT_EQ(0x1u, cmd.getRenderTargetViewExtent());
    cmd.setRenderTargetViewExtent(0x7ffu);
    EXPECT_EQ(0x7ffu, cmd.getRenderTargetViewExtent());

    cmd.setMinimumArrayElement(0x0u);
    EXPECT_EQ(0x0u, cmd.getMinimumArrayElement());
    cmd.setMinimumArrayElement(0x7ffu);
    EXPECT_EQ(0x7ffu, cmd.getMinimumArrayElement());

    cmd.setRenderTargetAndSampleUnormRotation(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_0DEG);
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_0DEG, cmd.getRenderTargetAndSampleUnormRotation());

    cmd.setRenderTargetAndSampleUnormRotation(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_90DEG);
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_90DEG, cmd.getRenderTargetAndSampleUnormRotation());

    cmd.setRenderTargetAndSampleUnormRotation(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_180DEG);
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_180DEG, cmd.getRenderTargetAndSampleUnormRotation());

    cmd.setRenderTargetAndSampleUnormRotation(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_270DEG);
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_270DEG, cmd.getRenderTargetAndSampleUnormRotation());

    cmd.setDecompressInL3(RENDER_SURFACE_STATE::DECOMPRESS_IN_L3_DISABLE);
    EXPECT_EQ(RENDER_SURFACE_STATE::DECOMPRESS_IN_L3_DISABLE, cmd.getDecompressInL3());

    cmd.setMIPCountLOD(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getMIPCountLOD()); // patched
    cmd.setMIPCountLOD(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getMIPCountLOD()); // patched

    cmd.setSurfaceMinLOD(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getSurfaceMinLOD()); // patched
    cmd.setSurfaceMinLOD(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getSurfaceMinLOD()); // patched

    cmd.setMipTailStartLOD(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getMipTailStartLOD()); // patched
    cmd.setMipTailStartLOD(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getMipTailStartLOD()); // patched

    cmd.setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);              // patched
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, cmd.getCoherencyType()); // patched

    cmd.setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT);              // patched
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT, cmd.getCoherencyType()); // patched

    cmd.setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_MULTI_GPU_COHERENT);
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_MULTI_GPU_COHERENT, cmd.getCoherencyType());

    cmd.setYOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getYOffset());
    cmd.setYOffset(0x1cu);
    EXPECT_EQ(0x1cu, cmd.getYOffset());

    cmd.setXOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getXOffset());
    cmd.setXOffset(0x1fcu);
    EXPECT_EQ(0x1fcu, cmd.getXOffset());

    cmd.setResourceMinLod(0x0u);
    EXPECT_EQ(0x0u, cmd.getResourceMinLod());
    cmd.setResourceMinLod(0xfffu);
    EXPECT_EQ(0xfffu, cmd.getResourceMinLod());

    cmd.setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, cmd.getShaderChannelSelectAlpha());

    cmd.setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE, cmd.getShaderChannelSelectAlpha());

    cmd.setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, cmd.getShaderChannelSelectAlpha());

    cmd.setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN, cmd.getShaderChannelSelectAlpha());

    cmd.setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE, cmd.getShaderChannelSelectAlpha());

    cmd.setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, cmd.getShaderChannelSelectAlpha());

    cmd.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, cmd.getShaderChannelSelectBlue());

    cmd.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE, cmd.getShaderChannelSelectBlue());

    cmd.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, cmd.getShaderChannelSelectBlue());

    cmd.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN, cmd.getShaderChannelSelectBlue());

    cmd.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE, cmd.getShaderChannelSelectBlue());

    cmd.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, cmd.getShaderChannelSelectBlue());

    cmd.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, cmd.getShaderChannelSelectGreen());

    cmd.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE, cmd.getShaderChannelSelectGreen());

    cmd.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, cmd.getShaderChannelSelectGreen());

    cmd.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN, cmd.getShaderChannelSelectGreen());

    cmd.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE, cmd.getShaderChannelSelectGreen());

    cmd.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, cmd.getShaderChannelSelectGreen());

    cmd.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO, cmd.getShaderChannelSelectRed());

    cmd.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE, cmd.getShaderChannelSelectRed());

    cmd.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, cmd.getShaderChannelSelectRed());

    cmd.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN, cmd.getShaderChannelSelectRed());

    cmd.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE, cmd.getShaderChannelSelectRed());

    cmd.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    EXPECT_EQ(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA, cmd.getShaderChannelSelectRed());

    cmd.setMemoryCompressionMode(RENDER_SURFACE_STATE::MEMORY_COMPRESSION_MODE_HORIZONTAL);
    EXPECT_EQ(RENDER_SURFACE_STATE::MEMORY_COMPRESSION_MODE_HORIZONTAL, cmd.getMemoryCompressionMode());

    cmd.setSurfaceBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getSurfaceBaseAddress());
    cmd.setSurfaceBaseAddress(0xffffffffffffffffull);
    EXPECT_EQ(0xffffffffffffffffull, cmd.getSurfaceBaseAddress());

    cmd.setQuiltWidth(0x0u);
    EXPECT_EQ(0x0u, cmd.getQuiltWidth());
    cmd.setQuiltWidth(0x1fu);
    EXPECT_EQ(0x1fu, cmd.getQuiltWidth());

    cmd.setQuiltHeight(0x0u);
    EXPECT_EQ(0x0u, cmd.getQuiltHeight());
    cmd.setQuiltHeight(0x1fu);
    EXPECT_EQ(0x1fu, cmd.getQuiltHeight());

    cmd.setProceduralTexture(0x0u);
    EXPECT_EQ(0x0u, cmd.getProceduralTexture());
    cmd.setProceduralTexture(0x1u);
    EXPECT_EQ(0x1u, cmd.getProceduralTexture());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_RGBA16_FLOAT);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_RGBA16_FLOAT, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_Y210);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_Y210, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_YUY2);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_YUY2, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_Y410_1010102);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_Y410_1010102, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_Y216);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_Y216, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_Y416);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_Y416, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_P010);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_P010, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_P016);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_P016, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_AYUV);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_AYUV, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_ARGB_8B);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_ARGB_8B, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPY);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPY, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPUV);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPUV, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPUVY);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_YCRCB_SWAPUVY, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_RGB_10B);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_RGB_10B, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_NV21NV12);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_NV21NV12, cmd.getCompressionFormat());

    cmd.setClearColorAddress(0x0u);                     // patched
    EXPECT_EQ(0x0u, cmd.getClearColorAddress());        // patched
    cmd.setClearColorAddress(0xffffffc0u);              // patched
    EXPECT_EQ(0xffffffc0u, cmd.getClearColorAddress()); // patched

    cmd.setClearColorAddressHigh(0x0u);                 // patched
    EXPECT_EQ(0x0u, cmd.getClearColorAddressHigh());    // patched
    cmd.setClearColorAddressHigh(0xffffu);              // patched
    EXPECT_EQ(0xffffu, cmd.getClearColorAddressHigh()); // patched

    cmd.setYOffsetForUOrUvPlane(0x0u);
    EXPECT_EQ(0x0u, cmd.getYOffsetForUOrUvPlane());
    cmd.setYOffsetForUOrUvPlane(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getYOffsetForUOrUvPlane());

    cmd.setXOffsetForUOrUvPlane(0x0u);
    EXPECT_EQ(0x0u, cmd.getXOffsetForUOrUvPlane());
    cmd.setXOffsetForUOrUvPlane(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getXOffsetForUOrUvPlane());

    cmd.setHalfPitchForChroma(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_DISABLE);
    EXPECT_EQ(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_DISABLE, cmd.getHalfPitchForChroma());

    cmd.setHalfPitchForChroma(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_ENABLE);
    EXPECT_EQ(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_ENABLE, cmd.getHalfPitchForChroma());

    cmd.setYOffsetForVPlane(0x0u);
    EXPECT_EQ(0x0u, cmd.getYOffsetForVPlane());
    cmd.setYOffsetForVPlane(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getYOffsetForVPlane());

    cmd.setXOffsetForVPlane(0x0u);
    EXPECT_EQ(0x0u, cmd.getXOffsetForVPlane());
    cmd.setXOffsetForVPlane(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getXOffsetForVPlane());

    cmd.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_NONE, cmd.getAuxiliarySurfaceMode());

    cmd.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_CCS_D);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_CCS_D, cmd.getAuxiliarySurfaceMode());

    cmd.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_APPEND);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_APPEND, cmd.getAuxiliarySurfaceMode());

    cmd.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE, cmd.getAuxiliarySurfaceMode());

    cmd.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_CCS_E, cmd.getAuxiliarySurfaceMode());

    cmd.setAuxiliarySurfacePitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getAuxiliarySurfacePitch());
    cmd.setAuxiliarySurfacePitch(0x3ffu);
    EXPECT_EQ(0x3ffu, cmd.getAuxiliarySurfacePitch());

    cmd.setAuxiliarySurfaceQPitch(0x0u);                  // patched
    EXPECT_EQ(0x0u, cmd.getAuxiliarySurfaceQPitch());     // patched
    cmd.setAuxiliarySurfaceQPitch(0x1fffcu);              // patched
    EXPECT_EQ(0x1fffcu, cmd.getAuxiliarySurfaceQPitch()); // patched

    cmd.setAuxiliarySurfaceBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getAuxiliarySurfaceBaseAddress());
    cmd.setAuxiliarySurfaceBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getAuxiliarySurfaceBaseAddress());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenStateBaseAddressWhenInitCalledThenFieldsSetToDefault) { // patched
    using STATE_BASE_ADDRESS = NEO::XeHpcCoreFamily::STATE_BASE_ADDRESS;
    STATE_BASE_ADDRESS cmd;
    cmd.init();

    EXPECT_EQ(STATE_BASE_ADDRESS::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(STATE_BASE_ADDRESS::_3D_COMMAND_SUB_OPCODE_STATE_BASE_ADDRESS, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(STATE_BASE_ADDRESS::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(STATE_BASE_ADDRESS::COMMAND_SUBTYPE_GFXPIPE_COMMON, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(STATE_BASE_ADDRESS::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.GeneralStateMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.GeneralStateBaseAddress);
    EXPECT_EQ(STATE_BASE_ADDRESS::COHERENCY_SETTING_MODIFY_ENABLE_DISABLE_WRITE_TO_THIS_DW, cmd.TheStructure.Common.CoherencySettingModifyEnable);
    EXPECT_EQ(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED, cmd.TheStructure.Common.EnableMemoryCompressionForAllStatelessAccesses);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.StatelessDataPortAccessMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, cmd.TheStructure.Common.L1CachePolicyL1CacheControl); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceStateMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.SurfaceStateBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DynamicStateMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.DynamicStateBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.InstructionMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.InstructionBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.GeneralStateBufferSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DynamicStateBufferSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.IndirectObjectBufferSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.InstructionBufferSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.BindlessSurfaceStateBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.BindlessSurfaceStateSize);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenStateBaseAddressWhenSetterUsedThenGetterReturnsValidValue) { // patched
    using STATE_BASE_ADDRESS = NEO::XeHpcCoreFamily::STATE_BASE_ADDRESS;
    STATE_BASE_ADDRESS cmd;
    cmd.init();

    cmd.setGeneralStateMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getGeneralStateMemoryObjectControlState());  // patched
    cmd.setGeneralStateMemoryObjectControlState(0x7eu);              // patched
    EXPECT_EQ(0x7eu, cmd.getGeneralStateMemoryObjectControlState()); // patched

    cmd.setGeneralStateBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getGeneralStateBaseAddress());
    cmd.setGeneralStateBaseAddress(0x1fffffffffff000ull);              // patched
    EXPECT_EQ(0x1fffffffffff000ull, cmd.getGeneralStateBaseAddress()); // patched

    cmd.setCoherencySettingModifyEnable(STATE_BASE_ADDRESS::COHERENCY_SETTING_MODIFY_ENABLE_DISABLE_WRITE_TO_THIS_DW);
    EXPECT_EQ(STATE_BASE_ADDRESS::COHERENCY_SETTING_MODIFY_ENABLE_DISABLE_WRITE_TO_THIS_DW, cmd.getCoherencySettingModifyEnable());

    cmd.setCoherencySettingModifyEnable(STATE_BASE_ADDRESS::COHERENCY_SETTING_MODIFY_ENABLE_ENABLE_WRITE_TO_THIS_DW);
    EXPECT_EQ(STATE_BASE_ADDRESS::COHERENCY_SETTING_MODIFY_ENABLE_ENABLE_WRITE_TO_THIS_DW, cmd.getCoherencySettingModifyEnable());

    cmd.setEnableMemoryCompressionForAllStatelessAccesses(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED);
    EXPECT_EQ(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED, cmd.getEnableMemoryCompressionForAllStatelessAccesses());

    cmd.setEnableMemoryCompressionForAllStatelessAccesses(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_ENABLED);
    EXPECT_EQ(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_ENABLED, cmd.getEnableMemoryCompressionForAllStatelessAccesses());

    cmd.setStatelessDataPortAccessMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getStatelessDataPortAccessMemoryObjectControlState());  // patched
    cmd.setStatelessDataPortAccessMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getStatelessDataPortAccessMemoryObjectControlState()); // patched

    cmd.setSurfaceStateMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getSurfaceStateMemoryObjectControlState());  // patched
    cmd.setSurfaceStateMemoryObjectControlState(0x7eu);              // patched
    EXPECT_EQ(0x7eu, cmd.getSurfaceStateMemoryObjectControlState()); // patched

    cmd.setSurfaceStateBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getSurfaceStateBaseAddress());
    cmd.setSurfaceStateBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getSurfaceStateBaseAddress());

    cmd.setDynamicStateMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getDynamicStateMemoryObjectControlState());  // patched
    cmd.setDynamicStateMemoryObjectControlState(0x7eu);              // patched
    EXPECT_EQ(0x7eu, cmd.getDynamicStateMemoryObjectControlState()); // patched

    cmd.setDynamicStateBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getDynamicStateBaseAddress());
    cmd.setDynamicStateBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getDynamicStateBaseAddress());

    cmd.setInstructionMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getInstructionMemoryObjectControlState());  // patched
    cmd.setInstructionMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getInstructionMemoryObjectControlState()); // patched

    cmd.setInstructionBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getInstructionBaseAddress());
    cmd.setInstructionBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getInstructionBaseAddress());

    cmd.setGeneralStateBufferSize(0x0u);
    EXPECT_EQ(0x0u, cmd.getGeneralStateBufferSize());
    cmd.setGeneralStateBufferSize(0xfffffu);
    EXPECT_EQ(0xfffffu, cmd.getGeneralStateBufferSize());

    cmd.setDynamicStateBufferSize(0x0u);
    EXPECT_EQ(0x0u, cmd.getDynamicStateBufferSize());
    cmd.setDynamicStateBufferSize(0xfffffu);
    EXPECT_EQ(0xfffffu, cmd.getDynamicStateBufferSize());

    cmd.setIndirectObjectBufferSize(0x0u);
    EXPECT_EQ(0x0u, cmd.getIndirectObjectBufferSize());
    cmd.setIndirectObjectBufferSize(0xfffffu);
    EXPECT_EQ(0xfffffu, cmd.getIndirectObjectBufferSize());

    cmd.setInstructionBufferSize(0x0u);
    EXPECT_EQ(0x0u, cmd.getInstructionBufferSize());
    cmd.setInstructionBufferSize(0xfffffu);
    EXPECT_EQ(0xfffffu, cmd.getInstructionBufferSize());

    cmd.setBindlessSurfaceStateMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getBindlessSurfaceStateMemoryObjectControlState());  // patched
    cmd.setBindlessSurfaceStateMemoryObjectControlState(0x7eu);              // patched
    EXPECT_EQ(0x7eu, cmd.getBindlessSurfaceStateMemoryObjectControlState()); // patched

    cmd.setBindlessSurfaceStateBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getBindlessSurfaceStateBaseAddress());
    cmd.setBindlessSurfaceStateBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getBindlessSurfaceStateBaseAddress());

    cmd.setBindlessSurfaceStateSize(0x0u);
    EXPECT_EQ(0x0u, cmd.getBindlessSurfaceStateSize());
    cmd.setBindlessSurfaceStateSize(0xffffffffu);              // patched
    EXPECT_EQ(0xffffffffu, cmd.getBindlessSurfaceStateSize()); // patched
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiReportPerfCountWhenInitCalledThenFieldsSetToDefault) {
    using MI_REPORT_PERF_COUNT = NEO::XeHpcCoreFamily::MI_REPORT_PERF_COUNT;
    MI_REPORT_PERF_COUNT cmd;
    cmd.init();

    EXPECT_EQ(MI_REPORT_PERF_COUNT::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(MI_REPORT_PERF_COUNT::MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_REPORT_PERF_COUNT::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CoreModeEnable);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.MemoryAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ReportId);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiReportPerfCountWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_REPORT_PERF_COUNT = NEO::XeHpcCoreFamily::MI_REPORT_PERF_COUNT;
    MI_REPORT_PERF_COUNT cmd;
    cmd.init();

    cmd.setCoreModeEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getCoreModeEnable());
    cmd.setCoreModeEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getCoreModeEnable());

    cmd.setMemoryAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getMemoryAddress());
    cmd.setMemoryAddress(0xffffffffffffffc0ull);
    EXPECT_EQ(0xffffffffffffffc0ull, cmd.getMemoryAddress());

    cmd.setReportId(0x0u);
    EXPECT_EQ(0x0u, cmd.getReportId());
    cmd.setReportId(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getReportId());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiSetPredicateWhenInitCalledThenFieldsSetToDefault) {
    using MI_SET_PREDICATE = NEO::XeHpcCoreFamily::MI_SET_PREDICATE;
    MI_SET_PREDICATE cmd;
    cmd.init();

    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_DISABLE, cmd.TheStructure.Common.PredicateEnable); // patched
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_NEVER, cmd.TheStructure.Common.PredicateEnableWparid);
    EXPECT_EQ(MI_SET_PREDICATE::MI_COMMAND_OPCODE_MI_SET_PREDICATE, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_SET_PREDICATE::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiSetPredicateWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_SET_PREDICATE = NEO::XeHpcCoreFamily::MI_SET_PREDICATE;
    MI_SET_PREDICATE cmd;
    cmd.init();

    cmd.setPredicateEnable(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_DISABLE);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_DISABLE, cmd.getPredicateEnable());

    cmd.setPredicateEnable(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_ON_CLEAR);              // patched
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_ON_CLEAR, cmd.getPredicateEnable()); // patched

    cmd.setPredicateEnable(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_ON_SET);              // patched
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_ON_SET, cmd.getPredicateEnable()); // patched

    cmd.setPredicateEnable(MI_SET_PREDICATE::PREDICATE_ENABLE_NOOP_ALWAYS);              // patched
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_NOOP_ALWAYS, cmd.getPredicateEnable()); // patched

    cmd.setPredicateEnableWparid(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_NEVER);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_NEVER, cmd.getPredicateEnableWparid());

    cmd.setPredicateEnableWparid(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_ON_ZERO_VALUE);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_ON_ZERO_VALUE, cmd.getPredicateEnableWparid());

    cmd.setPredicateEnableWparid(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE, cmd.getPredicateEnableWparid());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiConditionalBatchBufferEndWhenInitCalledThenFieldsSetToDefault) {
    using MI_CONDITIONAL_BATCH_BUFFER_END = NEO::XeHpcCoreFamily::MI_CONDITIONAL_BATCH_BUFFER_END;
    MI_CONDITIONAL_BATCH_BUFFER_END cmd;
    cmd.init();

    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_GREATER_THAN_IDD, cmd.TheStructure.Common.CompareOperation);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PredicateEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.EndCurrentBatchBufferLevel);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_MASK_MODE_COMPARE_MASK_MODE_DISABLED, cmd.TheStructure.Common.CompareMaskMode);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::MI_COMMAND_OPCODE_MI_CONDITIONAL_BATCH_BUFFER_END, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CompareDataDword);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.CompareAddress);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiConditionalBatchBufferEndWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_CONDITIONAL_BATCH_BUFFER_END = NEO::XeHpcCoreFamily::MI_CONDITIONAL_BATCH_BUFFER_END;
    MI_CONDITIONAL_BATCH_BUFFER_END cmd;
    cmd.init();

    cmd.setCompareOperation(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_GREATER_THAN_IDD);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_GREATER_THAN_IDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_GREATER_THAN_OR_EQUAL_IDD);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_GREATER_THAN_OR_EQUAL_IDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_LESS_THAN_IDD);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_LESS_THAN_IDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_LESS_THAN_OR_EQUAL_IDD);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_LESS_THAN_OR_EQUAL_IDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_EQUAL_IDD);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_EQUAL_IDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_NOT_EQUAL_IDD);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION_MAD_NOT_EQUAL_IDD, cmd.getCompareOperation());

    cmd.setPredicateEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getPredicateEnable());
    cmd.setPredicateEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getPredicateEnable());

    cmd.setEndCurrentBatchBufferLevel(0x0u);
    EXPECT_EQ(0x0u, cmd.getEndCurrentBatchBufferLevel());
    cmd.setEndCurrentBatchBufferLevel(0x1u);
    EXPECT_EQ(0x1u, cmd.getEndCurrentBatchBufferLevel());

    cmd.setCompareMaskMode(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_MASK_MODE_COMPARE_MASK_MODE_DISABLED);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_MASK_MODE_COMPARE_MASK_MODE_DISABLED, cmd.getCompareMaskMode());

    cmd.setCompareMaskMode(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_MASK_MODE_COMPARE_MASK_MODE_ENABLED);
    EXPECT_EQ(MI_CONDITIONAL_BATCH_BUFFER_END::COMPARE_MASK_MODE_COMPARE_MASK_MODE_ENABLED, cmd.getCompareMaskMode());

    cmd.setCompareDataDword(0x0u);
    EXPECT_EQ(0x0u, cmd.getCompareDataDword());
    cmd.setCompareDataDword(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getCompareDataDword());

    cmd.setCompareAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getCompareAddress());
    cmd.setCompareAddress(0xfffffffffffff8ull);              // patched
    EXPECT_EQ(0xfffffffffffff8ull, cmd.getCompareAddress()); // patched
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenPostsyncDataWhenInitCalledThenFieldsSetToDefault) {
    using POSTSYNC_DATA = NEO::XeHpcCoreFamily::POSTSYNC_DATA;
    POSTSYNC_DATA cmd;
    cmd.init();

    EXPECT_EQ(POSTSYNC_DATA::OPERATION_NO_WRITE, cmd.TheStructure.Common.Operation);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MocsIndexToMocsTables);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SystemMemoryFenceRequest);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DataportSubsliceCacheFlush);
    EXPECT_EQ_VAL(0x0ull, cmd.TheStructure.Common.DestinationAddress); // patched
    EXPECT_EQ_VAL(0x0ull, cmd.TheStructure.Common.ImmediateData);      // patched
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenPostsyncDataWhenSetterUsedThenGetterReturnsValidValue) {
    using POSTSYNC_DATA = NEO::XeHpcCoreFamily::POSTSYNC_DATA;
    POSTSYNC_DATA cmd;
    cmd.init();

    cmd.setOperation(POSTSYNC_DATA::OPERATION_NO_WRITE);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION_NO_WRITE, cmd.getOperation());

    cmd.setOperation(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, cmd.getOperation());

    cmd.setOperation(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP);
    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP, cmd.getOperation());

    cmd.setMocs(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getMocs());  // patched
    cmd.setMocs(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getMocs()); // patched

    cmd.setSystemMemoryFenceRequest(0x0u);
    EXPECT_EQ(0x0u, cmd.getSystemMemoryFenceRequest());
    cmd.setSystemMemoryFenceRequest(0x1u);
    EXPECT_EQ(0x1u, cmd.getSystemMemoryFenceRequest());

    cmd.setDataportSubsliceCacheFlush(0x0u);
    EXPECT_EQ(0x0u, cmd.getDataportSubsliceCacheFlush());
    cmd.setDataportSubsliceCacheFlush(0x1u);
    EXPECT_EQ(0x1u, cmd.getDataportSubsliceCacheFlush());

    cmd.setDestinationAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getDestinationAddress());
    cmd.setDestinationAddress(0xffffffffffffffffull);
    EXPECT_EQ(0xffffffffffffffffull, cmd.getDestinationAddress());

    cmd.setImmediateData(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getImmediateData());
    cmd.setImmediateData(0xffffffffffffffffull);
    EXPECT_EQ(0xffffffffffffffffull, cmd.getImmediateData());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenInterfaceDescriptorDataWhenInitCalledThenFieldsSetToDefault) {
    using INTERFACE_DESCRIPTOR_DATA = NEO::XeHpcCoreFamily::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA cmd;
    cmd.init();

    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::FLOATING_POINT_MODE_IEEE_754, cmd.TheStructure.Common.FloatingPointMode);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_MULTIPLE, cmd.TheStructure.Common.SingleProgramFlow);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ, cmd.TheStructure.Common.DenormMode);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE, cmd.TheStructure.Common.ThreadPreemptionDisable);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::BINDING_TABLE_ENTRY_COUNT_PREFETCH_DISABLED, cmd.TheStructure.Common.BindingTableEntryCount);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.NumberOfThreadsInGpgpuThreadGroup);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K, cmd.TheStructure.Common.SharedLocalMemorySize); // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RTNE, cmd.TheStructure.Common.RoundingMode);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, cmd.TheStructure.Common.ThreadGroupDispatchSize);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_NONE, cmd.TheStructure.Common.NumberOfBarriers);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::BTD_MODE_DISABLE, cmd.TheStructure.Common.BtdMode);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_MAX, cmd.TheStructure.Common.PreferredSlmAllocationSize);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenInterfaceDescriptorDataWhenSetterUsedThenGetterReturnsValidValue) { // patched
    using INTERFACE_DESCRIPTOR_DATA = NEO::XeHpcCoreFamily::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA cmd;
    cmd.init();

    cmd.setFloatingPointMode(INTERFACE_DESCRIPTOR_DATA::FLOATING_POINT_MODE_IEEE_754);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::FLOATING_POINT_MODE_IEEE_754, cmd.getFloatingPointMode());

    cmd.setSingleProgramFlow(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_MULTIPLE);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_MULTIPLE, cmd.getSingleProgramFlow());

    cmd.setSingleProgramFlow(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_SINGLE);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_SINGLE, cmd.getSingleProgramFlow());

    cmd.setDenormMode(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ, cmd.getDenormMode());

    cmd.setDenormMode(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL, cmd.getDenormMode());

    cmd.setBindingTableEntryCount(INTERFACE_DESCRIPTOR_DATA::BINDING_TABLE_ENTRY_COUNT_PREFETCH_DISABLED);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::BINDING_TABLE_ENTRY_COUNT_PREFETCH_DISABLED, cmd.getBindingTableEntryCount());

    cmd.setBindingTableEntryCount(INTERFACE_DESCRIPTOR_DATA::BINDING_TABLE_ENTRY_COUNT_PREFETCH_COUNT_MIN);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::BINDING_TABLE_ENTRY_COUNT_PREFETCH_COUNT_MIN, cmd.getBindingTableEntryCount());

    cmd.setBindingTableEntryCount(INTERFACE_DESCRIPTOR_DATA::BINDING_TABLE_ENTRY_COUNT_PREFETCH_COUNT_MAX);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::BINDING_TABLE_ENTRY_COUNT_PREFETCH_COUNT_MAX, cmd.getBindingTableEntryCount());

    cmd.setNumberOfThreadsInGpgpuThreadGroup(0x0u);
    EXPECT_EQ(0x0u, cmd.getNumberOfThreadsInGpgpuThreadGroup());
    cmd.setNumberOfThreadsInGpgpuThreadGroup(0x3ffu);
    EXPECT_EQ(0x3ffu, cmd.getNumberOfThreadsInGpgpuThreadGroup());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K, cmd.getSharedLocalMemorySize()); // patched

    cmd.setRoundingMode(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RTNE);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RTNE, cmd.getRoundingMode());

    cmd.setRoundingMode(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RU);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RU, cmd.getRoundingMode());

    cmd.setRoundingMode(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RD);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RD, cmd.getRoundingMode());

    cmd.setRoundingMode(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RTZ);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RTZ, cmd.getRoundingMode());

    cmd.setThreadGroupDispatchSize(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, cmd.getThreadGroupDispatchSize());

    cmd.setThreadGroupDispatchSize(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4, cmd.getThreadGroupDispatchSize());

    cmd.setThreadGroupDispatchSize(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, cmd.getThreadGroupDispatchSize());

    cmd.setThreadGroupDispatchSize(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, cmd.getThreadGroupDispatchSize());

    cmd.setNumberOfBarriers(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_NONE);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_NONE, cmd.getNumberOfBarriers());

    cmd.setNumberOfBarriers(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B1);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B1, cmd.getNumberOfBarriers());

    cmd.setNumberOfBarriers(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B2);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B2, cmd.getNumberOfBarriers());

    cmd.setNumberOfBarriers(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B4);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B4, cmd.getNumberOfBarriers());

    cmd.setNumberOfBarriers(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B8);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B8, cmd.getNumberOfBarriers());

    cmd.setNumberOfBarriers(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B16);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B16, cmd.getNumberOfBarriers());

    cmd.setNumberOfBarriers(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B24);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B24, cmd.getNumberOfBarriers());

    cmd.setNumberOfBarriers(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B32);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_B32, cmd.getNumberOfBarriers());

    cmd.setBtdMode(INTERFACE_DESCRIPTOR_DATA::BTD_MODE_DISABLE);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::BTD_MODE_DISABLE, cmd.getBtdMode());

    cmd.setBtdMode(INTERFACE_DESCRIPTOR_DATA::BTD_MODE_ENABLE);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::BTD_MODE_ENABLE, cmd.getBtdMode());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_MAX);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_MAX, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_0KB);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_0KB, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_16KB);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_16KB, cmd.getPreferredSlmAllocationSize()); // patched

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_32KB);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_32KB, cmd.getPreferredSlmAllocationSize()); // patched

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_64KB);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_64KB, cmd.getPreferredSlmAllocationSize()); // patched

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_96KB);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_96KB, cmd.getPreferredSlmAllocationSize()); // patched

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_128KB);              // patched
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_128KB, cmd.getPreferredSlmAllocationSize()); // patched
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenComputeWalkerWhenInitCalledThenFieldsSetToDefault) { // patched
    using COMPUTE_WALKER = NEO::XeHpcCoreFamily::COMPUTE_WALKER;
    COMPUTE_WALKER cmd;
    cmd.init();

    EXPECT_EQ(COMPUTE_WALKER::DWORD_LENGTH_FIXED_SIZE, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_STANDARD, cmd.TheStructure.Common.CfeSubopcodeVariant);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_COMPUTE_WALKER, cmd.TheStructure.Common.CfeSubopcode);
    EXPECT_EQ(COMPUTE_WALKER::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND, cmd.TheStructure.Common.ComputeCommandOpcode);
    EXPECT_EQ(COMPUTE_WALKER::PIPELINE_COMPUTE, cmd.TheStructure.Common.Pipeline);
    EXPECT_EQ(COMPUTE_WALKER::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.IndirectDataLength);
    EXPECT_EQ(COMPUTE_WALKER::PARTITION_TYPE_DISABLED, cmd.TheStructure.Common.PartitionType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.IndirectDataStartAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MessageSimd);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TileLayout);  // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WalkOrder);   // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.EmitLocalId); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SimdSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ExecutionMask);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.LocalXMaximum);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.LocalYMaximum);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.LocalZMaximum);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ThreadGroupIdXDimension);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ThreadGroupIdYDimension);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ThreadGroupIdZDimension);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ThreadGroupIdStartingX);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ThreadGroupIdStartingY);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ThreadGroupIdStartingZ);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PartitionId);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PartitionSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PreemptX);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PreemptY);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PreemptZ);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenComputeWalkerWhenSetterUsedThenGetterReturnsValidValue) { // patched
    using COMPUTE_WALKER = NEO::XeHpcCoreFamily::COMPUTE_WALKER;
    COMPUTE_WALKER cmd;
    cmd.init();

    cmd.setCfeSubopcodeVariant(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_STANDARD);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_STANDARD, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcode(COMPUTE_WALKER::CFE_SUBOPCODE_COMPUTE_WALKER);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_COMPUTE_WALKER, cmd.getCfeSubopcode());

    cmd.setComputeCommandOpcode(COMPUTE_WALKER::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND);
    EXPECT_EQ(COMPUTE_WALKER::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND, cmd.getComputeCommandOpcode());

    cmd.setIndirectDataLength(0x0u);
    EXPECT_EQ(0x0u, cmd.getIndirectDataLength());
    cmd.setIndirectDataLength(0x1ffffu);
    EXPECT_EQ(0x1ffffu, cmd.getIndirectDataLength());

    cmd.setPartitionType(COMPUTE_WALKER::PARTITION_TYPE_DISABLED);
    EXPECT_EQ(COMPUTE_WALKER::PARTITION_TYPE_DISABLED, cmd.getPartitionType());

    cmd.setPartitionType(COMPUTE_WALKER::PARTITION_TYPE_X);
    EXPECT_EQ(COMPUTE_WALKER::PARTITION_TYPE_X, cmd.getPartitionType());

    cmd.setPartitionType(COMPUTE_WALKER::PARTITION_TYPE_Y);
    EXPECT_EQ(COMPUTE_WALKER::PARTITION_TYPE_Y, cmd.getPartitionType());

    cmd.setPartitionType(COMPUTE_WALKER::PARTITION_TYPE_Z);
    EXPECT_EQ(COMPUTE_WALKER::PARTITION_TYPE_Z, cmd.getPartitionType());

    cmd.setIndirectDataStartAddress(0x0u);
    EXPECT_EQ(0x0u, cmd.getIndirectDataStartAddress());
    cmd.setIndirectDataStartAddress(0xffffffc0u);
    EXPECT_EQ(0xffffffc0u, cmd.getIndirectDataStartAddress());

    cmd.setMessageSimd(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getMessageSimd()); // patched

    cmd.setMessageSimd(0x3u);              // patched
    EXPECT_EQ(0x3u, cmd.getMessageSimd()); // patched

    cmd.setTileLayout(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getTileLayout()); // patched

    cmd.setTileLayout(0x7u);              // patched
    EXPECT_EQ(0x7u, cmd.getTileLayout()); // patched

    cmd.setWalkOrder(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getWalkOrder()); // patched

    cmd.setWalkOrder(0x7u);              // patched
    EXPECT_EQ(0x7u, cmd.getWalkOrder()); // patched

    cmd.setEmitLocalId(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getEmitLocalId()); // patched

    cmd.setEmitLocalId(0x7u);              // patched
    EXPECT_EQ(0x7u, cmd.getEmitLocalId()); // patched

    cmd.setSimdSize(COMPUTE_WALKER::SIMD_SIZE_SIMT16);
    EXPECT_EQ(COMPUTE_WALKER::SIMD_SIZE_SIMT16, cmd.getSimdSize());

    cmd.setSimdSize(COMPUTE_WALKER::SIMD_SIZE_SIMT32);
    EXPECT_EQ(COMPUTE_WALKER::SIMD_SIZE_SIMT32, cmd.getSimdSize());

    cmd.setExecutionMask(0x0u);
    EXPECT_EQ(0x0u, cmd.getExecutionMask());
    cmd.setExecutionMask(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getExecutionMask());

    cmd.setLocalXMaximum(0x0u);
    EXPECT_EQ(0x0u, cmd.getLocalXMaximum());
    cmd.setLocalXMaximum(0x3ffu);
    EXPECT_EQ(0x3ffu, cmd.getLocalXMaximum());

    cmd.setLocalYMaximum(0x0u);
    EXPECT_EQ(0x0u, cmd.getLocalYMaximum());
    cmd.setLocalYMaximum(0x3ffu);
    EXPECT_EQ(0x3ffu, cmd.getLocalYMaximum());

    cmd.setLocalZMaximum(0x0u);
    EXPECT_EQ(0x0u, cmd.getLocalZMaximum());
    cmd.setLocalZMaximum(0x3ffu);
    EXPECT_EQ(0x3ffu, cmd.getLocalZMaximum());

    cmd.setThreadGroupIdXDimension(0x0u);
    EXPECT_EQ(0x0u, cmd.getThreadGroupIdXDimension());
    cmd.setThreadGroupIdXDimension(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getThreadGroupIdXDimension());

    cmd.setThreadGroupIdYDimension(0x0u);
    EXPECT_EQ(0x0u, cmd.getThreadGroupIdYDimension());
    cmd.setThreadGroupIdYDimension(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getThreadGroupIdYDimension());

    cmd.setThreadGroupIdZDimension(0x0u);
    EXPECT_EQ(0x0u, cmd.getThreadGroupIdZDimension());
    cmd.setThreadGroupIdZDimension(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getThreadGroupIdZDimension());

    cmd.setThreadGroupIdStartingX(0x0u);
    EXPECT_EQ(0x0u, cmd.getThreadGroupIdStartingX());
    cmd.setThreadGroupIdStartingX(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getThreadGroupIdStartingX());

    cmd.setThreadGroupIdStartingY(0x0u);
    EXPECT_EQ(0x0u, cmd.getThreadGroupIdStartingY());
    cmd.setThreadGroupIdStartingY(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getThreadGroupIdStartingY());

    cmd.setThreadGroupIdStartingZ(0x0u);
    EXPECT_EQ(0x0u, cmd.getThreadGroupIdStartingZ());
    cmd.setThreadGroupIdStartingZ(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getThreadGroupIdStartingZ());

    cmd.setPartitionId(0x0u);
    EXPECT_EQ(0x0u, cmd.getPartitionId());
    cmd.setPartitionId(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getPartitionId());

    cmd.setPartitionSize(0x0u);
    EXPECT_EQ(0x0u, cmd.getPartitionSize());
    cmd.setPartitionSize(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getPartitionSize());

    cmd.setPreemptX(0x0u);
    EXPECT_EQ(0x0u, cmd.getPreemptX());
    cmd.setPreemptX(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getPreemptX());

    cmd.setPreemptY(0x0u);
    EXPECT_EQ(0x0u, cmd.getPreemptY());
    cmd.setPreemptY(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getPreemptY());

    cmd.setPreemptZ(0x0u);
    EXPECT_EQ(0x0u, cmd.getPreemptZ());
    cmd.setPreemptZ(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getPreemptZ());

    EXPECT_EQ(reinterpret_cast<uint32_t *>(&cmd.TheStructure.Common.InlineData), cmd.getInlineDataPointer());
    EXPECT_EQ(sizeof(cmd.TheStructure.Common.InlineData), cmd.getInlineDataSize());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenCfeStateWhenInitCalledThenFieldsSetToDefault) {
    using CFE_STATE = NEO::XeHpcCoreFamily::CFE_STATE;
    CFE_STATE cmd;
    cmd.init();

    EXPECT_EQ(CFE_STATE::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(CFE_STATE::CFE_SUBOPCODE_VARIANT_STANDARD, cmd.TheStructure.Common.CfeSubopcodeVariant);
    EXPECT_EQ(CFE_STATE::CFE_SUBOPCODE_CFE_STATE, cmd.TheStructure.Common.CfeSubopcode);
    EXPECT_EQ(CFE_STATE::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND, cmd.TheStructure.Common.ComputeCommandOpcode);
    EXPECT_EQ(CFE_STATE::PIPELINE_COMPUTE, cmd.TheStructure.Common.Pipeline);
    EXPECT_EQ(CFE_STATE::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.NumberOfWalkers);
    EXPECT_EQ(CFE_STATE::OVER_DISPATCH_CONTROL_NORMAL, cmd.TheStructure.Common.OverDispatchControl);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MaximumNumberOfThreads);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ResumeIndicatorDebugkey);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WalkerNumberDebugkey);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenCfeStateWhenSetterUsedThenGetterReturnsValidValue) {
    using CFE_STATE = NEO::XeHpcCoreFamily::CFE_STATE;
    CFE_STATE cmd;
    cmd.init();

    cmd.setCfeSubopcodeVariant(CFE_STATE::CFE_SUBOPCODE_VARIANT_STANDARD);
    EXPECT_EQ(CFE_STATE::CFE_SUBOPCODE_VARIANT_STANDARD, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcode(CFE_STATE::CFE_SUBOPCODE_CFE_STATE);
    EXPECT_EQ(CFE_STATE::CFE_SUBOPCODE_CFE_STATE, cmd.getCfeSubopcode());

    cmd.setComputeCommandOpcode(CFE_STATE::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND);
    EXPECT_EQ(CFE_STATE::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND, cmd.getComputeCommandOpcode());

    cmd.setNumberOfWalkers(0x1u);
    EXPECT_EQ(0x1u, cmd.getNumberOfWalkers());
    cmd.setNumberOfWalkers(0x7u);
    EXPECT_EQ(0x7u, cmd.getNumberOfWalkers());

    cmd.setOverDispatchControl(CFE_STATE::OVER_DISPATCH_CONTROL_NONE);
    EXPECT_EQ(CFE_STATE::OVER_DISPATCH_CONTROL_NONE, cmd.getOverDispatchControl());

    cmd.setOverDispatchControl(CFE_STATE::OVER_DISPATCH_CONTROL_LOW);
    EXPECT_EQ(CFE_STATE::OVER_DISPATCH_CONTROL_LOW, cmd.getOverDispatchControl());

    cmd.setOverDispatchControl(CFE_STATE::OVER_DISPATCH_CONTROL_NORMAL);
    EXPECT_EQ(CFE_STATE::OVER_DISPATCH_CONTROL_NORMAL, cmd.getOverDispatchControl());

    cmd.setOverDispatchControl(CFE_STATE::OVER_DISPATCH_CONTROL_HIGH);
    EXPECT_EQ(CFE_STATE::OVER_DISPATCH_CONTROL_HIGH, cmd.getOverDispatchControl());

    cmd.setMaximumNumberOfThreads(0x0u);
    EXPECT_EQ(0x0u, cmd.getMaximumNumberOfThreads());
    cmd.setMaximumNumberOfThreads(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getMaximumNumberOfThreads());

    cmd.setResumeIndicatorDebugkey(0x0u);
    EXPECT_EQ(0x0u, cmd.getResumeIndicatorDebugkey());
    cmd.setResumeIndicatorDebugkey(0x1u);
    EXPECT_EQ(0x1u, cmd.getResumeIndicatorDebugkey());

    cmd.setWalkerNumberDebugkey(0x0u);
    EXPECT_EQ(0x0u, cmd.getWalkerNumberDebugkey());
    cmd.setWalkerNumberDebugkey(0x3ffu);
    EXPECT_EQ(0x3ffu, cmd.getWalkerNumberDebugkey());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiArbCheckWhenInitCalledThenFieldsSetToDefault) {
    using MI_ARB_CHECK = NEO::XeHpcCoreFamily::MI_ARB_CHECK;
    MI_ARB_CHECK cmd;
    cmd.init();

    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PreParserDisable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MaskBits);
    EXPECT_EQ(MI_ARB_CHECK::MI_COMMAND_OPCODE_MI_ARB_CHECK, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_ARB_CHECK::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiArbCheckWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_ARB_CHECK = NEO::XeHpcCoreFamily::MI_ARB_CHECK;
    MI_ARB_CHECK cmd;
    cmd.init();

    cmd.setPreParserDisable(0x0u);
    EXPECT_EQ(0x0u, cmd.getPreParserDisable());
    cmd.setPreParserDisable(0x1u);
    EXPECT_EQ(0x1u, cmd.getPreParserDisable());

    cmd.setMaskBits(0x0u);
    EXPECT_EQ(0x0u, cmd.getMaskBits());
    cmd.setMaskBits(0xffu);
    EXPECT_EQ(0xffu, cmd.getMaskBits());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiBatchBufferStartWhenInitCalledThenFieldsSetToDefault) {
    using MI_BATCH_BUFFER_START = NEO::XeHpcCoreFamily::MI_BATCH_BUFFER_START;
    MI_BATCH_BUFFER_START cmd;
    cmd.init();

    EXPECT_EQ(MI_BATCH_BUFFER_START::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_GGTT, cmd.TheStructure.Common.AddressSpaceIndicator);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PredicationEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.EnableCommandCache);
    EXPECT_EQ(MI_BATCH_BUFFER_START::MI_COMMAND_OPCODE_MI_BATCH_BUFFER_START, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_BATCH_BUFFER_START::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.BatchBufferStartAddress);
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, cmd.TheStructure.Mi_Mode_Nestedbatchbufferenableis0.SecondLevelBatchBuffer); // patched
    EXPECT_EQ(MI_BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER_CHAIN, cmd.TheStructure.Mi_Mode_Nestedbatchbufferenableis1.NestedLevelBatchBuffer);             // patched
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiBatchBufferStartWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_BATCH_BUFFER_START = NEO::XeHpcCoreFamily::MI_BATCH_BUFFER_START;
    MI_BATCH_BUFFER_START cmd;
    cmd.init();

    cmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_GGTT);
    EXPECT_EQ(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_GGTT, cmd.getAddressSpaceIndicator());

    cmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
    EXPECT_EQ(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT, cmd.getAddressSpaceIndicator());

    cmd.setPredicationEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getPredicationEnable());
    cmd.setPredicationEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getPredicationEnable());

    cmd.setEnableCommandCache(0x0u);
    EXPECT_EQ(0x0u, cmd.getEnableCommandCache());
    cmd.setEnableCommandCache(0x1u);
    EXPECT_EQ(0x1u, cmd.getEnableCommandCache());

    cmd.setBatchBufferStartAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getBatchBufferStartAddress());
    cmd.setBatchBufferStartAddress(0xfffffffffffcull);
    EXPECT_EQ(0xfffffffffffcull, cmd.getBatchBufferStartAddress());

    cmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH);
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, cmd.getSecondLevelBatchBuffer());

    cmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, cmd.getSecondLevelBatchBuffer());

    cmd.setNestedLevelBatchBuffer(MI_BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER_CHAIN);
    EXPECT_EQ(MI_BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER_CHAIN, cmd.getNestedLevelBatchBuffer());

    cmd.setNestedLevelBatchBuffer(MI_BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER_NESTED);
    EXPECT_EQ(MI_BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER_NESTED, cmd.getNestedLevelBatchBuffer());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiLoadRegisterMemWhenInitCalledThenFieldsSetToDefault) {
    using MI_LOAD_REGISTER_MEM = NEO::XeHpcCoreFamily::MI_LOAD_REGISTER_MEM;
    MI_LOAD_REGISTER_MEM cmd;
    cmd.init();

    EXPECT_EQ(MI_LOAD_REGISTER_MEM::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WorkloadPartitionIdOffsetEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MmioRemapEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.AddCsMmioStartOffset);
    EXPECT_EQ(MI_LOAD_REGISTER_MEM::MI_COMMAND_OPCODE_MI_LOAD_REGISTER_MEM, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_LOAD_REGISTER_MEM::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.RegisterAddress);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.MemoryAddress);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiLoadRegisterMemWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_LOAD_REGISTER_MEM = NEO::XeHpcCoreFamily::MI_LOAD_REGISTER_MEM;
    MI_LOAD_REGISTER_MEM cmd;
    cmd.init();

    cmd.setWorkloadPartitionIdOffsetEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getWorkloadPartitionIdOffsetEnable());
    cmd.setWorkloadPartitionIdOffsetEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getWorkloadPartitionIdOffsetEnable());

    cmd.setMmioRemapEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getMmioRemapEnable());
    cmd.setMmioRemapEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getMmioRemapEnable());

    cmd.setAddCsMmioStartOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getAddCsMmioStartOffset());
    cmd.setAddCsMmioStartOffset(0x1u);
    EXPECT_EQ(0x1u, cmd.getAddCsMmioStartOffset());

    cmd.setRegisterAddress(0x0u);
    EXPECT_EQ(0x0u, cmd.getRegisterAddress());
    cmd.setRegisterAddress(0x7ffffcu);
    EXPECT_EQ(0x7ffffcu, cmd.getRegisterAddress());

    cmd.setMemoryAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getMemoryAddress());
    cmd.setMemoryAddress(0xfffffffffffffffcull);
    EXPECT_EQ(0xfffffffffffffffcull, cmd.getMemoryAddress());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiLoadRegisterRegWhenInitCalledThenFieldsSetToDefault) {
    using MI_LOAD_REGISTER_REG = NEO::XeHpcCoreFamily::MI_LOAD_REGISTER_REG;
    MI_LOAD_REGISTER_REG cmd;
    cmd.init();

    EXPECT_EQ(MI_LOAD_REGISTER_REG::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MmioRemapEnableSource);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MmioRemapEnableDestination);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.AddCsMmioStartOffsetSource);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.AddCsMmioStartOffsetDestination);
    EXPECT_EQ(MI_LOAD_REGISTER_REG::MI_COMMAND_OPCODE_MI_LOAD_REGISTER_REG, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_LOAD_REGISTER_REG::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceRegisterAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationRegisterAddress);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiLoadRegisterRegWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_LOAD_REGISTER_REG = NEO::XeHpcCoreFamily::MI_LOAD_REGISTER_REG;
    MI_LOAD_REGISTER_REG cmd;
    cmd.init();

    cmd.setMmioRemapEnableSource(0x0u);
    EXPECT_EQ(0x0u, cmd.getMmioRemapEnableSource());
    cmd.setMmioRemapEnableSource(0x1u);
    EXPECT_EQ(0x1u, cmd.getMmioRemapEnableSource());

    cmd.setMmioRemapEnableDestination(0x0u);
    EXPECT_EQ(0x0u, cmd.getMmioRemapEnableDestination());
    cmd.setMmioRemapEnableDestination(0x1u);
    EXPECT_EQ(0x1u, cmd.getMmioRemapEnableDestination());

    cmd.setAddCsMmioStartOffsetSource(0x0u);
    EXPECT_EQ(0x0u, cmd.getAddCsMmioStartOffsetSource());
    cmd.setAddCsMmioStartOffsetSource(0x1u);
    EXPECT_EQ(0x1u, cmd.getAddCsMmioStartOffsetSource());

    cmd.setAddCsMmioStartOffsetDestination(0x0u);
    EXPECT_EQ(0x0u, cmd.getAddCsMmioStartOffsetDestination());
    cmd.setAddCsMmioStartOffsetDestination(0x1u);
    EXPECT_EQ(0x1u, cmd.getAddCsMmioStartOffsetDestination());

    cmd.setSourceRegisterAddress(0x0u);
    EXPECT_EQ(0x0u, cmd.getSourceRegisterAddress());
    cmd.setSourceRegisterAddress(0x7ffffcu);
    EXPECT_EQ(0x7ffffcu, cmd.getSourceRegisterAddress());

    cmd.setDestinationRegisterAddress(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationRegisterAddress());
    cmd.setDestinationRegisterAddress(0x7ffffcu);
    EXPECT_EQ(0x7ffffcu, cmd.getDestinationRegisterAddress());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiSemaphoreWaitWhenInitCalledThenFieldsSetToDefault) {
    using MI_SEMAPHORE_WAIT = NEO::XeHpcCoreFamily::MI_SEMAPHORE_WAIT;
    MI_SEMAPHORE_WAIT cmd;
    cmd.init();

    EXPECT_EQ(MI_SEMAPHORE_WAIT::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_GREATER_THAN_SDD, cmd.TheStructure.Common.CompareOperation);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_SIGNAL_MODE, cmd.TheStructure.Common.WaitMode);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE_MEMORY_POLL, cmd.TheStructure.Common.RegisterPollMode); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.IndirectSemaphoreDataDword);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WorkloadPartitionIdOffsetEnable);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS, cmd.TheStructure.Common.MemoryType);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SemaphoreDataDword);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.SemaphoreAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WaitTokenNumber);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiSemaphoreWaitWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_SEMAPHORE_WAIT = NEO::XeHpcCoreFamily::MI_SEMAPHORE_WAIT;
    MI_SEMAPHORE_WAIT cmd;
    cmd.init();

    cmd.setCompareOperation(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_GREATER_THAN_SDD);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_GREATER_THAN_SDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_LESS_THAN_SDD);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_LESS_THAN_SDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_LESS_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_LESS_THAN_OR_EQUAL_SDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD, cmd.getCompareOperation());

    cmd.setCompareOperation(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, cmd.getCompareOperation());

    cmd.setWaitMode(MI_SEMAPHORE_WAIT::WAIT_MODE_SIGNAL_MODE);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_SIGNAL_MODE, cmd.getWaitMode());

    cmd.setWaitMode(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE, cmd.getWaitMode());

    cmd.setRegisterPollMode(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE_MEMORY_POLL);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE_MEMORY_POLL, cmd.getRegisterPollMode());

    cmd.setRegisterPollMode(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE_REGISTER_POLL);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE_REGISTER_POLL, cmd.getRegisterPollMode());

    cmd.setIndirectSemaphoreDataDword(0x0u);
    EXPECT_EQ(0x0u, cmd.getIndirectSemaphoreDataDword());
    cmd.setIndirectSemaphoreDataDword(0x1u);
    EXPECT_EQ(0x1u, cmd.getIndirectSemaphoreDataDword());

    cmd.setWorkloadPartitionIdOffsetEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getWorkloadPartitionIdOffsetEnable());
    cmd.setWorkloadPartitionIdOffsetEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getWorkloadPartitionIdOffsetEnable());

    cmd.setMemoryType(MI_SEMAPHORE_WAIT::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS, cmd.getMemoryType());

    cmd.setMemoryType(MI_SEMAPHORE_WAIT::MEMORY_TYPE_GLOBAL_GRAPHICS_ADDRESS);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::MEMORY_TYPE_GLOBAL_GRAPHICS_ADDRESS, cmd.getMemoryType());

    cmd.setSemaphoreDataDword(0x0u);
    EXPECT_EQ(0x0u, cmd.getSemaphoreDataDword());
    cmd.setSemaphoreDataDword(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getSemaphoreDataDword());

    cmd.setSemaphoreGraphicsAddress(0x0ull);                            // patched
    EXPECT_EQ(0x0ull, cmd.getSemaphoreGraphicsAddress());               // patched
    cmd.setSemaphoreGraphicsAddress(0x1fffffffffffffcull);              // patched
    EXPECT_EQ(0x1fffffffffffffcull, cmd.getSemaphoreGraphicsAddress()); // patched

    cmd.setWaitTokenNumber(0x0u);
    EXPECT_EQ(0x0u, cmd.getWaitTokenNumber());
    cmd.setWaitTokenNumber(0xffu);
    EXPECT_EQ(0xffu, cmd.getWaitTokenNumber());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiStoreDataImmWhenInitCalledThenFieldsSetToDefault) {
    using MI_STORE_DATA_IMM = NEO::XeHpcCoreFamily::MI_STORE_DATA_IMM;
    MI_STORE_DATA_IMM cmd;
    cmd.init();

    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_DWORD, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ForceWriteCompletionCheck);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WorkloadPartitionIdOffsetEnable);
    EXPECT_EQ(MI_STORE_DATA_IMM::MI_COMMAND_OPCODE_MI_STORE_DATA_IMM, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_STORE_DATA_IMM::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CoreModeEnable);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.Address);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DataDword0);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DataDword1);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiStoreDataImmWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_STORE_DATA_IMM = NEO::XeHpcCoreFamily::MI_STORE_DATA_IMM;
    MI_STORE_DATA_IMM cmd;
    cmd.init();

    cmd.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_DWORD);
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_DWORD, cmd.getDwordLength());

    cmd.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_QWORD);
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_QWORD, cmd.getDwordLength());

    cmd.setForceWriteCompletionCheck(0x0u);
    EXPECT_EQ(0x0u, cmd.getForceWriteCompletionCheck());
    cmd.setForceWriteCompletionCheck(0x1u);
    EXPECT_EQ(0x1u, cmd.getForceWriteCompletionCheck());

    cmd.setWorkloadPartitionIdOffsetEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getWorkloadPartitionIdOffsetEnable());
    cmd.setWorkloadPartitionIdOffsetEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getWorkloadPartitionIdOffsetEnable());

    cmd.setCoreModeEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getCoreModeEnable());
    cmd.setCoreModeEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getCoreModeEnable());

    cmd.setAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getAddress());
    cmd.setAddress(0x1fffffffffffffcull);              // patched
    EXPECT_EQ(0x1fffffffffffffcull, cmd.getAddress()); // patched

    cmd.setDataDword0(0x0u);
    EXPECT_EQ(0x0u, cmd.getDataDword0());
    cmd.setDataDword0(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getDataDword0());

    cmd.setDataDword1(0x0u);
    EXPECT_EQ(0x0u, cmd.getDataDword1());
    cmd.setDataDword1(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getDataDword1());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiStoreRegisterMemWhenInitCalledThenFieldsSetToDefault) {
    using MI_STORE_REGISTER_MEM = NEO::XeHpcCoreFamily::MI_STORE_REGISTER_MEM;
    MI_STORE_REGISTER_MEM cmd;
    cmd.init();

    EXPECT_EQ(MI_STORE_REGISTER_MEM::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WorkloadPartitionIdOffsetEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MmioRemapEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.AddCsMmioStartOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PredicateEnable);
    EXPECT_EQ(MI_STORE_REGISTER_MEM::MI_COMMAND_OPCODE_MI_STORE_REGISTER_MEM, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_STORE_REGISTER_MEM::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.RegisterAddress);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.MemoryAddress);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMiStoreRegisterMemWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_STORE_REGISTER_MEM = NEO::XeHpcCoreFamily::MI_STORE_REGISTER_MEM;
    MI_STORE_REGISTER_MEM cmd;
    cmd.init();

    cmd.setWorkloadPartitionIdOffsetEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getWorkloadPartitionIdOffsetEnable());
    cmd.setWorkloadPartitionIdOffsetEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getWorkloadPartitionIdOffsetEnable());

    cmd.setMmioRemapEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getMmioRemapEnable());
    cmd.setMmioRemapEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getMmioRemapEnable());

    cmd.setAddCsMmioStartOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getAddCsMmioStartOffset());
    cmd.setAddCsMmioStartOffset(0x1u);
    EXPECT_EQ(0x1u, cmd.getAddCsMmioStartOffset());

    cmd.setPredicateEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getPredicateEnable());
    cmd.setPredicateEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getPredicateEnable());

    cmd.setRegisterAddress(0x0u);
    EXPECT_EQ(0x0u, cmd.getRegisterAddress());
    cmd.setRegisterAddress(0x7ffffcu);
    EXPECT_EQ(0x7ffffcu, cmd.getRegisterAddress());

    cmd.setMemoryAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getMemoryAddress());
    cmd.setMemoryAddress(0xfffffffffffffffcull);
    EXPECT_EQ(0xfffffffffffffffcull, cmd.getMemoryAddress());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenPipelineSelectWhenInitCalledThenFieldsSetToDefault) {
    using PIPELINE_SELECT = NEO::XeHpcCoreFamily::PIPELINE_SELECT;
    PIPELINE_SELECT cmd;
    cmd.init();

    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MaskBits);
    EXPECT_EQ(PIPELINE_SELECT::_3D_COMMAND_SUB_OPCODE_PIPELINE_SELECT, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(PIPELINE_SELECT::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(PIPELINE_SELECT::COMMAND_SUBTYPE_GFXPIPE_SINGLE_DW, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(PIPELINE_SELECT::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenPipelineSelectWhenSetterUsedThenGetterReturnsValidValue) {
    using PIPELINE_SELECT = NEO::XeHpcCoreFamily::PIPELINE_SELECT;
    PIPELINE_SELECT cmd;
    cmd.init();

    cmd.setMaskBits(0x0u);
    EXPECT_EQ(0x0u, cmd.getMaskBits());
    cmd.setMaskBits(0xffu);
    EXPECT_EQ(0xffu, cmd.getMaskBits());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenStateComputeModeWhenInitCalledThenFieldsSetToDefault) {
    using STATE_COMPUTE_MODE = NEO::XeHpcCoreFamily::STATE_COMPUTE_MODE;
    STATE_COMPUTE_MODE cmd;
    cmd.init();

    EXPECT_EQ(STATE_COMPUTE_MODE::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(STATE_COMPUTE_MODE::_3D_COMMAND_SUB_OPCODE_STATE_COMPUTE_MODE, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(STATE_COMPUTE_MODE::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(STATE_COMPUTE_MODE::COMMAND_SUBTYPE_GFXPIPE_COMMON, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(STATE_COMPUTE_MODE::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, cmd.TheStructure.Common.ForceNonCoherent);
    EXPECT_EQ(STATE_COMPUTE_MODE::FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE_ENABLED, cmd.TheStructure.Common.FastClearDisabledOnCompressedSurface);
    EXPECT_EQ(STATE_COMPUTE_MODE::DISABLE_SLM_READ_MERGE_OPTIMIZATION_ENABLED, cmd.TheStructure.Common.DisableSlmReadMergeOptimization);
    EXPECT_EQ(STATE_COMPUTE_MODE::DISABLE_ATOMIC_ON_CLEAR_DATA_ENABLE, cmd.TheStructure.Common.DisableAtomicOnClearData);
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_HW_DEFAULT, cmd.TheStructure.Common.EuThreadSchedulingMode); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.LargeGrfMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Mask);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenStateComputeModeWhenSetterUsedThenGetterReturnsValidValue) {
    using STATE_COMPUTE_MODE = NEO::XeHpcCoreFamily::STATE_COMPUTE_MODE;
    STATE_COMPUTE_MODE cmd;
    cmd.init();

    cmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, cmd.getForceNonCoherent());

    cmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_CPU_NON_COHERENT);
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_CPU_NON_COHERENT, cmd.getForceNonCoherent());

    cmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, cmd.getForceNonCoherent());

    cmd.setFastClearDisabledOnCompressedSurface(STATE_COMPUTE_MODE::FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE_ENABLED);
    EXPECT_EQ(STATE_COMPUTE_MODE::FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE_ENABLED, cmd.getFastClearDisabledOnCompressedSurface());

    cmd.setFastClearDisabledOnCompressedSurface(STATE_COMPUTE_MODE::FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE_DISABLED);
    EXPECT_EQ(STATE_COMPUTE_MODE::FAST_CLEAR_DISABLED_ON_COMPRESSED_SURFACE_DISABLED, cmd.getFastClearDisabledOnCompressedSurface());

    cmd.setDisableSlmReadMergeOptimization(STATE_COMPUTE_MODE::DISABLE_SLM_READ_MERGE_OPTIMIZATION_ENABLED);
    EXPECT_EQ(STATE_COMPUTE_MODE::DISABLE_SLM_READ_MERGE_OPTIMIZATION_ENABLED, cmd.getDisableSlmReadMergeOptimization());

    cmd.setDisableSlmReadMergeOptimization(STATE_COMPUTE_MODE::DISABLE_SLM_READ_MERGE_OPTIMIZATION_DISABLED);
    EXPECT_EQ(STATE_COMPUTE_MODE::DISABLE_SLM_READ_MERGE_OPTIMIZATION_DISABLED, cmd.getDisableSlmReadMergeOptimization());

    cmd.setDisableAtomicOnClearData(STATE_COMPUTE_MODE::DISABLE_ATOMIC_ON_CLEAR_DATA_ENABLE);
    EXPECT_EQ(STATE_COMPUTE_MODE::DISABLE_ATOMIC_ON_CLEAR_DATA_ENABLE, cmd.getDisableAtomicOnClearData());

    cmd.setDisableAtomicOnClearData(STATE_COMPUTE_MODE::DISABLE_ATOMIC_ON_CLEAR_DATA_DISABLE);
    EXPECT_EQ(STATE_COMPUTE_MODE::DISABLE_ATOMIC_ON_CLEAR_DATA_DISABLE, cmd.getDisableAtomicOnClearData());

    cmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_HW_DEFAULT);              // patched
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_HW_DEFAULT, cmd.getEuThreadSchedulingMode()); // patched

    cmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST);              // patched
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST, cmd.getEuThreadSchedulingMode()); // patched

    cmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN);              // patched
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN, cmd.getEuThreadSchedulingMode()); // patched

    cmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);              // patched
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN, cmd.getEuThreadSchedulingMode()); // patched

    cmd.setLargeGrfMode(0x0u);
    EXPECT_EQ(0x0u, cmd.getLargeGrfMode());
    cmd.setLargeGrfMode(0x1u);
    EXPECT_EQ(0x1u, cmd.getLargeGrfMode());

    cmd.setMaskBits(0x0u);                 // patched
    EXPECT_EQ(0x0u, cmd.getMaskBits());    // patched
    cmd.setMaskBits(0xffffu);              // patched
    EXPECT_EQ(0xffffu, cmd.getMaskBits()); // patched
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMemSetWhenInitCalledThenFieldsSetToDefault) { // patched
    using MEM_SET = NEO::XeHpcCoreFamily::MEM_SET;
    MEM_SET cmd;
    cmd.init();

    EXPECT_EQ(MEM_SET::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CompressionFormat40);
    EXPECT_EQ(MEM_SET::DESTINATION_COMPRESSION_ENABLE_DISABLE, cmd.TheStructure.Common.DestinationCompressionEnable);
    EXPECT_EQ(MEM_SET::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE, cmd.TheStructure.Common.DestinationCompressible);
    EXPECT_EQ(MEM_SET::FILL_TYPE_LINEAR_FILL, cmd.TheStructure.Common.FillType);
    EXPECT_EQ(MEM_SET::INSTRUCTION_TARGETOPCODE_MEM_SET, cmd.TheStructure.Common.InstructionTarget_Opcode); // patched
    EXPECT_EQ(MEM_SET::CLIENT_2D_PROCESSOR, cmd.TheStructure.Common.Client);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillWidth);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillHeight);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationPitch);
    EXPECT_EQ_VAL(0x0u, cmd.TheStructure.Common.DestinationStartAddress); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationMOCS);             // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillData);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMemSetWhenSetterUsedThenGetterReturnsValidValue) {
    using MEM_SET = NEO::XeHpcCoreFamily::MEM_SET;
    MEM_SET cmd;
    cmd.init();

    cmd.setCompressionFormat40(0x0u);
    EXPECT_EQ(0x0u, cmd.getCompressionFormat40());
    cmd.setCompressionFormat40(0x1fu);
    EXPECT_EQ(0x1fu, cmd.getCompressionFormat40());

    cmd.setDestinationCompressionEnable(MEM_SET::DESTINATION_COMPRESSION_ENABLE_DISABLE);
    EXPECT_EQ(MEM_SET::DESTINATION_COMPRESSION_ENABLE_DISABLE, cmd.getDestinationCompressionEnable());

    cmd.setDestinationCompressionEnable(MEM_SET::DESTINATION_COMPRESSION_ENABLE_ENABLE);
    EXPECT_EQ(MEM_SET::DESTINATION_COMPRESSION_ENABLE_ENABLE, cmd.getDestinationCompressionEnable());

    cmd.setDestinationCompressible(MEM_SET::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE);
    EXPECT_EQ(MEM_SET::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE, cmd.getDestinationCompressible());

    cmd.setDestinationCompressible(MEM_SET::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
    EXPECT_EQ(MEM_SET::DESTINATION_COMPRESSIBLE_COMPRESSIBLE, cmd.getDestinationCompressible());

    cmd.setFillType(MEM_SET::FILL_TYPE_LINEAR_FILL);
    EXPECT_EQ(MEM_SET::FILL_TYPE_LINEAR_FILL, cmd.getFillType());

    cmd.setFillType(MEM_SET::FILL_TYPE_MATRIX_FILL);
    EXPECT_EQ(MEM_SET::FILL_TYPE_MATRIX_FILL, cmd.getFillType());

    cmd.setInstructionTargetOpcode(0x0u);
    EXPECT_EQ(0x0u, cmd.getInstructionTargetOpcode());
    cmd.setInstructionTargetOpcode(0x7fu);
    EXPECT_EQ(0x7fu, cmd.getInstructionTargetOpcode());

    cmd.setClient(MEM_SET::CLIENT_2D_PROCESSOR);
    EXPECT_EQ(MEM_SET::CLIENT_2D_PROCESSOR, cmd.getClient());

    cmd.setFillWidth(0x1u);
    EXPECT_EQ(0x1u, cmd.getFillWidth());
    cmd.setFillWidth(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getFillWidth());

    cmd.setFillHeight(0x1u);
    EXPECT_EQ(0x1u, cmd.getFillHeight());
    cmd.setFillHeight(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getFillHeight());

    cmd.setDestinationPitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationPitch());
    cmd.setDestinationPitch(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getDestinationPitch());

    cmd.setDestinationStartAddress(0x0u);                     // patched
    EXPECT_EQ(0x0u, cmd.getDestinationStartAddress());        // patched
    cmd.setDestinationStartAddress(0xffffffffu);              // patched
    EXPECT_EQ(0xffffffffu, cmd.getDestinationStartAddress()); // patched

    cmd.setDestinationMOCS(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getDestinationMOCS());  // patched
    cmd.setDestinationMOCS(0x7fu);              // patched
    EXPECT_EQ(0x7fu, cmd.getDestinationMOCS()); // patched

    cmd.setFillData(0x0u);
    EXPECT_EQ(0x0u, cmd.getFillData());
    cmd.setFillData(0xffu);
    EXPECT_EQ(0xffu, cmd.getFillData());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMemCopyWhenInitCalledThenFieldsSetToDefault) {
    using MEM_COPY = NEO::XeHpcCoreFamily::MEM_COPY;
    MEM_COPY cmd;
    cmd.init();

    EXPECT_EQ(MEM_COPY::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CompressionFormat); // patched
    EXPECT_EQ(MEM_COPY::DESTINATION_COMPRESSION_ENABLE_DISABLE, cmd.TheStructure.Common.DestinationCompressionEnable);
    EXPECT_EQ(MEM_COPY::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE, cmd.TheStructure.Common.DestinationCompressible);
    EXPECT_EQ(MEM_COPY::SOURCE_COMPRESSIBLE_NOT_COMPRESSIBLE, cmd.TheStructure.Common.SourceCompressible);
    EXPECT_EQ(MEM_COPY::COPY_TYPE_LINEAR_COPY, cmd.TheStructure.Common.CopyType);
    EXPECT_EQ(MEM_COPY::INSTRUCTIONTARGET_OPCODE_OPCODE, cmd.TheStructure.Common.InstructionTarget_Opcode); // patched
    EXPECT_EQ(MEM_COPY::CLIENT_2D_PROCESSOR, cmd.TheStructure.Common.Client);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TransferWidth);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TransferHeight);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourcePitch);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationPitch);
    EXPECT_EQ_VAL(0x0u, cmd.TheStructure.Common.SourceBaseAddress);      // patched
    EXPECT_EQ_VAL(0x0u, cmd.TheStructure.Common.DestinationBaseAddress); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationMocs);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceMocs);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenMemCopyWhenSetterUsedThenGetterReturnsValidValue) {
    using MEM_COPY = NEO::XeHpcCoreFamily::MEM_COPY;
    MEM_COPY cmd;
    cmd.init();

    cmd.setCompressionFormat(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getCompressionFormat());  // patched
    cmd.setCompressionFormat(0x1fu);              // patched
    EXPECT_EQ(0x1fu, cmd.getCompressionFormat()); // patched

    cmd.setDestinationCompressionEnable(MEM_COPY::DESTINATION_COMPRESSION_ENABLE_DISABLE);
    EXPECT_EQ(MEM_COPY::DESTINATION_COMPRESSION_ENABLE_DISABLE, cmd.getDestinationCompressionEnable());

    cmd.setDestinationCompressionEnable(MEM_COPY::DESTINATION_COMPRESSION_ENABLE_ENABLE);
    EXPECT_EQ(MEM_COPY::DESTINATION_COMPRESSION_ENABLE_ENABLE, cmd.getDestinationCompressionEnable());

    cmd.setDestinationCompressible(MEM_COPY::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE);
    EXPECT_EQ(MEM_COPY::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE, cmd.getDestinationCompressible());

    cmd.setDestinationCompressible(MEM_COPY::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
    EXPECT_EQ(MEM_COPY::DESTINATION_COMPRESSIBLE_COMPRESSIBLE, cmd.getDestinationCompressible());

    cmd.setSourceCompressible(MEM_COPY::SOURCE_COMPRESSIBLE_NOT_COMPRESSIBLE);
    EXPECT_EQ(MEM_COPY::SOURCE_COMPRESSIBLE_NOT_COMPRESSIBLE, cmd.getSourceCompressible());

    cmd.setSourceCompressible(MEM_COPY::SOURCE_COMPRESSIBLE_COMPRESSIBLE);
    EXPECT_EQ(MEM_COPY::SOURCE_COMPRESSIBLE_COMPRESSIBLE, cmd.getSourceCompressible());

    cmd.setCopyType(MEM_COPY::COPY_TYPE_LINEAR_COPY);
    EXPECT_EQ(MEM_COPY::COPY_TYPE_LINEAR_COPY, cmd.getCopyType());

    cmd.setCopyType(MEM_COPY::COPY_TYPE_MATRIX_COPY);
    EXPECT_EQ(MEM_COPY::COPY_TYPE_MATRIX_COPY, cmd.getCopyType());

    cmd.setInstructionTargetOpcode(0x0u);
    EXPECT_EQ(0x0u, cmd.getInstructionTargetOpcode());
    cmd.setInstructionTargetOpcode(0x7fu);
    EXPECT_EQ(0x7fu, cmd.getInstructionTargetOpcode());

    cmd.setClient(MEM_COPY::CLIENT_2D_PROCESSOR);
    EXPECT_EQ(MEM_COPY::CLIENT_2D_PROCESSOR, cmd.getClient());

    cmd.setDestinationX2CoordinateRight(0x1u);                  // patched
    EXPECT_EQ(0x1u, cmd.getDestinationX2CoordinateRight());     // patched
    cmd.setDestinationX2CoordinateRight(0x3ffffu);              // patched
    EXPECT_EQ(0x3ffffu, cmd.getDestinationX2CoordinateRight()); // patched

    cmd.setDestinationY2CoordinateBottom(0x1u);                  // patched
    EXPECT_EQ(0x1u, cmd.getDestinationY2CoordinateBottom());     // patched
    cmd.setDestinationY2CoordinateBottom(0x3ffffu);              // patched
    EXPECT_EQ(0x3ffffu, cmd.getDestinationY2CoordinateBottom()); // patched

    cmd.setSourcePitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getSourcePitch());
    cmd.setSourcePitch(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getSourcePitch());

    cmd.setDestinationPitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationPitch());
    cmd.setDestinationPitch(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getDestinationPitch());

    cmd.setSourceBaseAddress(0x0u);                             // patched
    EXPECT_EQ(0x0u, cmd.getSourceBaseAddress());                // patched
    cmd.setSourceBaseAddress(0xffffffffffffffffu);              // patched
    EXPECT_EQ(0xffffffffffffffffu, cmd.getSourceBaseAddress()); // patched

    cmd.setDestinationBaseAddress(0x0u);                             // patched
    EXPECT_EQ(0x0u, cmd.getDestinationBaseAddress());                // patched
    cmd.setDestinationBaseAddress(0xffffffffffffffffu);              // patched
    EXPECT_EQ(0xffffffffffffffffu, cmd.getDestinationBaseAddress()); // patched

    cmd.setDestinationMOCS(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getDestinationMOCS());  // patched
    cmd.setDestinationMOCS(0x7fu);              // patched
    EXPECT_EQ(0x7fu, cmd.getDestinationMOCS()); // patched

    cmd.setSourceMOCS(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getSourceMOCS());  // patched
    cmd.setSourceMOCS(0x7fu);              // patched
    EXPECT_EQ(0x7fu, cmd.getSourceMOCS()); // patched
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, Given3dstateBindingTablePoolAllocWhenInitCalledThenFieldsSetToDefault) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = NEO::XeHpcCoreFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;
    _3DSTATE_BINDING_TABLE_POOL_ALLOC cmd;
    cmd.init();

    EXPECT_EQ(_3DSTATE_BINDING_TABLE_POOL_ALLOC::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(_3DSTATE_BINDING_TABLE_POOL_ALLOC::_3D_COMMAND_SUB_OPCODE_3DSTATE_BINDING_TABLE_POOL_ALLOC, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(_3DSTATE_BINDING_TABLE_POOL_ALLOC::_3D_COMMAND_OPCODE_3DSTATE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(_3DSTATE_BINDING_TABLE_POOL_ALLOC::COMMAND_SUBTYPE_GFXPIPE_3D, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(_3DSTATE_BINDING_TABLE_POOL_ALLOC::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.BindingTablePoolBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.BindingTablePoolBufferSize); // patched
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, Given3dstateBindingTablePoolAllocWhenSetterUsedThenGetterReturnsValidValue) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = NEO::XeHpcCoreFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;
    _3DSTATE_BINDING_TABLE_POOL_ALLOC cmd;
    cmd.init();

    cmd.setSurfaceObjectControlStateIndexToMocsTables(0x0u);
    EXPECT_EQ(0x0u, cmd.getSurfaceObjectControlStateIndexToMocsTables());
    cmd.setSurfaceObjectControlStateIndexToMocsTables(0x7eu);              // patched
    EXPECT_EQ(0x7eu, cmd.getSurfaceObjectControlStateIndexToMocsTables()); // patched

    cmd.setBindingTablePoolBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getBindingTablePoolBaseAddress());
    cmd.setBindingTablePoolBaseAddress(0xfffffffff000ull);              // patched
    EXPECT_EQ(0xfffffffff000ull, cmd.getBindingTablePoolBaseAddress()); // patched

    cmd.setBindingTablePoolBufferSize(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getBindingTablePoolBufferSize()); // patched

    cmd.setBindingTablePoolBufferSize(0xfffffu);              // patched
    EXPECT_EQ(0xfffffu, cmd.getBindingTablePoolBufferSize()); // patched
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, Given3dstateBtdWhenInitCalledThenFieldsSetToDefault) {
    using _3DSTATE_BTD = NEO::XeHpcCoreFamily::_3DSTATE_BTD;
    _3DSTATE_BTD cmd;
    cmd.init();

    EXPECT_EQ(_3DSTATE_BTD::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(_3DSTATE_BTD::_3D_COMMAND_SUB_OPCODE_3DSTATE_BTD, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(_3DSTATE_BTD::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(_3DSTATE_BTD::COMMAND_SUBTYPE_GFXPIPE_COMMON, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(_3DSTATE_BTD::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DispatchTimeoutCounter);
    EXPECT_EQ(_3DSTATE_BTD::AMFS_MODE_NORMAL_MODE, cmd.TheStructure.Common.AmfsMode);
    EXPECT_EQ(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_128KB, cmd.TheStructure.Common.PerDssMemoryBackedBufferSize);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.MemoryBackedBufferBasePointer);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, Given3dstateBtdWhenSetterUsedThenGetterReturnsValidValue) {
    using _3DSTATE_BTD = NEO::XeHpcCoreFamily::_3DSTATE_BTD;
    _3DSTATE_BTD cmd;
    cmd.init();

    cmd.setDispatchTimeoutCounter(0x0u);
    EXPECT_EQ(0x0u, cmd.getDispatchTimeoutCounter());
    cmd.setDispatchTimeoutCounter(0x3u);
    EXPECT_EQ(0x3u, cmd.getDispatchTimeoutCounter());

    cmd.setAmfsMode(_3DSTATE_BTD::AMFS_MODE_NORMAL_MODE);
    EXPECT_EQ(_3DSTATE_BTD::AMFS_MODE_NORMAL_MODE, cmd.getAmfsMode());

    cmd.setAmfsMode(_3DSTATE_BTD::AMFS_MODE_TOUCH_MODE);
    EXPECT_EQ(_3DSTATE_BTD::AMFS_MODE_TOUCH_MODE, cmd.getAmfsMode());

    cmd.setAmfsMode(_3DSTATE_BTD::AMFS_MODE_BACKFILL_MODE);
    EXPECT_EQ(_3DSTATE_BTD::AMFS_MODE_BACKFILL_MODE, cmd.getAmfsMode());

    cmd.setAmfsMode(_3DSTATE_BTD::AMFS_MODE_FALLBACK_MODE);
    EXPECT_EQ(_3DSTATE_BTD::AMFS_MODE_FALLBACK_MODE, cmd.getAmfsMode());

    cmd.setPerDssMemoryBackedBufferSize(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_2KB);
    EXPECT_EQ(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_2KB, cmd.getPerDssMemoryBackedBufferSize());

    cmd.setPerDssMemoryBackedBufferSize(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_4KB);
    EXPECT_EQ(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_4KB, cmd.getPerDssMemoryBackedBufferSize());

    cmd.setPerDssMemoryBackedBufferSize(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_8KB);
    EXPECT_EQ(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_8KB, cmd.getPerDssMemoryBackedBufferSize());

    cmd.setPerDssMemoryBackedBufferSize(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_16KB);
    EXPECT_EQ(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_16KB, cmd.getPerDssMemoryBackedBufferSize());

    cmd.setPerDssMemoryBackedBufferSize(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_32KB);
    EXPECT_EQ(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_32KB, cmd.getPerDssMemoryBackedBufferSize());

    cmd.setPerDssMemoryBackedBufferSize(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_64KB);
    EXPECT_EQ(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_64KB, cmd.getPerDssMemoryBackedBufferSize());

    cmd.setPerDssMemoryBackedBufferSize(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_128KB);
    EXPECT_EQ(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_128KB, cmd.getPerDssMemoryBackedBufferSize());

    cmd.setMemoryBackedBufferBasePointer(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getMemoryBackedBufferBasePointer());
    cmd.setMemoryBackedBufferBasePointer(0xfffffffffffffc00ull);
    EXPECT_EQ(0xfffffffffffffc00ull, cmd.getMemoryBackedBufferBasePointer());
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenStateSipWhenInitCalledThenFieldsSetToDefault) {
    using STATE_SIP = NEO::XeHpcCoreFamily::STATE_SIP;
    STATE_SIP cmd;
    cmd.init();

    EXPECT_EQ(STATE_SIP::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(STATE_SIP::_3D_COMMAND_SUB_OPCODE_STATE_SIP, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(STATE_SIP::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(STATE_SIP::COMMAND_SUBTYPE_GFXPIPE_COMMON, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(STATE_SIP::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.SystemInstructionPointer);
}

XE_HPC_CORETEST_F(CommandsXeHpcCoreTest, GivenStateSipWhenSetterUsedThenGetterReturnsValidValue) {
    using STATE_SIP = NEO::XeHpcCoreFamily::STATE_SIP;
    STATE_SIP cmd;
    cmd.init();

    cmd.setSystemInstructionPointer(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getSystemInstructionPointer());
    cmd.setSystemInstructionPointer(0xfffffffffffffff0ull);
    EXPECT_EQ(0xfffffffffffffff0ull, cmd.getSystemInstructionPointer());
}
