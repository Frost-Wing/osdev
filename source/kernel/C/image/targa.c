/**
 * @file targa.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-12-29
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <image/targa.h>

void decode_targa_image(const uint64* targa_pointer, uvec2 position, uint64 width, uint64 height) {
    targa_header* header = (targa_header*)targa_pointer;
    uint8* image = (uint8*)(targa_pointer);
    image += sizeof(targa_header);

    if (header->imageType != 2) {
        error("This file is not a TrueColor or DirectColor Targa!", __FILE__);
        return;
    }

    for (uint32 y = 0; y < header->height; ++y) {
        for (uint32 x = 0; x < header->width; ++x) {
            uint32 index = ((y * header->width + x) * (header->bpp / 8));

            uint8 b = image[index];
            uint8 g = image[index + 1];
            uint8 r = image[index + 2];

            uint32 color = (r << 16) | (g << 8) | b;
            // uint32 color = (b << 16) | (g << 8) | r;

            uint32 screenX = header->xOrigin + x + position.x;
            uint32 screenY = header->yOrigin + y + position.y;

            if(screenX <= width && screenY <= height) glWritePixel((uvec2){screenX, screenY}, color);
        }
    }
}

void decode_targa_image_border(const uint64* targa_pointer, uvec2 position, uint32 _color) {
    targa_header* header = (targa_header*)targa_pointer;
    uint8* image = (uint8*)(targa_pointer);
    image += sizeof(targa_header);

    if (header->imageType != 2) {
        error("This file is not a TrueColor or DirectColor Targa!", __FILE__);
        return;
    }

    printf("%d", header->bpp);

    for (uint32 y = 0; y < header->height; ++y) {
        for (uint32 x = 0; x < header->width; ++x) {
            uint32 index = ((y * header->width + x) * (header->bpp / 8));

            uint8 b = image[index];
            uint8 g = image[index + 1];
            uint8 r = image[index + 2];

            uint32 color = (r << 16) | (g << 8) | b;

            if(color > 0x000000) color = _color;

            uint32 screenX = header->xOrigin + x + position.x;
            uint32 screenY = header->yOrigin + y + position.y;

            glWritePixel((uvec2){screenX, screenY}, color);
        }
    }
}