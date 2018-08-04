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
    int Pitch;
} win32_offscreen_buffer;

typedef struct _win32_window_dimension
{
    int Width;
    int Height;
} win32_window_dimension;

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

internal void
RenderGradient(win32_offscreen_buffer buffer, int blueOffset, int greenOffset)
{
    uint8 *row = (uint8 *)buffer.Memory;
    for (int y = 0; y < buffer.Height; y++)
    {
        uint32 *pixels = (uint32 *)row;
        for  (int x = 0; x < buffer.Width; x++)
        {
            uint8 blue = (x + blueOffset);
            uint8 green = (x + greenOffset);
            *pixels = (green << 8 | blue);
            pixels++;
        }
        row += buffer.Pitch;
    }
}

internal void
Render(win32_offscreen_buffer buffer)
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
            if (y <= halfHeight) 
            {
                *pixels = 0x00FF0000;
            }
            else
            {
                *pixels = 0x00000FF;
            }
            pixels++;
        }
        row += buffer.Pitch;
    }
}

internal void
RenderHalfTriangle(win32_offscreen_buffer buffer, float x1, float y1, float inverseSlope1, float x2, float y2, float inverseSlope2, int scanlineStart, int scanlineEnd)
{
    float xLow  = ((inverseSlope1 * (scanlineStart - y1)) + x1);
    float xHigh = ((inverseSlope2 * (scanlineStart - y2)) + x2);
    
    for (int i = scanlineStart; i < scanlineEnd; i++) 
    {
        uint8 *scanlineRowBegin = (uint8 *)buffer.Memory;
        uint8 *scanlineRowEnd = (uint8 *)buffer.Memory;

        int32 xLowOffset = (float)ceil(xLow);
        int32 xHighOffset = (float)ceil(xHigh);

        scanlineRowBegin += (i * buffer.Pitch) + (xLowOffset * buffer.BytesPerPixel);
        scanlineRowEnd += (i * buffer.Pitch) + (xHighOffset * buffer.BytesPerPixel);

        while (scanlineRowBegin < scanlineRowEnd)
        {
            uint32 *pixels = (uint32 *)scanlineRowBegin;
            *pixels = 0xAAFF1144;
            scanlineRowBegin += buffer.BytesPerPixel;
        }

        xLow += inverseSlope1;
        xHigh += inverseSlope2;
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
    buffer->Info.bmiHeader.biHeight         = -buffer->Height;
    buffer->Info.bmiHeader.biPlanes         = 1;
    buffer->Info.bmiHeader.biBitCount       = 32;
    buffer->Info.bmiHeader.biCompression    = BI_RGB;
    buffer->Info.bmiHeader.biSizeImage      = 0;
    buffer->Info.bmiHeader.biXPelsPerMeter  = 0;
    buffer->Info.bmiHeader.biYPelsPerMeter  = 0;
    buffer->Info.bmiHeader.biClrUsed        = 0;
    buffer->Info.bmiHeader.biClrImportant   = 0;

    buffer->Pitch = Align16(width * bytesPerPixel);

    buffer->BitmapHandle = CreateDIBSection(buffer->BitmapDeviceContext, &buffer->Info, DIB_RGB_COLORS, &buffer->Memory, 0, 0);
    /*
    int bitmapMemorySize = (buffer->Pitch * buffer->Height);
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
            float triangleX1 = 200.0f;
            float triangleY1 = 100.0f;
            float triangleX2 = 400.0f;
            float triangleY2 = 100.0f;
            float slope1 = 1.0f;
            float slope2 = 0.2f;

            int scanlineStart = ceil(triangleY1);

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
                RenderHalfTriangle(GlobalBackBuffer, triangleX1, triangleY1, slope1, triangleX2, triangleY2, slope2, scanlineStart, scanlineStart + 50);
                //Render(GlobalBackBuffer);
                //RenderGradient(GlobalBackBuffer, xOffset, yOffset);
                win32_window_dimension dimension = Win32GetWindowDimension(GlobalWindowHandle);
                Win32DisplayBufferInWindow(deviceContext, dimension.Width, dimension.Height, &GlobalBackBuffer);
            }
        }
    }
    return 0;
}
