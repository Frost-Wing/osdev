#include <opengl/glcontext.h>
#include <stddef.h>

struct GLContext g_gl_context;

extern struct limine_framebuffer *framebuffer;

struct GLContext* glCreateContext()
{
    if (g_gl_context.Initialized)
        return &g_gl_context;
    
    g_gl_context.ColorBuffer = framebuffer;
    g_gl_context.Initialized = true;

    return &g_gl_context;
}

struct GLContext* glGetCurrentContext()
{
    if (g_gl_context.Initialized)
        return &g_gl_context;
}

bool glContextInitialized()
{
    return g_gl_context.Initialized && g_gl_context.ColorBuffer != NULL;
}

void glDestroyContext(struct GLContext* context)
{
    if (context == NULL)
    {
        g_gl_context.ColorBuffer = NULL;
        g_gl_context.Initialized = false;
        return;
    }

    context->ColorBuffer = NULL;
    context->Initialized = false;
}