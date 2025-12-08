<!---

Copyright (C) 2025 Intel Corporation

SPDX-License-Identifier: MIT

-->

# RAS Error Threshold Management Extension

* [Overview](#Overview)
* [Definitions](#Definitions)

# Overview

The following new experimental APIs for RAS error threshold management in the Level Zero sysman interface are introduced.

zesIntelRasGetSupportedCategoriesExp
    - This API is designed to fetch the number of error categories supported before the call to the get and set threshold APIs.
    - Rather than calling zesRasGetStateExp() to fetch the number of error categories (where the RAS state has no relation to the RAS config parameters), it is more convenient for the user to call this API which is designed for this specific purpose only.

zesIntelRasGetConfigExp
    - This API is designed to retrieve the RAS error thresholds for the given RAS error categories.

zesIntelRasSetConfigExp
    - This API is designed to set the RAS error thresholds for the given RAS error categories.

zesIntelRasGetStateExp
    - The existing API zesRasGetStateExp() fetches the ras states of the error categories according to the number of the error categories (pCount). If pCount < maxSupportedCategories, only the first pCount number of error categories will be returned. There is no option to fetch the error states for the remaining errorCategories other than having the option to increase the pCount value.
    - This API is designed to fetch the errorCount for the required number of error categories.

# Definitions

```cpp
///////////////////////////////////////////////////////////////////////////////
/// @brief Ras Config Driver Experimental Extension Structure
typedef struct _zes_intel_ras_config_exp_t {
    zes_structure_type_ext_t stype;        ///< [in] type of this structure
    void *pNext;                           ///< [in][optional] must be null or a pointer to an extension-specific
                                           ///< structure (i.e. contains stype and pNext).
    zes_ras_error_category_exp_t category; ///< [in] RAS error category
    uint64_t threshold;                    ///< [in][out] Error count threshold to trigger RAS action
                                           ///< [in] when calling zesIntelRasSetConfigExp
                                           ///< [out] when calling zesIntelRasGetConfigExp
} zes_intel_ras_config_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Ras State Driver Experimental Extension Structure
typedef struct _zes_intel_ras_state_exp_t {
    zes_structure_type_ext_t stype;        ///< [in] type of this structure
    void *pNext;                           ///< [in][optional] must be null or a pointer to an extension-specific
                                           ///< structure (i.e. contains stype and pNext).
    zes_ras_error_category_exp_t category; ///< [in] Error category.
    uint64_t errorCounter;                 ///< [out] Current value of RAS counter for specific error category.
} zes_intel_ras_state_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief RAS Get Supported Driver Experimental Extension Error Categories
ze_result_t ZE_APICALL zesIntelRasGetSupportedCategoriesExp(
    zes_ras_handle_t hRas,                    ///< [in] Handle for the RAS module.
    uint32_t *pCount,                         ///< [in,out] pointer to the number of categories.
                                              ///< if count is zero, then the driver shall update the value with the
                                              ///< total number of categories supported.
                                              ///< if count is non-zero, then driver shall only retrieve that number
                                              ///< of categories.
    zes_ras_error_category_exp_t *pCategories ///< [in][out][optional] array of category types.
                                              ///< if count is less than the number of categories supported, then
                                              ///< driver shall only retrieve that number of categories.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief RAS Get Driver Experimental Extension Config
ze_result_t ZE_APICALL zesIntelRasGetConfigExp(
    zes_ras_handle_t hRas,              ///< [in] Handle for the RAS module.
    const uint32_t count,               ///< [in] Number of elements in the pConfig array.
    zes_intel_ras_config_exp_t *pConfig ///< [in][out] Array of RAS configurations to get.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief RAS Set Driver Experimental Extension Config
ze_result_t ZE_APICALL zesIntelRasSetConfigExp(
    zes_ras_handle_t hRas,                    ///< [in] Handle for the RAS module.
    const uint32_t count,                     ///< [in] Number of elements in the pConfig array.
    const zes_intel_ras_config_exp_t *pConfig ///< [in] Array of RAS configurations to set.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Ras Get Driver Experimental Extension State
ze_result_t ZE_APICALL zesIntelRasGetStateExp(
    zes_ras_handle_t hRas,            ///< [in] Handle for the RAS module.
    const uint32_t count,             ///< [in] Number of elements in the pState array.
    zes_intel_ras_state_exp_t *pState ///< [in][out] Array of RAS error states.
);

```