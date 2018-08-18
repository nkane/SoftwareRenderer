#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define internal            static
#define local_persist       static
#define global_variable     static

#define Align16(value) ((value + 15) & ~15)

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

typedef struct _v3f
{
    float X;
    float Y;
    float Z;
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

global_variable bool GlobalRunning;
global_variable HWND GlobalWindowHandle;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable int GlobalI = 0;

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

internal void
Render(win32_offscreen_buffer buffer)
{
    // NOTE: win32 DIBs start from the bottom and go up
    //       this code will render from the bottom of the
    //       memory address space to the top of the
    //       address space.
    //       image should be red on top and green on bottom
    uint8 *row = (uint8 *)buffer.Memory;
    // NOTE: go to the end of the image buffer - last address
    row += (buffer.BytesPerRow * (buffer.Height - 1));
    uint32 halfHeight = buffer.Height / 2;
    for (int y = 0; y < buffer.Height; y++)
    {
        uint32 *pixels = (uint32 *)row;
        for (int x = 0; x < buffer.Width; x++)
        {
            // Alpha - 0xFF000000
            // Red   - 0x00FF0000
            // Green - 0x0000FF00
            // Blue  - 0x000000FF
            // NOTE: render top half red and bottom half green
            if (y <= halfHeight) 
            {
                *pixels = 0x00FF0000;
            }
            else
            {
                *pixels = 0x0000FF00;
            }
            pixels++;
        }
        row -= buffer.BytesPerRow;
    }

    // NOTE: win32 DIBs start from the bottom and go up
    //       this code will render from the top of the
    //       memory address space to the bottom of the
    //       address space.
    //       image should be green on top and red on bottom
    //       -> in memory it will appear as if it should be
    //          the reverse (i.e., red on top and green on
    //          bottom); however, this is not the case.
    /*          
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
            // NOTE: render top half red and bottom half green
            if (y <= halfHeight) 
            {
                *pixels = 0x00FF0000;
            }
            else
            {
                *pixels = 0x0000FF00;
            }
            pixels++;
        }
        row += buffer.BytesPerRow;
    }
    */
}


internal void
RenderHalfTriangle(win32_offscreen_buffer buffer, int scanlineStart, int scanlineEnd, v2f point1, float inverseSlope1, v2f point2,  float inverseSlope2)
{
#if 1
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

        // NOTE: because of the way DIBs are stored, the beginning row will be a higher memory address
        while (scanlineRowBegin < scanlineRowEnd)
        {
            uint32 *pixels = (uint32 *)scanlineRowBegin;
            *pixels = 0xFFFFFFFF;
            scanlineRowBegin += buffer.BytesPerPixel;
        }

        xLeftOffset += inverseSlope1;
        xRightOffset += inverseSlope2;
    }
#endif

#if 0
    // NOTE: rendering from the "top" of the bitmap memory
    // start from right position
    float xLow = ((inverseSlope1 * (scanlineStart - point1.Y)) + point1.X);
    // start from left position
    float xHigh = ((inverseSlope2 * (scanlineStart - point2.Y)) + point2.X);
    for (int i = scanlineStart; i < scanlineEnd; i++)
    {
        uint8 *scanlineRowBegin = (uint8 *)buffer.Memory;
        uint8 *scanlineRowEnd = (uint8 *)buffer.Memory;
        int32 xLowOffset = (float)ceil(xLow);
        int32 xHighOffset = (float)ceil(xHigh);
        scanlineRowBegin += (i * buffer.BytesPerRow) + (xLowOffset * buffer.BytesPerPixel);
        scanlineRowEnd += (i * buffer.BytesPerRow) + (xHighOffset * buffer.BytesPerPixel);
        while (scanlineRowBegin < scanlineRowEnd)
        {
            uint32 *pixels = (uint32 *)scanlineRowBegin;
            *pixels = 0xFFFFFFFF;
            scanlineRowBegin += buffer.BytesPerPixel;
        }
        xLow += inverseSlope1;
        xHigh += inverseSlope2;
    }
#endif

}

internal void
RenderTriangle(win32_offscreen_buffer buffer, v2f point1, v2f point2, v2f point3, float red, float green, float blue, float alpha)
{
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
        //RenderHalfTriangle(GlobalBackBuffer, scanlineStart, scanlineEnd, point1, inverseSlope1, point1, inverseSlope2);
        RenderHalfTriangle(GlobalBackBuffer, scanlineStart, scanlineEnd, point1, inverseSlope2, point1, inverseSlope1);
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
        // NOTE: doesn't draw
        //RenderHalfTriangle(GlobalBackBuffer, scanlineStart, scanlineEnd, start1, inverseSlope1, start2, inverseSlope2);
        // NOTE: doesn't draw correct direction
        RenderHalfTriangle(GlobalBackBuffer, scanlineStart, scanlineEnd, start2, inverseSlope2, start1, inverseSlope1);
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

    /*
    if (buffer->Memory)
    {
        VirtualFree(buffer->Memory, 0, MEM_RELEASE);
    }
    */

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

    // TODO(nick): not sure what this does?
    //buffer->BytesPerRow = Align16(width * bytesPerPixel);
    buffer->BytesPerRow = width * bytesPerPixel;

    buffer->BitmapHandle = CreateDIBSection(buffer->BitmapDeviceContext, &buffer->Info, DIB_RGB_COLORS, &buffer->Memory, 0, 0);
    /*
    int bitmapMemorySize = (buffer->BytesPerRow * buffer->Height);
    buffer->Memory = (void *)VirtualAlloc(0, bitmapMemorySize, (MEM_COMMIT | MEM_COMMIT), PAGE_READWRITE);
    */
}

internal void
Win32DisplayBufferInWindow(HDC deviceContext, int windowWidth, int windowHeight, win32_offscreen_buffer *buffer)
{
    GetObject(buffer->BitmapHandle, sizeof(BITMAP), &buffer->Bitmap);
    SelectObject(buffer->BitmapDeviceContext, buffer->BitmapHandle);
    BitBlt(deviceContext, 0, 0, buffer->Bitmap.bmWidth, buffer->Bitmap.bmHeight, buffer->BitmapDeviceContext, 0, 0, SRCCOPY);

    /*
    // NOTE(nick): not sure why - but this fixes weird artifacts of StretchDIBits?
    SetStretchBltMode(deviceContext, HALFTONE);
    StretchDIBits(deviceContext,
                  0, 0, windowWidth, windowHeight,
                  0, 0, buffer.Width, buffer.Height,
                  buffer.Memory,
                  &buffer.Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
      */
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

// TODO(nick): tutorial #1 - win32 window context
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
                                            1280,
                                            720,
                                            0,
                                            0,
                                            handleInstance,
                                            0);

        if (GlobalWindowHandle) 
        {
            GlobalRunning = true;
            HDC deviceContext = GetDC(GlobalWindowHandle);
            Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

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

                // TODO(nick): tutorial #2 
                // 1) dibsections
                // 2) rendering back buffers
                //Render(GlobalBackBuffer);

                // TODO(nick): tutorial #3 
                // 1) Render Rectangle
                // 2) Rendering half a triangle
                /*
                v2f trianglePoint1 = 
                {
                    .X = 500.0f,
                    .Y = 100.0f,
                };

                v2f trianglePoint2 = 
                {
                    .X = 600.0f,
                    .Y = 100.0f,
                };

                RenderHalfTriangle(GlobalBackBuffer, 100, 300, trianglePoint1, 1.0f, trianglePoint2, 0.1f);
                */

                // TODO(nick): tutorial #4
                v2f trianglePoint1 = 
                {
                    .X = 100.0f,
                    .Y = 50.0f,
                };

                v2f trianglePoint2 = 
                {
                    .X = 125.0f,
                    .Y = 200.0f,
                };

                v2f trianglePoint3 = 
                {
                    .X = 50.0f,
                    .Y = 100.0f,
                };

                //local_persist float value = 0.0f;
                //value += 0.01f;

                //trianglePoint3.Y += sin(value) * 10.0f; 
                //trianglePoint3.X += cos(value) * 10.0f; 

                RenderTriangle(GlobalBackBuffer, trianglePoint1, trianglePoint2, trianglePoint3, 0.0f, 0.0f, 0.0f, 0.0f);

                // TODO(nick): extra tutorial #4
                /*
                v2f trianglePoint1 = 
                {
                    .X = 200.0f,
                    .Y = 200.0f,
                };

                v2f trianglePoint2 = 
                {
                    .X = 200.0f,
                    .Y = 200.0f,
                };

                v2f trianglePoint3 = 
                {
                    .X = 200.0f,
                    .Y = 200.0f,
                };

                local_persist float value = 0.0f;
                value += 0.001f;

                trianglePoint1.Y += sin(value + (6.28f/3.0f)) * 50.0f; 
                trianglePoint1.X += cos(value + (6.28f/3.0f)) * 50.0f; 

                trianglePoint2.Y += sin(value + (2.0f*6.28f/3.0f)) * 100.0f; 
                trianglePoint2.X += cos(value + (2.0f*6.28f/3.0f)) * 100.0f; 
                
                trianglePoint3.Y += sin(value) * 100.0f; 
                trianglePoint3.X += cos(value) * 100.0f; 

                RenderTriangle(GlobalBackBuffer, trianglePoint1, trianglePoint2, trianglePoint3, 0.0f, 0.0f, 0.0f, 0.0f);
                */

                win32_window_dimension dimension = Win32GetWindowDimension(GlobalWindowHandle);
                Win32DisplayBufferInWindow(deviceContext, dimension.Width, dimension.Height, &GlobalBackBuffer);
                ClearBuffer(GlobalBackBuffer);
            }
        }
    }
    return 0;
}
