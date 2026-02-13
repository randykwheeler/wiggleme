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
#include <chrono>


#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Gdiplus;

// --- Globals ---
std::atomic<bool> g_bRunning(false);
HWND g_hPitchSlider, g_hYawSlider, g_hIntervalSlider, g_hDelaySlider;
HWND g_hHumanCheck, g_hChaosCheck, g_hStartBtn, g_hStatus;
int g_pitch = 10, g_yaw = 10;
int g_delay = 5; // Default 5s
double g_interval = 5.0;
bool g_bHumanMode = true;
bool g_bChaosMode = false;

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

// --- WindMouse Algorithm (Advanced Human Physics) ---
void WindMouse(double startX, double startY, double endX, double endY, 
               double gravity, double wind, double minWait, double maxWait, 
               double maxStep, double targetArea) {
    double dist = hypot(endX - startX, endY - startY);
    double windX = 0, windY = 0, vx = 0, vy = 0;
    double cx = startX, cy = startY;

    while (dist > 1.0 && g_bRunning) {
        wind = (double)min((int)wind, (int)dist);
        if (dist >= targetArea) {
            windX = windX / sqrt(3) + (rand() % (int)(wind * 2 + 1) - wind) / sqrt(5);
            windY = windY / sqrt(3) + (rand() % (int)(wind * 2 + 1) - wind) / sqrt(5);
        } else {
            windX /= 2;
            windY /= 2;
            if (maxStep < 3) maxStep = (double)(rand() % 3 + 3);
            else maxStep /= 1.5;
        }

        vx += windX + gravity * (endX - cx) / dist;
        vy += windY + gravity * (endY - cy) / dist;

        double vmag = hypot(vx, vy);
        if (vmag > maxStep) {
            double scale = (maxStep / 2.0 + (rand() % (int)(maxStep / 2.0 + 1))) / vmag;
            vx *= scale;
            vy *= scale;
        }

        cx += vx;
        cy += vy;
        SetCursorPos((int)cx, (int)cy);

        dist = hypot(endX - cx, endY - cy);
        double wait = minWait + (rand() % (int)(maxWait - minWait + 1));
        Sleep((DWORD)wait);
    }
}

// --- Wiggler Thread ---
void WigglerLoop(HWND hMain) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    POINT lastSetPos;
    GetCursorPos(&lastSetPos);
    auto startTime = std::chrono::steady_clock::now();

    while (g_bRunning) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 1000.0;

        int p = g_pitch;
        int y = g_yaw;
        double interval = g_interval;

        // Safety Trigger: Check if the user moved the mouse manually
        // GRACE PERIOD: Ignore all movement for the first g_delay seconds
        POINT cur; GetCursorPos(&cur);
        if (elapsed >= (double)g_delay) {
            if (abs(cur.x - lastSetPos.x) > 30 || abs(cur.y - lastSetPos.y) > 30) {
                g_bRunning = false;
                PostMessage(hMain, WM_COMMAND, 107, 0); // Trigger stop signal
                break;
            }
        } else {
            // Keep lastSetPos synced during grace period to prevent "snap-back" trigger immediately after
            lastSetPos = cur;
        }

        if (elapsed >= (double)g_delay) {
            if (g_bChaosMode) {
                int targetX = rand() % screenW;
                int targetY = rand() % screenH;
                WindMouse((double)cur.x, (double)cur.y, (double)targetX, (double)targetY, 9.0, 3.0, 1.0, 5.0, 12.0, 10.0);
                GetCursorPos(&lastSetPos);
            } else if (p > 0 || y > 0) {
                if (g_bHumanMode) {
                    int dx = (y > 0) ? (rand() % (y * 2)) - y : 0;
                    int dy = (p > 0) ? (rand() % (p * 2)) - p : 0;
                    WindMouse((double)cur.x, (double)cur.y, (double)(cur.x + dx), (double)(cur.y + dy), 9.0, 3.0, 2.0, 10.0, 10.0, 8.0);
                    GetCursorPos(&lastSetPos);
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
        }

        DWORD stop = GetTickCount() + (DWORD)(interval * 1000);
        while (GetTickCount() < stop && g_bRunning) {
            GetCursorPos(&cur);
            auto nowCheck = std::chrono::steady_clock::now();
            double elapsedCheck = std::chrono::duration_cast<std::chrono::milliseconds>(nowCheck - startTime).count() / 1000.0;
            
            if (elapsedCheck >= (double)g_delay) {
                if (abs(cur.x - lastSetPos.x) > 30 || abs(cur.y - lastSetPos.y) > 30) {
                    g_bRunning = false;
                    PostMessage(hMain, WM_COMMAND, 107, 0);
                    break;
                }
            } else {
                lastSetPos = cur;
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

        CreateWindow(L"STATIC", L"Start Delay (Seconds):", WS_CHILD | WS_VISIBLE, 20, 200, 150, 20, hwnd, NULL, NULL, NULL);
        g_hDelaySlider = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 20, 220, 200, 30, hwnd, (HMENU)108, NULL, NULL);
        SendMessage(g_hDelaySlider, TBM_SETRANGE, TRUE, MAKELONG(2, 10));
        SendMessage(g_hDelaySlider, TBM_SETPOS, TRUE, 5);

        g_hHumanCheck = CreateWindow(L"BUTTON", L"Human Mode (Natural)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 260, 200, 20, hwnd, (HMENU)104, NULL, NULL);
        SendMessage(g_hHumanCheck, BM_SETCHECK, BST_CHECKED, 0);

        g_hChaosCheck = CreateWindow(L"BUTTON", L"CHAOS MODE!", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 290, 200, 20, hwnd, (HMENU)106, NULL, NULL);

        g_hStartBtn = CreateWindow(L"BUTTON", L"WIGGLE ME!", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 320, 200, 50, hwnd, (HMENU)105, NULL, NULL);
        
        g_hStatus = CreateWindow(L"STATIC", L"Move mouse to stop!", WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 380, 200, 20, hwnd, NULL, NULL, NULL);

        // Load Mascot
        wchar_t exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);
        PathRemoveFileSpec(exePath);
        std::wstring splashPath = std::wstring(exePath) + L"\\Resources\\mascot.png";
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
        g_delay = (int)SendMessage(g_hDelaySlider, TBM_GETPOS, 0, 0);
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

    HWND hwnd = CreateWindowEx(0, L"WiggleMeClass", L"Wiggle Me!", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 380, 460, NULL, NULL, hInst, NULL);
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
