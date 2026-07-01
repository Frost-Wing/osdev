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
#ifndef TARGA_H
#define TARGA_H

#include <basics.h>
#include <opengl/glbackend.h>
#include <graphics.h>

/**
 * @brief Targa header.
 * 
 */
typedef struct {
    uint8  idLength;
    uint8  colorMapType;
    uint8  imageType;
    uint16 colorMapOrigin;
    uint16 colorMapLength;
    // uint8  colorMapDepth;
    uint16 xOrigin;
    uint16 yOrigin;
    uint16 width;
    uint16 height;
    uint8  bpp;
    uint8  imageDescriptor;
} targa_header;

/**
 * @brief decodes and displays a targa image.
 * 
 * @param targa_pointer The memory address pointer where the targa is loaded.
 */
void decode_targa_image(const uint64* targa_pointer, uvec2 position, uint64 width, uint64 height);

#endif