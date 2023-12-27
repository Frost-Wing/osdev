/**
 * @file glvec.h
 * @author GAMINGNOOBdev (https://github.com/GAMINGNOOBdev)
 * @brief Vector types for use with OpenGL functions
 * @version 0.1
 * @date 2023-10-24
 * 
 * @copyright Copyright (c) GAMINGNOOBdev 2023
 */
#ifndef __OPENGL__GLVEC_H_
#define __OPENGL__GLVEC_H_ 1

#include <stdint.h>

/**
 * @brief 2D Integer Vector
 * 
 * @note To create a new `ivec2`: @code (ivec2){X, Y} @endcode
 */
typedef struct
{
    int64_t x;
    int64_t y;
} ivec2;

/**
 * @brief 2D Unsigned Integer Vector
 * 
 * @note To create a new `uvec2`: @code (uvec2){X, Y} @endcode
 */
typedef struct
{
    uint64_t x;
    uint64_t y;
} uvec2;

/**
 * @brief 2D Vector
 * 
 * @note To create a new `vec2`: @code (vec2){X, Y} @endcode
 */
typedef struct
{
    float x;
    float y;
} vec2;

/**
 * @brief 3D Integer Vector
 * 
 * @note To create a new `ivec3`: @code (ivec3){X, Y, Z} @endcode
 */
typedef struct
{
    int64_t x;
    int64_t y;
    int64_t z;
} ivec3;

/**
 * @brief 3D Unsigned Integer Vector
 * 
 * @note To create a new `uvec3`: @code (uvec3){X, Y, Z} @endcode
 */
typedef struct
{
    uint64_t x;
    uint64_t y;
    uint64_t z;
} uvec3;

/**
 * @brief 3D Vector
 * 
 * @note To create a new `vec3`: @code (vec3){X, Y, Z} @endcode
 */
typedef struct
{
    float x;
    float y;
    float z;
} vec3;

/**
 * @brief 4D Integer Vector
 * 
 * @note To create a new `ivec4`: @code (ivec4){X, Y, Z, W} @endcode
 */
typedef struct
{
    int64_t x;
    int64_t y;
    int64_t z;
    int64_t w;
} ivec4;

/**
 * @brief 4D Unsigned Integer Vector
 * 
 * @note To create a new `uvec4`: @code (uvec4){X, Y, Z, W} @endcode
 */
typedef struct
{
    uint64_t x;
    uint64_t y;
    uint64_t z;
    uint64_t w;
} uvec4;

/**
 * @brief 4D Vector
 * 
 * @note To create a new `vec4`: @code (vec4){X, Y, Z, W} @endcode
 */
typedef struct
{
    float x;
    float y;
    float z;
    float w;
} vec4;

#endif