#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define internal        static
#define local_persist   static
#define global_variable static

#define Align16(value) ((value + 15) & ~15)
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}

typedef int8_t      int8;
typedef int16_t     int16;
typedef int32_t     int32;
typedef int64_t     int64;

typedef uint8_t     uint8;
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef uint64_t    uint64;

typedef float      real32;
typedef double     real64;

typedef int32        bool32;

#include "vector3.h"

typedef struct _win32_offscreen_buffer
{
    BITMAPINFO      Info;
    HDC             BitmapDeviceContext;
    HBITMAP         BitmapHandle;
    BITMAP          Bitmap;
    void *          Memory;
    int             Width;
    int             Height;
    int             BytesPerPixel;
    int             Pitch;
} win32_offscreen_buffer;

typedef struct _win32_window_dimension
{
    int Width;
    int Height;
} win32_window_dimension;

#define MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)

global_variable bool GlobalRunning;
global_variable HWND GlobalWindowHandle;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable int64 GlobalPerfCountFrequency;
global_variable real64 GlobalTargetSecondsPerFrame = 1.0f / (real64)GameUpdateHz;

LARGE_INTEGER
Win32GetWallClock(void)
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

real32
Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    LARGE_INTEGER elapsedMicroseconds;
    elapsedMicroseconds.QuadPart = end.QuadPart - start.QuadPart;
    elapsedMicroseconds.QuadPart *= 1000000;
    real64 result = ((real32)elapsedMicroseconds.QuadPart / (real32)GlobalPerfCountFrequency);
    return result;
}

win32_window_dimension
Win32GetWindowDimension(HWND window)
{
    win32_window_dimension result;
    RECT clientRectangle;
    GetClientRect(window, &clientRectangle);
    result.Width = clientRectangle.right - clientRectangle.left;
    result.Height = clientRectangle.bottom - clientRectangle.top;
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
    buffer->Width                           = width;
    buffer->Height                          = height;
    buffer->BytesPerPixel                   = 4;
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

    buffer->Pitch = Align16(width * buffer->BytesPerPixel);
    buffer->BitmapHandle = CreateDIBSection(buffer->BitmapDeviceContext, &buffer->Info, 
                                            DIB_RGB_COLORS, &buffer->Memory,
                                            0, 0);
}

internal void
Win32DisplayBufferInWindow(HDC deviceContext, int windowWidth, int windowHeight, win32_offscreen_buffer *buffer)
{
    GetObject(buffer->BitmapHandle, sizeof(BITMAP), &buffer->Bitmap);
    SelectObject(buffer->BitmapDeviceContext, buffer->BitmapHandle);
    BitBlt(deviceContext, 0, 0, buffer->Bitmap.bmWidth, buffer->Bitmap.bmHeight, buffer->BitmapDeviceContext, 0, 0, SRCCOPY);
}

internal void
ClearBuffer(win32_offscreen_buffer *buffer)
{
    uint8 *pixel = (uint8 *)buffer->Memory;
    for (int x = 0; x < buffer->Width; x++)
    {
        for (int y = 0; y < buffer->Height; y++)
        {
            int idx = (x + y * buffer->Width) * buffer->BytesPerPixel;
            pixel[idx]     = 0;     //blue
            pixel[idx + 1] = 0;     // green
            pixel[idx + 2] = 0;     // red
        }
    }

}

internal void
RenderPixel(win32_offscreen_buffer *buffer, int x, int y, vector3i color)
{
    uint8 *pixel = (uint8 *)buffer->Memory;
    if (x >= 0 && y >= 0 && x < buffer->Width && y < buffer->Height)
    {
        int idx = (x + y * buffer->Width) * buffer->BytesPerPixel;
        pixel[idx]     = color.Blue;
        pixel[idx + 1] = color.Green; 
        pixel[idx + 2] = color.Red;   
    }
}

internal void
DrawHalfTriangle(win32_offscreen_buffer *buffer, real32 scanlineStart, real32 scanlineEnd, vector3f point_1, real32 slope_1, vector3f point_2, real32 slope_2, vector3i color)
{
    real32 rightScanlinePosition = point_1.X + (scanlineStart - point_1.Y) * slope_1;
    real32 leftScanlinePosition = point_2.X + (scanlineStart - point_2.Y) * slope_2;
    for (int i = scanlineStart; i < scanlineEnd; i++)
    {
        int low = ceil(leftScanlinePosition);
        int high = ceil(rightScanlinePosition);
        if (i >= 0 && i < buffer->Height)
        {
            for (int j = low; j < high; j++)
            {
                if (j >= 0 && j < buffer->Width)
                {
                    RenderPixel(buffer, j, i, color);
                }
            }
        }
        rightScanlinePosition += slope_1;
        leftScanlinePosition += slope_2;
    }
}

internal void
DrawTriangle(win32_offscreen_buffer *buffer, vector3f point_1, vector3f point_2, vector3f point_3, vector3i color)
{
    if (point_2.Y < point_1.Y && point_2.Y <= point_3.Y)
    {
        vector3f temp = point_1;
        point_1 = point_2;
        point_2 = point_3;
        point_3 = temp;
    }
    if (point_3.Y < point_2.Y && point_3.Y < point_1.Y)
    {
        vector3f temp = point_1;
        point_1 = point_3;
        point_3 = point_2;
        point_2 = temp;
    }
    vector3f vertexLeft = Subtract_Vector3f(point_2, point_1);
    vector3f vertexRight = Subtract_Vector3f(point_3, point_1);
    real32 zCrossProduct = Cross_Product_2D_Vector3f(vertexLeft, vertexRight);
    // keep clockwise faces
    // cull counter clockwise faces
    if (zCrossProduct < 0) 
    {
        return;
    }
    // scan top half
    real32 yScanlineStart = ceil(point_1.Y);
    real32 yScanlineEnd = ceil(min(point_2.Y, point_3.Y));
    if (yScanlineEnd != yScanlineStart) 
    {
        vector3f vector_1 = Subtract_Vector3f(point_2, point_1);
        vector3f vector_2 = Subtract_Vector3f(point_3, point_1);
        DrawHalfTriangle(buffer, yScanlineStart, yScanlineEnd, point_1, vector_1.X / vector_1.Y, point_1, vector_2.X / vector_2.Y, color);
    }
    // scan bottom half
    yScanlineStart = yScanlineEnd;
    vector3f vector_1;
    vector3f vector_2;
    vector3f start_1;
    vector3f start_2;
    if (point_2.Y > point_3.Y)
    {
        yScanlineEnd = ceil(point_2.Y);
        vector_1 = Subtract_Vector3f(point_2, point_1);
        vector_2 = Subtract_Vector3f(point_2, point_3);
        start_1 = point_1;
        start_2 = point_3;
    }
    else 
    {
        yScanlineEnd = ceil(point_3.Y);
        vector_1 = Subtract_Vector3f(point_3, point_2);
        vector_2 = Subtract_Vector3f(point_3, point_1);
        start_1 = point_2;
        start_2 = point_1;
    }
    if (yScanlineStart != yScanlineEnd)
    {
        DrawHalfTriangle(buffer, yScanlineStart, yScanlineEnd, start_1, vector_1.X / vector_1.Y, start_2, vector_2.X / vector_2.Y, color);
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
    UINT desiredScheduleMS = 1;
    bool32 sleepIsGranular = (timeBeginPeriod(desiredScheduleMS) == TIMERR_NOERROR);
    LARGE_INTEGER perfCountFreqencyResult;
    QueryPerformanceCounter(&perfCountFreqencyResult);
    GlobalPerfCountFrequency = perfCountFreqencyResult.QuadPart;

    WNDCLASSA windowClass = { 0 };
    windowClass.style                  = (CS_HREDRAW | CS_VREDRAW | CS_OWNDC);
    windowClass.lpfnWndProc            = Win32MainWindowCallBack;
    windowClass.hInstance              = handleInstance;
    windowClass.hCursor                = LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground          = (HBRUSH)GetStockObject(BLACK_BRUSH);
    windowClass.lpszClassName          = "HiddenPixelWindowClass";

    int width = 1280;
    int height = 720;

    if (RegisterClassA(&windowClass))
    {
        GlobalWindowHandle = CreateWindowExA(0,
                                            windowClass.lpszClassName,
                                            "Hidden Pixel Engine - Alpha 0.1.0",
                                            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                            CW_USEDEFAULT,
                                            CW_USEDEFAULT,
                                            width,
                                            height,
                                            0,
                                            0,
                                            handleInstance,
                                            0);

        if (GlobalWindowHandle) 
        {
            GlobalRunning = true;
            HDC deviceContext = GetDC(GlobalWindowHandle);
            Win32ResizeDIBSection(&GlobalBackBuffer, width, height);
            real32 value = 0.0f;
            LARGE_INTEGER lastCounter = Win32GetWallClock();
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

                value += 0.01f;
                real32 a = sin(value - 1.5f - sinf(value / 1.3538f)) * 100.0f + 320.0f;
                real32 b = cos(value - 1.5f - sin(value / 1.3538f)) * 100.0f + 240.0f;
                vector3f point_1 = 
                { 
                    .X = a,
                    .Y = b,
                    .Z = 0.0f
                };

                real32 c = sin(value + 2.0f) * 100.0f + 320.0f;
                real32 d = cos(value + 2.0f) * 100.0f + 240.0f;
                vector3f point_2 = 
                {
                    .X = c,
                    .Y = d,
                    .Z = 0.0f
                };

                real32 e = sin(value + 1.0f) * 100.0f + 320.0f;
                real32 f = cos(value + 1.0f) * 100.0f + 240.0f;
                vector3f point_3 = 
                {
                    .X = e,
                    .Y = f,
                    .Z = 0.0f
                };

                vector3i color_green = 
                {
                    .Red = 0,
                    .Blue = 0,
                    .Green = 255,
                };

                //real32 g = sin(value - 3.5f) * 100.0f + 320.0f;
                //real32 h = cos(value - 3.5f) * 100.0f + 240.0f;

                ClearBuffer(&GlobalBackBuffer);
                DrawTriangle(&GlobalBackBuffer, point_1, point_2, point_3, color_green);
                win32_window_dimension dimension = Win32GetWindowDimension(GlobalWindowHandle);
                Win32DisplayBufferInWindow(deviceContext, dimension.Width, dimension.Height, &GlobalBackBuffer);

                /*
                LARGE_INTEGER workCounter = Win32GetWallClock();
                real32 secondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter, workCounter);
                if (secondsElapsedForFrame < GlobalTargetSecondsPerFrame)
                {
                    if (sleepIsGranular)
                    {
                        DWORD sleepMS = (DWORD)(1000.0f * (GlobalTargetSecondsPerFrame - secondsElapsedForFrame));
                        if (sleepMS > 0)
                        {
                            Sleep(sleepMS);
                        }
                    }
                    real32 testSecondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter, Win32GetWallClock());
                    //Assert(testSecondsElapsedForFrame < GlobalTargetSecondsPerFrame);
                    while (secondsElapsedForFrame < GlobalTargetSecondsPerFrame)
                    {
                        secondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter, Win32GetWallClock());
                        char buf[1028] = { 0 };
                        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "(%f, %f) \n", secondsElapsedForFrame, GlobalTargetSecondsPerFrame);
                        OutputDebugStringA(buf);
                    }
                }
                LARGE_INTEGER endCounter = Win32GetWallClock();
                lastCounter = endCounter;
                */
            }
        }
    }
    return 0;
}