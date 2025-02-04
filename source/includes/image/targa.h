/**
 * @file targa.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-12-29
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <basics.h>
#include <opengl/glbackend.h>

/**
 * @brief Targa header.
 * 
 */
typedef struct {
    int8  idLength;
    int8  colorMapType;
    int8  imageType;
    int16 colorMapOrigin;
    int16 colorMapLength;
    // int8  colorMapDepth;
    int16 xOrigin;
    int16 yOrigin;
    int16 width;
    int16 height;
    int8  bpp;
    int8  imageDescriptor;
} targa_header;

/**
 * @brief decodes and displays a targa image.
 * 
 * @param targa_pointer The memory address pointer where the targa is loaded.
 */
void decode_targa_image(const int64* targa_pointer, uvec2 position, int64 width, int64 height);