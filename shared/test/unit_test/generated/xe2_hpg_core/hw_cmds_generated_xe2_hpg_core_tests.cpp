/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using CommandsXe2HpgCoreTest = ::testing::Test;

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMediaSurfaceStateWhenInitCalledThenFieldsSetToDefault) {
    using MEDIA_SURFACE_STATE = NEO::Xe2HpgCoreFamily::MEDIA_SURFACE_STATE;
    MEDIA_SURFACE_STATE cmd;
    cmd.init();

    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8, cmd.TheStructure.Common.CompressionFormat);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ROTATION_NO_ROTATION_OR_0_DEGREE, cmd.TheStructure.Common.Rotation);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Cr_VCb_UPixelOffsetVDirection);
    EXPECT_EQ(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_FRAME_PICTURE, cmd.TheStructure.Common.PictureStructure);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Width);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Height);
    EXPECT_EQ(MEDIA_SURFACE_STATE::TILE_MODE_TILEMODE_LINEAR, cmd.TheStructure.Common.TileMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfacePitch);
    EXPECT_EQ(MEDIA_SURFACE_STATE::ADDRESS_CONTROL_CLAMP, cmd.TheStructure.Common.AddressControl);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Cr_VCb_UPixelOffsetUDirection);
    EXPECT_EQ(MEDIA_SURFACE_STATE::SURFACE_FORMAT_YCRCB_NORMAL, cmd.TheStructure.Common.SurfaceFormat);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.YOffsetForU_Cb);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.XOffsetForU_Cb);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.VerticalLineStrideOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.VerticalLineStride);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceBaseAddressHigh);
    EXPECT_EQ(0x0u, cmd.TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMediaSurfaceStateWhenSetterUsedThenGetterReturnsValidValue) {
    using MEDIA_SURFACE_STATE = NEO::Xe2HpgCoreFamily::MEDIA_SURFACE_STATE;
    MEDIA_SURFACE_STATE cmd;
    cmd.init();

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8_G8);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8_G8, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8_G8_B8_A8);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8_G8_B8_A8, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R10_G10_B10_A2);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R10_G10_B10_A2, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R11_G11_B10);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R11_G11_B10, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16_G16);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16_G16, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16_G16_B16_A16);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16_G16_B16_A16, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32_G32);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32_G32, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32_G32_B32_A32);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32_G32_B32_A32, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_Y16_U16_Y16_V16);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_Y16_U16_Y16_V16, cmd.getCompressionFormat());

    cmd.setCompressionFormat(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_ML8);
    EXPECT_EQ(MEDIA_SURFACE_STATE::COMPRESSION_FORMAT_CMF_ML8, cmd.getCompressionFormat());

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

    cmd.setTileMode(MEDIA_SURFACE_STATE::TILE_MODE_TILES_64K);
    EXPECT_EQ(MEDIA_SURFACE_STATE::TILE_MODE_TILES_64K, cmd.getTileMode());

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
    cmd.setSurfaceMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getSurfaceMemoryObjectControlState()); // patched

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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenPipeControlWhenInitCalledThenFieldsSetToDefault) {
    using PIPE_CONTROL = NEO::Xe2HpgCoreFamily::PIPE_CONTROL;
    PIPE_CONTROL cmd;
    cmd.init();

    EXPECT_EQ(PIPE_CONTROL::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PredicateEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DataportFlush);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.L3ReadOnlyCacheInvalidationEnable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.UnTypedDataPortCacheFlush); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CompressionControlSurface_CcsFlush);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WorkloadPartitionIdOffsetEnable);
    EXPECT_EQ(PIPE_CONTROL::_3D_COMMAND_SUB_OPCODE_PIPE_CONTROL, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(PIPE_CONTROL::_3D_COMMAND_OPCODE_PIPE_CONTROL, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(PIPE_CONTROL::COMMAND_SUBTYPE_GFXPIPE_3D, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(PIPE_CONTROL::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ProtectedMemoryApplicationId);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_NO_WRITE, cmd.TheStructure.Common.PostSyncOperation);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TlbInvalidate);
    EXPECT_EQ(0x1u, cmd.TheStructure.Common.CommandStreamerStallEnable); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.StoreDataIndex);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ProtectedMemoryEnable);
    EXPECT_EQ(PIPE_CONTROL::LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION, cmd.TheStructure.Common.LriPostSyncOperation);
    EXPECT_EQ(PIPE_CONTROL::DESTINATION_ADDRESS_TYPE_PPGTT, cmd.TheStructure.Common.DestinationAddressType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ProtectedMemoryDisable);
    EXPECT_EQ(PIPE_CONTROL::TBIMR_FORCE_BATCH_CLOSURE_NO_BATCH_CLOSURE, cmd.TheStructure.Common.TbimrForceBatchClosure);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Address);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.AddressHigh);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.ImmediateData);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenPipeControlWhenSetterUsedThenGetterReturnsValidValue) {
    using PIPE_CONTROL = NEO::Xe2HpgCoreFamily::PIPE_CONTROL;
    PIPE_CONTROL cmd;
    cmd.init();

    cmd.setPredicateEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getPredicateEnable());
    cmd.setPredicateEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getPredicateEnable());

    cmd.setDataportFlush(0x0u);
    EXPECT_EQ(0x0u, cmd.getDataportFlush());
    cmd.setDataportFlush(0x1u);
    EXPECT_EQ(0x1u, cmd.getDataportFlush());

    cmd.setL3ReadOnlyCacheInvalidationEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getL3ReadOnlyCacheInvalidationEnable());
    cmd.setL3ReadOnlyCacheInvalidationEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getL3ReadOnlyCacheInvalidationEnable());

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

    cmd.setProtectedMemoryApplicationId(0x0u);
    EXPECT_EQ(0x0u, cmd.getProtectedMemoryApplicationId());
    cmd.setProtectedMemoryApplicationId(0x1u);
    EXPECT_EQ(0x1u, cmd.getProtectedMemoryApplicationId());

    cmd.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_NO_WRITE);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_NO_WRITE, cmd.getPostSyncOperation());

    cmd.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, cmd.getPostSyncOperation());

    cmd.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_PS_DEPTH_COUNT);
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_PS_DEPTH_COUNT, cmd.getPostSyncOperation());

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

    cmd.setProtectedMemoryEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getProtectedMemoryEnable());
    cmd.setProtectedMemoryEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getProtectedMemoryEnable());

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

    cmd.setTbimrForceBatchClosure(PIPE_CONTROL::TBIMR_FORCE_BATCH_CLOSURE_NO_BATCH_CLOSURE);
    EXPECT_EQ(PIPE_CONTROL::TBIMR_FORCE_BATCH_CLOSURE_NO_BATCH_CLOSURE, cmd.getTbimrForceBatchClosure());

    cmd.setTbimrForceBatchClosure(PIPE_CONTROL::TBIMR_FORCE_BATCH_CLOSURE_CLOSE_BATCH);
    EXPECT_EQ(PIPE_CONTROL::TBIMR_FORCE_BATCH_CLOSURE_CLOSE_BATCH, cmd.getTbimrForceBatchClosure());

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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiAtomicWhenInitCalledThenFieldsSetToDefault) {
    using MI_ATOMIC = NEO::Xe2HpgCoreFamily::MI_ATOMIC;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiAtomicWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_ATOMIC = NEO::Xe2HpgCoreFamily::MI_ATOMIC;
    MI_ATOMIC cmd;
    cmd.init();

    cmd.setDwordLength(MI_ATOMIC::DWORD_LENGTH_INLINE_DATA_0);
    EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH_INLINE_DATA_0, cmd.getDwordLength());

    cmd.setDwordLength(MI_ATOMIC::DWORD_LENGTH_INLINE_DATA_1);
    EXPECT_EQ(MI_ATOMIC::DWORD_LENGTH_INLINE_DATA_1, cmd.getDwordLength());

    cmd.setAtomicOpcode(MI_ATOMIC::ATOMIC_4B_MOVE);              // patched
    EXPECT_EQ(MI_ATOMIC::ATOMIC_4B_MOVE, cmd.getAtomicOpcode()); // patched

    cmd.setAtomicOpcode(MI_ATOMIC::ATOMIC_4B_INCREMENT);              // patched
    EXPECT_EQ(MI_ATOMIC::ATOMIC_4B_INCREMENT, cmd.getAtomicOpcode()); // patched

    cmd.setAtomicOpcode(MI_ATOMIC::ATOMIC_4B_DECREMENT);              // patched
    EXPECT_EQ(MI_ATOMIC::ATOMIC_4B_DECREMENT, cmd.getAtomicOpcode()); // patched

    cmd.setAtomicOpcode(MI_ATOMIC::ATOMIC_8B_MOVE);              // patched
    EXPECT_EQ(MI_ATOMIC::ATOMIC_8B_MOVE, cmd.getAtomicOpcode()); // patched

    cmd.setAtomicOpcode(MI_ATOMIC::ATOMIC_8B_INCREMENT);              // patched
    EXPECT_EQ(MI_ATOMIC::ATOMIC_8B_INCREMENT, cmd.getAtomicOpcode()); // patched

    cmd.setAtomicOpcode(MI_ATOMIC::ATOMIC_8B_DECREMENT);              // patched
    EXPECT_EQ(MI_ATOMIC::ATOMIC_8B_DECREMENT, cmd.getAtomicOpcode()); // patched

    cmd.setAtomicOpcode(MI_ATOMIC::ATOMIC_8B_ADD);              // patched
    EXPECT_EQ(MI_ATOMIC::ATOMIC_8B_ADD, cmd.getAtomicOpcode()); // patched

    cmd.setAtomicOpcode(MI_ATOMIC::ATOMIC_8B_CMP_WR);              // patched
    EXPECT_EQ(MI_ATOMIC::ATOMIC_8B_CMP_WR, cmd.getAtomicOpcode()); // patched

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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiBatchBufferEndWhenInitCalledThenFieldsSetToDefault) {
    using MI_BATCH_BUFFER_END = NEO::Xe2HpgCoreFamily::MI_BATCH_BUFFER_END;
    MI_BATCH_BUFFER_END cmd;
    cmd.init();

    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PredicateEnable);
    EXPECT_EQ(MI_BATCH_BUFFER_END::MI_COMMAND_OPCODE_MI_BATCH_BUFFER_END, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_BATCH_BUFFER_END::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiBatchBufferEndWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_BATCH_BUFFER_END = NEO::Xe2HpgCoreFamily::MI_BATCH_BUFFER_END;
    MI_BATCH_BUFFER_END cmd;
    cmd.init();

    cmd.setPredicateEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getPredicateEnable());
    cmd.setPredicateEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getPredicateEnable());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiLoadRegisterImmWhenInitCalledThenFieldsSetToDefault) {
    using MI_LOAD_REGISTER_IMM = NEO::Xe2HpgCoreFamily::MI_LOAD_REGISTER_IMM;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiLoadRegisterImmWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_LOAD_REGISTER_IMM = NEO::Xe2HpgCoreFamily::MI_LOAD_REGISTER_IMM;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiNoopWhenInitCalledThenFieldsSetToDefault) {
    using MI_NOOP = NEO::Xe2HpgCoreFamily::MI_NOOP;
    MI_NOOP cmd;
    cmd.init();

    EXPECT_EQ(0x0u, cmd.TheStructure.Common.IdentificationNumber);
    EXPECT_EQ(MI_NOOP::MI_COMMAND_OPCODE_MI_NOOP, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_NOOP::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiNoopWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_NOOP = NEO::Xe2HpgCoreFamily::MI_NOOP;
    MI_NOOP cmd;
    cmd.init();

    cmd.setIdentificationNumber(0x0u);
    EXPECT_EQ(0x0u, cmd.getIdentificationNumber());
    cmd.setIdentificationNumber(0x3fffffu);
    EXPECT_EQ(0x3fffffu, cmd.getIdentificationNumber());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenRenderSurfaceStateWhenInitCalledThenFieldsSetToDefault) {
    using RENDER_SURFACE_STATE = NEO::Xe2HpgCoreFamily::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE cmd;
    cmd.init();

    EXPECT_EQ(RENDER_SURFACE_STATE::MEDIA_BOUNDARY_PIXEL_MODE_NORMAL_MODE, cmd.TheStructure.Common.MediaBoundaryPixelMode);
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_CACHE_READ_WRITE_MODE_WRITE_ONLY_CACHE, cmd.TheStructure.Common.RenderCacheReadWriteMode);
    EXPECT_EQ(true, cmd.TheStructure.Common.EnableSamplerRouteToLsc); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.VerticalLineStrideOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.VerticalLineStride);
    EXPECT_EQ(RENDER_SURFACE_STATE::TILE_MODE_LINEAR, cmd.TheStructure.Common.TileMode);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_16, cmd.TheStructure.Common.SurfaceHorizontalAlignment);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4, cmd.TheStructure.Common.SurfaceVerticalAlignment);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceFormat);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D, cmd.TheStructure.Common.SurfaceType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceQpitch); // patched
    EXPECT_EQ(RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_DISABLE, cmd.TheStructure.Common.SampleTapDiscardDisable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.BaseMipLevel);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MemoryObjectControlStateIndexToMocsTables); // patched
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
    EXPECT_EQ(RENDER_SURFACE_STATE::RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_0DEG, cmd.TheStructure.Common.RenderTargetAndSampleUnormRotation); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MipCountLod);                                                                                      // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceMinLod);                                                                                    // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MipTailStartLod);                                                                                  // patched
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP, cmd.TheStructure.Common.L1CacheControlCachePolicy);                                  // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.YOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.XOffset);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_NONE, cmd.TheStructure.Common.AuxiliarySurfaceMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ResourceMinLod); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ShaderChannelSelectAlpha);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ShaderChannelSelectBlue);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ShaderChannelSelectGreen);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ShaderChannelSelectRed);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.SurfaceBaseAddress);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8, cmd.TheStructure.Common.CompressionFormat);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MipRegionDepthInLog2);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DisallowLowQualityFiltering);          // patched
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsPlanar.YOffsetForUOrUvPlane); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsPlanar.XOffsetForUOrUvPlane); // patched
    EXPECT_EQ(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_DISABLE, cmd.TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsPlanar.YOffsetForVPlane); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsPlanar.XOffsetForVPlane); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfacePitch);
    EXPECT_EQ(0x0u, cmd.TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceQpitch);                                                                        // patched
    EXPECT_EQ(0x0ull, cmd.TheStructure._PropertyauxiliarySurfaceModeIsnotAux_NoneAnd_PropertyauxiliarySurfaceModeIsnotAux_Append.AuxiliarySurfaceBaseAddress); // patched
    EXPECT_EQ(0x0ull, cmd.TheStructure.PropertyauxiliarySurfaceModeIsAux_Append.AppendCounterAddress);                                                         // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.AuxiliarySurfaceModeIsnotAux_Append.MipRegionWidthInLog2);
    EXPECT_EQ(0x0u, cmd.TheStructure.AuxiliarySurfaceModeIsnotAux_Append.MipRegionHeightInLog2);
    EXPECT_EQ(0x0u, cmd.TheStructure.AuxiliarySurfaceModeIsnotAux_Append.ProceduralTexture);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenRenderSurfaceStateWhenSetterUsedThenGetterReturnsValidValue) { // patched
    using RENDER_SURFACE_STATE = NEO::Xe2HpgCoreFamily::RENDER_SURFACE_STATE;
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

    cmd.setEnableSamplerRouteToLsc(0x0u);
    EXPECT_EQ(0x0u, cmd.getEnableSamplerRouteToLsc());
    cmd.setEnableSamplerRouteToLsc(0x1u);
    EXPECT_EQ(0x1u, cmd.getEnableSamplerRouteToLsc());

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

    cmd.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT_R1_UNORM);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_FORMAT_R1_UNORM, cmd.getSurfaceFormat());

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

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_RES5);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_RES5, cmd.getSurfaceType());

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, cmd.getSurfaceType());

    cmd.setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, cmd.getSurfaceType());

    cmd.setSurfaceQPitch(0x0u);
    EXPECT_EQ(0x0u, cmd.getSurfaceQPitch());
    cmd.setSurfaceQPitch(0x1fffcu);
    EXPECT_EQ(0x1fffcu, cmd.getSurfaceQPitch());

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

    cmd.setMIPCountLOD(0x0u);
    EXPECT_EQ(0x0u, cmd.getMIPCountLOD());
    cmd.setMIPCountLOD(0xfu);
    EXPECT_EQ(0xfu, cmd.getMIPCountLOD());

    cmd.setSurfaceMinLOD(0x0u);
    EXPECT_EQ(0x0u, cmd.getSurfaceMinLOD());
    cmd.setSurfaceMinLOD(0xfu);
    EXPECT_EQ(0xfu, cmd.getSurfaceMinLOD());

    cmd.setMipTailStartLOD(0x0u);
    EXPECT_EQ(0x0u, cmd.getMipTailStartLOD());
    cmd.setMipTailStartLOD(0xfu);
    EXPECT_EQ(0xfu, cmd.getMipTailStartLOD());

    cmd.setYOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getYOffset());
    cmd.setYOffset(0x1cu);
    EXPECT_EQ(0x1cu, cmd.getYOffset());

    cmd.setXOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getXOffset());
    cmd.setXOffset(0x1fcu);
    EXPECT_EQ(0x1fcu, cmd.getXOffset());

    cmd.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_NONE, cmd.getAuxiliarySurfaceMode());

    cmd.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_APPEND);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_APPEND, cmd.getAuxiliarySurfaceMode());

    cmd.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_MCS);
    EXPECT_EQ(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE_AUX_MCS, cmd.getAuxiliarySurfaceMode());

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

    cmd.setSurfaceBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getSurfaceBaseAddress());
    cmd.setSurfaceBaseAddress(0xffffffffffffffffull);
    EXPECT_EQ(0xffffffffffffffffull, cmd.getSurfaceBaseAddress());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8_G8);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8_G8, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8_G8_B8_A8);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R8_G8_B8_A8, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R10_G10_B10_A2);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R10_G10_B10_A2, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R11_G11_B10);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R11_G11_B10, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16_G16);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16_G16, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16_G16_B16_A16);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R16_G16_B16_A16, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32_G32);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32_G32, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32_G32_B32_A32);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_R32_G32_B32_A32, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_Y16_U16_Y16_V16);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_Y16_U16_Y16_V16, cmd.getCompressionFormat());

    cmd.setCompressionFormat(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_ML8);
    EXPECT_EQ(RENDER_SURFACE_STATE::COMPRESSION_FORMAT_CMF_ML8, cmd.getCompressionFormat());

    cmd.setMipRegionDepthInLog2(0x0u);
    EXPECT_EQ(0x0u, cmd.getMipRegionDepthInLog2());
    cmd.setMipRegionDepthInLog2(0xfu);
    EXPECT_EQ(0xfu, cmd.getMipRegionDepthInLog2());

    cmd.setDisallowLowQualityFiltering(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getDisallowLowQualityFiltering()); // patched
    cmd.setDisallowLowQualityFiltering(0x1u);              // patched
    EXPECT_EQ(0x1u, cmd.getDisallowLowQualityFiltering()); // patched

    cmd.setYOffsetForUOrUvPlane(0x0u);                 // patched
    EXPECT_EQ(0x0u, cmd.getYOffsetForUOrUvPlane());    // patched
    cmd.setYOffsetForUOrUvPlane(0x3fffu);              // patched
    EXPECT_EQ(0x3fffu, cmd.getYOffsetForUOrUvPlane()); // patched

    cmd.setXOffsetForUOrUvPlane(0x0u);                 // patched
    EXPECT_EQ(0x0u, cmd.getXOffsetForUOrUvPlane());    // patched
    cmd.setXOffsetForUOrUvPlane(0x3fffu);              // patched
    EXPECT_EQ(0x3fffu, cmd.getXOffsetForUOrUvPlane()); // patched

    cmd.setHalfPitchForChroma(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_DISABLE);
    EXPECT_EQ(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_DISABLE, cmd.getHalfPitchForChroma());

    cmd.setHalfPitchForChroma(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_ENABLE);
    EXPECT_EQ(RENDER_SURFACE_STATE::HALF_PITCH_FOR_CHROMA_ENABLE, cmd.getHalfPitchForChroma());

    cmd.setYOffsetForVPlane(0x0u);                 // patched
    EXPECT_EQ(0x0u, cmd.getYOffsetForVPlane());    // patched
    cmd.setYOffsetForVPlane(0x3fffu);              // patched
    EXPECT_EQ(0x3fffu, cmd.getYOffsetForVPlane()); // patched

    cmd.setXOffsetForVPlane(0x0u);                 // patched
    EXPECT_EQ(0x0u, cmd.getXOffsetForVPlane());    // patched
    cmd.setXOffsetForVPlane(0x3fffu);              // patched
    EXPECT_EQ(0x3fffu, cmd.getXOffsetForVPlane()); // patched

    cmd.setAuxiliarySurfacePitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getAuxiliarySurfacePitch());
    cmd.setAuxiliarySurfacePitch(0x3ffu);
    EXPECT_EQ(0x3ffu, cmd.getAuxiliarySurfacePitch());

    cmd.setAuxiliarySurfaceQPitch(0x0u);
    EXPECT_EQ(0x0u, cmd.getAuxiliarySurfaceQPitch());
    cmd.setAuxiliarySurfaceQPitch(0x1fffcu);
    EXPECT_EQ(0x1fffcu, cmd.getAuxiliarySurfaceQPitch());

    cmd.setAuxiliarySurfaceBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getAuxiliarySurfaceBaseAddress());
    cmd.setAuxiliarySurfaceBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getAuxiliarySurfaceBaseAddress());

    cmd.setAppendCounterAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getAppendCounterAddress());
    cmd.setAppendCounterAddress(0xfffffffffffffffcull);
    EXPECT_EQ(0xfffffffffffffffcull, cmd.getAppendCounterAddress());

    cmd.setMipRegionWidthInLog2(0x0u);
    EXPECT_EQ(0x0u, cmd.getMipRegionWidthInLog2());
    cmd.setMipRegionWidthInLog2(0xfu);
    EXPECT_EQ(0xfu, cmd.getMipRegionWidthInLog2());

    cmd.setMipRegionHeightInLog2(0x0u);
    EXPECT_EQ(0x0u, cmd.getMipRegionHeightInLog2());
    cmd.setMipRegionHeightInLog2(0xfu);
    EXPECT_EQ(0xfu, cmd.getMipRegionHeightInLog2());

    cmd.setProceduralTexture(0x0u);
    EXPECT_EQ(0x0u, cmd.getProceduralTexture());
    cmd.setProceduralTexture(0x1u);
    EXPECT_EQ(0x1u, cmd.getProceduralTexture());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenSamplerStateWhenInitCalledThenFieldsSetToDefault) {
    using SAMPLER_STATE = NEO::Xe2HpgCoreFamily::SAMPLER_STATE;
    SAMPLER_STATE cmd;
    cmd.init();

    EXPECT_EQ(SAMPLER_STATE::LOD_ALGORITHM_LEGACY, cmd.TheStructure.Common.LodAlgorithm);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TextureLodBias);
    EXPECT_EQ(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST, cmd.TheStructure.Common.MinModeFilter);
    EXPECT_EQ(SAMPLER_STATE::MAG_MODE_FILTER_NEAREST, cmd.TheStructure.Common.MagModeFilter);
    EXPECT_EQ(SAMPLER_STATE::MIP_MODE_FILTER_NONE, cmd.TheStructure.Common.MipModeFilter);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_CUBE_CORNER_MODE_ENABLE_DISABLE, cmd.TheStructure.Common.LowQualityCubeCornerModeEnable);
    EXPECT_EQ(SAMPLER_STATE::LOD_PRECLAMP_MODE_NONE, cmd.TheStructure.Common.LodPreclampMode);
    EXPECT_EQ(SAMPLER_STATE::CUBE_SURFACE_CONTROL_MODE_PROGRAMMED, cmd.TheStructure.Common.CubeSurfaceControlMode);
    EXPECT_EQ(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_ALWAYS, cmd.TheStructure.Common.ShadowFunction);
    EXPECT_EQ(SAMPLER_STATE::CHROMAKEY_MODE_KEYFILTER_KILL_ON_ANY_MATCH, cmd.TheStructure.Common.ChromakeyMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ChromakeyIndex);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MaxLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MinLod);
    EXPECT_EQ(SAMPLER_STATE::LOD_CLAMP_MAGNIFICATION_MODE_MIPNONE, cmd.TheStructure.Common.LodClampMagnificationMode);
    EXPECT_EQ(SAMPLER_STATE::SRGB_DECODE_DECODE_EXT, cmd.TheStructure.Common.SrgbDecode);
    EXPECT_EQ(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_DISABLE, cmd.TheStructure.Common.ReturnFilterWeightForNullTexels);
    EXPECT_EQ(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_DISABLE, cmd.TheStructure.Common.ReturnFilterWeightForBorderTexels);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ExtendedIndirectStatePointer);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TczAddressControlMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TcyAddressControlMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TcxAddressControlMode);
    EXPECT_EQ(SAMPLER_STATE::MIP_LINEAR_FILTER_QUALITY_FULL_QUALITY, cmd.TheStructure.Common.MipLinearFilterQuality); // patched
    EXPECT_EQ(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_21, cmd.TheStructure.Common.MaximumAnisotropy);
    EXPECT_EQ(SAMPLER_STATE::REDUCTION_TYPE_STD_FILTER, cmd.TheStructure.Common.ReductionType);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenSamplerStateWhenSetterUsedThenGetterReturnsValidValue) {
    using SAMPLER_STATE = NEO::Xe2HpgCoreFamily::SAMPLER_STATE;
    SAMPLER_STATE cmd;
    cmd.init();

    cmd.setLodAlgorithm(SAMPLER_STATE::LOD_ALGORITHM_LEGACY);
    EXPECT_EQ(SAMPLER_STATE::LOD_ALGORITHM_LEGACY, cmd.getLodAlgorithm());

    cmd.setLodAlgorithm(SAMPLER_STATE::LOD_ALGORITHM_EWA_APPROXIMATION);
    EXPECT_EQ(SAMPLER_STATE::LOD_ALGORITHM_EWA_APPROXIMATION, cmd.getLodAlgorithm());

    cmd.setTextureLodBias(0x0u);
    EXPECT_EQ(0x0u, cmd.getTextureLodBias());
    cmd.setTextureLodBias(0x1fffu);
    EXPECT_EQ(0x1fffu, cmd.getTextureLodBias());

    cmd.setMinModeFilter(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST);
    EXPECT_EQ(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST, cmd.getMinModeFilter());

    cmd.setMinModeFilter(SAMPLER_STATE::MIN_MODE_FILTER_LINEAR);
    EXPECT_EQ(SAMPLER_STATE::MIN_MODE_FILTER_LINEAR, cmd.getMinModeFilter());

    cmd.setMinModeFilter(SAMPLER_STATE::MIN_MODE_FILTER_ANISOTROPIC);
    EXPECT_EQ(SAMPLER_STATE::MIN_MODE_FILTER_ANISOTROPIC, cmd.getMinModeFilter());

    cmd.setMagModeFilter(SAMPLER_STATE::MAG_MODE_FILTER_NEAREST);
    EXPECT_EQ(SAMPLER_STATE::MAG_MODE_FILTER_NEAREST, cmd.getMagModeFilter());

    cmd.setMagModeFilter(SAMPLER_STATE::MAG_MODE_FILTER_LINEAR);
    EXPECT_EQ(SAMPLER_STATE::MAG_MODE_FILTER_LINEAR, cmd.getMagModeFilter());

    cmd.setMagModeFilter(SAMPLER_STATE::MAG_MODE_FILTER_ANISOTROPIC);
    EXPECT_EQ(SAMPLER_STATE::MAG_MODE_FILTER_ANISOTROPIC, cmd.getMagModeFilter());

    cmd.setMipModeFilter(SAMPLER_STATE::MIP_MODE_FILTER_NONE);
    EXPECT_EQ(SAMPLER_STATE::MIP_MODE_FILTER_NONE, cmd.getMipModeFilter());

    cmd.setMipModeFilter(SAMPLER_STATE::MIP_MODE_FILTER_NEAREST);
    EXPECT_EQ(SAMPLER_STATE::MIP_MODE_FILTER_NEAREST, cmd.getMipModeFilter());

    cmd.setMipModeFilter(SAMPLER_STATE::MIP_MODE_FILTER_LINEAR);
    EXPECT_EQ(SAMPLER_STATE::MIP_MODE_FILTER_LINEAR, cmd.getMipModeFilter());

    cmd.setLowQualityCubeCornerModeEnable(SAMPLER_STATE::LOW_QUALITY_CUBE_CORNER_MODE_ENABLE_DISABLE);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_CUBE_CORNER_MODE_ENABLE_DISABLE, cmd.getLowQualityCubeCornerModeEnable());

    cmd.setLowQualityCubeCornerModeEnable(SAMPLER_STATE::LOW_QUALITY_CUBE_CORNER_MODE_ENABLE_ENABLE);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_CUBE_CORNER_MODE_ENABLE_ENABLE, cmd.getLowQualityCubeCornerModeEnable());

    cmd.setLodPreclampMode(SAMPLER_STATE::LOD_PRECLAMP_MODE_NONE);
    EXPECT_EQ(SAMPLER_STATE::LOD_PRECLAMP_MODE_NONE, cmd.getLodPreclampMode());

    cmd.setLodPreclampMode(SAMPLER_STATE::LOD_PRECLAMP_MODE_OGL);
    EXPECT_EQ(SAMPLER_STATE::LOD_PRECLAMP_MODE_OGL, cmd.getLodPreclampMode());

    cmd.setLodPreclampMode(SAMPLER_STATE::LOD_PRECLAMP_MODE_D3D);
    EXPECT_EQ(SAMPLER_STATE::LOD_PRECLAMP_MODE_D3D, cmd.getLodPreclampMode());

    cmd.setCubeSurfaceControlMode(SAMPLER_STATE::CUBE_SURFACE_CONTROL_MODE_PROGRAMMED);
    EXPECT_EQ(SAMPLER_STATE::CUBE_SURFACE_CONTROL_MODE_PROGRAMMED, cmd.getCubeSurfaceControlMode());

    cmd.setCubeSurfaceControlMode(SAMPLER_STATE::CUBE_SURFACE_CONTROL_MODE_OVERRIDE);
    EXPECT_EQ(SAMPLER_STATE::CUBE_SURFACE_CONTROL_MODE_OVERRIDE, cmd.getCubeSurfaceControlMode());

    cmd.setShadowFunction(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_ALWAYS);
    EXPECT_EQ(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_ALWAYS, cmd.getShadowFunction());

    cmd.setShadowFunction(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_NEVER);
    EXPECT_EQ(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_NEVER, cmd.getShadowFunction());

    cmd.setShadowFunction(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_LESS);
    EXPECT_EQ(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_LESS, cmd.getShadowFunction());

    cmd.setShadowFunction(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_EQUAL);
    EXPECT_EQ(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_EQUAL, cmd.getShadowFunction());

    cmd.setShadowFunction(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_LEQUAL);
    EXPECT_EQ(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_LEQUAL, cmd.getShadowFunction());

    cmd.setShadowFunction(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_GREATER);
    EXPECT_EQ(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_GREATER, cmd.getShadowFunction());

    cmd.setShadowFunction(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_NOTEQUAL);
    EXPECT_EQ(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_NOTEQUAL, cmd.getShadowFunction());

    cmd.setShadowFunction(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_GEQUAL);
    EXPECT_EQ(SAMPLER_STATE::SHADOW_FUNCTION_PREFILTEROP_GEQUAL, cmd.getShadowFunction());

    cmd.setChromakeyMode(SAMPLER_STATE::CHROMAKEY_MODE_KEYFILTER_KILL_ON_ANY_MATCH);
    EXPECT_EQ(SAMPLER_STATE::CHROMAKEY_MODE_KEYFILTER_KILL_ON_ANY_MATCH, cmd.getChromakeyMode());

    cmd.setChromakeyMode(SAMPLER_STATE::CHROMAKEY_MODE_KEYFILTER_REPLACE_BLACK);
    EXPECT_EQ(SAMPLER_STATE::CHROMAKEY_MODE_KEYFILTER_REPLACE_BLACK, cmd.getChromakeyMode());

    cmd.setChromakeyIndex(0x0u);
    EXPECT_EQ(0x0u, cmd.getChromakeyIndex());
    cmd.setChromakeyIndex(0x3u);
    EXPECT_EQ(0x3u, cmd.getChromakeyIndex());

    cmd.setMaxLod(0x0u);
    EXPECT_EQ(0x0u, cmd.getMaxLod());
    cmd.setMaxLod(0xfffu);
    EXPECT_EQ(0xfffu, cmd.getMaxLod());

    cmd.setMinLod(0x0u);
    EXPECT_EQ(0x0u, cmd.getMinLod());
    cmd.setMinLod(0xfffu);
    EXPECT_EQ(0xfffu, cmd.getMinLod());

    cmd.setLodClampMagnificationMode(SAMPLER_STATE::LOD_CLAMP_MAGNIFICATION_MODE_MIPNONE);
    EXPECT_EQ(SAMPLER_STATE::LOD_CLAMP_MAGNIFICATION_MODE_MIPNONE, cmd.getLodClampMagnificationMode());

    cmd.setLodClampMagnificationMode(SAMPLER_STATE::LOD_CLAMP_MAGNIFICATION_MODE_MIPFILTER);
    EXPECT_EQ(SAMPLER_STATE::LOD_CLAMP_MAGNIFICATION_MODE_MIPFILTER, cmd.getLodClampMagnificationMode());

    cmd.setSrgbDecode(SAMPLER_STATE::SRGB_DECODE_DECODE_EXT);
    EXPECT_EQ(SAMPLER_STATE::SRGB_DECODE_DECODE_EXT, cmd.getSrgbDecode());

    cmd.setSrgbDecode(SAMPLER_STATE::SRGB_DECODE_SKIP_DECODE_EXT);
    EXPECT_EQ(SAMPLER_STATE::SRGB_DECODE_SKIP_DECODE_EXT, cmd.getSrgbDecode());

    cmd.setReturnFilterWeightForNullTexels(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_DISABLE);
    EXPECT_EQ(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_DISABLE, cmd.getReturnFilterWeightForNullTexels());

    cmd.setReturnFilterWeightForNullTexels(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_ENABLE);
    EXPECT_EQ(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_ENABLE, cmd.getReturnFilterWeightForNullTexels());

    cmd.setReturnFilterWeightForBorderTexels(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_DISABLE);
    EXPECT_EQ(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_DISABLE, cmd.getReturnFilterWeightForBorderTexels());

    cmd.setReturnFilterWeightForBorderTexels(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_ENABLE);
    EXPECT_EQ(SAMPLER_STATE::RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_ENABLE, cmd.getReturnFilterWeightForBorderTexels());

    cmd.setExtendedIndirectStatePointer(0x0u);
    EXPECT_EQ(0x0u, cmd.getExtendedIndirectStatePointer());
    cmd.setExtendedIndirectStatePointer(0xff000000u);        // patched
    EXPECT_EQ(0xffu, cmd.getExtendedIndirectStatePointer()); // patched

    cmd.setTczAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP, cmd.getTczAddressControlMode());

    cmd.setTczAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR, cmd.getTczAddressControlMode());

    cmd.setTczAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP, cmd.getTczAddressControlMode());

    cmd.setTczAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CUBE);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CUBE, cmd.getTczAddressControlMode());

    cmd.setTczAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER, cmd.getTczAddressControlMode());

    cmd.setTczAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_ONCE);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_ONCE, cmd.getTczAddressControlMode());

    cmd.setTczAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_HALF_BORDER);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_HALF_BORDER, cmd.getTczAddressControlMode());

    cmd.setTczAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_101);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_101, cmd.getTczAddressControlMode());

    cmd.setTcyAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP, cmd.getTcyAddressControlMode());

    cmd.setTcyAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR, cmd.getTcyAddressControlMode());

    cmd.setTcyAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP, cmd.getTcyAddressControlMode());

    cmd.setTcyAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CUBE);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CUBE, cmd.getTcyAddressControlMode());

    cmd.setTcyAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER, cmd.getTcyAddressControlMode());

    cmd.setTcyAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_ONCE);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_ONCE, cmd.getTcyAddressControlMode());

    cmd.setTcyAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_HALF_BORDER);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_HALF_BORDER, cmd.getTcyAddressControlMode());

    cmd.setTcyAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_101);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_101, cmd.getTcyAddressControlMode());

    cmd.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP, cmd.getTcxAddressControlMode());

    cmd.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR, cmd.getTcxAddressControlMode());

    cmd.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP, cmd.getTcxAddressControlMode());

    cmd.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CUBE);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CUBE, cmd.getTcxAddressControlMode());

    cmd.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER, cmd.getTcxAddressControlMode());

    cmd.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_ONCE);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_ONCE, cmd.getTcxAddressControlMode());

    cmd.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_HALF_BORDER);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_HALF_BORDER, cmd.getTcxAddressControlMode());

    cmd.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_101);
    EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR_101, cmd.getTcxAddressControlMode());

    cmd.setMipLinearFilterQuality(SAMPLER_STATE::MIP_LINEAR_FILTER_QUALITY_FULL_QUALITY);              // patched
    EXPECT_EQ(SAMPLER_STATE::MIP_LINEAR_FILTER_QUALITY_FULL_QUALITY, cmd.getMipLinearFilterQuality()); // patched

    cmd.setMipLinearFilterQuality(SAMPLER_STATE::MIP_LINEAR_FILTER_QUALITY_HIGH_QUALITY);              // patched
    EXPECT_EQ(SAMPLER_STATE::MIP_LINEAR_FILTER_QUALITY_HIGH_QUALITY, cmd.getMipLinearFilterQuality()); // patched

    cmd.setMipLinearFilterQuality(SAMPLER_STATE::MIP_LINEAR_FILTER_QUALITY_MEDIUM_QUALITY);              // patched
    EXPECT_EQ(SAMPLER_STATE::MIP_LINEAR_FILTER_QUALITY_MEDIUM_QUALITY, cmd.getMipLinearFilterQuality()); // patched

    cmd.setMipLinearFilterQuality(SAMPLER_STATE::MIP_LINEAR_FILTER_QUALITY_LOW_QUALITY);              // patched
    EXPECT_EQ(SAMPLER_STATE::MIP_LINEAR_FILTER_QUALITY_LOW_QUALITY, cmd.getMipLinearFilterQuality()); // patched

    cmd.setMaximumAnisotropy(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_21);
    EXPECT_EQ(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_21, cmd.getMaximumAnisotropy());

    cmd.setMaximumAnisotropy(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_41);
    EXPECT_EQ(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_41, cmd.getMaximumAnisotropy());

    cmd.setMaximumAnisotropy(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_61);
    EXPECT_EQ(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_61, cmd.getMaximumAnisotropy());

    cmd.setMaximumAnisotropy(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_81);
    EXPECT_EQ(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_81, cmd.getMaximumAnisotropy());

    cmd.setMaximumAnisotropy(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_101);
    EXPECT_EQ(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_101, cmd.getMaximumAnisotropy());

    cmd.setMaximumAnisotropy(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_121);
    EXPECT_EQ(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_121, cmd.getMaximumAnisotropy());

    cmd.setMaximumAnisotropy(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_141);
    EXPECT_EQ(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_141, cmd.getMaximumAnisotropy());

    cmd.setMaximumAnisotropy(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_161);
    EXPECT_EQ(SAMPLER_STATE::MAXIMUM_ANISOTROPY_RATIO_161, cmd.getMaximumAnisotropy());

    cmd.setReductionType(SAMPLER_STATE::REDUCTION_TYPE_STD_FILTER);
    EXPECT_EQ(SAMPLER_STATE::REDUCTION_TYPE_STD_FILTER, cmd.getReductionType());

    cmd.setReductionType(SAMPLER_STATE::REDUCTION_TYPE_COMPARISON);
    EXPECT_EQ(SAMPLER_STATE::REDUCTION_TYPE_COMPARISON, cmd.getReductionType());

    cmd.setReductionType(SAMPLER_STATE::REDUCTION_TYPE_MINIMUM);
    EXPECT_EQ(SAMPLER_STATE::REDUCTION_TYPE_MINIMUM, cmd.getReductionType());

    cmd.setReductionType(SAMPLER_STATE::REDUCTION_TYPE_MAXIMUM);
    EXPECT_EQ(SAMPLER_STATE::REDUCTION_TYPE_MAXIMUM, cmd.getReductionType());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateBaseAddressWhenInitCalledThenFieldsSetToDefault) {
    using STATE_BASE_ADDRESS = NEO::Xe2HpgCoreFamily::STATE_BASE_ADDRESS;
    STATE_BASE_ADDRESS cmd;
    cmd.init();

    EXPECT_EQ(STATE_BASE_ADDRESS::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(STATE_BASE_ADDRESS::_3D_COMMAND_SUB_OPCODE_STATE_BASE_ADDRESS, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(STATE_BASE_ADDRESS::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(STATE_BASE_ADDRESS::COMMAND_SUBTYPE_GFXPIPE_COMMON, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(STATE_BASE_ADDRESS::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.GeneralStateMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.GeneralStateBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.StatelessDataPortAccessMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, cmd.TheStructure.Common.L1CacheControlCachePolicy); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceStateMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.SurfaceStateBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DynamicStateMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.DynamicStateBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.InstructionMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.InstructionBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.GeneralStateBufferSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DynamicStateBufferSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.InstructionBufferSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.BindlessSurfaceStateBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.BindlessSurfaceStateSize);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.BindlessSamplerStateMemoryObjectControlStateIndexToMocsTables);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.BindlessSamplerStateBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.BindlessSamplerStateBufferSize);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateBaseAddressWhenSetterUsedThenGetterReturnsValidValue) {
    using STATE_BASE_ADDRESS = NEO::Xe2HpgCoreFamily::STATE_BASE_ADDRESS;
    STATE_BASE_ADDRESS cmd;
    cmd.init();

    cmd.setGeneralStateMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getGeneralStateMemoryObjectControlState());  // patched
    cmd.setGeneralStateMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getGeneralStateMemoryObjectControlState()); // patched

    cmd.setGeneralStateBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getGeneralStateBaseAddress());
    cmd.setGeneralStateBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getGeneralStateBaseAddress());

    cmd.setStatelessDataPortAccessMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getStatelessDataPortAccessMemoryObjectControlState());  // patched
    cmd.setStatelessDataPortAccessMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getStatelessDataPortAccessMemoryObjectControlState()); // patched

    cmd.setSurfaceStateMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getSurfaceStateMemoryObjectControlState());  // patched
    cmd.setSurfaceStateMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getSurfaceStateMemoryObjectControlState()); // patched

    cmd.setSurfaceStateBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getSurfaceStateBaseAddress());
    cmd.setSurfaceStateBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getSurfaceStateBaseAddress());

    cmd.setDynamicStateMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getDynamicStateMemoryObjectControlState());  // patched
    cmd.setDynamicStateMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getDynamicStateMemoryObjectControlState()); // patched

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

    cmd.setInstructionBufferSize(0x0u);
    EXPECT_EQ(0x0u, cmd.getInstructionBufferSize());
    cmd.setInstructionBufferSize(0xfffffu);
    EXPECT_EQ(0xfffffu, cmd.getInstructionBufferSize());

    cmd.setBindlessSurfaceStateMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getBindlessSurfaceStateMemoryObjectControlState());  // patched
    cmd.setBindlessSurfaceStateMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getBindlessSurfaceStateMemoryObjectControlState()); // patched

    cmd.setBindlessSurfaceStateBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getBindlessSurfaceStateBaseAddress());
    cmd.setBindlessSurfaceStateBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getBindlessSurfaceStateBaseAddress());

    cmd.setBindlessSurfaceStateSize(0x0u);
    EXPECT_EQ(0x0u, cmd.getBindlessSurfaceStateSize());
    cmd.setBindlessSurfaceStateSize(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getBindlessSurfaceStateSize());

    cmd.setBindlessSamplerStateMemoryObjectControlState(0x0u);               // patched
    EXPECT_EQ(0x0u, cmd.getBindlessSamplerStateMemoryObjectControlState());  // patched
    cmd.setBindlessSamplerStateMemoryObjectControlState(0x3fu);              // patched
    EXPECT_EQ(0x3fu, cmd.getBindlessSamplerStateMemoryObjectControlState()); // patched

    cmd.setBindlessSamplerStateBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getBindlessSamplerStateBaseAddress());
    cmd.setBindlessSamplerStateBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getBindlessSamplerStateBaseAddress());

    cmd.setBindlessSamplerStateBufferSize(0x0u);
    EXPECT_EQ(0x0u, cmd.getBindlessSamplerStateBufferSize());
    cmd.setBindlessSamplerStateBufferSize(0xfffffu);
    EXPECT_EQ(0xfffffu, cmd.getBindlessSamplerStateBufferSize());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiReportPerfCountWhenInitCalledThenFieldsSetToDefault) {
    using MI_REPORT_PERF_COUNT = NEO::Xe2HpgCoreFamily::MI_REPORT_PERF_COUNT;
    MI_REPORT_PERF_COUNT cmd;
    cmd.init();

    EXPECT_EQ(MI_REPORT_PERF_COUNT::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(MI_REPORT_PERF_COUNT::MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_REPORT_PERF_COUNT::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CoreModeEnable);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.MemoryAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ReportId);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiReportPerfCountWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_REPORT_PERF_COUNT = NEO::Xe2HpgCoreFamily::MI_REPORT_PERF_COUNT;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiSetPredicateWhenInitCalledThenFieldsSetToDefault) {
    using MI_SET_PREDICATE = NEO::Xe2HpgCoreFamily::MI_SET_PREDICATE;
    MI_SET_PREDICATE cmd;
    cmd.init();

    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_DISABLE, cmd.TheStructure.Common.PredicateEnable); // patched
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_NEVER, cmd.TheStructure.Common.PredicateEnableWparid);
    EXPECT_EQ(MI_SET_PREDICATE::MI_COMMAND_OPCODE_MI_SET_PREDICATE, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_SET_PREDICATE::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiSetPredicateWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_SET_PREDICATE = NEO::Xe2HpgCoreFamily::MI_SET_PREDICATE;
    MI_SET_PREDICATE cmd;
    cmd.init();

    cmd.setPredicateEnable(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_DISABLE);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_PREDICATE_DISABLE, cmd.getPredicateEnable());

    cmd.setPredicateEnable(MI_SET_PREDICATE::PREDICATE_ENABLE_NOOP_ON_RESULT2_CLEAR);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_NOOP_ON_RESULT2_CLEAR, cmd.getPredicateEnable());

    cmd.setPredicateEnable(MI_SET_PREDICATE::PREDICATE_ENABLE_NOOP_ON_RESULT2_SET);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_NOOP_ON_RESULT2_SET, cmd.getPredicateEnable());

    cmd.setPredicateEnable(MI_SET_PREDICATE::PREDICATE_ENABLE_NOOP_ALWAYS);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_NOOP_ALWAYS, cmd.getPredicateEnable());

    cmd.setPredicateEnableWparid(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_NEVER);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_NEVER, cmd.getPredicateEnableWparid());

    cmd.setPredicateEnableWparid(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_ON_ZERO_VALUE);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_ON_ZERO_VALUE, cmd.getPredicateEnableWparid());

    cmd.setPredicateEnableWparid(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE);
    EXPECT_EQ(MI_SET_PREDICATE::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE, cmd.getPredicateEnableWparid());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiConditionalBatchBufferEndWhenInitCalledThenFieldsSetToDefault) {
    using MI_CONDITIONAL_BATCH_BUFFER_END = NEO::Xe2HpgCoreFamily::MI_CONDITIONAL_BATCH_BUFFER_END;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiConditionalBatchBufferEndWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_CONDITIONAL_BATCH_BUFFER_END = NEO::Xe2HpgCoreFamily::MI_CONDITIONAL_BATCH_BUFFER_END;
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
    cmd.setCompareAddress(0xfffffffffffffff8ull);
    EXPECT_EQ(0xfffffffffffffff8ull, cmd.getCompareAddress());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenPostsyncDataWhenInitCalledThenFieldsSetToDefault) {
    using POSTSYNC_DATA = NEO::Xe2HpgCoreFamily::POSTSYNC_DATA;
    POSTSYNC_DATA cmd;
    cmd.init();

    EXPECT_EQ(POSTSYNC_DATA::OPERATION_NO_WRITE, cmd.TheStructure.Common.Operation);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MocsIndexToMocsTables);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SystemMemoryFenceRequest);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DataportSubsliceCacheFlush);
    EXPECT_EQ_VAL(0x0ull, cmd.TheStructure.Common.DestinationAddress); // patched
    EXPECT_EQ_VAL(0x0ull, cmd.TheStructure.Common.ImmediateData);      // patched
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenPostsyncDataWhenSetterUsedThenGetterReturnsValidValue) {
    using POSTSYNC_DATA = NEO::Xe2HpgCoreFamily::POSTSYNC_DATA;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenInterfaceDescriptorDataWhenInitCalledThenFieldsSetToDefault) {
    using INTERFACE_DESCRIPTOR_DATA = NEO::Xe2HpgCoreFamily::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA cmd;
    cmd.init();

    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::FLOATING_POINT_MODE_IEEE_754, cmd.TheStructure.Common.FloatingPointMode);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_MULTIPLE, cmd.TheStructure.Common.SingleProgramFlow);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ, cmd.TheStructure.Common.DenormMode);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, cmd.TheStructure.Common.SamplerCount);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::BINDING_TABLE_ENTRY_COUNT_PREFETCH_DISABLED, cmd.TheStructure.Common.BindingTableEntryCount);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.NumberOfThreadsInGpgpuThreadGroup);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K, cmd.TheStructure.Common.SharedLocalMemorySize);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::ROUNDING_MODE_RTNE, cmd.TheStructure.Common.RoundingMode);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, cmd.TheStructure.Common.ThreadGroupDispatchSize);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS_NONE, cmd.TheStructure.Common.NumberOfBarriers);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::BTD_MODE_DISABLE, cmd.TheStructure.Common.BtdMode);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K, cmd.TheStructure.Common.PreferredSlmAllocationSize);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenInterfaceDescriptorDataWhenSetterUsedThenGetterReturnsValidValue) {
    using INTERFACE_DESCRIPTOR_DATA = NEO::Xe2HpgCoreFamily::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA cmd;
    cmd.init();

    cmd.setFloatingPointMode(INTERFACE_DESCRIPTOR_DATA::FLOATING_POINT_MODE_IEEE_754);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::FLOATING_POINT_MODE_IEEE_754, cmd.getFloatingPointMode());

    cmd.setFloatingPointMode(INTERFACE_DESCRIPTOR_DATA::FLOATING_POINT_MODE_ALTERNATE);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::FLOATING_POINT_MODE_ALTERNATE, cmd.getFloatingPointMode());

    cmd.setSingleProgramFlow(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_MULTIPLE);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_MULTIPLE, cmd.getSingleProgramFlow());

    cmd.setSingleProgramFlow(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_SINGLE);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SINGLE_PROGRAM_FLOW_SINGLE, cmd.getSingleProgramFlow());

    cmd.setDenormMode(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ, cmd.getDenormMode());

    cmd.setDenormMode(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL, cmd.getDenormMode());

    cmd.setSamplerCount(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_NO_SAMPLERS_USED, cmd.getSamplerCount());

    cmd.setSamplerCount(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_1_AND_4_SAMPLERS_USED);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_1_AND_4_SAMPLERS_USED, cmd.getSamplerCount());

    cmd.setSamplerCount(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_5_AND_8_SAMPLERS_USED);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_5_AND_8_SAMPLERS_USED, cmd.getSamplerCount());

    cmd.setSamplerCount(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_9_AND_12_SAMPLERS_USED);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_9_AND_12_SAMPLERS_USED, cmd.getSamplerCount());

    cmd.setSamplerCount(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_13_AND_16_SAMPLERS_USED);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT_BETWEEN_13_AND_16_SAMPLERS_USED, cmd.getSamplerCount());

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

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_192K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_192K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_256K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_256K, cmd.getSharedLocalMemorySize());

    cmd.setSharedLocalMemorySize(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_384K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_384K, cmd.getSharedLocalMemorySize());

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

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_224K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_224K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_256K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_256K, cmd.getPreferredSlmAllocationSize());

    cmd.setPreferredSlmAllocationSize(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_384K);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_384K, cmd.getPreferredSlmAllocationSize());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenComputeWalkerWhenInitCalledThenFieldsSetToDefault) {
    using COMPUTE_WALKER = NEO::Xe2HpgCoreFamily::COMPUTE_WALKER;
    COMPUTE_WALKER cmd;
    cmd.init();

    EXPECT_EQ(COMPUTE_WALKER::DWORD_LENGTH_FIXED_SIZE, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DispatchComplete);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_STANDARD, cmd.TheStructure.Common.CfeSubopcodeVariant);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_COMPUTE_WALKER, cmd.TheStructure.Common.CfeSubopcode);
    EXPECT_EQ(COMPUTE_WALKER::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND, cmd.TheStructure.Common.ComputeCommandOpcode);
    EXPECT_EQ(COMPUTE_WALKER::PIPELINE_COMPUTE, cmd.TheStructure.Common.Pipeline);
    EXPECT_EQ(COMPUTE_WALKER::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DebugObjectId);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.IndirectDataLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PartitionDispatchParameter);
    EXPECT_EQ(COMPUTE_WALKER::PARTITION_TYPE_DISABLED, cmd.TheStructure.Common.PartitionType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.IndirectDataStartAddress);
    EXPECT_EQ(COMPUTE_WALKER::DISPATCH_WALK_ORDER_LINEAR_WALK, cmd.TheStructure.Common.DispatchWalkOrder);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MessageSimd);
    EXPECT_EQ(COMPUTE_WALKER::TILE_LAYOUT_LINEAR, cmd.TheStructure.Common.TileLayout);
    EXPECT_EQ(COMPUTE_WALKER::WALK_ORDER_WALK_012, cmd.TheStructure.Common.WalkOrder);
    EXPECT_EQ(COMPUTE_WALKER::EMIT_LOCAL_EMIT_NONE, cmd.TheStructure.Common.EmitLocal);
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
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WalkerId);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.OverDispatchTgCount);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenComputeWalkerWhenSetterUsedThenGetterReturnsValidValue) {
    using COMPUTE_WALKER = NEO::Xe2HpgCoreFamily::COMPUTE_WALKER;
    COMPUTE_WALKER cmd;
    cmd.init();

    cmd.setDispatchComplete(0x0u);
    EXPECT_EQ(0x0u, cmd.getDispatchComplete());
    cmd.setDispatchComplete(0x1u);
    EXPECT_EQ(0x1u, cmd.getDispatchComplete());

    cmd.setCfeSubopcodeVariant(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_STANDARD);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_STANDARD, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcodeVariant(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_PASS1_RESUME);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_PASS1_RESUME, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcodeVariant(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_PASS2_RESUME);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_PASS2_RESUME, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcodeVariant(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_BTD_PASS2);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_BTD_PASS2, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcodeVariant(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_PASS1_PASS2_RESUME);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_PASS1_PASS2_RESUME, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcodeVariant(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_TG_RESUME);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_TG_RESUME, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcodeVariant(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_W_DONE);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_VARIANT_W_DONE, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcode(COMPUTE_WALKER::CFE_SUBOPCODE_COMPUTE_WALKER);
    EXPECT_EQ(COMPUTE_WALKER::CFE_SUBOPCODE_COMPUTE_WALKER, cmd.getCfeSubopcode());

    cmd.setComputeCommandOpcode(COMPUTE_WALKER::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND);
    EXPECT_EQ(COMPUTE_WALKER::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND, cmd.getComputeCommandOpcode());

    cmd.setDebugObjectId(0x0u);
    EXPECT_EQ(0x0u, cmd.getDebugObjectId());
    cmd.setDebugObjectId(0xffffffu);
    EXPECT_EQ(0xffffffu, cmd.getDebugObjectId());

    cmd.setIndirectDataLength(0x0u);
    EXPECT_EQ(0x0u, cmd.getIndirectDataLength());
    cmd.setIndirectDataLength(0x1ffffu);
    EXPECT_EQ(0x1ffffu, cmd.getIndirectDataLength());

    cmd.setPartitionDispatchParameter(0x0u);
    EXPECT_EQ(0x0u, cmd.getPartitionDispatchParameter());
    cmd.setPartitionDispatchParameter(0xfffu);
    EXPECT_EQ(0xfffu, cmd.getPartitionDispatchParameter());

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

    cmd.setDispatchWalkOrder(COMPUTE_WALKER::DISPATCH_WALK_ORDER_LINEAR_WALK);
    EXPECT_EQ(COMPUTE_WALKER::DISPATCH_WALK_ORDER_LINEAR_WALK, cmd.getDispatchWalkOrder());

    cmd.setDispatchWalkOrder(COMPUTE_WALKER::DISPATCH_WALK_ORDER_Y_ORDER_WALK);
    EXPECT_EQ(COMPUTE_WALKER::DISPATCH_WALK_ORDER_Y_ORDER_WALK, cmd.getDispatchWalkOrder());

    cmd.setMessageSimd(COMPUTE_WALKER::MESSAGE_SIMD_SIMT16);
    EXPECT_EQ(COMPUTE_WALKER::MESSAGE_SIMD_SIMT16, cmd.getMessageSimd());

    cmd.setMessageSimd(COMPUTE_WALKER::MESSAGE_SIMD_SIMT32);
    EXPECT_EQ(COMPUTE_WALKER::MESSAGE_SIMD_SIMT32, cmd.getMessageSimd());

    cmd.setTileLayout(COMPUTE_WALKER::TILE_LAYOUT_LINEAR);
    EXPECT_EQ(COMPUTE_WALKER::TILE_LAYOUT_LINEAR, cmd.getTileLayout());

    cmd.setTileLayout(COMPUTE_WALKER::TILE_LAYOUT_TILEY_32BPE);
    EXPECT_EQ(COMPUTE_WALKER::TILE_LAYOUT_TILEY_32BPE, cmd.getTileLayout());

    cmd.setTileLayout(COMPUTE_WALKER::TILE_LAYOUT_TILEY_64BPE);
    EXPECT_EQ(COMPUTE_WALKER::TILE_LAYOUT_TILEY_64BPE, cmd.getTileLayout());

    cmd.setTileLayout(COMPUTE_WALKER::TILE_LAYOUT_TILEY_128BPE);
    EXPECT_EQ(COMPUTE_WALKER::TILE_LAYOUT_TILEY_128BPE, cmd.getTileLayout());

    cmd.setWalkOrder(COMPUTE_WALKER::WALK_ORDER_WALK_012);
    EXPECT_EQ(COMPUTE_WALKER::WALK_ORDER_WALK_012, cmd.getWalkOrder());

    cmd.setWalkOrder(COMPUTE_WALKER::WALK_ORDER_WALK_021);
    EXPECT_EQ(COMPUTE_WALKER::WALK_ORDER_WALK_021, cmd.getWalkOrder());

    cmd.setWalkOrder(COMPUTE_WALKER::WALK_ORDER_WALK_102);
    EXPECT_EQ(COMPUTE_WALKER::WALK_ORDER_WALK_102, cmd.getWalkOrder());

    cmd.setWalkOrder(COMPUTE_WALKER::WALK_ORDER_WALK_120);
    EXPECT_EQ(COMPUTE_WALKER::WALK_ORDER_WALK_120, cmd.getWalkOrder());

    cmd.setWalkOrder(COMPUTE_WALKER::WALK_ORDER_WALK_201);
    EXPECT_EQ(COMPUTE_WALKER::WALK_ORDER_WALK_201, cmd.getWalkOrder());

    cmd.setWalkOrder(COMPUTE_WALKER::WALK_ORDER_WALK_210);
    EXPECT_EQ(COMPUTE_WALKER::WALK_ORDER_WALK_210, cmd.getWalkOrder());

    cmd.setEmitLocalId(COMPUTE_WALKER::EMIT_LOCAL_EMIT_NONE);              // patched
    EXPECT_EQ(COMPUTE_WALKER::EMIT_LOCAL_EMIT_NONE, cmd.getEmitLocalId()); // patched

    cmd.setEmitLocalId(COMPUTE_WALKER::EMIT_LOCAL_EMIT_X);              // patched
    EXPECT_EQ(COMPUTE_WALKER::EMIT_LOCAL_EMIT_X, cmd.getEmitLocalId()); // patched

    cmd.setEmitLocalId(COMPUTE_WALKER::EMIT_LOCAL_EMIT_XY);              // patched
    EXPECT_EQ(COMPUTE_WALKER::EMIT_LOCAL_EMIT_XY, cmd.getEmitLocalId()); // patched

    cmd.setEmitLocalId(COMPUTE_WALKER::EMIT_LOCAL_EMIT_XYZ);              // patched
    EXPECT_EQ(COMPUTE_WALKER::EMIT_LOCAL_EMIT_XYZ, cmd.getEmitLocalId()); // patched

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

    cmd.setWalkerId(0x0u);
    EXPECT_EQ(0x0u, cmd.getWalkerId());
    cmd.setWalkerId(0xfu);
    EXPECT_EQ(0xfu, cmd.getWalkerId());

    cmd.setOverDispatchTgCount(0x0u);
    EXPECT_EQ(0x0u, cmd.getOverDispatchTgCount());
    cmd.setOverDispatchTgCount(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getOverDispatchTgCount());

    EXPECT_EQ(reinterpret_cast<uint32_t *>(&cmd.TheStructure.Common.InlineData), cmd.getInlineDataPointer());
    EXPECT_EQ(sizeof(cmd.TheStructure.Common.InlineData), cmd.getInlineDataSize());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenCfeStateWhenInitCalledThenFieldsSetToDefault) {
    using CFE_STATE = NEO::Xe2HpgCoreFamily::CFE_STATE;
    CFE_STATE cmd;
    cmd.init();

    EXPECT_EQ(CFE_STATE::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(CFE_STATE::CFE_SUBOPCODE_VARIANT_STANDARD, cmd.TheStructure.Common.CfeSubopcodeVariant);
    EXPECT_EQ(CFE_STATE::CFE_SUBOPCODE_CFE_STATE, cmd.TheStructure.Common.CfeSubopcode);
    EXPECT_EQ(CFE_STATE::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND, cmd.TheStructure.Common.ComputeCommandOpcode);
    EXPECT_EQ(CFE_STATE::PIPELINE_COMPUTE, cmd.TheStructure.Common.Pipeline);
    EXPECT_EQ(CFE_STATE::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(CFE_STATE::STACK_ID_CONTROL_2K, cmd.TheStructure.Common.StackIdControl);
    EXPECT_EQ(CFE_STATE::OVER_DISPATCH_CONTROL_NORMAL, cmd.TheStructure.Common.OverDispatchControl);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MaximumNumberOfThreads);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ResumeIndicatorDebugkey);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WalkerNumberDebugkey);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenCfeStateWhenSetterUsedThenGetterReturnsValidValue) {
    using CFE_STATE = NEO::Xe2HpgCoreFamily::CFE_STATE;
    CFE_STATE cmd;
    cmd.init();

    cmd.setCfeSubopcodeVariant(CFE_STATE::CFE_SUBOPCODE_VARIANT_STANDARD);
    EXPECT_EQ(CFE_STATE::CFE_SUBOPCODE_VARIANT_STANDARD, cmd.getCfeSubopcodeVariant());

    cmd.setCfeSubopcode(CFE_STATE::CFE_SUBOPCODE_CFE_STATE);
    EXPECT_EQ(CFE_STATE::CFE_SUBOPCODE_CFE_STATE, cmd.getCfeSubopcode());

    cmd.setComputeCommandOpcode(CFE_STATE::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND);
    EXPECT_EQ(CFE_STATE::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND, cmd.getComputeCommandOpcode());

    cmd.setStackIdControl(CFE_STATE::STACK_ID_CONTROL_2K);
    EXPECT_EQ(CFE_STATE::STACK_ID_CONTROL_2K, cmd.getStackIdControl());

    cmd.setStackIdControl(CFE_STATE::STACK_ID_CONTROL_1K);
    EXPECT_EQ(CFE_STATE::STACK_ID_CONTROL_1K, cmd.getStackIdControl());

    cmd.setStackIdControl(CFE_STATE::STACK_ID_CONTROL_512);
    EXPECT_EQ(CFE_STATE::STACK_ID_CONTROL_512, cmd.getStackIdControl());

    cmd.setStackIdControl(CFE_STATE::STACK_ID_CONTROL_256);
    EXPECT_EQ(CFE_STATE::STACK_ID_CONTROL_256, cmd.getStackIdControl());

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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiArbCheckWhenInitCalledThenFieldsSetToDefault) {
    using MI_ARB_CHECK = NEO::Xe2HpgCoreFamily::MI_ARB_CHECK;
    MI_ARB_CHECK cmd;
    cmd.init();

    EXPECT_EQ(0x0u, cmd.TheStructure.Common.PreParserDisable);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MaskBits);
    EXPECT_EQ(MI_ARB_CHECK::MI_COMMAND_OPCODE_MI_ARB_CHECK, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_ARB_CHECK::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiArbCheckWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_ARB_CHECK = NEO::Xe2HpgCoreFamily::MI_ARB_CHECK;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiBatchBufferStartWhenInitCalledThenFieldsSetToDefault) {
    using MI_BATCH_BUFFER_START = NEO::Xe2HpgCoreFamily::MI_BATCH_BUFFER_START;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiBatchBufferStartWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_BATCH_BUFFER_START = NEO::Xe2HpgCoreFamily::MI_BATCH_BUFFER_START;
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
    cmd.setBatchBufferStartAddress(0xfffffffffffcull);              // patched
    EXPECT_EQ(0xfffffffffffcull, cmd.getBatchBufferStartAddress()); // patched

    cmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH);
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, cmd.getSecondLevelBatchBuffer());

    cmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, cmd.getSecondLevelBatchBuffer());

    cmd.setNestedLevelBatchBuffer(MI_BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER_CHAIN);
    EXPECT_EQ(MI_BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER_CHAIN, cmd.getNestedLevelBatchBuffer());

    cmd.setNestedLevelBatchBuffer(MI_BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER_NESTED);
    EXPECT_EQ(MI_BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER_NESTED, cmd.getNestedLevelBatchBuffer());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiLoadRegisterMemWhenInitCalledThenFieldsSetToDefault) {
    using MI_LOAD_REGISTER_MEM = NEO::Xe2HpgCoreFamily::MI_LOAD_REGISTER_MEM;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiLoadRegisterMemWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_LOAD_REGISTER_MEM = NEO::Xe2HpgCoreFamily::MI_LOAD_REGISTER_MEM;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiLoadRegisterRegWhenInitCalledThenFieldsSetToDefault) {
    using MI_LOAD_REGISTER_REG = NEO::Xe2HpgCoreFamily::MI_LOAD_REGISTER_REG;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiLoadRegisterRegWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_LOAD_REGISTER_REG = NEO::Xe2HpgCoreFamily::MI_LOAD_REGISTER_REG;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiSemaphoreWaitWhenInitCalledThenFieldsSetToDefault) {
    using MI_SEMAPHORE_WAIT = NEO::Xe2HpgCoreFamily::MI_SEMAPHORE_WAIT;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiSemaphoreWaitWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_SEMAPHORE_WAIT = NEO::Xe2HpgCoreFamily::MI_SEMAPHORE_WAIT;
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

    cmd.setSemaphoreGraphicsAddress(0x0ull);                             // patched
    EXPECT_EQ(0x0ull, cmd.getSemaphoreGraphicsAddress());                // patched
    cmd.setSemaphoreGraphicsAddress(0xfffffffffffffffcull);              // patched
    EXPECT_EQ(0xfffffffffffffffcull, cmd.getSemaphoreGraphicsAddress()); // patched

    cmd.setWaitTokenNumber(0x0u);
    EXPECT_EQ(0x0u, cmd.getWaitTokenNumber());
    cmd.setWaitTokenNumber(0xffu);
    EXPECT_EQ(0xffu, cmd.getWaitTokenNumber());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiStoreDataImmWhenInitCalledThenFieldsSetToDefault) {
    using MI_STORE_DATA_IMM = NEO::Xe2HpgCoreFamily::MI_STORE_DATA_IMM;
    MI_STORE_DATA_IMM cmd;
    cmd.init();

    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_DWORD, cmd.TheStructure.Common.DwordLength); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ForceWriteCompletionCheck);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.WorkloadPartitionIdOffsetEnable);
    EXPECT_EQ(MI_STORE_DATA_IMM::MI_COMMAND_OPCODE_MI_STORE_DATA_IMM, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_STORE_DATA_IMM::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.CoreModeEnable);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.Address);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DataDword0);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DataDword1);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiStoreDataImmWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_STORE_DATA_IMM = NEO::Xe2HpgCoreFamily::MI_STORE_DATA_IMM;
    MI_STORE_DATA_IMM cmd;
    cmd.init();

    cmd.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_DWORD);              // patched
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_DWORD, cmd.getDwordLength()); // patched
    cmd.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_QWORD);              // patched
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_QWORD, cmd.getDwordLength()); // patched

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
    cmd.setAddress(0xfffffffffffffffcull);
    EXPECT_EQ(0xfffffffffffffffcull, cmd.getAddress());

    cmd.setDataDword0(0x0u);
    EXPECT_EQ(0x0u, cmd.getDataDword0());
    cmd.setDataDword0(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getDataDword0());

    cmd.setDataDword1(0x0u);
    EXPECT_EQ(0x0u, cmd.getDataDword1());
    cmd.setDataDword1(0xffffffffu);
    EXPECT_EQ(0xffffffffu, cmd.getDataDword1());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiStoreRegisterMemWhenInitCalledThenFieldsSetToDefault) {
    using MI_STORE_REGISTER_MEM = NEO::Xe2HpgCoreFamily::MI_STORE_REGISTER_MEM;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiStoreRegisterMemWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_STORE_REGISTER_MEM = NEO::Xe2HpgCoreFamily::MI_STORE_REGISTER_MEM;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenPipelineSelectWhenInitCalledThenFieldsSetToDefault) {
    using PIPELINE_SELECT = NEO::Xe2HpgCoreFamily::PIPELINE_SELECT;
    PIPELINE_SELECT cmd;
    cmd.init();

    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_3D, cmd.TheStructure.Common.PipelineSelection);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.MaskBits);
    EXPECT_EQ(PIPELINE_SELECT::_3D_COMMAND_SUB_OPCODE_PIPELINE_SELECT, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(PIPELINE_SELECT::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(PIPELINE_SELECT::COMMAND_SUBTYPE_GFXPIPE_SINGLE_DW, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(PIPELINE_SELECT::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenPipelineSelectWhenSetterUsedThenGetterReturnsValidValue) {
    using PIPELINE_SELECT = NEO::Xe2HpgCoreFamily::PIPELINE_SELECT;
    PIPELINE_SELECT cmd;
    cmd.init();

    cmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_3D);
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_3D, cmd.getPipelineSelection());

    cmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU, cmd.getPipelineSelection());

    cmd.setMaskBits(0x0u);
    EXPECT_EQ(0x0u, cmd.getMaskBits());
    cmd.setMaskBits(0xffu);
    EXPECT_EQ(0xffu, cmd.getMaskBits());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateComputeModeWhenInitCalledThenFieldsSetToDefault) {
    using STATE_COMPUTE_MODE = NEO::Xe2HpgCoreFamily::STATE_COMPUTE_MODE;
    STATE_COMPUTE_MODE cmd;
    cmd.init();

    EXPECT_EQ(STATE_COMPUTE_MODE::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(STATE_COMPUTE_MODE::_3D_COMMAND_SUB_OPCODE_STATE_COMPUTE_MODE, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(STATE_COMPUTE_MODE::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(STATE_COMPUTE_MODE::COMMAND_SUBTYPE_GFXPIPE_COMMON, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(STATE_COMPUTE_MODE::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, cmd.TheStructure.Common.ZPassAsyncComputeThreadLimit);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.NpZAsyncThrottleSettings);
    EXPECT_EQ(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, cmd.TheStructure.Common.AsyncComputeThreadLimit);
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_HW_DEFAULT, cmd.TheStructure.Common.EuThreadSchedulingMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.LargeGrfMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Mask1);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_0, cmd.TheStructure.Common.MidthreadPreemptionDelayTimer);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M2, cmd.TheStructure.Common.MidthreadPreemptionOverdispatchThreadGroupCount);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE_REGULAR, cmd.TheStructure.Common.MidthreadPreemptionOverdispatchTestMode);
    EXPECT_EQ(STATE_COMPUTE_MODE::UAV_COHERENCY_MODE_DRAIN_DATAPORT_MODE, cmd.TheStructure.Common.UavCoherencyMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.Mask2);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateComputeModeWhenSetterUsedThenGetterReturnsValidValue) {
    using STATE_COMPUTE_MODE = NEO::Xe2HpgCoreFamily::STATE_COMPUTE_MODE;
    STATE_COMPUTE_MODE cmd;
    cmd.init();

    cmd.setZPassAsyncComputeThreadLimit(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60);
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, cmd.getZPassAsyncComputeThreadLimit());

    cmd.setZPassAsyncComputeThreadLimit(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64);
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, cmd.getZPassAsyncComputeThreadLimit());

    cmd.setZPassAsyncComputeThreadLimit(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_56);
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_56, cmd.getZPassAsyncComputeThreadLimit());

    cmd.setZPassAsyncComputeThreadLimit(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_48);
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_48, cmd.getZPassAsyncComputeThreadLimit());

    cmd.setZPassAsyncComputeThreadLimit(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_40);
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_40, cmd.getZPassAsyncComputeThreadLimit());

    cmd.setZPassAsyncComputeThreadLimit(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_32);
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_32, cmd.getZPassAsyncComputeThreadLimit());

    cmd.setNpZAsyncThrottleSettings(STATE_COMPUTE_MODE::NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_32);
    EXPECT_EQ(STATE_COMPUTE_MODE::NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_32, cmd.getNpZAsyncThrottleSettings());

    cmd.setNpZAsyncThrottleSettings(STATE_COMPUTE_MODE::NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_40);
    EXPECT_EQ(STATE_COMPUTE_MODE::NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_40, cmd.getNpZAsyncThrottleSettings());

    cmd.setNpZAsyncThrottleSettings(STATE_COMPUTE_MODE::NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_48);
    EXPECT_EQ(STATE_COMPUTE_MODE::NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_48, cmd.getNpZAsyncThrottleSettings());

    cmd.setAsyncComputeThreadLimit(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_DISABLED);
    EXPECT_EQ(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, cmd.getAsyncComputeThreadLimit());

    cmd.setAsyncComputeThreadLimit(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_2);
    EXPECT_EQ(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_2, cmd.getAsyncComputeThreadLimit());

    cmd.setAsyncComputeThreadLimit(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_8);
    EXPECT_EQ(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_8, cmd.getAsyncComputeThreadLimit());

    cmd.setAsyncComputeThreadLimit(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_16);
    EXPECT_EQ(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_16, cmd.getAsyncComputeThreadLimit());

    cmd.setAsyncComputeThreadLimit(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_24);
    EXPECT_EQ(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_24, cmd.getAsyncComputeThreadLimit());

    cmd.setAsyncComputeThreadLimit(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_32);
    EXPECT_EQ(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_32, cmd.getAsyncComputeThreadLimit());

    cmd.setAsyncComputeThreadLimit(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_40);
    EXPECT_EQ(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_40, cmd.getAsyncComputeThreadLimit());

    cmd.setAsyncComputeThreadLimit(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_48);
    EXPECT_EQ(STATE_COMPUTE_MODE::ASYNC_COMPUTE_THREAD_LIMIT_MAX_48, cmd.getAsyncComputeThreadLimit());

    cmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_HW_DEFAULT);
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_HW_DEFAULT, cmd.getEuThreadSchedulingMode());

    cmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST);
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST, cmd.getEuThreadSchedulingMode());

    cmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN);
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN, cmd.getEuThreadSchedulingMode());

    cmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    EXPECT_EQ(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN, cmd.getEuThreadSchedulingMode());

    cmd.setLargeGrfMode(0x0u);
    EXPECT_EQ(0x0u, cmd.getLargeGrfMode());
    cmd.setLargeGrfMode(0x1u);
    EXPECT_EQ(0x1u, cmd.getLargeGrfMode());

    cmd.setMask1(0x0u);
    EXPECT_EQ(0x0u, cmd.getMask1());
    cmd.setMask1(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getMask1());

    cmd.setMidthreadPreemptionDelayTimer(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_0);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_0, cmd.getMidthreadPreemptionDelayTimer());

    cmd.setMidthreadPreemptionDelayTimer(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_50);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_50, cmd.getMidthreadPreemptionDelayTimer());

    cmd.setMidthreadPreemptionDelayTimer(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_100);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_100, cmd.getMidthreadPreemptionDelayTimer());

    cmd.setMidthreadPreemptionDelayTimer(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_150);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_150, cmd.getMidthreadPreemptionDelayTimer());

    cmd.setMidthreadPreemptionOverdispatchThreadGroupCount(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M2);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M2, cmd.getMidthreadPreemptionOverdispatchThreadGroupCount());

    cmd.setMidthreadPreemptionOverdispatchThreadGroupCount(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M4);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M4, cmd.getMidthreadPreemptionOverdispatchThreadGroupCount());

    cmd.setMidthreadPreemptionOverdispatchThreadGroupCount(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M8);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M8, cmd.getMidthreadPreemptionOverdispatchThreadGroupCount());

    cmd.setMidthreadPreemptionOverdispatchThreadGroupCount(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M16);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M16, cmd.getMidthreadPreemptionOverdispatchThreadGroupCount());

    cmd.setMidthreadPreemptionOverdispatchTestMode(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE_REGULAR);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE_REGULAR, cmd.getMidthreadPreemptionOverdispatchTestMode());

    cmd.setMidthreadPreemptionOverdispatchTestMode(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE_TEST_MODE);
    EXPECT_EQ(STATE_COMPUTE_MODE::MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE_TEST_MODE, cmd.getMidthreadPreemptionOverdispatchTestMode());

    cmd.setUavCoherencyMode(STATE_COMPUTE_MODE::UAV_COHERENCY_MODE_DRAIN_DATAPORT_MODE);
    EXPECT_EQ(STATE_COMPUTE_MODE::UAV_COHERENCY_MODE_DRAIN_DATAPORT_MODE, cmd.getUavCoherencyMode());

    cmd.setUavCoherencyMode(STATE_COMPUTE_MODE::UAV_COHERENCY_MODE_FLUSH_DATAPORT_L1);
    EXPECT_EQ(STATE_COMPUTE_MODE::UAV_COHERENCY_MODE_FLUSH_DATAPORT_L1, cmd.getUavCoherencyMode());

    cmd.setMask2(0x0u);
    EXPECT_EQ(0x0u, cmd.getMask2());
    cmd.setMask2(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getMask2());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMemSetWhenInitCalledThenFieldsSetToDefault) {
    using MEM_SET = NEO::Xe2HpgCoreFamily::MEM_SET;
    MEM_SET cmd;
    cmd.init();

    EXPECT_EQ(MEM_SET::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(MEM_SET::FILL_TYPE_LINEAR_FILL, cmd.TheStructure.Common.FillType);
    EXPECT_EQ(MEM_SET::MODE_FILL_MODE, cmd.TheStructure.Common.Mode);
    EXPECT_EQ(MEM_SET::INSTRUCTION_TARGETOPCODE_MEM_SET, cmd.TheStructure.Common.InstructionTarget_Opcode);
    EXPECT_EQ(MEM_SET::CLIENT_2D_PROCESSOR, cmd.TheStructure.Common.Client);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillWidth);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillHeight);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationPitch);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationStartAddressLow);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationStartAddressHigh);
    EXPECT_EQ(MEM_SET::DESTINATION_ENCRYPT_EN_DISABLE, cmd.TheStructure.Common.DestinationEncryptEn);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationMocs);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillData);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMemSetWhenSetterUsedThenGetterReturnsValidValue) {
    using MEM_SET = NEO::Xe2HpgCoreFamily::MEM_SET;
    MEM_SET cmd;
    cmd.init();

    cmd.setFillType(MEM_SET::FILL_TYPE_LINEAR_FILL);
    EXPECT_EQ(MEM_SET::FILL_TYPE_LINEAR_FILL, cmd.getFillType());

    cmd.setFillType(MEM_SET::FILL_TYPE_MATRIX_FILL);
    EXPECT_EQ(MEM_SET::FILL_TYPE_MATRIX_FILL, cmd.getFillType());

    cmd.setFillType(MEM_SET::FILL_TYPE_FAST_CLEAR_LINEAR_FILL);
    EXPECT_EQ(MEM_SET::FILL_TYPE_FAST_CLEAR_LINEAR_FILL, cmd.getFillType());

    cmd.setFillType(MEM_SET::FILL_TYPE_FAST_CLEAR_MATRIX_FILL);
    EXPECT_EQ(MEM_SET::FILL_TYPE_FAST_CLEAR_MATRIX_FILL, cmd.getFillType());

    cmd.setMode(MEM_SET::MODE_FILL_MODE);
    EXPECT_EQ(MEM_SET::MODE_FILL_MODE, cmd.getMode());

    EXPECT_THROW(cmd.setMode(MEM_SET::MODE_EVICT_MODE), std::exception);

    cmd.setInstructionTargetOpcode(MEM_SET::INSTRUCTION_TARGETOPCODE_MEM_SET);
    EXPECT_EQ(MEM_SET::INSTRUCTION_TARGETOPCODE_MEM_SET, cmd.getInstructionTargetOpcode());

    cmd.setClient(MEM_SET::CLIENT_2D_PROCESSOR);
    EXPECT_EQ(MEM_SET::CLIENT_2D_PROCESSOR, cmd.getClient());

    cmd.setFillWidth(0x1u);
    EXPECT_EQ(0x1u, cmd.getFillWidth());
    cmd.setFillWidth(0xffffffu);
    EXPECT_EQ(0xffffffu, cmd.getFillWidth());

    cmd.setFillHeight(0x1u);
    EXPECT_EQ(0x1u, cmd.getFillHeight());
    cmd.setFillHeight(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getFillHeight());

    cmd.setDestinationPitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationPitch());
    cmd.setDestinationPitch(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getDestinationPitch());

    cmd.setDestinationStartAddress(0x0u);                             // patched
    EXPECT_EQ(0x0u, cmd.getDestinationStartAddress());                // patched
    cmd.setDestinationStartAddress(0xffffffffffffffffu);              // patched
    EXPECT_EQ(0xffffffffffffffffu, cmd.getDestinationStartAddress()); // patched

    cmd.setDestinationEncryptEn(MEM_SET::DESTINATION_ENCRYPT_EN_DISABLE);
    EXPECT_EQ(MEM_SET::DESTINATION_ENCRYPT_EN_DISABLE, cmd.getDestinationEncryptEn());

    cmd.setDestinationEncryptEn(MEM_SET::DESTINATION_ENCRYPT_EN_ENABLE);
    EXPECT_EQ(MEM_SET::DESTINATION_ENCRYPT_EN_ENABLE, cmd.getDestinationEncryptEn());

    cmd.setDestinationMOCS(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getDestinationMOCS()); // patched
    cmd.setDestinationMOCS(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getDestinationMOCS()); // patched

    cmd.setFillData(0x0u);
    EXPECT_EQ(0x0u, cmd.getFillData());
    cmd.setFillData(0xffu);
    EXPECT_EQ(0xffu, cmd.getFillData());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMemCopyWhenInitCalledThenFieldsSetToDefault) {
    using MEM_COPY = NEO::Xe2HpgCoreFamily::MEM_COPY;
    MEM_COPY cmd;
    cmd.init();

    EXPECT_EQ(MEM_COPY::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(MEM_COPY::COPY_TYPE_LINEAR_COPY, cmd.TheStructure.Common.CopyType);
    EXPECT_EQ(MEM_COPY::MODE_BYTE_COPY, cmd.TheStructure.Common.Mode);
    EXPECT_EQ(MEM_COPY::INSTRUCTIONTARGET_OPCODE_OPCODE, cmd.TheStructure.Common.InstructionTarget_Opcode); // patched
    EXPECT_EQ(MEM_COPY::CLIENT_2D_PROCESSOR, cmd.TheStructure.Common.Client);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TransferWidth);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TransferHeight);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourcePitch);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationPitch);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceStartAddressLow);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceStartAddressHigh);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationStartAddressLow);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationStartAddressHigh);
    EXPECT_EQ(MEM_COPY::DESTINATION_ENCRYPT_EN_DISABLE, cmd.TheStructure.Common.DestinationEncryptEn);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationMocs);
    EXPECT_EQ(MEM_COPY::SOURCE_ENCRYPT_EN_DISABLE, cmd.TheStructure.Common.SourceEncryptEn);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceMocs);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMemCopyWhenSetterUsedThenGetterReturnsValidValue) {
    using MEM_COPY = NEO::Xe2HpgCoreFamily::MEM_COPY;
    MEM_COPY cmd;
    cmd.init();

    cmd.setCopyType(MEM_COPY::COPY_TYPE_LINEAR_COPY);
    EXPECT_EQ(MEM_COPY::COPY_TYPE_LINEAR_COPY, cmd.getCopyType());

    cmd.setCopyType(MEM_COPY::COPY_TYPE_MATRIX_COPY);
    EXPECT_EQ(MEM_COPY::COPY_TYPE_MATRIX_COPY, cmd.getCopyType());

    cmd.setMode(MEM_COPY::MODE_BYTE_COPY);
    EXPECT_EQ(MEM_COPY::MODE_BYTE_COPY, cmd.getMode());

    cmd.setMode(MEM_COPY::MODE_PAGE_COPY);
    EXPECT_EQ(MEM_COPY::MODE_PAGE_COPY, cmd.getMode());

    cmd.setInstructionTargetOpcode(MEM_COPY::INSTRUCTIONTARGET_OPCODE_OPCODE);              // patched
    EXPECT_EQ(MEM_COPY::INSTRUCTIONTARGET_OPCODE_OPCODE, cmd.getInstructionTargetOpcode()); // patched

    cmd.setClient(MEM_COPY::CLIENT_2D_PROCESSOR);
    EXPECT_EQ(MEM_COPY::CLIENT_2D_PROCESSOR, cmd.getClient());

    cmd.setDestinationX2CoordinateRight(0x1u);                   // patched
    EXPECT_EQ(0x1u, cmd.getDestinationX2CoordinateRight());      // patched
    cmd.setDestinationX2CoordinateRight(0xffffffu);              // patched
    EXPECT_EQ(0xffffffu, cmd.getDestinationX2CoordinateRight()); // patched

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

    cmd.setDestinationEncryptEn(MEM_COPY::DESTINATION_ENCRYPT_EN_DISABLE);
    EXPECT_EQ(MEM_COPY::DESTINATION_ENCRYPT_EN_DISABLE, cmd.getDestinationEncryptEn());

    cmd.setDestinationEncryptEn(MEM_COPY::DESTINATION_ENCRYPT_EN_ENABLE);
    EXPECT_EQ(MEM_COPY::DESTINATION_ENCRYPT_EN_ENABLE, cmd.getDestinationEncryptEn());

    cmd.setDestinationMOCS(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getDestinationMOCS()); // patched
    cmd.setDestinationMOCS(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getDestinationMOCS()); // patched

    cmd.setSourceEncryptEn(MEM_COPY::SOURCE_ENCRYPT_EN_DISABLE);
    EXPECT_EQ(MEM_COPY::SOURCE_ENCRYPT_EN_DISABLE, cmd.getSourceEncryptEn());

    cmd.setSourceEncryptEn(MEM_COPY::SOURCE_ENCRYPT_EN_ENABLE);
    EXPECT_EQ(MEM_COPY::SOURCE_ENCRYPT_EN_ENABLE, cmd.getSourceEncryptEn());

    cmd.setSourceMOCS(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getSourceMOCS()); // patched
    cmd.setSourceMOCS(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getSourceMOCS()); // patched
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenXyBlockCopyBltWhenInitCalledThenFieldsSetToDefault) {
    using XY_BLOCK_COPY_BLT = NEO::Xe2HpgCoreFamily::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT cmd;
    cmd.init();

    EXPECT_EQ(XY_BLOCK_COPY_BLT::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1, cmd.TheStructure.Common.NumberOfMultisamples);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION_NONE, cmd.TheStructure.Common.SpecialModeOfOperation);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::COLOR_DEPTH_8_BIT_COLOR, cmd.TheStructure.Common.ColorDepth);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE, cmd.TheStructure.Common.InstructionTarget_Opcode); // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::CLIENT_2D_PROCESSOR, cmd.TheStructure.Common.Client);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationPitch);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::DESTINATION_ENCRYPT_EN_DISABLE, cmd.TheStructure.Common.DestinationEncryptEn);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationMocs);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::DESTINATION_TILING_LINEAR, cmd.TheStructure.Common.DestinationTiling);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationX1Coordinate_Left);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationY1Coordinate_Top);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationX2Coordinate_Right);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationY2Coordinate_Bottom);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.DestinationBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationXOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationYOffset);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::DESTINATION_TARGET_MEMORY_LOCAL_MEM, cmd.TheStructure.Common.DestinationTargetMemory);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceX1Coordinate_Left);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceY1Coordinate_Top);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourcePitch);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SOURCE_ENCRYPT_EN_DISABLE, cmd.TheStructure.Common.SourceEncryptEn);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceMocs);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SOURCE_TILING_LINEAR, cmd.TheStructure.Common.SourceTiling);
    EXPECT_EQ_VAL(0x0ull, cmd.TheStructure.Common.SourceBaseAddress); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceXOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceYOffset);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SOURCE_TARGET_MEMORY_LOCAL_MEM, cmd.TheStructure.Common.SourceTargetMemory);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationSurfaceHeight);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationSurfaceWidth);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_1D, cmd.TheStructure.Common.DestinationSurfaceType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationSurfaceQpitch);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationSurfaceDepth);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationHorizontalAlign);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationVerticalAlign);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationMipTailStartLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationArrayIndex);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceSurfaceHeight);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceSurfaceWidth);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SOURCE_SURFACE_TYPE_SURFTYPE_1D, cmd.TheStructure.Common.SourceSurfaceType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceSurfaceQpitch);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceSurfaceDepth);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceHorizontalAlign);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceVerticalAlign);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceMipTailStartLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SourceArrayIndex);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenXyBlockCopyBltWhenSetterUsedThenGetterReturnsValidValue) {
    using XY_BLOCK_COPY_BLT = NEO::Xe2HpgCoreFamily::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT cmd;
    cmd.init();

    cmd.setNumberOfMultisamples(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16, cmd.getNumberOfMultisamples());

    cmd.setSpecialModeOfOperation(XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION_NONE);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION_NONE, cmd.getSpecialModeOfOperation());

    cmd.setSpecialModeOfOperation(XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE, cmd.getSpecialModeOfOperation());

    cmd.setSpecialModeOfOperation(XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION_PARTIAL_RESOLVE);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SPECIAL_MODE_OF_OPERATION_PARTIAL_RESOLVE, cmd.getSpecialModeOfOperation());

    cmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH_8_BIT_COLOR);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::COLOR_DEPTH_8_BIT_COLOR, cmd.getColorDepth());

    cmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH_16_BIT_COLOR);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::COLOR_DEPTH_16_BIT_COLOR, cmd.getColorDepth());

    cmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH_32_BIT_COLOR);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::COLOR_DEPTH_32_BIT_COLOR, cmd.getColorDepth());

    cmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH_64_BIT_COLOR);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::COLOR_DEPTH_64_BIT_COLOR, cmd.getColorDepth());

    cmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH_96_BIT_COLOR_ONLY_LINEAR_CASE_IS_SUPPORTED);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::COLOR_DEPTH_96_BIT_COLOR_ONLY_LINEAR_CASE_IS_SUPPORTED, cmd.getColorDepth());

    cmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH_128_BIT_COLOR);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::COLOR_DEPTH_128_BIT_COLOR, cmd.getColorDepth());

    cmd.setInstructionTargetOpcode(XY_BLOCK_COPY_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE, cmd.getInstructionTargetOpcode()); // patched

    cmd.setClient(XY_BLOCK_COPY_BLT::CLIENT_2D_PROCESSOR);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::CLIENT_2D_PROCESSOR, cmd.getClient());

    cmd.setDestinationPitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationPitch());
    cmd.setDestinationPitch(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getDestinationPitch());

    cmd.setDestinationEncryptEn(XY_BLOCK_COPY_BLT::DESTINATION_ENCRYPT_EN_DISABLE);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::DESTINATION_ENCRYPT_EN_DISABLE, cmd.getDestinationEncryptEn());

    cmd.setDestinationEncryptEn(XY_BLOCK_COPY_BLT::DESTINATION_ENCRYPT_EN_ENABLE);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::DESTINATION_ENCRYPT_EN_ENABLE, cmd.getDestinationEncryptEn());

    cmd.setDestinationMOCS(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getDestinationMOCS()); // patched
    cmd.setDestinationMOCS(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getDestinationMOCS()); // patched

    cmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING_LINEAR);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TILING_LINEAR, cmd.getDestinationTiling()); // patched

    cmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING_XMAJOR);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TILING_XMAJOR, cmd.getDestinationTiling()); // patched

    cmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING_TILE4);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TILING_TILE4, cmd.getDestinationTiling()); // patched

    cmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING_TILE64);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TILING_TILE64, cmd.getDestinationTiling()); // patched

    cmd.setDestinationX1CoordinateLeft(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationX1CoordinateLeft());
    cmd.setDestinationX1CoordinateLeft(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getDestinationX1CoordinateLeft());

    cmd.setDestinationY1CoordinateTop(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationY1CoordinateTop());
    cmd.setDestinationY1CoordinateTop(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getDestinationY1CoordinateTop());

    cmd.setDestinationX2CoordinateRight(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationX2CoordinateRight());
    cmd.setDestinationX2CoordinateRight(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getDestinationX2CoordinateRight());

    cmd.setDestinationY2CoordinateBottom(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationY2CoordinateBottom());
    cmd.setDestinationY2CoordinateBottom(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getDestinationY2CoordinateBottom());

    cmd.setDestinationBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getDestinationBaseAddress());
    cmd.setDestinationBaseAddress(0xffffffffffffffffull);
    EXPECT_EQ(0xffffffffffffffffull, cmd.getDestinationBaseAddress());

    cmd.setDestinationXOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationXOffset());
    cmd.setDestinationXOffset(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getDestinationXOffset());

    cmd.setDestinationYOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationYOffset());
    cmd.setDestinationYOffset(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getDestinationYOffset());

    cmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY_LOCAL_MEM);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TARGET_MEMORY_LOCAL_MEM, cmd.getDestinationTargetMemory()); // patched

    cmd.setDestinationTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY_SYSTEM_MEM);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TARGET_MEMORY_SYSTEM_MEM, cmd.getDestinationTargetMemory()); // patched

    cmd.setSourceX1CoordinateLeft(0x0u);
    EXPECT_EQ(0x0u, cmd.getSourceX1CoordinateLeft());
    cmd.setSourceX1CoordinateLeft(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getSourceX1CoordinateLeft());

    cmd.setSourceY1CoordinateTop(0x0u);
    EXPECT_EQ(0x0u, cmd.getSourceY1CoordinateTop());
    cmd.setSourceY1CoordinateTop(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getSourceY1CoordinateTop());

    cmd.setSourcePitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getSourcePitch());
    cmd.setSourcePitch(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getSourcePitch());

    cmd.setSourceEncryptEn(XY_BLOCK_COPY_BLT::SOURCE_ENCRYPT_EN_DISABLE);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SOURCE_ENCRYPT_EN_DISABLE, cmd.getSourceEncryptEn());

    cmd.setSourceEncryptEn(XY_BLOCK_COPY_BLT::SOURCE_ENCRYPT_EN_ENABLE);
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SOURCE_ENCRYPT_EN_ENABLE, cmd.getSourceEncryptEn());

    cmd.setSourceMOCS(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getSourceMOCS()); // patched
    cmd.setSourceMOCS(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getSourceMOCS()); // patched

    cmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING_LINEAR);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TILING_LINEAR, cmd.getSourceTiling()); // patched

    cmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING_XMAJOR);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TILING_XMAJOR, cmd.getSourceTiling()); // patched

    cmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING_TILE4);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TILING_TILE4, cmd.getSourceTiling()); // patched

    cmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING_TILE64);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TILING_TILE64, cmd.getSourceTiling()); // patched

    cmd.setSourceBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getSourceBaseAddress());
    cmd.setSourceBaseAddress(0xffffffffffffffffull);
    EXPECT_EQ(0xffffffffffffffffull, cmd.getSourceBaseAddress());

    cmd.setSourceXOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getSourceXOffset());
    cmd.setSourceXOffset(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getSourceXOffset());

    cmd.setSourceYOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getSourceYOffset());
    cmd.setSourceYOffset(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getSourceYOffset());

    cmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY_LOCAL_MEM);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TARGET_MEMORY_LOCAL_MEM, cmd.getSourceTargetMemory()); // patched

    cmd.setSourceTargetMemory(XY_BLOCK_COPY_BLT::TARGET_MEMORY_SYSTEM_MEM);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::TARGET_MEMORY_SYSTEM_MEM, cmd.getSourceTargetMemory()); // patched

    cmd.setDestinationSurfaceHeight(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationSurfaceHeight());
    cmd.setDestinationSurfaceHeight(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getDestinationSurfaceHeight());

    cmd.setDestinationSurfaceWidth(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationSurfaceWidth());
    cmd.setDestinationSurfaceWidth(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getDestinationSurfaceWidth());

    cmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_1D);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_1D, cmd.getDestinationSurfaceType()); // patched

    cmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_2D);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_2D, cmd.getDestinationSurfaceType()); // patched

    cmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_3D);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_3D, cmd.getDestinationSurfaceType()); // patched

    cmd.setDestinationSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_CUBE);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_CUBE, cmd.getDestinationSurfaceType()); // patched

    cmd.setDestinationLod(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationLod());
    cmd.setDestinationLod(0xfu);
    EXPECT_EQ(0xfu, cmd.getDestinationLod());

    cmd.setDestinationSurfaceQpitch(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationSurfaceQpitch());
    cmd.setDestinationSurfaceQpitch(0x7fffu);
    EXPECT_EQ(0x7fffu, cmd.getDestinationSurfaceQpitch());

    cmd.setDestinationSurfaceDepth(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationSurfaceDepth());
    cmd.setDestinationSurfaceDepth(0x7ffu);
    EXPECT_EQ(0x7ffu, cmd.getDestinationSurfaceDepth());

    cmd.setDestinationHorizontalAlign(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationHorizontalAlign());
    cmd.setDestinationHorizontalAlign(0x3u);
    EXPECT_EQ(0x3u, cmd.getDestinationHorizontalAlign());

    cmd.setDestinationVerticalAlign(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationVerticalAlign());
    cmd.setDestinationVerticalAlign(0x3u);
    EXPECT_EQ(0x3u, cmd.getDestinationVerticalAlign());

    cmd.setDestinationMipTailStartLOD(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getDestinationMipTailStartLOD()); // patched
    cmd.setDestinationMipTailStartLOD(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getDestinationMipTailStartLOD()); // patched

    cmd.setDestinationArrayIndex(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationArrayIndex());
    cmd.setDestinationArrayIndex(0x7ffu);
    EXPECT_EQ(0x7ffu, cmd.getDestinationArrayIndex());

    cmd.setSourceSurfaceHeight(0x1u);
    EXPECT_EQ(0x1u, cmd.getSourceSurfaceHeight());
    cmd.setSourceSurfaceHeight(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getSourceSurfaceHeight());

    cmd.setSourceSurfaceWidth(0x1u);
    EXPECT_EQ(0x1u, cmd.getSourceSurfaceWidth());
    cmd.setSourceSurfaceWidth(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getSourceSurfaceWidth());

    cmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_1D);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_1D, cmd.getSourceSurfaceType()); // patched

    cmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_2D);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_2D, cmd.getSourceSurfaceType()); // patched

    cmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_3D);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_3D, cmd.getSourceSurfaceType()); // patched

    cmd.setSourceSurfaceType(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_CUBE);              // patched
    EXPECT_EQ(XY_BLOCK_COPY_BLT::SURFACE_TYPE_SURFTYPE_CUBE, cmd.getSourceSurfaceType()); // patched

    cmd.setSourceLod(0x0u);
    EXPECT_EQ(0x0u, cmd.getSourceLod());
    cmd.setSourceLod(0xfu);
    EXPECT_EQ(0xfu, cmd.getSourceLod());

    cmd.setSourceSurfaceQpitch(0x0u);
    EXPECT_EQ(0x0u, cmd.getSourceSurfaceQpitch());
    cmd.setSourceSurfaceQpitch(0x7fffu);
    EXPECT_EQ(0x7fffu, cmd.getSourceSurfaceQpitch());

    cmd.setSourceSurfaceDepth(0x1u);
    EXPECT_EQ(0x1u, cmd.getSourceSurfaceDepth());
    cmd.setSourceSurfaceDepth(0x7ffu);
    EXPECT_EQ(0x7ffu, cmd.getSourceSurfaceDepth());

    cmd.setSourceHorizontalAlign(0x0u);
    EXPECT_EQ(0x0u, cmd.getSourceHorizontalAlign());
    cmd.setSourceHorizontalAlign(0x3u);
    EXPECT_EQ(0x3u, cmd.getSourceHorizontalAlign());

    cmd.setSourceVerticalAlign(0x0u);
    EXPECT_EQ(0x0u, cmd.getSourceVerticalAlign());
    cmd.setSourceVerticalAlign(0x3u);
    EXPECT_EQ(0x3u, cmd.getSourceVerticalAlign());

    cmd.setSourceMipTailStartLOD(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getSourceMipTailStartLOD()); // patched
    cmd.setSourceMipTailStartLOD(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getSourceMipTailStartLOD()); // patched

    cmd.setSourceArrayIndex(0x1u);
    EXPECT_EQ(0x1u, cmd.getSourceArrayIndex());
    cmd.setSourceArrayIndex(0x7ffu);
    EXPECT_EQ(0x7ffu, cmd.getSourceArrayIndex());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiMemFenceWhenInitCalledThenFieldsSetToDefault) {
    using MI_MEM_FENCE = NEO::Xe2HpgCoreFamily::MI_MEM_FENCE;
    MI_MEM_FENCE cmd;
    cmd.init();

    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE_RELEASE_FENCE, cmd.TheStructure.Common.FenceType);
    EXPECT_EQ(MI_MEM_FENCE::MI_COMMAND_SUB_OPCODE_MI_MEM_FENCE, cmd.TheStructure.Common.MiCommandSubOpcode);
    EXPECT_EQ(MI_MEM_FENCE::MI_COMMAND_OPCODE_MI_EXTENDED, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_MEM_FENCE::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiMemFenceWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_MEM_FENCE = NEO::Xe2HpgCoreFamily::MI_MEM_FENCE;
    MI_MEM_FENCE cmd;
    cmd.init();

    cmd.setFenceType(MI_MEM_FENCE::FENCE_TYPE_RELEASE_FENCE);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE_RELEASE_FENCE, cmd.getFenceType());

    cmd.setFenceType(MI_MEM_FENCE::FENCE_TYPE_ACQUIRE_FENCE);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE_ACQUIRE_FENCE, cmd.getFenceType());

    cmd.setFenceType(MI_MEM_FENCE::FENCE_TYPE_MI_WRITE_FENCE);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE_MI_WRITE_FENCE, cmd.getFenceType());

    cmd.setMiCommandSubOpcode(MI_MEM_FENCE::MI_COMMAND_SUB_OPCODE_MI_MEM_FENCE);
    EXPECT_EQ(MI_MEM_FENCE::MI_COMMAND_SUB_OPCODE_MI_MEM_FENCE, cmd.getMiCommandSubOpcode());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, Given3dstateBindingTablePoolAllocWhenInitCalledThenFieldsSetToDefault) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = NEO::Xe2HpgCoreFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;
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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, Given3dstateBindingTablePoolAllocWhenSetterUsedThenGetterReturnsValidValue) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = NEO::Xe2HpgCoreFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;
    _3DSTATE_BINDING_TABLE_POOL_ALLOC cmd;
    cmd.init();

    cmd.setSurfaceObjectControlStateIndexToMocsTables(0x0u);
    EXPECT_EQ(0x0u, cmd.getSurfaceObjectControlStateIndexToMocsTables());
    cmd.setSurfaceObjectControlStateIndexToMocsTables(0x3eu);              // patched
    EXPECT_EQ(0x3eu, cmd.getSurfaceObjectControlStateIndexToMocsTables()); // patched

    cmd.setBindingTablePoolBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getBindingTablePoolBaseAddress());
    cmd.setBindingTablePoolBaseAddress(0x7fffff000ull);              // patched
    EXPECT_EQ(0x7fffff000ull, cmd.getBindingTablePoolBaseAddress()); // patched

    cmd.setBindingTablePoolBufferSize(0x0u);                  // patched
    EXPECT_EQ(0x0u, cmd.getBindingTablePoolBufferSize());     // patched
    cmd.setBindingTablePoolBufferSize(0xfffffu);              // patched
    EXPECT_EQ(0xfffffu, cmd.getBindingTablePoolBufferSize()); // patched
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenSamplerBorderColorStateWhenInitCalledThenFieldsSetToDefault) {
    using SAMPLER_BORDER_COLOR_STATE = NEO::Xe2HpgCoreFamily::SAMPLER_BORDER_COLOR_STATE;
    SAMPLER_BORDER_COLOR_STATE cmd;
    cmd.init();

    EXPECT_EQ(0.0f, cmd.TheStructure.Common.BorderColorRed);
    EXPECT_EQ(0.0f, cmd.TheStructure.Common.BorderColorGreen);
    EXPECT_EQ(0.0f, cmd.TheStructure.Common.BorderColorBlue);
    EXPECT_EQ(0.0f, cmd.TheStructure.Common.BorderColorAlpha);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenSamplerBorderColorStateWhenSetterUsedThenGetterReturnsValidValue) {
    using SAMPLER_BORDER_COLOR_STATE = NEO::Xe2HpgCoreFamily::SAMPLER_BORDER_COLOR_STATE;
    SAMPLER_BORDER_COLOR_STATE cmd;
    cmd.init();

    cmd.setBorderColorRed(0.0f);
    EXPECT_FLOAT_EQ(0.0f, cmd.getBorderColorRed());
    cmd.setBorderColorRed(0xffffffff.0p0f);
    EXPECT_FLOAT_EQ(0xffffffff.0p0f, cmd.getBorderColorRed());

    cmd.setBorderColorGreen(0.0f);
    EXPECT_FLOAT_EQ(0.0f, cmd.getBorderColorGreen());
    cmd.setBorderColorGreen(0xffffffff.0p0f);
    EXPECT_FLOAT_EQ(0xffffffff.0p0f, cmd.getBorderColorGreen());

    cmd.setBorderColorBlue(0.0f);
    EXPECT_FLOAT_EQ(0.0f, cmd.getBorderColorBlue());
    cmd.setBorderColorBlue(0xffffffff.0p0f);
    EXPECT_FLOAT_EQ(0xffffffff.0p0f, cmd.getBorderColorBlue());

    cmd.setBorderColorAlpha(0.0f);
    EXPECT_FLOAT_EQ(0.0f, cmd.getBorderColorAlpha());
    cmd.setBorderColorAlpha(0xffffffff.0p0f);
    EXPECT_FLOAT_EQ(0xffffffff.0p0f, cmd.getBorderColorAlpha());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateSystemMemFenceAddressWhenInitCalledThenFieldsSetToDefault) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = NEO::Xe2HpgCoreFamily::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    STATE_SYSTEM_MEM_FENCE_ADDRESS cmd;
    cmd.init();

    EXPECT_EQ(STATE_SYSTEM_MEM_FENCE_ADDRESS::DWORD_LENGTH_LENGTH_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.ContextRestoreInvalid);
    EXPECT_EQ(STATE_SYSTEM_MEM_FENCE_ADDRESS::_3D_COMMAND_SUB_OPCODE_STATE_SYSTEM_MEM_FENCE_ADDRESS, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(STATE_SYSTEM_MEM_FENCE_ADDRESS::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(STATE_SYSTEM_MEM_FENCE_ADDRESS::COMMAND_SUBTYPE_GFXPIPE_COMMON_, cmd.TheStructure.Common.CommandSubtype); // patched
    EXPECT_EQ(STATE_SYSTEM_MEM_FENCE_ADDRESS::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.SystemMemoryFenceAddress);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateSystemMemFenceAddressWhenSetterUsedThenGetterReturnsValidValue) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = NEO::Xe2HpgCoreFamily::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    STATE_SYSTEM_MEM_FENCE_ADDRESS cmd;
    cmd.init();

    cmd.setContextRestoreInvalid(0x0u);
    EXPECT_EQ(0x0u, cmd.getContextRestoreInvalid());
    cmd.setContextRestoreInvalid(0x1u);
    EXPECT_EQ(0x1u, cmd.getContextRestoreInvalid());

    cmd.setSystemMemoryFenceAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getSystemMemoryFenceAddress());
    cmd.setSystemMemoryFenceAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getSystemMemoryFenceAddress());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, Given3dstateBtdWhenInitCalledThenFieldsSetToDefault) {
    using _3DSTATE_BTD = NEO::Xe2HpgCoreFamily::_3DSTATE_BTD;
    _3DSTATE_BTD cmd;
    cmd.init();

    EXPECT_EQ(_3DSTATE_BTD::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(_3DSTATE_BTD::_3D_COMMAND_SUB_OPCODE_3DSTATE_BTD, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(_3DSTATE_BTD::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(_3DSTATE_BTD::COMMAND_SUBTYPE_GFXPIPE_COMMON, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(_3DSTATE_BTD::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_64, cmd.TheStructure.Common.DispatchTimeoutCounter);
    EXPECT_EQ(_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_128, cmd.TheStructure.Common.ControlsTheMaximumNumberOfOutstandingRayqueriesPerSs);
    EXPECT_EQ(_3DSTATE_BTD::PER_DSS_MEMORY_BACKED_BUFFER_SIZE_2KB, cmd.TheStructure.Common.PerDssMemoryBackedBufferSize); // patched
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.MemoryBackedBufferBasePointer);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, Given3dstateBtdWhenSetterUsedThenGetterReturnsValidValue) {
    using _3DSTATE_BTD = NEO::Xe2HpgCoreFamily::_3DSTATE_BTD;
    _3DSTATE_BTD cmd;
    cmd.init();

    cmd.setDispatchTimeoutCounter(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_64);
    EXPECT_EQ(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_64, cmd.getDispatchTimeoutCounter());

    cmd.setDispatchTimeoutCounter(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_128);
    EXPECT_EQ(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_128, cmd.getDispatchTimeoutCounter());

    cmd.setDispatchTimeoutCounter(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_192);
    EXPECT_EQ(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_192, cmd.getDispatchTimeoutCounter());

    cmd.setDispatchTimeoutCounter(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_256);
    EXPECT_EQ(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_256, cmd.getDispatchTimeoutCounter());

    cmd.setDispatchTimeoutCounter(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_512);
    EXPECT_EQ(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_512, cmd.getDispatchTimeoutCounter());

    cmd.setDispatchTimeoutCounter(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_1024);
    EXPECT_EQ(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_1024, cmd.getDispatchTimeoutCounter());

    cmd.setDispatchTimeoutCounter(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_2048);
    EXPECT_EQ(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_2048, cmd.getDispatchTimeoutCounter());

    cmd.setDispatchTimeoutCounter(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_4096);
    EXPECT_EQ(_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER_4096, cmd.getDispatchTimeoutCounter());

    cmd.setControlsTheMaximumNumberOfOutstandingRayqueriesPerSs(_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_128);
    EXPECT_EQ(_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_128, cmd.getControlsTheMaximumNumberOfOutstandingRayqueriesPerSs());

    cmd.setControlsTheMaximumNumberOfOutstandingRayqueriesPerSs(_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_256);
    EXPECT_EQ(_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_256, cmd.getControlsTheMaximumNumberOfOutstandingRayqueriesPerSs());

    cmd.setControlsTheMaximumNumberOfOutstandingRayqueriesPerSs(_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_512);
    EXPECT_EQ(_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_512, cmd.getControlsTheMaximumNumberOfOutstandingRayqueriesPerSs());

    cmd.setControlsTheMaximumNumberOfOutstandingRayqueriesPerSs(_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_1024);
    EXPECT_EQ(_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_1024, cmd.getControlsTheMaximumNumberOfOutstandingRayqueriesPerSs());

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

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiUserInterruptWhenInitCalledThenFieldsSetToDefault) {
    using MI_USER_INTERRUPT = NEO::Xe2HpgCoreFamily::MI_USER_INTERRUPT;
    MI_USER_INTERRUPT cmd;
    cmd.init();

    EXPECT_EQ(MI_USER_INTERRUPT::MI_COMMAND_OPCODE_MI_USER_INTERRUPT, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_USER_INTERRUPT::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiUserInterruptWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_USER_INTERRUPT = NEO::Xe2HpgCoreFamily::MI_USER_INTERRUPT;
    MI_USER_INTERRUPT cmd;
    cmd.init();
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateSipWhenInitCalledThenFieldsSetToDefault) {
    using STATE_SIP = NEO::Xe2HpgCoreFamily::STATE_SIP;
    STATE_SIP cmd;
    cmd.init();

    EXPECT_EQ(STATE_SIP::DWORD_LENGTH_DWORD_COUNT_N, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(STATE_SIP::_3D_COMMAND_SUB_OPCODE_STATE_SIP, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(STATE_SIP::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(STATE_SIP::COMMAND_SUBTYPE_GFXPIPE_COMMON, cmd.TheStructure.Common.CommandSubtype);
    EXPECT_EQ(STATE_SIP::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.SystemInstructionPointer);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateSipWhenSetterUsedThenGetterReturnsValidValue) {
    using STATE_SIP = NEO::Xe2HpgCoreFamily::STATE_SIP;
    STATE_SIP cmd;
    cmd.init();

    cmd.setSystemInstructionPointer(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getSystemInstructionPointer());
    cmd.setSystemInstructionPointer(0xfffffffffffffff0ull);
    EXPECT_EQ(0xfffffffffffffff0ull, cmd.getSystemInstructionPointer());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiFlushDwWhenInitCalledThenFieldsSetToDefault) {
    using MI_FLUSH_DW = NEO::Xe2HpgCoreFamily::MI_FLUSH_DW;
    MI_FLUSH_DW cmd;
    cmd.init();

    EXPECT_EQ(MI_FLUSH_DW::DWORD_LENGTH_EXCLUDES_DWORD_0_1_, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.NotifyEnable);
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE, cmd.TheStructure.Common.PostSyncOperation);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.TlbInvalidate);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.StoreDataIndex);
    EXPECT_EQ(MI_FLUSH_DW::MI_COMMAND_OPCODE_MI_FLUSH_DW, cmd.TheStructure.Common.MiCommandOpcode);
    EXPECT_EQ(MI_FLUSH_DW::COMMAND_TYPE_MI_COMMAND, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(MI_FLUSH_DW::DESTINATION_ADDRESS_TYPE_PPGTT, cmd.TheStructure.Common.DestinationAddressType);
    EXPECT_EQ_VAL(0x0ull, cmd.TheStructure.Common.DestinationAddress); // patched
    EXPECT_EQ_VAL(0x0ull, cmd.TheStructure.Common.ImmediateData);      // patched
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenMiFlushDwWhenSetterUsedThenGetterReturnsValidValue) {
    using MI_FLUSH_DW = NEO::Xe2HpgCoreFamily::MI_FLUSH_DW;
    MI_FLUSH_DW cmd;
    cmd.init();

    cmd.setNotifyEnable(0x0u);
    EXPECT_EQ(0x0u, cmd.getNotifyEnable());
    cmd.setNotifyEnable(0x1u);
    EXPECT_EQ(0x1u, cmd.getNotifyEnable());

    cmd.setPostSyncOperation(MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE);
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE, cmd.getPostSyncOperation());

    cmd.setPostSyncOperation(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, cmd.getPostSyncOperation());

    cmd.setPostSyncOperation(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_TIMESTAMP_REGISTER);
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_TIMESTAMP_REGISTER, cmd.getPostSyncOperation());

    cmd.setTlbInvalidate(0x0u);
    EXPECT_EQ(0x0u, cmd.getTlbInvalidate());
    cmd.setTlbInvalidate(0x1u);
    EXPECT_EQ(0x1u, cmd.getTlbInvalidate());

    cmd.setStoreDataIndex(0x0u);
    EXPECT_EQ(0x0u, cmd.getStoreDataIndex());
    cmd.setStoreDataIndex(0x1u);
    EXPECT_EQ(0x1u, cmd.getStoreDataIndex());

    cmd.setDestinationAddressType(MI_FLUSH_DW::DESTINATION_ADDRESS_TYPE_PPGTT);
    EXPECT_EQ(MI_FLUSH_DW::DESTINATION_ADDRESS_TYPE_PPGTT, cmd.getDestinationAddressType());

    cmd.setDestinationAddressType(MI_FLUSH_DW::DESTINATION_ADDRESS_TYPE_GGTT);
    EXPECT_EQ(MI_FLUSH_DW::DESTINATION_ADDRESS_TYPE_GGTT, cmd.getDestinationAddressType());

    cmd.setDestinationAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getDestinationAddress());
    cmd.setDestinationAddress(0xfffffffffff8ull);              // patched
    EXPECT_EQ(0xfffffffffff8ull, cmd.getDestinationAddress()); // patched

    cmd.setImmediateData(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getImmediateData());
    cmd.setImmediateData(0xffffffffffffffffull);
    EXPECT_EQ(0xffffffffffffffffull, cmd.getImmediateData());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateContextDataBaseAddressWhenInitCalledThenFieldsSetToDefault) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = NEO::Xe2HpgCoreFamily::STATE_CONTEXT_DATA_BASE_ADDRESS;
    STATE_CONTEXT_DATA_BASE_ADDRESS cmd;
    cmd.init();

    EXPECT_EQ(STATE_CONTEXT_DATA_BASE_ADDRESS::DWORD_LENGTH_LENGTH_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(STATE_CONTEXT_DATA_BASE_ADDRESS::_3D_COMMAND_SUB_OPCODE_STATE_CONTEXT_DATA_BASE_ADDRESS, cmd.TheStructure.Common._3DCommandSubOpcode);
    EXPECT_EQ(STATE_CONTEXT_DATA_BASE_ADDRESS::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED, cmd.TheStructure.Common._3DCommandOpcode);
    EXPECT_EQ(STATE_CONTEXT_DATA_BASE_ADDRESS::COMMAND_SUBTYPE_GFXPIPE_COMMON_, cmd.TheStructure.Common.CommandSubtype); // patched
    EXPECT_EQ(STATE_CONTEXT_DATA_BASE_ADDRESS::COMMAND_TYPE_GFXPIPE, cmd.TheStructure.Common.CommandType);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.ContextDataBaseAddress);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenStateContextDataBaseAddressWhenSetterUsedThenGetterReturnsValidValue) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = NEO::Xe2HpgCoreFamily::STATE_CONTEXT_DATA_BASE_ADDRESS;
    STATE_CONTEXT_DATA_BASE_ADDRESS cmd;
    cmd.init();

    cmd.setContextDataBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getContextDataBaseAddress());
    cmd.setContextDataBaseAddress(0xfffffffffffff000ull);
    EXPECT_EQ(0xfffffffffffff000ull, cmd.getContextDataBaseAddress());
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenXyFastColorBltWhenInitCalledThenFieldsSetToDefault) { // patched
    using XY_FAST_COLOR_BLT = NEO::Xe2HpgCoreFamily::XY_FAST_COLOR_BLT;
    XY_FAST_COLOR_BLT cmd;
    cmd.init();

    EXPECT_EQ(XY_FAST_COLOR_BLT::DWORD_LENGTH_EXCLUDES_DWORD_0_1, cmd.TheStructure.Common.DwordLength);
    EXPECT_EQ(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1, cmd.TheStructure.Common.NumberOfMultisamples);
    EXPECT_EQ(XY_FAST_COLOR_BLT::SPECIAL_MODE_OF_OPERATION_NONE, cmd.TheStructure.Common.SpecialModeOfOperation);
    EXPECT_EQ(XY_FAST_COLOR_BLT::COLOR_DEPTH_8_BIT_COLOR, cmd.TheStructure.Common.ColorDepth);
    EXPECT_EQ(XY_FAST_COLOR_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE, cmd.TheStructure.Common.InstructionTarget_Opcode); // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::CLIENT_2D_PROCESSOR, cmd.TheStructure.Common.Client);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationPitch);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_ENCRYPT_EN_DISABLE, cmd.TheStructure.Common.DestinationEncryptEn);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationMocs);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_TILING_LINEAR, cmd.TheStructure.Common.DestinationTiling);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationX1Coordinate_Left);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationY1Coordinate_Top);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationX2Coordinate_Right);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationY2Coordinate_Bottom);
    EXPECT_EQ(0x0ull, cmd.TheStructure.Common.DestinationBaseAddress);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationXOffset);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationYOffset);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_TARGET_MEMORY_LOCAL_MEM, cmd.TheStructure.Common.DestinationTargetMemory);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationCompressionFormat);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.SurfaceFormat);
    EXPECT_EQ(XY_FAST_COLOR_BLT::AUX_SPECIAL_OPERATIONS_MODE_FAST_CLEAR_HW_FORMAT_CONVERSION, cmd.TheStructure.Common.AuxSpecialOperationsMode);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationSurfaceHeight);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationSurfaceWidth);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_1D, cmd.TheStructure.Common.DestinationSurfaceType);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationSurfaceQpitch);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationSurfaceDepth);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationHorizontalAlign);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationVerticalAlign);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationMipTailStartLod);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationDepthStencilResource);
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.DestinationArrayIndex);
}

XE2_HPG_CORETEST_F(CommandsXe2HpgCoreTest, GivenXyFastColorBltWhenSetterUsedThenGetterReturnsValidValue) {
    using XY_FAST_COLOR_BLT = NEO::Xe2HpgCoreFamily::XY_FAST_COLOR_BLT;
    XY_FAST_COLOR_BLT cmd;
    cmd.init();

    cmd.setNumberOfMultisamples(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1);
    EXPECT_EQ(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2);
    EXPECT_EQ(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4);
    EXPECT_EQ(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8);
    EXPECT_EQ(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8, cmd.getNumberOfMultisamples());

    cmd.setNumberOfMultisamples(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16);
    EXPECT_EQ(XY_FAST_COLOR_BLT::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16, cmd.getNumberOfMultisamples());

    cmd.setColorDepth(XY_FAST_COLOR_BLT::COLOR_DEPTH_8_BIT_COLOR);
    EXPECT_EQ(XY_FAST_COLOR_BLT::COLOR_DEPTH_8_BIT_COLOR, cmd.getColorDepth());

    cmd.setColorDepth(XY_FAST_COLOR_BLT::COLOR_DEPTH_16_BIT_COLOR);
    EXPECT_EQ(XY_FAST_COLOR_BLT::COLOR_DEPTH_16_BIT_COLOR, cmd.getColorDepth());

    cmd.setColorDepth(XY_FAST_COLOR_BLT::COLOR_DEPTH_32_BIT_COLOR);
    EXPECT_EQ(XY_FAST_COLOR_BLT::COLOR_DEPTH_32_BIT_COLOR, cmd.getColorDepth());

    cmd.setColorDepth(XY_FAST_COLOR_BLT::COLOR_DEPTH_64_BIT_COLOR);
    EXPECT_EQ(XY_FAST_COLOR_BLT::COLOR_DEPTH_64_BIT_COLOR, cmd.getColorDepth());

    cmd.setColorDepth(XY_FAST_COLOR_BLT::COLOR_DEPTH_128_BIT_COLOR);
    EXPECT_EQ(XY_FAST_COLOR_BLT::COLOR_DEPTH_128_BIT_COLOR, cmd.getColorDepth());

    cmd.setInstructionTargetOpcode(XY_FAST_COLOR_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE, cmd.getInstructionTargetOpcode()); // patched

    cmd.setClient(XY_FAST_COLOR_BLT::CLIENT_2D_PROCESSOR);
    EXPECT_EQ(XY_FAST_COLOR_BLT::CLIENT_2D_PROCESSOR, cmd.getClient());

    cmd.setDestinationPitch(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationPitch());
    cmd.setDestinationPitch(0x3ffffu);
    EXPECT_EQ(0x3ffffu, cmd.getDestinationPitch());

    cmd.setDestinationEncryptEn(XY_FAST_COLOR_BLT::DESTINATION_ENCRYPT_EN_DISABLE);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_ENCRYPT_EN_DISABLE, cmd.getDestinationEncryptEn());

    cmd.setDestinationEncryptEn(XY_FAST_COLOR_BLT::DESTINATION_ENCRYPT_EN_ENABLE);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_ENCRYPT_EN_ENABLE, cmd.getDestinationEncryptEn());

    cmd.setDestinationMOCS(0x0u);              // patched
    EXPECT_EQ(0x0u, cmd.getDestinationMOCS()); // patched
    cmd.setDestinationMOCS(0xfu);              // patched
    EXPECT_EQ(0xfu, cmd.getDestinationMOCS()); // patched

    cmd.setDestinationTiling(XY_FAST_COLOR_BLT::DESTINATION_TILING_LINEAR);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_TILING_LINEAR, cmd.getDestinationTiling());

    cmd.setDestinationTiling(XY_FAST_COLOR_BLT::DESTINATION_TILING_XMAJOR);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_TILING_XMAJOR, cmd.getDestinationTiling());

    cmd.setDestinationTiling(XY_FAST_COLOR_BLT::DESTINATION_TILING_TILE4);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_TILING_TILE4, cmd.getDestinationTiling());

    cmd.setDestinationTiling(XY_FAST_COLOR_BLT::DESTINATION_TILING_TILE64);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_TILING_TILE64, cmd.getDestinationTiling());

    cmd.setDestinationX1CoordinateLeft(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationX1CoordinateLeft());
    cmd.setDestinationX1CoordinateLeft(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getDestinationX1CoordinateLeft());

    cmd.setDestinationY1CoordinateTop(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationY1CoordinateTop());
    cmd.setDestinationY1CoordinateTop(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getDestinationY1CoordinateTop());

    cmd.setDestinationX2CoordinateRight(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationX2CoordinateRight());
    cmd.setDestinationX2CoordinateRight(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getDestinationX2CoordinateRight());

    cmd.setDestinationY2CoordinateBottom(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationY2CoordinateBottom());
    cmd.setDestinationY2CoordinateBottom(0xffffu);
    EXPECT_EQ(0xffffu, cmd.getDestinationY2CoordinateBottom());

    cmd.setDestinationBaseAddress(0x0ull);
    EXPECT_EQ(0x0ull, cmd.getDestinationBaseAddress());
    cmd.setDestinationBaseAddress(0xffffffffffffffffull);
    EXPECT_EQ(0xffffffffffffffffull, cmd.getDestinationBaseAddress());

    cmd.setDestinationXOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationXOffset());
    cmd.setDestinationXOffset(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getDestinationXOffset());

    cmd.setDestinationYOffset(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationYOffset());
    cmd.setDestinationYOffset(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getDestinationYOffset());

    cmd.setDestinationTargetMemory(XY_FAST_COLOR_BLT::DESTINATION_TARGET_MEMORY_LOCAL_MEM);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_TARGET_MEMORY_LOCAL_MEM, cmd.getDestinationTargetMemory());

    cmd.setDestinationTargetMemory(XY_FAST_COLOR_BLT::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_TARGET_MEMORY_SYSTEM_MEM, cmd.getDestinationTargetMemory());

    uint32_t fillColorValueMin[4] = {0x0u, 0x0u, 0x0u, 0x0u};   // patched
    cmd.setFillColor(fillColorValueMin);                        // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillColorValue[0]); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillColorValue[1]); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillColorValue[2]); // patched
    EXPECT_EQ(0x0u, cmd.TheStructure.Common.FillColorValue[3]); // patched

    uint32_t fillColorValueMax[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu}; // patched
    cmd.setFillColor(fillColorValueMax);                                                  // patched
    EXPECT_EQ(0xffffffffu, cmd.TheStructure.Common.FillColorValue[0]);                    // patched
    EXPECT_EQ(0xffffffffu, cmd.TheStructure.Common.FillColorValue[1]);                    // patched
    EXPECT_EQ(0xffffffffu, cmd.TheStructure.Common.FillColorValue[2]);                    // patched
    EXPECT_EQ(0xffffffffu, cmd.TheStructure.Common.FillColorValue[3]);                    // patched

    cmd.setDestinationCompressionFormat(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationCompressionFormat());
    cmd.setDestinationCompressionFormat(0xfu);
    EXPECT_EQ(0xfu, cmd.getDestinationCompressionFormat());

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32X32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32X32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_422_8_P208);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_422_8_P208, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_420_8_SAMPLE_8X8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_420_8_SAMPLE_8X8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_411_8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_411_8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_422_8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_422_8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_VDI);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_VDI, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_NORMAL_SAMPLE_8X8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_NORMAL_SAMPLE_8X8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPUVY_SAMPLE_8X8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPUVY_SAMPLE_8X8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPUV_SAMPLE_8X8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPUV_SAMPLE_8X8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPY_SAMPLE_8X8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPY_SAMPLE_8X8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_FLOAT_LD);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_FLOAT_LD, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_420_16_SAMPLE_8X8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_420_16_SAMPLE_8X8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16B16_UNORM_SAMPLE_8X8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16B16_UNORM_SAMPLE_8X8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_Y16_UNORM_SAMPLE_8X8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_Y16_UNORM_SAMPLE_8X8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_Y32_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_Y32_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_SFIXED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32A32_SFIXED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64_PASSTHRU);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64_PASSTHRU, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_FLOAT_LD);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_FLOAT_LD, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_SFIXED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32B32_SFIXED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_X32_TYPELESS_G8X24_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_X32_TYPELESS_G8X24_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L32A32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L32A32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16X16_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16X16_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16X16_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16X16_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A32X32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A32X32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L32X32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L32X32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I32X32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I32X32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16A16_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_FLOAT_LD);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_FLOAT_LD, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS_LD);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS_LD, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_SFIXED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32G32_SFIXED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64_PASSTHRU);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64_PASSTHRU, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8G8R8A8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8G8R8A8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8G8R8A8_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8G8R8A8_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10_SNORM_A2_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10_SNORM_A2_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_UNORM_SAMPLE_8X8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_UNORM_SAMPLE_8X8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R11G11B10_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R11G11B10_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10_FLOAT_A2_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10_FLOAT_A2_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R24_UNORM_X8_TYPELESS);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R24_UNORM_X8_TYPELESS, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_X24_TYPELESS_G8_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_X24_TYPELESS_G8_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_FLOAT_LD);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_FLOAT_LD, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R24_UNORM_X8_TYPELESS_LD);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R24_UNORM_X8_TYPELESS_LD, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L32_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L32_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A32_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A32_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L16A16_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L16A16_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I24X8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I24X8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L24X8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L24X8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A24X8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A24X8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A32_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A32_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_X8B8_UNORM_G8R8_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_X8B8_UNORM_G8R8_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A8X8_UNORM_G8R8_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A8X8_UNORM_G8R8_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8X8_UNORM_G8R8_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8X8_UNORM_G8R8_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8G8R8X8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8G8R8X8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8G8R8X8_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B8G8R8X8_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8X8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8X8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8X8_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8X8_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R9G9B9E5_SHAREDEXP);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R9G9B9E5_SHAREDEXP, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10X2_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10X2_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L16A16_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L16A16_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10X2_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10X2_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8B8G8A8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8B8G8A8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_SINT_NOA);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_SINT_NOA, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UINT_NOA);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UINT_NOA, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_YUV);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_YUV, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_SNCK);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_SNCK, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_NOA);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8A8_UNORM_NOA, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G6R5_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G6R5_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G6R5_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G6R5_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G5R5A1_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G5R5A1_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G5R5A1_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G5R5A1_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B4G4R4A4_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B4G4R4A4_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B4G4R4A4_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B4G4R4A4_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A8P8_UNORM_PALETTE0);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A8P8_UNORM_PALETTE0, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A8P8_UNORM_PALETTE1);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A8P8_UNORM_PALETTE1, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I16_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I16_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L16_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L16_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A16_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A16_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8A8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8A8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I16_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I16_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L16_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L16_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A16_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A16_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8A8_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8A8_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R5G5_SNORM_B6_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R5G5_SNORM_B6_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G5R5X1_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G5R5X1_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G5R5X1_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B5G5R5X1_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_SNORM_DX9);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8_SNORM_DX9, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_FLOAT_DX9);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16_FLOAT_DX9, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P8A8_UNORM_PALETTE0);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P8A8_UNORM_PALETTE0, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P8A8_UNORM_PALETTE1);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P8A8_UNORM_PALETTE1, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A1B5G5R5_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A1B5G5R5_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A4B4G4R4_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A4B4G4R4_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8A8_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8A8_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8A8_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8A8_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P4A4_UNORM_PALETTE0);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P4A4_UNORM_PALETTE0, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A4P4_UNORM_PALETTE0);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A4P4_UNORM_PALETTE0, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P8_UNORM_PALETTE0);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P8_UNORM_PALETTE0, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P8_UNORM_PALETTE1);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P8_UNORM_PALETTE1, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P4A4_UNORM_PALETTE1);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P4A4_UNORM_PALETTE1, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A4P4_UNORM_PALETTE1);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_A4P4_UNORM_PALETTE1, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_Y8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_Y8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_L8_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I8_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I8_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I8_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_I8_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_DXT1_RGB_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_DXT1_RGB_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R1_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R1_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_NORMAL);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_NORMAL, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPUVY);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPUVY, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P2_UNORM_PALETTE0);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P2_UNORM_PALETTE0, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P2_UNORM_PALETTE1);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_P2_UNORM_PALETTE1, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC1_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC1_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC2_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC2_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC3_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC3_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC4_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC4_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC5_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC5_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC1_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC1_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC2_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC2_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC3_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC3_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_MONO8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_MONO8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPUV);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPUV, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPY);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_YCRCB_SWAPY, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_DXT1_RGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_DXT1_RGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64B64A64_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64B64A64_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64B64_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64B64_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC4_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC4_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC5_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC5_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_FLOAT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_FLOAT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8B8_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8B8_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC6H_SF16);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC6H_SF16, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC7_UNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC7_UNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC7_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC7_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC6H_UF16);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_BC6H_UF16, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_420_8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_420_8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_420_16);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_PLANAR_420_16, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_UNORM_SRGB);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_UNORM_SRGB, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC1_RGB8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC1_RGB8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_RGB8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_RGB8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_EAC_R11);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_EAC_R11, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_EAC_RG11);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_EAC_RG11, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_EAC_SIGNED_R11);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_EAC_SIGNED_R11, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_EAC_SIGNED_RG11);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_EAC_SIGNED_RG11, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_SRGB8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_SRGB8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R16G16B16_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_SFIXED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R32_SFIXED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R10G10B10A2_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_SNORM);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_SNORM, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_USCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_USCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_SSCALED);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_SSCALED, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_B10G10R10A2_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64B64A64_PASSTHRU);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64B64A64_PASSTHRU, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64B64_PASSTHRU);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R64G64B64_PASSTHRU, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_RGB8_PTA);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_RGB8_PTA, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_SRGB8_PTA);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_SRGB8_PTA, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_EAC_RGBA8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_EAC_RGBA8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_EAC_SRGB8_A8);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_ETC2_EAC_SRGB8_A8, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_UINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_UINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_SINT);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_R8G8B8_SINT, cmd.getSurfaceFormat()); // patched

    cmd.setSurfaceFormat(XY_FAST_COLOR_BLT::SURFACE_FORMAT_RAW);              // patched
    EXPECT_EQ(XY_FAST_COLOR_BLT::SURFACE_FORMAT_RAW, cmd.getSurfaceFormat()); // patched

    cmd.setAuxSpecialOperationsMode(XY_FAST_COLOR_BLT::AUX_SPECIAL_OPERATIONS_MODE_FAST_CLEAR_HW_FORMAT_CONVERSION);
    EXPECT_EQ(XY_FAST_COLOR_BLT::AUX_SPECIAL_OPERATIONS_MODE_FAST_CLEAR_HW_FORMAT_CONVERSION, cmd.getAuxSpecialOperationsMode());

    cmd.setAuxSpecialOperationsMode(XY_FAST_COLOR_BLT::AUX_SPECIAL_OPERATIONS_MODE_FAST_CLEAR_BYPASS_HW_FORMAT_CONVERSION);
    EXPECT_EQ(XY_FAST_COLOR_BLT::AUX_SPECIAL_OPERATIONS_MODE_FAST_CLEAR_BYPASS_HW_FORMAT_CONVERSION, cmd.getAuxSpecialOperationsMode());

    cmd.setAuxSpecialOperationsMode(XY_FAST_COLOR_BLT::AUX_SPECIAL_OPERATIONS_MODE_FORCE_UNCOMPRESS);
    EXPECT_EQ(XY_FAST_COLOR_BLT::AUX_SPECIAL_OPERATIONS_MODE_FORCE_UNCOMPRESS, cmd.getAuxSpecialOperationsMode());

    cmd.setDestinationSurfaceHeight(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationSurfaceHeight());
    cmd.setDestinationSurfaceHeight(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getDestinationSurfaceHeight());

    cmd.setDestinationSurfaceWidth(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationSurfaceWidth());
    cmd.setDestinationSurfaceWidth(0x3fffu);
    EXPECT_EQ(0x3fffu, cmd.getDestinationSurfaceWidth());

    cmd.setDestinationSurfaceType(XY_FAST_COLOR_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_1D);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_1D, cmd.getDestinationSurfaceType());

    cmd.setDestinationSurfaceType(XY_FAST_COLOR_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_2D);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_2D, cmd.getDestinationSurfaceType());

    cmd.setDestinationSurfaceType(XY_FAST_COLOR_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_3D);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_3D, cmd.getDestinationSurfaceType());

    cmd.setDestinationSurfaceType(XY_FAST_COLOR_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_CUBE);
    EXPECT_EQ(XY_FAST_COLOR_BLT::DESTINATION_SURFACE_TYPE_SURFTYPE_CUBE, cmd.getDestinationSurfaceType());

    cmd.setDestinationLod(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationLod());
    cmd.setDestinationLod(0xfu);
    EXPECT_EQ(0xfu, cmd.getDestinationLod());

    cmd.setDestinationSurfaceQpitch(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationSurfaceQpitch());
    cmd.setDestinationSurfaceQpitch(0x7fffu);
    EXPECT_EQ(0x7fffu, cmd.getDestinationSurfaceQpitch());

    cmd.setDestinationSurfaceDepth(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationSurfaceDepth());
    cmd.setDestinationSurfaceDepth(0x7ffu);
    EXPECT_EQ(0x7ffu, cmd.getDestinationSurfaceDepth());

    cmd.setDestinationHorizontalAlign(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationHorizontalAlign());
    cmd.setDestinationHorizontalAlign(0x3u);
    EXPECT_EQ(0x3u, cmd.getDestinationHorizontalAlign());

    cmd.setDestinationVerticalAlign(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationVerticalAlign());
    cmd.setDestinationVerticalAlign(0x3u);
    EXPECT_EQ(0x3u, cmd.getDestinationVerticalAlign());

    cmd.setDestinationMipTailStartLod(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationMipTailStartLod());
    cmd.setDestinationMipTailStartLod(0xfu);
    EXPECT_EQ(0xfu, cmd.getDestinationMipTailStartLod());

    cmd.setDestinationDepthStencilResource(0x0u);
    EXPECT_EQ(0x0u, cmd.getDestinationDepthStencilResource());
    cmd.setDestinationDepthStencilResource(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationDepthStencilResource());

    cmd.setDestinationArrayIndex(0x1u);
    EXPECT_EQ(0x1u, cmd.getDestinationArrayIndex());
    cmd.setDestinationArrayIndex(0x7ffu);
    EXPECT_EQ(0x7ffu, cmd.getDestinationArrayIndex());
}
