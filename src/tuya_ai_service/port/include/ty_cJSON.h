/**
 * @file ty_cJSON.h
 * @brief ty_cJSON module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TY_CJSON_H__
#define __TY_CJSON_H__

#include "tuya_cloud_types.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define ty_cJSON_True  cJSON_True
#define ty_cJSON_False cJSON_False

#define ty_cJSON_CreateString      cJSON_CreateString
#define ty_cJSON_CreateArray       cJSON_CreateArray
#define ty_cJSON_CreateObject      cJSON_CreateObject
#define ty_cJSON_Parse             cJSON_Parse
#define ty_cJSON_AddStringToObject cJSON_AddStringToObject
#define ty_cJSON_AddNumberToObject cJSON_AddNumberToObject
#define ty_cJSON_AddItemToArray    cJSON_AddItemToArray
#define ty_cJSON_AddBoolToObject   cJSON_AddBoolToObject
#define ty_cJSON_AddItemToObject   cJSON_AddItemToObject
#define ty_cJSON_AddArrayToObject  cJSON_AddArrayToObject
#define ty_cJSON_Delete            cJSON_Delete
#define ty_cJSON_PrintUnformatted  cJSON_PrintUnformatted
#define ty_cJSON_Duplicate         cJSON_Duplicate

#define ty_cJSON_IsNumber       cJSON_IsNumber
#define ty_cJSON_IsBool         cJSON_IsBool
#define ty_cJSON_IsTrue         cJSON_IsTrue
#define ty_cJSON_IsObject       cJSON_IsObject
#define ty_cJSON_IsString       cJSON_IsString
#define ty_cJSON_GetNumberValue cJSON_GetNumberValue
#define ty_cJSON_GetObjectItem  cJSON_GetObjectItem
#define ty_cJSON_GetArraySize   cJSON_GetArraySize
#define ty_cJSON_GetArrayItem   cJSON_GetArrayItem
#define ty_cJSON_GetStringValue cJSON_GetStringValue
#define ty_cJSON_FreeBuffer     cJSON_free

/* Macro for iterating over an array or object */
#define ty_cJSON_ArrayForEach(element, array)                                                                          \
    for (element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef cJSON ty_cJSON;

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __TY_CJSON_H__ */
