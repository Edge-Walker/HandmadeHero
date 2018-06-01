/*
==============================================
$File: $
$Date: $
$Rev: $
$Creator: Jase Grills$
$Notice: (C) Copyright 2018$

============================================== */

#include <windows.h>
#include <stdint.h>

// globals (TODO: all globals should be members of something)
static bool Running = true;

static BITMAPINFO BitmapInfo;
static void* BitmapMemory;
static HBITMAP BitmapHandle;
static HDC BitmapDeviceContext;
static int BitmapWidth;
static int BitmapHeight;
static int BytesPerPixel = 4;

// resize device independent bitmap section
static void Win32ResizeDIBSection(LONG Width, LONG Height) {

    // destroy current bitmap buffer if it already exists
    if (BitmapMemory) {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }

    BitmapWidth = Width;
    BitmapHeight = Height;

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biHeight = Width;
    BitmapInfo.bmiHeader.biWidth = -BitmapHeight; // negative so we can follow convention of left-to-right major top-to-bottom drawing
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32; // RGB + 8 bits of padding for alignment
    BitmapInfo.bmiHeader.biCompression = BI_RGB; // uncompressed

    // allocate enough memory for our bitmap buffer
    int BitmapMemorySize = (BitmapWidth * BitmapHeight * BytesPerPixel);
    BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    int Pitch = Width * BytesPerPixel; // length of a row of pixels, in bytes
    int8_t *Row = (int8_t*)BitmapMemory; // position of the first row (?)

    // traverse drawable area and draw into it in top-to-bottom, left-to-right order
    for (int Y = 0; Y < BitmapHeight; ++Y) {
        int32_t *Pixel = (int32_t*)Row;
        for (int X = 0; X < BitmapWidth; ++X) {

            /* Our pixels in memory: 0x xx BB GG RR */            
            *Pixel = 0;
            ++Pixel;

            *Pixel = 255;
            ++Pixel;

            *Pixel = 255;
            ++Pixel;
        }

        Row += Pitch;
    }
}

/*
    HDC DeviceContext   Device context for drawing
    RECT *ClientRect    rectangle within the window that we will draw to
    int X               x coordinate of leftmost pixel
    int Y               y coordinate of bottom pixel
    int DrawWidth       Width of drawing area
    int DrawHeight      Height of drawing area
*/
static void UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int DrawWidth, int DrawHeight) {

    int WindowWidth = ClientRect->right - ClientRect->left;
    int WindowHeight = ClientRect->bottom - ClientRect->top;

    // copies one rectangle to another, handling scaling if necessary
    StretchDIBits(DeviceContext,
        /*X, Y, Width, Height,
        X, Y, Width, Height,*/ // this is identical because the window is going to be entirely redrawn, and the buffer is sized the same as the window
        0, 0, BitmapWidth, BitmapHeight,
        0, 0, WindowWidth, WindowHeight,
        BitmapMemory,
        &BitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY);
}

/// <summary>
/// 
/// main callback for communication with Windows
/// </summary>
/// <param name="WindowHandle">handle to our window</param>
/// <param name="Message">message code from Windows</param>
/// <param name="wParam">message parameter 1 (LPTR)</param>
/// <param name="lParam">message parameter 1 (LPTR)</param>
/// <returns></returns>
LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam) {
    LRESULT Result = 0;

    switch (Message) {

        case WM_SIZE:       // window size has changed
        {
            OutputDebugStringA("WM_SIZE: window size message received\n");

            RECT ClientRect;
            GetClientRect(Window, &ClientRect); // gets the window's content area (excludes borders, standard buttons, etc)
            int Width = ClientRect.right - ClientRect.left;
            int Height = ClientRect.top - ClientRect.bottom;
            Win32ResizeDIBSection(Width, Height);
            break;
        }

        case WM_CLOSE:      // window close command has been issued
        {
            OutputDebugStringA("WM_CLOSE: window closed message received\n");
            Running = false;
            break;
        }

        case WM_ACTIVATEAPP: // the user has focused the window
        {
            OutputDebugStringA("WM_ACTIVATEAPP: window activated message received\n");
            break;
        }

        case WM_DESTROY:    // window has been destroyed
        {
            OutputDebugStringA("WM_DESTROY: window destroyed message received\n");
            break;
        }

        case WM_PAINT: // Called whenever the window contents need to be repainted
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);

            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            RECT ClientRect;

            GetClientRect(Window, &ClientRect);
            UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);

            EndPaint(Window, &Paint);
            break;
        }

        default:
        {
            OutputDebugStringA("Window message default \n");
            Result = DefWindowProcW(Window, Message, wParam, lParam);  // the default window procedure for Windows
            break;
        }
    }

    return Result;
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode) {
    // MessageBoxW(0, L"Handmade Hero has run.　日本語に弱いです", L"Handmade Hero　日本語", MB_OK);

    WNDCLASS WindowClass = {};
    /*
    WindowClass.style =
        CS_OWNDC |             // specifies that this window gets its own dedicated device context for drawing
        CS_HREDRAW |           // redraw the window if its horizontal size changes
        CS_VREDRAW;            // redraw the window if its verical size changes*/
    WindowClass.lpfnWndProc = Win32MainWindowCallback;   // passes information to a specified Windows procedure to manipulate this window's behavior or appearance
    WindowClass.hInstance = Instance;

    WindowClass.lpszClassName = L"HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass)) {
        HWND WindowHandle;
        WindowHandle =
            CreateWindowEx(
                0,
                WindowClass.lpszClassName,
                L"Handmade Hero Tutorial Series",
                WS_OVERLAPPEDWINDOW | WS_VISIBLE, // contains a number of common styles
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                Instance,
                0);

        if (WindowHandle) {

            int XOffset = 0;
            int YOffset = 0;
            Running = true;

            // start the standard message loop for communication with Windows
            while (Running) {
                MSG Message;
                BOOL MessageResult = GetMessage(&Message, 0, 0, 0);// Should it be GetMessageW to specify unicode?
                if (MessageResult > 0) {
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                HDC DeviceContext = GetDC(WindowHandle);
                RECT ClientRect;
                GetClientRect(WindowHandle, &ClientRect);
                int WindowWidth = ClientRect.right - ClientRect.left;
                int WindowHeight = ClientRect.bottom - ClientRect.top;
                UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);
                ReleaseDC(WindowHandle, DeviceContext);

                ++XOffset;
                YOffset += 2;
            }
        } else { // log failure(?) failure here is extremely rare

            return(0);
        }
    }
}