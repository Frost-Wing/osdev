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

void decode_targa_image(const int64* targa_pointer, uvec2 position) {
    targa_header* header = (targa_header*)targa_pointer;
    int8* image = (int8*)(targa_pointer);
    image += sizeof(targa_header);

    if (header->imageType != 2) {
        error("This file is not a TrueColor or DirectColor Targa!", __FILE__);
        return;
    }

    printf("%d", header->bpp);

    for (int32 y = 0; y < header->height; ++y) {
        for (int32 x = 0; x < header->width; ++x) {
            int32 index = ((y * header->width + x) * (header->bpp / 8));

            int8 b = image[index];
            int8 g = image[index + 1];
            int8 r = image[index + 2];

            int32 color = (r << 16) | (g << 8) | b;

            int32 screenX = header->xOrigin + x + position.x;
            int32 screenY = header->yOrigin + y + position.y;

            glWritePixel((uvec2){screenX, screenY}, color);
        }
    }
}