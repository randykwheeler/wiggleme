#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <shlwapi.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <shellapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

// --- Algorithm IDs ---
#define ALGO_WINDMOUSE      0
#define ALGO_ENHANCED_WM    1
#define ALGO_BEZIER          2
#define ALGO_FITTS           3

// --- Custom messages ---
#define WM_UPDATE_STATUS (WM_APP + 1)
#define WM_TRAYICON      (WM_APP + 2)
#define IDI_TRAYICON     1001

// --- Globals ---
std::atomic<bool> g_bRunning(false);
HWND g_hPitchSlider, g_hYawSlider, g_hIntervalSlider, g_hDelaySlider;
HWND g_hPitchVal, g_hYawVal, g_hIntervalVal, g_hDelayVal;
HWND g_hAlgoCombo, g_hChaosCheck, g_hTopCheck, g_hStartBtn, g_hStatus;
std::atomic<int> g_pitch(10), g_yaw(10);
std::atomic<int> g_delay(5);
std::atomic<double> g_interval(5.0);
std::atomic<int> g_algorithm(ALGO_WINDMOUSE);
std::atomic<bool> g_bChaosMode(false);
std::atomic<int> g_overrideThreshold(30);

HFONT g_hFont = NULL;
NOTIFYICONDATA g_nid = {};
bool g_bTrayActive = false;

// ============================================================================
// Perlin Noise (1D, used by Enhanced WindMouse for smooth jitter)
// ============================================================================
static int perm[512];
static bool permInitialized = false;

static void InitPerlin() {
    if (permInitialized) return;
    int p[256];
    for (int i = 0; i < 256; i++) p[i] = i;
    for (int i = 255; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = p[i]; p[i] = p[j]; p[j] = tmp;
    }
    for (int i = 0; i < 512; i++) perm[i] = p[i & 255];
    permInitialized = true;
}

static double Fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static double Lerp(double a, double b, double t) { return a + t * (b - a); }
static double Grad1D(int hash, double x) { return (hash & 1) ? x : -x; }

static double Perlin1D(double x) {
    InitPerlin();
    int xi = (int)floor(x) & 255;
    double xf = x - floor(x);
    double u = Fade(xf);
    return Lerp(Grad1D(perm[xi], xf), Grad1D(perm[xi + 1], xf - 1.0), u);
}

// ============================================================================
// DPI helper
// ============================================================================
static int GetDpiThreshold(HWND hwnd) {
    UINT dpi = 96;
    HMODULE hUser32 = GetModuleHandle(L"user32.dll");
    if (hUser32) {
        typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);
        auto pGetDpi = (GetDpiForWindowFunc)GetProcAddress(hUser32, "GetDpiForWindow");
        if (pGetDpi) dpi = pGetDpi(hwnd);
    }
    return MulDiv(30, dpi, 96);
}

// ============================================================================
// System Tray helpers
// ============================================================================
static void AddTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = IDI_TRAYICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Wiggle Me!");
    Shell_NotifyIcon(NIM_ADD, &g_nid);
    g_bTrayActive = true;
}

static void RemoveTrayIcon() {
    if (g_bTrayActive) {
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
        g_bTrayActive = false;
    }
}

// ============================================================================
// Algorithm 1: WindMouse (Original)
// ============================================================================
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
            int halfStep = max(1, (int)(maxStep / 2.0 + 1));
            double scale = (maxStep / 2.0 + (rand() % halfStep)) / vmag;
            vx *= scale;
            vy *= scale;
        }

        cx += vx;
        cy += vy;
        SetCursorPos((int)cx, (int)cy);

        dist = hypot(endX - cx, endY - cy);
        double wait = minWait + (rand() % max(1, (int)(maxWait - minWait + 1)));
        Sleep((DWORD)wait);
    }
}

// ============================================================================
// Algorithm 2: Enhanced WindMouse (Adaptive Speed + Perlin Jitter + Pauses)
// ============================================================================
void EnhancedWindMouse(double startX, double startY, double endX, double endY) {
    double totalDist = hypot(endX - startX, endY - startY);
    if (totalDist < 1.0) return;

    double gravity = 9.0, windMag = 3.0;
    double maxStep = 15.0, targetArea = max(12.0, totalDist * 0.15);
    double windX = 0, windY = 0, vx = 0, vy = 0;
    double cx = startX, cy = startY;
    double noiseT = (double)(rand() % 1000);

    while (g_bRunning) {
        double dist = hypot(endX - cx, endY - cy);
        if (dist <= 1.0) break;

        double speedFactor = min(1.0, dist / totalDist + 0.2);
        double curMaxStep = maxStep * speedFactor;

        double w = min(windMag, dist);
        if (dist >= targetArea) {
            windX = windX / sqrt(3) + (rand() % max(1, (int)(w * 2 + 1)) - w) / sqrt(5);
            windY = windY / sqrt(3) + (rand() % max(1, (int)(w * 2 + 1)) - w) / sqrt(5);
        } else {
            windX /= 2;
            windY /= 2;
            if (curMaxStep < 3) curMaxStep = (double)(rand() % 3 + 3);
            else curMaxStep /= 1.5;
        }

        vx += windX + gravity * (endX - cx) / dist;
        vy += windY + gravity * (endY - cy) / dist;

        double vmag = hypot(vx, vy);
        if (vmag > curMaxStep) {
            int halfStep = max(1, (int)(curMaxStep / 2.0 + 1));
            double scale = (curMaxStep / 2.0 + (rand() % halfStep)) / vmag;
            vx *= scale;
            vy *= scale;
        }

        noiseT += 0.05;
        double jitterX = Perlin1D(noiseT) * 2.5;
        double jitterY = Perlin1D(noiseT + 100.0) * 2.5;

        cx += vx + jitterX;
        cy += vy + jitterY;
        SetCursorPos((int)cx, (int)cy);

        if (rand() % 100 < 3) {
            Sleep((DWORD)(50 + rand() % 150));
        }

        double baseWait = 2.0 + (1.0 - speedFactor) * 8.0;
        Sleep((DWORD)(baseWait + rand() % 4));
    }
}

// ============================================================================
// Algorithm 3: Bezier Curve Path
// ============================================================================
struct Vec2 { double x, y; };

static Vec2 CubicBezier(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, double t) {
    double u = 1 - t, uu = u * u, uuu = uu * u;
    double tt = t * t, ttt = tt * t;
    Vec2 p;
    p.x = uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x;
    p.y = uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y;
    return p;
}

void BezierMove(double startX, double startY, double endX, double endY) {
    double dist = hypot(endX - startX, endY - startY);
    if (dist < 1.0) return;

    Vec2 p0 = { startX, startY };
    Vec2 p3 = { endX, endY };

    double perpX = -(endY - startY) / dist;
    double perpY = (endX - startX) / dist;

    double arc1 = dist * (0.1 + (rand() % 30) / 100.0) * ((rand() % 2) ? 1.0 : -1.0);
    double arc2 = dist * (0.1 + (rand() % 30) / 100.0) * ((rand() % 2) ? 1.0 : -1.0);

    Vec2 p1 = { startX + (endX - startX) * 0.3 + perpX * arc1,
                startY + (endY - startY) * 0.3 + perpY * arc1 };
    Vec2 p2 = { startX + (endX - startX) * 0.7 + perpX * arc2,
                startY + (endY - startY) * 0.7 + perpY * arc2 };

    int steps = max(10, (int)(dist / 2.0));
    steps = min(steps, 300);

    for (int i = 1; i <= steps && g_bRunning; i++) {
        double rawT = (double)i / steps;
        double t = rawT * rawT * (3.0 - 2.0 * rawT);

        Vec2 pos = CubicBezier(p0, p1, p2, p3, t);

        double noise = Perlin1D(rawT * 10.0 + (double)(rand() % 100)) * 1.5;
        pos.x += noise;
        pos.y += Perlin1D(rawT * 10.0 + 50.0) * 1.5;

        SetCursorPos((int)pos.x, (int)pos.y);

        double speed = 1.0 + 4.0 * sin(rawT * 3.14159);
        double wait = max(1.0, 8.0 / speed);
        Sleep((DWORD)wait);
    }
}

// ============================================================================
// Algorithm 4: Fitts's Law + Overshoot
// ============================================================================
void FittsMove(double startX, double startY, double endX, double endY) {
    double dist = hypot(endX - startX, endY - startY);
    if (dist < 1.0) return;

    double targetWidth = 20.0;
    double fittsTime = 100.0 + 150.0 * log2(2.0 * dist / targetWidth + 1.0);
    fittsTime = max(100.0, min(fittsTime, 2000.0));

    bool overshoot = (dist > 100.0) && (rand() % 100 < 60);
    double overshootDist = 0;
    if (overshoot) {
        overshootDist = dist * (0.05 + (rand() % 15) / 100.0);
    }

    double dirX = (endX - startX) / dist;
    double dirY = (endY - startY) / dist;
    double osTargetX = endX + dirX * overshootDist;
    double osTargetY = endY + dirY * overshootDist;

    double moveToX = overshoot ? osTargetX : endX;
    double moveToY = overshoot ? osTargetY : endY;
    double moveDist = hypot(moveToX - startX, moveToY - startY);

    int steps = max(10, (int)(fittsTime / 8.0));
    steps = min(steps, 300);

    double cx = startX, cy = startY;
    for (int i = 1; i <= steps && g_bRunning; i++) {
        double rawT = (double)i / steps;
        double t = 1.0 - pow(1.0 - rawT, 2.5);

        double targetX = startX + (moveToX - startX) * t;
        double targetY = startY + (moveToY - startY) * t;

        double perpX = -(moveToY - startY) / moveDist;
        double perpY = (moveToX - startX) / moveDist;
        double drift = Perlin1D(rawT * 8.0) * (dist * 0.03);
        targetX += perpX * drift;
        targetY += perpY * drift;

        targetX += Perlin1D(rawT * 20.0 + 200.0) * 1.2;
        targetY += Perlin1D(rawT * 20.0 + 300.0) * 1.2;

        cx = targetX;
        cy = targetY;
        SetCursorPos((int)cx, (int)cy);

        double wait = fittsTime / steps;
        Sleep((DWORD)max(1.0, wait));
    }

    if (overshoot && g_bRunning) {
        Sleep((DWORD)(30 + rand() % 70));

        double corrDist = hypot(endX - cx, endY - cy);
        int corrSteps = max(5, (int)(corrDist / 1.5));
        corrSteps = min(corrSteps, 80);

        double corrStartX = cx, corrStartY = cy;
        for (int i = 1; i <= corrSteps && g_bRunning; i++) {
            double rawT = (double)i / corrSteps;
            double t = rawT * rawT * (3.0 - 2.0 * rawT);

            double tx = corrStartX + (endX - corrStartX) * t;
            double ty = corrStartY + (endY - corrStartY) * t;

            tx += Perlin1D(rawT * 15.0 + 400.0) * 0.6;
            ty += Perlin1D(rawT * 15.0 + 500.0) * 0.6;

            SetCursorPos((int)tx, (int)ty);
            Sleep((DWORD)(4 + rand() % 6));
        }
    }

    if (g_bRunning) {
        int landX = (int)endX + (rand() % 3 - 1);
        int landY = (int)endY + (rand() % 3 - 1);
        SetCursorPos(landX, landY);
    }
}

// ============================================================================
// Dispatch: move cursor using the selected algorithm
// ============================================================================
void MoveWithAlgorithm(int algo, double sx, double sy, double ex, double ey) {
    switch (algo) {
    case ALGO_WINDMOUSE:
        WindMouse(sx, sy, ex, ey, 9.0, 3.0, 2.0, 10.0, 10.0, 8.0);
        break;
    case ALGO_ENHANCED_WM:
        EnhancedWindMouse(sx, sy, ex, ey);
        break;
    case ALGO_BEZIER:
        BezierMove(sx, sy, ex, ey);
        break;
    case ALGO_FITTS:
        FittsMove(sx, sy, ex, ey);
        break;
    default:
        WindMouse(sx, sy, ex, ey, 9.0, 3.0, 2.0, 10.0, 10.0, 8.0);
        break;
    }
}

// ============================================================================
// Wiggler Thread
// ============================================================================
void WigglerLoop(HWND hMain) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    POINT lastSetPos;
    GetCursorPos(&lastSetPos);
    auto startTime = std::chrono::steady_clock::now();
    int threshold = g_overrideThreshold.load();

    while (g_bRunning) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 1000.0;

        int p = g_pitch;
        int y = g_yaw;
        double interval = g_interval;
        int algo = g_algorithm;

        POINT cur; GetCursorPos(&cur);

        if (elapsed < (double)g_delay) {
            wchar_t* graceMsg = new wchar_t[100];
            swprintf(graceMsg, 100, L"Grace Period: %.1fs remaining...", (double)g_delay - elapsed);
            PostMessage(hMain, WM_UPDATE_STATUS, 0, (LPARAM)graceMsg);
            lastSetPos = cur;
        } else {
            if (abs(cur.x - lastSetPos.x) > threshold || abs(cur.y - lastSetPos.y) > threshold) {
                g_bRunning = false;
                PostMessage(hMain, WM_COMMAND, 107, 0);
                break;
            }
            wchar_t* statusMsg = new wchar_t[50];
            wcscpy_s(statusMsg, 50, L"Wiggling... Move mouse to stop.");
            PostMessage(hMain, WM_UPDATE_STATUS, 0, (LPARAM)statusMsg);
        }

        if (g_bRunning && elapsed >= (double)g_delay) {
            if (g_bChaosMode) {
                int targetX = rand() % screenW;
                int targetY = rand() % screenH;
                MoveWithAlgorithm(algo, (double)cur.x, (double)cur.y, (double)targetX, (double)targetY);
                GetCursorPos(&lastSetPos);
            } else if (p > 0 || y > 0) {
                int dx = (y > 0) ? (rand() % (y * 2 + 1)) - y : 0;
                int dy = (p > 0) ? (rand() % (p * 2 + 1)) - p : 0;
                MoveWithAlgorithm(algo, (double)cur.x, (double)cur.y, (double)(cur.x + dx), (double)(cur.y + dy));
                GetCursorPos(&lastSetPos);
            }
        }

        ULONGLONG stop = GetTickCount64() + (ULONGLONG)(interval * 1000);
        while (GetTickCount64() < stop && g_bRunning) {
            GetCursorPos(&cur);
            auto nowCheck = std::chrono::steady_clock::now();
            double elap = std::chrono::duration_cast<std::chrono::milliseconds>(nowCheck - startTime).count() / 1000.0;

            if (elap < (double)g_delay) {
                lastSetPos = cur;
                wchar_t* graceMsg = new wchar_t[100];
                swprintf(graceMsg, 100, L"Grace Period: %.1fs remaining...", (double)g_delay - elap);
                PostMessage(hMain, WM_UPDATE_STATUS, 0, (LPARAM)graceMsg);
            } else {
                if (abs(cur.x - lastSetPos.x) > threshold || abs(cur.y - lastSetPos.y) > threshold) {
                    g_bRunning = false;
                    PostMessage(hMain, WM_COMMAND, 107, 0);
                    break;
                }
            }
            Sleep(100);
        }
    }
}

// ============================================================================
// Slider value label update helper
// ============================================================================
static void UpdateSliderLabels() {
    wchar_t buf[32];
    swprintf(buf, 32, L"%d px", g_pitch.load());
    SetWindowText(g_hPitchVal, buf);
    swprintf(buf, 32, L"%d px", g_yaw.load());
    SetWindowText(g_hYawVal, buf);
    swprintf(buf, 32, L"%d s", (int)g_interval.load());
    SetWindowText(g_hIntervalVal, buf);
    swprintf(buf, 32, L"%d s", g_delay.load());
    SetWindowText(g_hDelayVal, buf);
}

// ============================================================================
// Window Procedure
// ============================================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        g_hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        // --- Pitch ---
        CreateWindow(L"STATIC", L"Pitch (Vertical):", WS_CHILD | WS_VISIBLE, 20, 20, 150, 20, hwnd, NULL, NULL, NULL);
        g_hPitchVal = CreateWindow(L"STATIC", L"10 px", WS_CHILD | WS_VISIBLE | SS_RIGHT, 230, 20, 50, 20, hwnd, NULL, NULL, NULL);
        g_hPitchSlider = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 20, 40, 260, 30, hwnd, (HMENU)101, NULL, NULL);
        SendMessage(g_hPitchSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessage(g_hPitchSlider, TBM_SETPOS, TRUE, 10);

        // --- Yaw ---
        CreateWindow(L"STATIC", L"Yaw (Horizontal):", WS_CHILD | WS_VISIBLE, 20, 75, 150, 20, hwnd, NULL, NULL, NULL);
        g_hYawVal = CreateWindow(L"STATIC", L"10 px", WS_CHILD | WS_VISIBLE | SS_RIGHT, 230, 75, 50, 20, hwnd, NULL, NULL, NULL);
        g_hYawSlider = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 20, 95, 260, 30, hwnd, (HMENU)102, NULL, NULL);
        SendMessage(g_hYawSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessage(g_hYawSlider, TBM_SETPOS, TRUE, 10);

        // --- Interval ---
        CreateWindow(L"STATIC", L"Interval:", WS_CHILD | WS_VISIBLE, 20, 130, 150, 20, hwnd, NULL, NULL, NULL);
        g_hIntervalVal = CreateWindow(L"STATIC", L"5 s", WS_CHILD | WS_VISIBLE | SS_RIGHT, 230, 130, 50, 20, hwnd, NULL, NULL, NULL);
        g_hIntervalSlider = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 20, 150, 260, 30, hwnd, (HMENU)103, NULL, NULL);
        SendMessage(g_hIntervalSlider, TBM_SETRANGE, TRUE, MAKELONG(1, 60));
        SendMessage(g_hIntervalSlider, TBM_SETPOS, TRUE, 5);

        // --- Start Delay ---
        CreateWindow(L"STATIC", L"Start Delay:", WS_CHILD | WS_VISIBLE, 20, 185, 150, 20, hwnd, NULL, NULL, NULL);
        g_hDelayVal = CreateWindow(L"STATIC", L"5 s", WS_CHILD | WS_VISIBLE | SS_RIGHT, 230, 185, 50, 20, hwnd, NULL, NULL, NULL);
        g_hDelaySlider = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 20, 205, 260, 30, hwnd, (HMENU)108, NULL, NULL);
        SendMessage(g_hDelaySlider, TBM_SETRANGE, TRUE, MAKELONG(2, 10));
        SendMessage(g_hDelaySlider, TBM_SETPOS, TRUE, 5);

        // --- Algorithm Selection ---
        CreateWindow(L"STATIC", L"Algorithm:", WS_CHILD | WS_VISIBLE, 20, 245, 80, 20, hwnd, NULL, NULL, NULL);
        g_hAlgoCombo = CreateWindow(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            100, 242, 180, 150, hwnd, (HMENU)109, NULL, NULL);
        SendMessage(g_hAlgoCombo, CB_ADDSTRING, 0, (LPARAM)L"WindMouse (Classic)");
        SendMessage(g_hAlgoCombo, CB_ADDSTRING, 0, (LPARAM)L"Enhanced WindMouse");
        SendMessage(g_hAlgoCombo, CB_ADDSTRING, 0, (LPARAM)L"Bezier Curves");
        SendMessage(g_hAlgoCombo, CB_ADDSTRING, 0, (LPARAM)L"Fitts's Law + Overshoot");
        SendMessage(g_hAlgoCombo, CB_SETCURSEL, 0, 0);

        // --- Checkboxes ---
        g_hChaosCheck = CreateWindow(L"BUTTON", L"CHAOS MODE!", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 275, 130, 20, hwnd, (HMENU)106, NULL, NULL);
        g_hTopCheck = CreateWindow(L"BUTTON", L"Always on Top", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 155, 275, 125, 20, hwnd, (HMENU)110, NULL, NULL);

        // --- Start Button ---
        g_hStartBtn = CreateWindow(L"BUTTON", L"WIGGLE ME!", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 305, 260, 50, hwnd, (HMENU)105, NULL, NULL);

        // --- Status ---
        g_hStatus = CreateWindow(L"STATIC", L"Ready. Move mouse to stop while active.", WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 365, 260, 20, hwnd, NULL, NULL, NULL);

        // Compute DPI-aware override threshold
        g_overrideThreshold = GetDpiThreshold(hwnd);

        // Set font on all children
        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessage(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
        }, (LPARAM)g_hFont);

        return 0;
    }
    case WM_HSCROLL: {
        g_pitch = (int)SendMessage(g_hPitchSlider, TBM_GETPOS, 0, 0);
        g_yaw = (int)SendMessage(g_hYawSlider, TBM_GETPOS, 0, 0);
        g_interval = (double)SendMessage(g_hIntervalSlider, TBM_GETPOS, 0, 0);
        g_delay = (int)SendMessage(g_hDelaySlider, TBM_GETPOS, 0, 0);
        UpdateSliderLabels();
        return 0;
    }
    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) {
            AddTrayIcon(hwnd);
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    }
    case WM_TRAYICON: {
        if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            RemoveTrayIcon();
        }
        return 0;
    }
    case WM_COMMAND: {
        int cmdId = LOWORD(wParam);
        if (cmdId == 105) { // Start/Stop Button
            if (!g_bRunning) {
                g_bRunning = true;
                g_algorithm = (int)SendMessage(g_hAlgoCombo, CB_GETCURSEL, 0, 0);
                g_bChaosMode = (SendMessage(g_hChaosCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                {
                    wchar_t lockText[50];
                    swprintf(lockText, 50, L"STOP (Lock: %ds)", g_delay.load());
                    SetWindowText(g_hStartBtn, lockText);
                }
                EnableWindow(g_hStartBtn, FALSE);
                EnableWindow(g_hAlgoCombo, FALSE);
                std::thread([](HWND btn, HWND hwnd, int delay) {
                    Sleep(delay * 1000);
                    if (g_bRunning) {
                        SetWindowText(btn, L"STOP!");
                        EnableWindow(btn, TRUE);
                    }
                }, g_hStartBtn, hwnd, g_delay.load()).detach();

                SetWindowText(g_hStatus, L"Wiggling... Move mouse to stop.");
                std::thread(WigglerLoop, hwnd).detach();
            } else {
                g_bRunning = false;
                SetWindowText(g_hStartBtn, L"WIGGLE ME!");
                SetWindowText(g_hStatus, L"Phew, stopped.");
                EnableWindow(g_hAlgoCombo, TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        else if (cmdId == 107) { // Safety stop signal
            g_bRunning = false;
            SetWindowText(g_hStartBtn, L"WIGGLE ME!");
            EnableWindow(g_hStartBtn, TRUE);
            EnableWindow(g_hAlgoCombo, TRUE);
            SetWindowText(g_hStatus, L"Manual override detected!");
            InvalidateRect(hwnd, NULL, TRUE);
        }
        if (cmdId == 106) {
            g_bChaosMode = (SendMessage(g_hChaosCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        if (cmdId == 110) { // Always on Top
            bool onTop = (SendMessage(g_hTopCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SetWindowPos(hwnd, onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        return 0;
    }
    case WM_UPDATE_STATUS: {
        wchar_t* msg = (wchar_t*)lParam;
        if (msg) {
            SetWindowText(g_hStatus, msg);
            delete[] msg;
        }
        return 0;
    }
    case WM_DESTROY:
        g_bRunning = false;
        RemoveTrayIcon();
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
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

    HWND hwnd = CreateWindowEx(0, L"WiggleMeClass", L"Wiggle Me!",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 440, NULL, NULL, hInst, NULL);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
