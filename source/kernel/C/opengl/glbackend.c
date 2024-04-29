#include <opengl/glbackend.h>
#include <opengl/glcontext.h>
#include <memory.h>
#include <kernel.h>

uint32_t g_glClearColor = 0;

void glWritePixel(uvec2 pixel, uint32_t color)
{
    if (!glContextInitialized())
        return;

    GET_CURRENT_GL_CONTEXT(context);

    if (pixel.x >= context->ColorBufferWidth || pixel.y >= context->ColorBufferHeight)
        return;

    context->ColorBuffer[pixel.y * context->ColorBufferWidth + pixel.x] = color;
}

uint32_t glReadPixel(uvec2 pixel)
{
    if (!glContextInitialized())
        return;

    GET_CURRENT_GL_CONTEXT(context);

    if (pixel.x >= context->ColorBufferWidth || pixel.y >= context->ColorBufferHeight)
        return 0;

    return context->ColorBuffer[pixel.y * context->ColorBufferWidth + pixel.x];
}

void glClearColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!glContextInitialized())
        return;
    
    g_glClearColor = (a << 24) | (r << 16) | (g << 8) | b;
}

void glClear(GLenum mask)
{
    if (!glContextInitialized())
        return;
    
    GET_CURRENT_GL_CONTEXT(context);

    if (mask & GL_COLOR_BUFFER_BIT)
        memset(context->ColorBuffer, g_glClearColor, context->ColorBufferWidth * context->ColorBufferHeight * sizeof(uint32_t));
}

static int abs(int value)
{
    if (value < 0)
        return -value;
    
    return value;
}

void glDrawRect(uvec2 start, uvec2 size, uint32_t col)
{
    if (!glContextInitialized())
        return;

    uvec2 offsetSize = (uvec2){start.x + size.x, start.y + size.y};

    for (uint32_t y = start.y; y < offsetSize.y; y++)
        for (uint32_t x = start.x; x < offsetSize.x; x++)
            glWritePixel((uvec2){x, y}, col);
}

void glDrawLine(uvec2 p1, uvec2 p2, uint32_t col)
{
    if (!glContextInitialized())
        return;

    uvec2 pdraw;
    ivec2 distance;
    ivec2 absoluteDistance;
    ivec2 p, e;
    int i;

    distance.x = p2.x - p1.x;
    distance.y = p2.y - p1.y;
    absoluteDistance.x = abs(distance.x);
    absoluteDistance.y = abs(distance.y);
    p.x = 2 * absoluteDistance.y - absoluteDistance.x;
    p.y = 2 * absoluteDistance.x - absoluteDistance.y;

    if (absoluteDistance.y <= absoluteDistance.x) {
        if (distance.x >= 0) {
            pdraw.x = p1.x;
            pdraw.y = p1.y;
            e.x = p2.x;
        } else {
            pdraw.x = p2.x;
            pdraw.y = p2.y;
            e.x = p1.x;
        }

        glWritePixel(pdraw, col);

        for (i = 0; pdraw.x < e.x; i++) {
            pdraw.x++;
            if (p.x < 0) {
                p.x = p.x + 2 * absoluteDistance.y;
            } else {
                if ( (distance.x < 0 && distance.y < 0) || (distance.x > 0 && distance.y > 0) ) {
                    pdraw.y++;
                } else {
                    pdraw.y--;
                }
                p.x = p.x + 2 * (absoluteDistance.y - absoluteDistance.x);
            }
            glWritePixel(pdraw, col);
        }
    }
    else
    {
        if (distance.y >= 0)
        {
            pdraw.x = p1.x;
            pdraw.y = p1.y;
            e.y = p2.y;
        }
        else
        {
            pdraw.x = p2.x;
            pdraw.y = p2.y;
            e.y = p1.y;
        }
        glWritePixel((uvec2){pdraw.y, pdraw.y}, col);

        for (i = 0; pdraw.y < e.y; i++) {
            pdraw.y++;
            if (p.y <= 0) {
                p.y = p.y + 2 * absoluteDistance.x;
            } else {
                if ( (distance.x < 0 && distance.y < 0) || (distance.x > 0 && distance.y > 0) ) {
                    pdraw.x++;
                } else {
                    pdraw.x--;
                }
                p.y = p.y + 2 * (absoluteDistance.x - absoluteDistance.y);
            }
            glWritePixel(pdraw, col);
        }
    }
}

#define SWAP(type,var1,var2)    \
            type tmp = var2;    \
            var2 = var1;        \
            var1 = tmp;

void glDrawTriangle(uvec2 t0, uvec2 t1, uvec2 t2, uint32_t col, bool fill)
{
    if (!glContextInitialized())
        return;

    if (!fill)
    {
        glDrawLine(t0, t1, col);
        glDrawLine(t1, t2, col);
        glDrawLine(t2, t0, col);
        return;
    }

    uvec2 t, tp, d1, d2;
    int y, minx, maxx;
    bool change0 = false;
    bool change1 = false;
    int sign_x1, sign_x2, e1, e2;

    if ( t0.y > t1.y )
    {
        int temp;
        temp = t1.y;
        t1.y = t0.y;
        t0.y = temp;
        SWAP( int, t0.x, t1.x )
    }
    if ( t0.y > t2.y )
    {
        int temp;
        temp = t2.y;
        t2.y = t0.y;
        t0.y = temp;
        SWAP( int, t0.x, t2.x )
    }
    if ( t1.y > t2.y )
    {
        int temp;
        temp = t2.y;
        t2.y = t1.y;
        t1.y = temp;
        SWAP( int, t1.x, t2.x )
    }

    t.x = t.y = t0.x;
    y = t0.y;
    d1.x = (int)( t1.x - t0.x );
    if (d1.x < 0)
    {
        d1.x = -d1.x;
        sign_x1 = -1;
    }
    else
        sign_x1 = 1;

    d1.y = (int)( t1.y - t0.y );

    d2.x = (int)( t2.x - t0.x );
    if (d2.x < 0)
    {
        d2.x = -d2.x;
        sign_x2 = -1;
    }
    else
        sign_x2 = 1;

    d2.y = (int)( t2.y - t0.y );

    if (d1.y > d1.x)
    {
        SWAP( int, d1.x, d1.y )
        change0 = true;
    }
    
    if (d2.y > d2.x)
    {
        SWAP( int, d2.x, d2.y )
        change1 = true;
    }

    e2 = (int) ( (int)d2.x >> 1 );
    if ( t0.y == t1.y )
        goto nextHalf;

    e1 = (int) ( (int)d1.x >> 1 );

    for (int i = 0; i < d1.x; )
    {
        tp.x = 0;
        tp.y = 0;
        if (t.x < t.y)
        {
            minx = t.x;
            maxx = t.y;
        }
        else
        {
            minx = t.y;
            maxx = t.x;
        }

        while (i < d1.x)
        {
            i++;
            e1 += d1.y;
            while (e1 >= d1.x)
            {
                e1 -= d1.x;
                if (change0)
                    tp.x = sign_x1;
                else
                    goto next0;
            }

            if (change1)
                break;
            
            t.x += sign_x1;
        }

    next0:

        while (1) {
            e2 += d2.y;
            while (e2 >= d2.x) 
            {
                e2 -= d2.x;
                if (change1)
                    tp.y = sign_x2;
                else
                    goto next1;
            }

            if (change1) 
                break;

            t.y = sign_x2;
        }

    next1:

        if (minx > t.x)
            minx = t.x;

        if (minx > t.y)
            minx = t.y;

        if (maxx < t.x)
            maxx = t.x;

        if (maxx < t.y)
            maxx = t.y;

        for (int j = minx; j <= maxx; j++)
            glWritePixel((uvec2){j, y}, col);

        if (!change0)
            t.x += sign_x1;

        t.x += tp.x;
        if (!change1)
            t.y += sign_x2;

        t.y += tp.y;
        y++;

        if (y == t1.y)
            break;
    }

nextHalf:

    d1.x = (int)(t2.x - t1.x);
    if (d1.x < 0)
    {
        d1.x = -d1.x;
        sign_x1 = -1;
    }
    else
        sign_x1 = 1;

    d1.y = (int)( t2.y - t1.y );
    t.x = t1.x;

    if (d1.y > d1.x)
    {
        SWAP( int, d1.y, d1.x )
        change0 = true;
    }
    else
        change0 = false;

    e1 = (int)( (int)d1.x >> 1 );

    for (int i = 0; i <= d1.x; i++)
    {
        tp.x = 0;
        tp.y = 0;
        if (t.x < t.y)
        {
            minx = t.x;
            maxx = t.y;
        }
        else
        {
            minx = t.y;
            maxx = t.x;
        }

        while (i < d1.x)
        {
            e1 += d1.y;

            while (e1 >= d1.x)
            {
                e1 -= d1.x;
                if (change0)
                {
                    tp.x = sign_x1;
                    break;
                }
                
                goto next2;
            }

            if (change0)
                break;
            
            t.x += sign_x1;

            if (i < d1.x)
                i++;
        }

    next2:

        while (t.y != t2.x)
        {
            e2 += d2.y;
            while (e2 >= d2.x)
            {
                e2 -= d2.x;
                if (change1)
                    tp.y = sign_x2;

                goto next3;
            }

            if (change1)
                break;

            t.y += sign_x2;
        }

    next3:

        if (minx > t.x)
            minx = t.x;

        if (minx > t.y)
            minx = t.y;

        if (maxx < t.x)
            maxx = t.x;

        if (maxx < t.y)
            maxx = t.y;

        for (int j = minx; j <= maxx; j++)
            glWritePixel((uvec2){j, y}, col);

        if (!change0)
            t.x += sign_x1;

        t.x += tp.x;
        if (!change1)
            t.y += sign_x2;

        t.y += tp.y;
        y++;

        if (y == t2.y)
            return;
    }
}