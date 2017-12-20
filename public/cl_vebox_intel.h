/*
 * Copyright (c) 2017, Intel Corporation
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
#ifndef __CL_EXT_VEBOX_INTEL_H
#define __CL_EXT_VEBOX_INTEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <CL/cl.h>

/***************************************
* cl_intel_video_enhancement extension *
****************************************/
#define CL_ACCELERATOR_TYPE_VE_INTEL 0x9
#define CL_DEVICE_VE_VERSION_INTEL 0x4160
#define CL_DEVICE_VE_ENGINE_COUNT_INTEL 0x4161
#define CL_DEVICE_VE_COLOR_PIPE_VERSION_INTEL 0x416A
#define CL_DEVICE_VE_CAMERA_PIPE_VERSION_INTEL 0x4177
#define CL_VE_VERSION_VER_1_INTEL 0x1
#define CL_VE_VERSION_VER_2_INTEL 0x2
#define CL_VE_VERSION_VER_3_INTEL 0x3
#define CL_QUEUE_VE_ENABLE_INTEL 0x4162
// VE Attributes
#define CL_VE_ACCELERATOR_ATTRIB_DENOISE_INTEL 0x4163
#define CL_VE_ACCELERATOR_ATTRIB_DEINTERLACE_INTEL 0x4164
#define CL_VE_ACCELERATOR_ATTRIB_HPC_INTEL 0x4165
#define CL_VE_ACCELERATOR_ATTRIB_STD_STE_INTEL 0x416B
#define CL_VE_ACCELERATOR_ATTRIB_GAMUT_COMP_INTEL 0x416C
#define CL_VE_ACCELERATOR_ATTRIB_GECC_INTEL 0x416D
#define CL_VE_ACCELERATOR_ATTRIB_ACE_INTEL 0x416E
#define CL_VE_ACCELERATOR_ATTRIB_ACE_ADVANCED_INTEL 0x416F
#define CL_VE_ACCELERATOR_ATTRIB_TCC_INTEL 0x4170
#define CL_VE_ACCELERATOR_ATTRIB_PROC_AMP_INTEL 0x4171
#define CL_VE_ACCELERATOR_ATTRIB_BACK_END_CSC_INTEL 0x4172
#define CL_VE_ACCELERATOR_ATTRIB_AOI_ALPHA_INTEL 0x4173
#define CL_VE_ACCELERATOR_ATTRIB_CCM_INTEL 0x4174
#define CL_VE_ACCELERATOR_ATTRIB_FWD_GAMMA_CORRECTION_INTEL 0x4175
#define CL_VE_ACCELERATOR_ATTRIB_FRONT_END_CSC_INTEL 0x4176
#define CL_VE_ACCELERATOR_ATTRIB_BLC_INTEL 0x4178
#define CL_VE_ACCELERATOR_ATTRIB_DEMOSAIC_INTEL 0x4179
#define CL_VE_ACCELERATOR_ATTRIB_WBC_INTEL 0x417A
#define CL_VE_ACCELERATOR_ATTRIB_VIGNETTE_INTEL 0x417B

// VE Statistics
#define CL_VE_ACCELERATOR_HISTOGRAMS_INTEL 0x4166
#define CL_VE_ACCELERATOR_STATISTICS_INTEL 0x4167
#define CL_VE_ACCELERATOR_STMM_INPUT_INTEL 0x4168
#define CL_VE_ACCELERATOR_STMM_OUTPUT_INTEL 0x4169

// Denoise Control
#define CL_VE_DENOISE_FACTOR_MAX_INTEL 64
#define CL_VE_DENOISE_FACTOR_MIN_INTEL 0
#define CL_VE_DENOISE_FACTOR_DEFAULT_INTEL 32

// Hot Pixel Correction ranges
#define CL_VE_HPC_THRESHOLD_MAX_INTEL 255
#define CL_VE_HPC_THRESHOLD_MIN_INTEL 0
#define CL_VE_HPC_THRESHOLD_DEFAULT_INTEL 0
#define CL_VE_HPC_PIXEL_COUNT_MAX_INTEL 8
#define CL_VE_HPC_PIXEL_COUNT_MIN_INTEL 0
#define CL_VE_HPC_PIXEL_COUNT_DEFAULT_INTEL 0

// Skin tone detection/enhancement ranges
#define CL_VE_STE_FACTOR_MIN_INTEL 0
#define CL_VE_STE_FACTOR_MAX_INTEL 10
#define CL_VE_STE_FACTOR_DEFAULT_INTEL 3

// Constants for gamut compression scaling factors
#define CL_VE_GAMUT_SCALING_FACTOR_MAX_INTEL 4.0f
#define CL_VE_GAMUT_SCALING_FACTOR_MIN_INTEL 0.0f
#define CL_VE_GAMUT_SCALING_FACTOR_DEFAULT_INTEL 0.0f
#define CL_VE_GAMUT_CHROMATICITY_CONTROLS_MAX_INTEL 1.0f
#define CL_VE_GAMUT_CHROMATICITY_CONTROLS_MIN_INTEL 0.0f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_RX_DEFAULT_INTEL 0.576f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_GX_DEFAULT_INTEL 0.331f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_BX_DEFAULT_INTEL 0.143f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_RY_DEFAULT_INTEL 0.343f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_GY_DEFAULT_INTEL 0.555f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_BY_DEFAULT_INTEL 0.104f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_RX_SRGB_INTEL 0.640f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_GX_SRGB_INTEL 0.300f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_BX_SRGB_INTEL 0.150f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_RY_SRGB_INTEL 0.330f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_GY_SRGB_INTEL 0.600f
#define CL_VE_GAMUT_CHROMATICITY_CONTRL_BY_SRGB_INTEL 0.060f

// Constants for gamut expansion / color correction
#define CL_VE_GECC_PIECE_COUNT_INTEL 11
#define CL_VE_GECC_TX_COEFFICIENTS_MIN_INTEL -4.0f
#define CL_VE_GECC_TX_COEFFICIENTS_MAX_INTEL 4.0f
#define CL_VE_GECC_TX_COEFF_C0_DEFAULT_INTEL 0.681f
#define CL_VE_GECC_TX_COEFF_C1_DEFAULT_INTEL 0.278f
#define CL_VE_GECC_TX_COEFF_C2_DEFAULT_INTEL 0.008f
#define CL_VE_GECC_TX_COEFF_C3_DEFAULT_INTEL 0.017f
#define CL_VE_GECC_TX_COEFF_C4_DEFAULT_INTEL 0.894f
#define CL_VE_GECC_TX_COEFF_C5_DEFAULT_INTEL -0.012f
#define CL_VE_GECC_TX_COEFF_C6_DEFAULT_INTEL -0.002f
#define CL_VE_GECC_TX_COEFF_C7_DEFAULT_INTEL 0.041f
#define CL_VE_GECC_TX_COEFF_C8_DEFAULT_INTEL 0.838f
#define CL_VE_GECC_TX_OFFSET_IN_MIN_INTEL -16384
#define CL_VE_GECC_TX_OFFSET_IN_MAX_INTEL 16383
#define CL_VE_GECC_TX_OFFSET_OUT_MIN_INTEL -4.0f
#define CL_VE_GECC_TX_OFFSET_OUT_MAX_INTEL 4.0f

// AOI Parameter defaults
#define CL_VE_AOI_RANGE_DEFAULT_INTEL 0
#define CL_VE_AOI_ALPHA_DEFAULT_INTEL 0

// CCM Config Parameter Range
#define CL_VE_CCM_COEFFICIENTS_MIN_INTEL -16.0f
#define CL_VE_CCM_COEFFICIENTS_MAX_INTEL 16.0f
#define CL_VE_CCM_COEFFICIENTS_DEFAULT_INTEL 0.0f

// CSC Config Parameter Range
#define CL_VE_CSC_OFFSET_MIN_INTEL -256.0f
#define CL_VE_CSC_OFFSET_MAX_INTEL 256.0f
#define CL_VE_CSC_COEFF_MIN_INTEL -4.0f
#define CL_VE_CSC_COEFF_MAX_INTEL 4.0f

// Constants for specific color spaces
#define CL_VE_GAMUT_CS_BT601_INTEL 0x0
#define CL_VE_GAMUT_CS_BT709_INTEL 0x1
#define CL_VE_GAMUT_CS_XVYCC601_INTEL 0x2
#define CL_VE_GAMUT_CS_XVYCC709_INTEL 0x3

// LACE/ACE Control
#define CL_VE_ACE_PIECE_COUNT_INTEL 10
#define CL_VE_ACE_LEVEL_MIN_INTEL 0
#define CL_VE_ACE_LEVEL_MAX_INTEL 9
#define CL_VE_ACE_LEVEL_DEFAULT_INTEL 5
#define CL_VE_ACE_STRENGTH_MIN_INTEL 0
#define CL_VE_ACE_STRENGTH_MAX_INTEL 6
#define CL_VE_ACE_STRENGTH_DEFAULT_INTEL 1
#define CL_VE_ACE_SKIN_THRESHOLD_MIN_INTEL 1
#define CL_VE_ACE_SKIN_THRESHOLD_MAX_INTEL 31
#define CL_VE_ACE_SKIN_THRESHOLD_DEFAULT_INTEL 26

// TCC Parameter Range
#define CL_VE_TCC_MIN_INTEL 0
#define CL_VE_TCC_MAX_INTEL 255
#define CL_VE_TCC_DEFAULT_INTEL 220

// Proc-Amp Ranges
#define CL_VE_PROCAMP_BRIGHTNESS_MIN_INTEL -100.0f
#define CL_VE_PROCAMP_BRIGHTNESS_MAX_INTEL 100.0f
#define CL_VE_PROCAMP_BRIGHTNESS_DEFAULT_INTEL 0.0f

#define CL_VE_PROCAMP_CONTRAST_MIN_INTEL 0.0f
#define CL_VE_PROCAMP_CONTRAST_MAX_INTEL 15.0f
#define CL_VE_PROCAMP_CONTRAST_DEFAULT_INTEL 1.0f

#define CL_VE_PROCAMP_HUE_MIN_INTEL -180.0f
#define CL_VE_PROCAMP_HUE_MAX_INTEL 180.0f
#define CL_VE_PROCAMP_HUE_DEFAULT_INTEL 0.0f

#define CL_VE_PROCAMP_SATURATION_MIN_INTEL 0.0f
#define CL_VE_PROCAMP_SATURATION_MAX_INTEL 8.0f
#define CL_VE_PROCAMP_SATURATION_DEFAULT_INTEL 1.0f

// BLC Parameter Range
#define CL_VE_BLC_MIN_INTEL -65536
#define CL_VE_BLC_MAX_INTEL 65535
#define CL_VE_BLC_DEFAULT_INTEL 0

// WBC Parameter Range
#define CL_VE_WBC_MIN_INTEL 0.0f
#define CL_VE_WBC_MAX_INTEL 16.0f
#define CL_VE_WBC_DEFAULT_INTEL 0.0f

// FGC Parameter Range
#define CL_VE_FGC_DEFAULT_INTEL 0

// Video enhancement kernel flags
#define CL_VE_FIRST_FRAME_INTEL (1 << 0)
#define CL_VE_RESET_DN_HISTORY_INTEL (1 << 1)
#define CL_VE_RESET_DI_HISTORY_INTEL (1 << 2)
#define CL_VE_RESET_ACE_HISTORY_INTEL (1 << 3)
#define CL_VE_RESET_STE_HISTORY_INTEL (1 << 4)
#define CL_VE_GENERATE_LACE_HISTOGRAM_128_BINS_INTEL (1 << 5)
#define CL_VE_GENERATE_LACE_HISTOGRAM_256_BINS_INTEL (1 << 6)

// Bayer pattern controls
#define CL_VE_BAYER_PATTERN_FORMAT_8BIT_INTEL 0x0
#define CL_VE_BAYER_PATTERN_FORMAT_16BIT_INTEL 0x1
#define CL_VE_BAYER_PATTERN_OFFSET_BG_INTEL 0x0
#define CL_VE_BAYER_PATTERN_OFFSET_RG_INTEL 0x1
#define CL_VE_BAYER_PATTERN_OFFSET_GR_INTEL 0x2
#define CL_VE_BAYER_PATTERN_OFFSET_GB_INTEL 0x3

// Default color-space conversion coefficients
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_OFFSET_IN_0 (-16.0f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_OFFSET_IN_1 (-128.0f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_OFFSET_IN_2 (-128.0f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_TX_COEFF_0_0 (1.164f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_TX_COEFF_0_1 (0.0f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_TX_COEFF_0_2 (1.596f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_TX_COEFF_1_0 (1.164f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_TX_COEFF_1_1 (-0.392f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_TX_COEFF_1_2 (-0.813f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_TX_COEFF_2_0 (1.164f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_TX_COEFF_2_1 (2.017f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_TX_COEFF_2_2 (0.0f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_OFFSET_OUT_0 (0.0f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_OFFSET_OUT_1 (0.0f)
#define CL_VE_CSC_DEFAULT_YUV_TO_RGB_OFFSET_OUT_2 (0.0f)

#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_OFFSET_IN_0 (0.0f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_OFFSET_IN_1 (0.0f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_OFFSET_IN_2 (0.0f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_TX_COEFF_0_0 (0.257f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_TX_COEFF_0_1 (0.504f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_TX_COEFF_0_2 (0.098f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_TX_COEFF_1_0 (-0.148f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_TX_COEFF_1_1 (-0.291f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_TX_COEFF_1_2 (0.439f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_TX_COEFF_2_0 (0.439f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_TX_COEFF_2_1 (-0.368f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_TX_COEFF_2_2 (-0.071f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_OFFSET_OUT_0 (16.0f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_OFFSET_OUT_1 (128.0f)
#define CL_VE_CSC_DEFAULT_RGB_TO_YUV_OFFSET_OUT_2 (128.0f)

// Forward Gamma Correction controls
#define CL_VE_FWD_GAMMA_SEGMENT_COUNT 64

typedef cl_uint cl_ve_accelerator_attrib_id;

typedef struct _cl_ve_dn_attrib_intel {
    cl_bool enable_luma;
    cl_bool enable_chroma;
    cl_bool auto_detect;
    cl_uint denoise_factor;
} cl_ve_dn_attrib_intel;

typedef struct _cl_ve_di_attrib_intel {
    cl_bool enabled;
    cl_bool motion_compensation_enabled;
    cl_bool top_first;
} cl_ve_di_attrib_intel;

typedef struct _cl_ve_std_ste_attrib_intel {
    cl_bool enabled;
    cl_uint ste_factor;
    cl_bool write_std_decisions_only;
} cl_ve_std_ste_attrib_intel;

typedef struct _cl_ve_gamut_comp_attrib_intel {
    cl_bool enabled;
    cl_bool advanced_mode_enable;
    cl_uint src_color_space;
    cl_float basic_mode_scaling_factor;
    cl_float display_rgb_x[3];
    cl_float display_rgb_y[3];
} cl_ve_gamut_comp_attrib_intel;

typedef struct _cl_ve_gecc_attrib_intel {
    cl_bool enabled;
    cl_bool use_advanced_mode;
    cl_float matrix[3][3];
    cl_int offset_in[3];
    cl_float offset_out[3];
    cl_uchar gamma_correction_in[CL_VE_GECC_PIECE_COUNT_INTEL];
    cl_uchar gamma_correction_out[CL_VE_GECC_PIECE_COUNT_INTEL];
    cl_uchar inv_gamma_correction_in[CL_VE_GECC_PIECE_COUNT_INTEL];
    cl_uchar inv_gamma_correction_out[CL_VE_GECC_PIECE_COUNT_INTEL];
} cl_ve_gecc_attrib_intel;

typedef struct _cl_ve_ace_attrib_intel {
    cl_bool enabled;
    cl_uchar skin_threshold;
    cl_uint level;
    cl_uint strength;
} cl_ve_ace_attrib_intel;

typedef struct _cl_ve_ace_advanced_attrib_intel {
    cl_bool enabled;
    cl_uchar luma_min;
    cl_uchar luma_max;
    cl_uchar luma_in[CL_VE_ACE_PIECE_COUNT_INTEL];
    cl_uchar luma_out[CL_VE_ACE_PIECE_COUNT_INTEL];
} cl_ve_ace_advanced_attrib_intel;

typedef struct _cl_ve_tcc_attrib_intel {
    cl_bool enabled;
    cl_uchar red_saturation;
    cl_uchar green_saturation;
    cl_uchar blue_saturation;
    cl_uchar cyan_saturation;
    cl_uchar magenta_saturation;
    cl_uchar yellow_saturation;
} cl_ve_tcc_attrib_intel;

typedef struct _cl_ve_procamp_attrib_intel {
    cl_bool enabled;
    cl_float brightness;
    cl_float contrast;
    cl_float hue;
    cl_float saturation;
} cl_ve_procamp_attrib_intel;

typedef struct _cl_ve_becsc_attrib_intel {
    cl_bool enabled;
    cl_bool yuv_channel_swap;
    cl_float offset_in[3];
    cl_float matrix[3][3];
    cl_float offset_out[3];
} cl_ve_becsc_attrib_intel;

typedef struct _cl_ve_aoi_alpha_attrib_intel {
    cl_bool aoi_enabled;
    cl_uint x_min;
    cl_uint x_max;
    cl_uint y_min;
    cl_uint y_max;
    cl_bool alpha_enable;
    cl_ushort alpha_value;
} cl_ve_aoi_alpha_attrib_intel;

typedef struct _cl_ve_hpc_attrib_intel {
    cl_bool enabled;
    cl_uchar threshold;
    cl_uchar count;
} cl_ve_hpc_attrib_intel;

typedef struct _cl_ve_blc_attrib_intel {
    cl_bool enabled;
    cl_int black_point_offset_red;
    cl_int black_point_offset_green_top;
    cl_int black_point_offset_green_bottom;
    cl_int black_point_offset_blue;
} cl_ve_blc_attrib_intel;

typedef struct _cl_ve_demosaic_attrib_intel {
    cl_uint input_width;
    cl_uint input_height;
    cl_uint input_stride;
    cl_uint bayer_pattern_offset;
    cl_uint bayer_pattern_format;
} cl_ve_demosaic_attrib_intel;

typedef struct _cl_ve_wbc_attrib_intel {
    cl_bool enabled;
    cl_float white_balance_red_correction;
    cl_float white_balance_green_top_correction;
    cl_float white_balance_green_bottom_correction;
    cl_float white_balance_blue_correction;
} cl_ve_wbc_attrib_intel;

typedef struct _cl_ve_vignette_attrib_intel {
    cl_bool enabled;
} cl_ve_vignette_attrib_intel;

typedef struct _cl_ve_ccm_attrib_intel {
    cl_bool enabled;
    cl_float matrix[3][3];
} cl_ve_ccm_attrib_intel;

typedef struct _cl_ve_fgc_attrib_intel {
    cl_bool enabled;
    cl_ushort pixel_value[CL_VE_FWD_GAMMA_SEGMENT_COUNT];
    cl_ushort red_channel_corrected_value[CL_VE_FWD_GAMMA_SEGMENT_COUNT];
    cl_ushort green_channel_corrected_value[CL_VE_FWD_GAMMA_SEGMENT_COUNT];
    cl_ushort blue_channel_corrected_value[CL_VE_FWD_GAMMA_SEGMENT_COUNT];
} cl_ve_fgc_attrib_intel;

typedef struct _cl_ve_fecsc_attrib_intel {
    cl_bool enabled;
    cl_float offset_in[3];
    cl_float matrix[3][3];
    cl_float offset_out[3];
} cl_ve_fecsc_attrib_intel;

typedef struct _cl_ve_attrib_desc_intel {
    cl_ve_accelerator_attrib_id attrib_id;
    void *attrib_data;
} cl_ve_attrib_desc_intel;

typedef struct _cl_ve_desc_intel {
    cl_uint attrib_count;
    cl_ve_attrib_desc_intel *attribs;
} cl_ve_desc_intel;

typedef struct _cl_vignette_format_intel {
    cl_ushort Red;
    cl_ushort GreenTop;
    cl_ushort Blue;
    cl_ushort GreenBottom;
} cl_vignette_format_intel;

#ifdef __cplusplus
}
#endif

#endif /* __CL_EXT_VEBOX_INTEL_H */
