/*
 * bitmap.h
 */

typedef struct _win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    //int BytesPerPixel;
} Win32_Offscreen_Buffer;


typedef struct _win32_window_dimension
{
    int Width;
    int Height;
} Win32_Window_Dimension;

typedef struct _bitmap_Header
{
    unsigned short   FileType;
    unsigned int     FileSize;
    unsigned short   Reserved1;
    unsigned short   Reserved2;
    unsigned int     BitmapOffset;
    unsigned int     Size;
    unsigned int     Width;
    unsigned int     Height;
    unsigned short   Planes;
    unsigned short   BitsPerPixel;
    unsigned int     Compression;
    unsigned int     SizeOfBitmap;
    unsigned int     HorizontalResolution;
    unsigned int     VerticalResolution;
    unsigned int     ColorsUsed;
    unsigned int     ColorsImportant;

    unsigned int     RedMask;
    unsigned int     GreenMask;
    unsigned int     BlueMask;
} Bitmap_Header;
