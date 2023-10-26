/**
 * @file glmat.h
 * @author GAMINGNOOBdev (https://github.com/GAMINGNOOBdev)
 * @brief Matrix types for use with OpenGL functions
 * @version 0.1
 * @date 2023-10-24
 * 
 * @copyright Copyright (c) GAMINGNOOBdev 2023
 */
#ifndef __OPENGL__GLMAT_H_
#define __OPENGL__GLMAT_H_ 1

#include "gl.h"

/**
 * @brief 3x3 Matrix
 */
typedef struct
{
    float Data[3][3];
} mat3x3;

/**
 * @brief 4x4 Matrix
 */
typedef struct
{
    float Data[4][4];
} mat4x4;

/**
 * @brief Create an identity matrix
 * 
 * @param _out Output matrix
 */
GLAPI void glMatrixIdentity3x3(mat3x3* _out);

/**
 * @brief Create an identity matrix
 * 
 * @param _out Output matrix
 */
GLAPI void glMatrixIdentity4x4(mat4x4* _out);

/**
 * @brief Create a 3D Perspective Projection matrix
 * 
 * @param[out] _out Output matrix
 * @param fov Field of View (in degrees!)
 * @param width Width of the view
 * @param height Height of the view
 * @param near Near clipping plane
 * @param far Far clipping plane
 */
GLAPI void glMatrixPerspective(mat4x4* _out, float fov, float width, float height, float near, float far);

/**
 * @brief Create a 2D Orthographic Projection Matrix
 * 
 * @param[out] _out Output matrix
 * @param left Left side coordinate of the view
 * @param right Right side coordinate of the view
 * @param top Top side coordinate of the view
 * @param bottom Bottom side coordinate of the view
 * @param near Near clipping plane
 * @param far Far clipping plane
 */
GLAPI void glMatrixOrtho(mat4x4* _out, float left, float right, float top, float bottom, float near, float far);

#endif