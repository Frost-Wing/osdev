/**
 * @file glcontext.h
 * @author GAMINGNOOBdev (https://github.com/GAMINGNOOBdev)
 * @brief Contains the opengl context specific functions/data structures
 * @version 0.1
 * @date 2023-10-24
 * 
 * @copyright Copyright (c) Pradosh & GAMINGNOOBdev 2023
 */
#ifndef __OPENGL__GLCONTEXT_H_
#define __OPENGL__GLCONTEXT_H_ 1

#include "gl.h"
#include <limine.h>
#include <stdbool.h>

struct GLContext
{
    struct limine_framebuffer* ColorBuffer;
    bool Initialized;
};

#define GET_CURRENT_GL_CONTEXT(name) struct GLContext* name = glGetCurrentContext()

/**
 * @brief Initializes an "OpenGL" context
 * 
 * Initializes an "OpenGL" context, if there isn't an active one it will create one.
 * In case there is an active context, the active context will be returned instead.
 * 
 * @returns The current active "OpenGL" context
 */
GLAPI struct GLContext* glCreateContext();

/**
 * @brief Gets the current active "OpenGL" context
 * 
 * @returns The current "OpenGL" context 
 */
GLAPI struct GLContext* glGetCurrentContext();

/**
 * @brief Checks if the current context (if existent) is initialized
 * 
 * @returns `true` if the context is initialized, otherwise `false` 
 */
GLAPI bool glContextInitialized();

/**
 * @brief Destroys a given "OpenGL" context
 * 
 * @param context Pointer to an active "OpenGL" context
 * 
 * @note To destroy the current context, pass `NULL` as value for `context`
 */
GLAPI void glDestroyContext(struct GLContext* context);

#endif