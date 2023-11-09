#include <opengl/glmat.h>
#include <stddef.h>

void glMatrixIdentity3x3(mat3x3* _out)
{
    if (_out == NULL)
        return;
    
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            _out->Data[i][j] = 0;

            if (i == j)
                _out->Data[i][j] = 1;
        }
    }
}

void glMatrixIdentity4x4(mat4x4* _out)
{
    if (_out == NULL)
        return;
    
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            _out->Data[i][j] = 0;

            if (i == j)
                _out->Data[i][j] = 1;
        }
    }
}

void glMatrixPerspective(mat4x4* _out, float fov, float width, float height, float near, float far)
{
    if (_out == NULL)
        return;

    glMatrixIdentity4x4(_out);

    ///TODO: --implement--
}

void glMatrixOrtho(mat4x4* _out, float left, float right, float top, float bottom, float near, float far)
{
    if (_out == NULL)
        return;

    glMatrixIdentity4x4(_out);

    ///TODO: --implement--
}