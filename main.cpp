#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <string>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <shlwapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Gdiplus;

// --- Globals ---
std::atomic<bool> g_bRunning(false);
std::atomic<int> g_pitch(5);
std::atomic<int> g_yaw(5);
std::atomic<double> g_interval(5.0);
std::atomic<bool> g_bHumanMode(true);
std::atomic<bool> g_bChaosMode(false);

HWND g_hPitchSlider, g_hYawSlider, g_hIntervalSlider, g_hHumanCheck, g_hChaosCheck, g_hStartBtn;
HWND g_hStatus;
Image* g_pMascotImage = NULL;
ULONG_PTR gdiplusToken;

// --- Bezier Logic ---
struct Point2D { double x, y; };
Point2D CubicBezier(Point2D p0, Point2D p1, Point2D p2, Point2D p3, double t) {
    double u = 1 - t; double tt = t * t; double uu = u * u;
    double uuu = uu * u; double ttt = tt * t;
    Point2D p;
    p.x = uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x;
    p.y = uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y;
    return p;
}

// --- Wiggler Thread ---
void WigglerLoop(HWND hMain) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    POINT lastSetPos;
    GetCursorPos(&lastSetPos);

    while (g_bRunning) {
        int p = g_pitch;
        int y = g_yaw;
        double interval = g_interval;

        // Safety Trigger: Check if the user moved the mouse manually
        POINT cur; GetCursorPos(&cur);
        if (abs(cur.x - lastSetPos.x) > 15 || abs(cur.y - lastSetPos.y) > 15) {
            g_bRunning = false;
            PostMessage(hMain, WM_COMMAND, 107, 0); // Trigger stop signal
            break;
        }

        if (g_bChaosMode) {
            int targetX = rand() % screenW;
            int targetY = rand() % screenH;
            int steps = 10;
            for (int i = 0; i <= steps && g_bRunning; ++i) {
                int nx = cur.x + (targetX - cur.x) * i / steps;
                int ny = cur.y + (targetY - cur.y) * i / steps;
                SetCursorPos(nx, ny);
                lastSetPos.x = nx; lastSetPos.y = ny;
                Sleep(5);
            }
        } else if (p > 0 || y > 0) {
            if (g_bHumanMode) {
                int dx = (y > 0) ? (rand() % (y * 2)) - y : 0;
                int dy = (p > 0) ? (rand() % (p * 2)) - p : 0;
                Point2D p0 = {(double)cur.x, (double)cur.y};
                Point2D p3 = {p0.x + dx, p0.y + dy};
                Point2D p1 = {p0.x + dx / 2.0 + (rand() % 10 - 5), p0.y + dy / 2.0 + (rand() % 10 - 5)};
                Point2D p2 = {p3.x - dx / 2.0 + (rand() % 10 - 5), p3.y - dy / 2.0 + (rand() % 10 - 5)};
                int steps = 15;
                for (int i = 0; i <= steps && g_bRunning; ++i) {
                    Point2D pt = CubicBezier(p0, p1, p2, p3, (double)i / steps);
                    SetCursorPos((int)pt.x, (int)pt.y);
                    lastSetPos.x = (int)pt.x; lastSetPos.y = (int)pt.y;
                    Sleep(10);
                }
            } else {
                int dx = (y > 0) ? (rand() % (y * 2 + 1)) - y : 0;
                int dy = (p > 0) ? (rand() % (p * 2 + 1)) - p : 0;
                mouse_event(MOUSEEVENTF_MOVE, dx, dy, 0, 0);
                GetCursorPos(&lastSetPos);
                Sleep(50);
                mouse_event(MOUSEEVENTF_MOVE, -dx, -dy, 0, 0);
                GetCursorPos(&lastSetPos);
            }
        }

        DWORD stop = GetTickCount() + (DWORD)(interval * 1000);
        while (GetTickCount() < stop && g_bRunning) {
            GetCursorPos(&cur);
            if (abs(cur.x - lastSetPos.x) > 15 || abs(cur.y - lastSetPos.y) > 15) {
                g_bRunning = false;
                PostMessage(hMain, WM_COMMAND, 107, 0);
                break;
            }
            Sleep(100);
        }
    }
}

// --- Window Procedure ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        // Controls
        CreateWindow(L"STATIC", L"Pitch (Vertical):", WS_CHILD | WS_VISIBLE, 20, 20, 150, 20, hwnd, NULL, NULL, NULL);
        g_hPitchSlider = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 20, 40, 200, 30, hwnd, (HMENU)101, NULL, NULL);
        SendMessage(g_hPitchSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessage(g_hPitchSlider, TBM_SETPOS, TRUE, 10);

        CreateWindow(L"STATIC", L"Yaw (Horizontal):", WS_CHILD | WS_VISIBLE, 20, 80, 150, 20, hwnd, NULL, NULL, NULL);
        g_hYawSlider = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 20, 100, 200, 30, hwnd, (HMENU)102, NULL, NULL);
        SendMessage(g_hYawSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessage(g_hYawSlider, TBM_SETPOS, TRUE, 10);

        CreateWindow(L"STATIC", L"Interval (Seconds):", WS_CHILD | WS_VISIBLE, 20, 140, 150, 20, hwnd, NULL, NULL, NULL);
        g_hIntervalSlider = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 20, 160, 200, 30, hwnd, (HMENU)103, NULL, NULL);
        SendMessage(g_hIntervalSlider, TBM_SETRANGE, TRUE, MAKELONG(1, 60));
        SendMessage(g_hIntervalSlider, TBM_SETPOS, TRUE, 5);

        g_hHumanCheck = CreateWindow(L"BUTTON", L"Human Mode (Natural)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 210, 200, 20, hwnd, (HMENU)104, NULL, NULL);
        SendMessage(g_hHumanCheck, BM_SETCHECK, BST_CHECKED, 0);

        g_hChaosCheck = CreateWindow(L"BUTTON", L"CHAOS MODE!", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 240, 200, 20, hwnd, (HMENU)106, NULL, NULL);

        g_hStartBtn = CreateWindow(L"BUTTON", L"WIGGLE ME!", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 280, 200, 50, hwnd, (HMENU)105, NULL, NULL);
        
        g_hStatus = CreateWindow(L"STATIC", L"Move mouse to stop!", WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 340, 200, 20, hwnd, NULL, NULL, NULL);

        // Load Mascot
        wchar_t exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);
        PathRemoveFileSpec(exePath);
        std::wstring splashPath = std::wstring(exePath) + L"\\Resources\\mascot.jpg";
        g_pMascotImage = Image::FromFile(splashPath.c_str());

        // Set font
        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessage(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
        }, (LPARAM)hFont);

        // Animation Timer
        SetTimer(hwnd, 1, 50, NULL);

        return 0;
    }
    case WM_TIMER: {
        if (g_bRunning) {
            RECT rcMascot = { 220, 20, 360, 160 };
            InvalidateRect(hwnd, &rcMascot, FALSE);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_pMascotImage) {
            Graphics graphics(hdc);
            int ox = g_bRunning ? (rand() % 6 - 3) : 0;
            int oy = g_bRunning ? (rand() % 6 - 3) : 0;
            graphics.DrawImage(g_pMascotImage, 230 + ox, 20 + oy, 120, 120);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_HSCROLL: {
        g_pitch = (int)SendMessage(g_hPitchSlider, TBM_GETPOS, 0, 0);
        g_yaw = (int)SendMessage(g_hYawSlider, TBM_GETPOS, 0, 0);
        g_interval = (double)SendMessage(g_hIntervalSlider, TBM_GETPOS, 0, 0);
        return 0;
    }
    case WM_COMMAND: {
        int cmdId = LOWORD(wParam);
        if (cmdId == 105) { // Button
            if (!g_bRunning) {
                g_bRunning = true;
                g_bHumanMode = (SendMessage(g_hHumanCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_bChaosMode = (SendMessage(g_hChaosCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SetWindowText(g_hStartBtn, L"STOP!");
                SetWindowText(g_hStatus, L"Wiggling... Move mouse to stop.");
                std::thread(WigglerLoop, hwnd).detach();
            } else {
                g_bRunning = false;
                SetWindowText(g_hStartBtn, L"WIGGLE ME!");
                SetWindowText(g_hStatus, L"Phew, stopped.");
                InvalidateRect(hwnd, NULL, TRUE); // Reset mascot position
            }
        }
        else if (cmdId == 107) { // Safety stop signal
            g_bRunning = false;
            SetWindowText(g_hStartBtn, L"WIGGLE ME!");
            SetWindowText(g_hStatus, L"Manual override detected!");
            InvalidateRect(hwnd, NULL, TRUE);
        }
        if (cmdId == 106) { // Chaos Checkbox toggled
             g_bChaosMode = (SendMessage(g_hChaosCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        return 0;
    }
    case WM_DESTROY:
        g_bRunning = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);
    srand((unsigned)time(0));

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WiggleMeClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, L"WiggleMeClass", L"Wiggle Me!", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 380, 420, NULL, NULL, hInst, NULL);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (g_pMascotImage) delete g_pMascotImage;
    GdiplusShutdown(gdiplusToken);
    return 0;
}
