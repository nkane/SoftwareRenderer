#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define internal            static
#define local_persist       static
#define global_variable     static

#define varying_size    6
#define screen_width    400
#define screen_height   300

typedef int8_t      int8;
typedef int16_t     int16;
typedef int32_t     int32;
typedef int64_t     int64;

typedef uint8_t     uint8;
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef uint64_t    uint64;

typedef struct _win32_offscreen_buffer
{
    BITMAPINFO Info;
    HDC BitmapDeviceContext;
    HBITMAP BitmapHandle;
    BITMAP Bitmap;
    void *Memory;
    int Width;
    int Height;
    int BytesPerPixel;
    int BytesPerRow;
} win32_offscreen_buffer;

typedef struct _win32_window_dimension
{
    int Width;
    int Height;
} win32_window_dimension;

typedef struct _v2f
{
    float X;
    float Y;
} v2f;

internal v2f
SubtractV2f(v2f v1, v2f v2)
{
    v2f result = 
    {
        .X = (v1.X - v2.X),
        .Y = (v1.Y - v2.Y),
    };
    return result;
}

internal float
DotProductV2f(v2f v1, v2f v2)
{
    float result = (v1.X * v2.X) + (v1.Y * v2.Y);
    return result;
}

internal float
CrossProductV2f(v2f v1, v2f v2)
{
    float result = (v1.X * v2.Y) - (v1.Y * v2.X);
    return result;
}

typedef union _v3f
{
    struct
    {
        float X;
        float Y;
        float Z;       
    };
    struct
    {
        float R;
        float G;
        float B;
    };
    float E[3];
} v3f;

internal v3f
SubtractV3f(v3f v1, v3f v2)
{
    v3f result = 
    {
        .X = (v1.X - v2.X),
        .Y = (v1.Y - v2.Y),
        .Z = (v1.Z - v2.Z),
    };
    return result;
}

typedef union _v4f
{
    struct
    {
        float X;
        float Y;
        float Z;
        float W;
    };
    struct
    {
        float R;
        float G;
        float B;
        float A;
    };
    float E[4];
} v4f;

typedef struct _vertex
{
    v3f     Point;
    float   VaryingArray[varying_size];
} vertex;

typedef struct _triangle
{
    vertex V1;
    vertex V2;
    vertex V3;
} triangle;

global_variable bool GlobalRunning;
global_variable HWND GlobalWindowHandle;
global_variable win32_offscreen_buffer GlobalBackBuffer;

win32_window_dimension
Win32GetWindowDimension(HWND window)
{
    win32_window_dimension result;
    RECT ClientRectangle;
    GetClientRect(window, &ClientRectangle);
    result.Width = ClientRectangle.right - ClientRectangle.left;
    result.Height = ClientRectangle.bottom - ClientRectangle.top;
    return result;
}

internal v2f*
CalculateVaryingSlope(triangle t)
{
    float *v1 = t.V1.VaryingArray;
    float *v2 = t.V2.VaryingArray;
    float *v3 = t.V3.VaryingArray;
    float w12 = t.V2.Point.X - t.V1.Point.X;
    float h12 = t.V2.Point.Y - t.V1.Point.Y;
    float w13 = t.V3.Point.X - t.V1.Point.X;
    float h13 = t.V3.Point.Y - t.V1.Point.Y;
    float quot = w13 * h12 - w12 * h13;
    if (quot == 0)
    {
        return NULL;
    }
    // TODO(nick): release this after usage.
    v2f *slopeArray = VirtualAlloc(0, (sizeof(v2f) * varying_size), (MEM_RESERVE | MEM_COMMIT), PAGE_READWRITE);
    for (int i = 0; i < varying_size; i++)
    {
        float r1 = v1[i];
        float r2 = v2[i];
        float r3 = v3[i];
        float dx = (h12 * (r3 - r1) + h13 * (r1 - r2)) / quot;
        float dy = (w12 * (r3 - r1) + w13 * (r1 - r2)) / -quot;
        slopeArray[i] = (v2f)
        {
            .X = dx,
            .Y = dy,
        };
    }
    return slopeArray;
}

internal v4f
FragmentShaderTest(float *varying, int length)
{
    v4f result = 
    {
        .R = varying[0],
        .G = varying[1],
        .B = varying[2],
        .A = varying[3],
    };
    return result;
}

internal void
IncrementVarying(float *varying, v2f *slopes, float x, float y)
{
    for (int i = 0; i < varying_size; i++)
    {
        varying[i] += slopes[i].X * x + slopes[i].Y * y;
    }
}

internal void
IncrementVaryingX(float *varying, v2f *slopes)
{
    for (int i = 0; i < varying_size; i++)
    {
        varying[i] += slopes[i].X;
    }
}

internal float * 
CalculateVaryingBase(vertex *base, v2f *slopes, float x, float y)
{
    // TODO(nick): make sure to free this!
    float *varyingBase = VirtualAlloc(0, (sizeof(float) * varying_size), (MEM_RESERVE | MEM_COMMIT), PAGE_READWRITE);
    CopyMemory(varyingBase, base, varying_size);
    float xDiff = (x - base->Point.X);
    float yDiff = (y - base->Point.Y);
    IncrementVarying(varyingBase, slopes, xDiff, yDiff);
    return varyingBase;
}

internal void 
RenderHalfTriangle(win32_offscreen_buffer buffer, int scanlineStart, int scanlineEnd, v2f point1, float inverseSlope1, v2f point2, float inverseSlope2, vertex baseVertex, v2f *varyingSlopes, v4f (*fragmentShaderFunction)(float *varyingBase, int length))
{
    // NOTE: in win32 first line of DIB buffer is actually the last memory line
    uint8 *row = (uint8 *)buffer.Memory;
    // NOTE: get to the end of the entire buffer
    row += (buffer.BytesPerRow * (buffer.Height - 1));

    float xLeftOffset  = ((inverseSlope1 * (scanlineStart - point1.Y)) + point1.X);
    float xRightOffset = ((inverseSlope2 * (scanlineStart - point2.Y)) + point2.X);

    for (int i = scanlineStart; i < scanlineEnd; i++) 
    {
        uint8 *scanlineRowBegin = (uint8 *)row;
        uint8 *scanlineRowEnd = (uint8 *)row;

        int32 xLowOffset = (float)ceil(xLeftOffset);
        int32 xHighOffset = (float)ceil(xRightOffset);

        // NOTE: get scanline to proper row position
        scanlineRowBegin -= (i * buffer.BytesPerRow);
        scanlineRowBegin += (xLowOffset * buffer.BytesPerPixel);

        scanlineRowEnd -= (i * buffer.BytesPerRow);
        scanlineRowEnd += (xHighOffset * buffer.BytesPerPixel);

        float *varyingBase = CalculateVaryingBase(&baseVertex, varyingSlopes, xLowOffset, i);

        // NOTE: because of the way DIBs are stored, the beginning row will be a higher memory address
        while (scanlineRowBegin < scanlineRowEnd)
        {
            v4f fragment = fragmentShaderFunction(varyingBase, varying_size);
            //SetPixelAlphaBlend(buffer, j, i, fragment.R, fragment.G, fragment.B, fragment.A);
            uint32 *pixels = (uint32 *)scanlineRowBegin;
            if (fragment.A >= 255)
            {
                uint32 pixelRed     = (*pixels) | 0x00FF0000;
                uint32 pixelGreen   = (*pixels) | 0x0000FF00;
                uint32 pixelBlue    = (*pixels) | 0x000000FF;
                uint32 pixelAlpha   = (*pixels) | 0xFF000000;
                *pixels |= (0xFFFFFFFF & (pixelRed << 16));
                *pixels |= (0xFFFFFFFF & (pixelGreen << 8));
                *pixels |= (0xFFFFFFFF & (pixelBlue));
            }
            else
            {
                float af = fragment.A / 255.0f;
                float oma = 1.0f - 255.0f;
                uint32 pixelRed     = (*pixels) | 0x00FF0000;
                uint32 pixelGreen   = (*pixels) | 0x0000FF00;
                uint32 pixelBlue    = (*pixels) | 0x000000FF;
                uint32 pixelAlpha   = (*pixels) | 0xFF000000;

                uint32 red = (uint32)fragment.R;
                red = (pixelRed * oma + red * af);
                uint32 green = (uint32)fragment.G;
                green = (pixelGreen * oma + green * af);
                uint32 blue = (uint32)fragment.B;
                blue = (pixelBlue * oma + blue * af);

                *pixels |= (0xFFFFFFFF & (red << 16));
                *pixels |= (0xFFFFFFFF & (green << 8));
                *pixels |= (0xFFFFFFFF & (blue));
            }
            scanlineRowBegin += buffer.BytesPerPixel;
            IncrementVaryingX(varyingBase, varyingSlopes);
        }

        VirtualFree(varyingBase, 0, MEM_RELEASE);

        xLeftOffset += inverseSlope1;
        xRightOffset += inverseSlope2;
    }
}

internal void
RenderTriangle(win32_offscreen_buffer buffer, triangle t, v4f (*fragmentShaderFunction)(float *varyingBase, int length))
{
    v2f point1 = 
    {
        .X = t.V1.Point.X,
        .Y = t.V1.Point.Y,
    };

    v2f point2 = 
    {
        .X = t.V2.Point.X,
        .Y = t.V2.Point.Y,
    };

    v2f point3 = 
    {
        .X = t.V3.Point.X,
        .Y = t.V3.Point.Y,
    };

    if (point2.Y < point1.Y && point2.Y < point3.Y)
    {
        v2f temp = point1;
        point1 = point2;
        point2 = point3;
        point3 = temp;
    }

    if (point3.Y < point2.Y && point3.Y < point1.Y)
    {
        v2f temp = point1;
        point1 = point3;
        point3 = point2;
        point2 = temp;
    }

    v2f vectorLeft = SubtractV2f(point2, point1);
    v2f vectorRight = SubtractV2f(point3, point1);

    float crossProduct = CrossProductV2f(vectorLeft, vectorRight);
    if (crossProduct < 0.0f) 
    {
        return;
    }
    
    // TODO(nick): free this ...
    v2f *varyingSlopes = CalculateVaryingSlope(t);
    if (varyingSlopes == NULL)
    {
        return;
    }

    int scanlineStart = ceil(point1.Y);
    float minY = (point2.Y < point3.Y) ? point2.Y : point3.Y;
    int scanlineEnd = ceil(minY);

    // render top half
    if (scanlineEnd != scanlineStart)
    {
        v2f vector1 = SubtractV2f(point2, point1);
        v2f vector2 = SubtractV2f(point3, point1);
        float inverseSlope1 = vector1.X / vector1.Y;
        float inverseSlope2 = vector2.X / vector2.Y;
        RenderHalfTriangle(GlobalBackBuffer, scanlineStart, scanlineEnd, point1, inverseSlope2, point1, inverseSlope1, t.V1, varyingSlopes, FragmentShaderTest);
    }

    // render bottom half
    scanlineStart = scanlineEnd;
    v2f vector1, vector2, start1, start2;
    // point2 is the lowest point in the y position
    if (point2.Y > point3.Y)
    {
        scanlineEnd = ceil(point2.Y);
        vector1 = SubtractV2f(point2, point1);
        vector2 = SubtractV2f(point2, point3);
        start1 = point1;
        start2 = point3;
    }
    else
    {
        scanlineEnd = ceil(point3.Y);
        vector1 = SubtractV2f(point3, point2);
        vector2 = SubtractV2f(point3, point1);
        start1 = point2;
        start2 = point1;
    }

    if (scanlineStart != scanlineEnd)
    {
        float inverseSlope1 = vector1.X / vector1.Y;
        float inverseSlope2 = vector2.X / vector2.Y;
        RenderHalfTriangle(GlobalBackBuffer, scanlineStart, scanlineEnd, start2, inverseSlope2, start1, inverseSlope1, t.V1, varyingSlopes, FragmentShaderTest);
    }
}

internal float
SlowInverseSlope(float x1, float y1, float x2, float y2)
{
    float result = ((x2 - x1) / (y2 - y1));
    return result;
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int width, int height)
{
    if (buffer->BitmapHandle)
    {
        DeleteObject(buffer->BitmapHandle);
    }
    if (!buffer->BitmapDeviceContext)
    {
        buffer->BitmapDeviceContext = CreateCompatibleDC(0);
    }
    buffer->Width = width;
    buffer->Height = height;
    int bytesPerPixel = 4;
    buffer->BytesPerPixel                   = bytesPerPixel;
    buffer->Info.bmiHeader.biSize           = sizeof(buffer->Info.bmiHeader);
    buffer->Info.bmiHeader.biWidth          = buffer->Width;
    buffer->Info.bmiHeader.biHeight         = buffer->Height;
    buffer->Info.bmiHeader.biPlanes         = 1;
    buffer->Info.bmiHeader.biBitCount       = 32;
    buffer->Info.bmiHeader.biCompression    = BI_RGB;
    buffer->Info.bmiHeader.biSizeImage      = 0;
    buffer->Info.bmiHeader.biXPelsPerMeter  = 0;
    buffer->Info.bmiHeader.biYPelsPerMeter  = 0;
    buffer->Info.bmiHeader.biClrUsed        = 0;
    buffer->Info.bmiHeader.biClrImportant   = 0;
    buffer->BytesPerRow = width * bytesPerPixel;
    buffer->BitmapHandle = CreateDIBSection(buffer->BitmapDeviceContext, &buffer->Info, DIB_RGB_COLORS, &buffer->Memory, 0, 0);
}

internal void
Win32DisplayBufferInWindow(HDC deviceContext, int windowWidth, int windowHeight, win32_offscreen_buffer *buffer)
{
    GetObject(buffer->BitmapHandle, sizeof(BITMAP), &buffer->Bitmap);
    SelectObject(buffer->BitmapDeviceContext, buffer->BitmapHandle);
    BitBlt(deviceContext, 0, 0, buffer->Bitmap.bmWidth, buffer->Bitmap.bmHeight, buffer->BitmapDeviceContext, 0, 0, SRCCOPY);
}

internal void
ClearBuffer(win32_offscreen_buffer buffer)
{
    uint8 *row = (uint8 *)buffer.Memory;
    uint32 halfHeight = buffer.Height / 2;

    for (int y = 0; y < buffer.Height; y++)
    {
        uint32 *pixels = (uint32 *)row;
        for (int x = 0; x < buffer.Width; x++)
        {
            // Blue  - 0x000000FF
            // Red   - 0x00FF0000
            // Green - 0x0000FF00
            *pixels = 0x00000000;
            pixels++;
        }
        row += buffer.BytesPerRow;
    }
}


LRESULT CALLBACK
Win32MainWindowCallBack(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message) 
    {
        case WM_CLOSE:
        {
             GlobalRunning = false;
        } break;
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVEAPP\n");
        } break;
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        };
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC deviceContext = BeginPaint(windowHandle, &Paint);
            EndPaint(windowHandle, &Paint);
        } break;
        default:
        {
            result = DefWindowProcA(windowHandle, message, wParam, lParam);
        } break;
    }
    return result;
}

INT WINAPI 
WinMain(HINSTANCE handleInstance, HINSTANCE handlePreviousInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSA windowClass = { 0 };

    windowClass.style                  = (CS_HREDRAW | CS_VREDRAW | CS_OWNDC);
    windowClass.lpfnWndProc            = Win32MainWindowCallBack;
    windowClass.hInstance              = handleInstance;
    windowClass.hCursor                = LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground          = (HBRUSH)GetStockObject(BLACK_BRUSH);
    windowClass.lpszClassName          = "HiddenPixelWindowClass";

    if (RegisterClassA(&windowClass))
    {
        GlobalWindowHandle = CreateWindowExA(0,
                                            windowClass.lpszClassName,
                                            "Hidden Pixel Engine - Alpha 0.1.0",
                                            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                            CW_USEDEFAULT,
                                            CW_USEDEFAULT,
                                            screen_width,
                                            screen_height,
                                            0,
                                            0,
                                            handleInstance,
                                            0);

        if (GlobalWindowHandle) 
        {
            GlobalRunning = true;
            HDC deviceContext = GetDC(GlobalWindowHandle);
            Win32ResizeDIBSection(&GlobalBackBuffer, screen_width, screen_height);

            while (GlobalRunning)
            {
                MSG message;
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                v3f trianglePoint1 = 
                {
                    .X = 100.0f,
                    .Y = 50.0f,
                    .Z = 0.0f,
                };

                v3f trianglePoint2 = 
                {
                    .X = 125.0f,
                    .Y = 200.0f,
                    .Z = 0.0f,
                };

                v3f trianglePoint3 = 
                {
                    .X = 50.0f,
                    .Y = 100.0f,
                    .Z = 0.0f,
                };

                triangle t = 
                {
                    .V1 = 
                    {
                        .Point = trianglePoint1,
                        .VaryingArray = 
                        {
                            255, 0, 0, 255,
                        },
                    },
                    .V2 = 
                    {
                        .Point = trianglePoint2,
                        .VaryingArray = 
                        {
                            255, 0, 0, 255,
                        },
                    },
                    .V3 = 
                    {
                        .Point = trianglePoint3,
                        .VaryingArray = 
                        {
                            255, 0, 0, 255,
                        },
                    },
                };

                RenderTriangle(GlobalBackBuffer, t, FragmentShaderTest);

                win32_window_dimension dimension = Win32GetWindowDimension(GlobalWindowHandle);
                Win32DisplayBufferInWindow(deviceContext, dimension.Width, dimension.Height, &GlobalBackBuffer);
                ClearBuffer(GlobalBackBuffer);
            }
        }
    }
    return 0;
}
