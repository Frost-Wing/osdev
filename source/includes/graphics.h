/**
 * @file graphics.h
 * @author Pradosh (pradoshgame@gmail.com) and (partially) GAMINGNOOB (https://github.com/GAMINGNOOBdev)
 * @brief Contains all the print functions.
 * @version 0.1
 * @date 2023-10-21
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

// TODO: graphics.h is under development, this part is temporary.

/**
 * @brief 2D Integer Vector
 * 
 * @note To create a new `ivec2`: @code (ivec2){X, Y} @endcode
 */
typedef struct {
    int x;
    int y;
} ivec2;

/**
 * @brief Pixel drawing function
 * 
 * @author Pradosh (pradoshgame@gmail.com)
 * 
 * @param x X position of the pixel on-screen
 * @param y Y position of the pixel on-screen
 * @param color 8-bit color
 */
void put_pixel(int x, int y, uint32_t color);

/**
 * @brief  Line drawing function
 * 
 * @author GAMINGNOOB (https://github.com/GAMINGNOOBdev)
 * 
 * @param p1 Line start position
 * @param p2 Line end position
 * @param col Color of the line
*/
void draw_line(ivec2 p1, ivec2 p2, uint32_t col);

/**
 * @brief Triangle drawing function
 * 
 * @author GAMINGNOOB (https://github.com/GAMINGNOOBdev)
 * 
 * @param t0 First vertex of the triangle
 * @param t1 Second vertex of the triangle
 * @param t2 Third vertex of the triangle
 * @param col Color of the contents
 * @param fill Whether or not to fill the triangle
 */
void draw_triangle(ivec2 t0, ivec2 t1, ivec2 t2, uint32_t col, bool fill);