// ������� ��������� ������ � ������ �������� ���������� Windows
//                  
//                          LED-SSD
//                           
//          WIN7 � ������, x32, x64, C++ 14, RU EN unicode             
//                          
// "��������� ��" = 30 ��� ����� � ����������������� ��� Windows �� C++
//                  (C) Vinni, ������ 2025 �.
//

//https://chatgpt.com/share/67f58dc1-9378-8001-ada7-08f38eedde20

#define WINVER _WIN32_WINNT_WIN7
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define NTDDI_VERSION NTDDI_WIN7
//#define WIN32_LEAN_AND_MEAN
#define NOCOMM

#include "resource.h"
#include <windows.h>
#include <pdh.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <string>
#include <gdiplus.h>
using namespace Gdiplus;
ULONG_PTR gdiplusToken;


#ifdef _DEBUG
    #include <fstream>
    #include <iostream>
    import std;
#endif
#pragma comment(lib, "pdh.lib")         // ������ �� ����������
#pragma comment(lib, "Wtsapi32.lib" )   // ������ � �������
#pragma comment(lib, "gdiplus.lib")     // ����������� ������ � ����

#define hKey HKEY_CURRENT_USER

// GUID - ���������� ������������� ������
class __declspec(uuid("8a002844-4745-4336-a9a1-98ff80bce4c2")) AppIcon;
// ��� ��������
const wchar_t *szwMutex = L"36�85�51e72�4504997�28�0�243101�";
const wchar_t *szWindowClass = L"LED-SSD";
const wchar_t *szPause = L"Pause";
      wchar_t  szTip[128] = L"";
      wchar_t  szTipP[128] = L"";
const wchar_t* szwSelectedDisk = L"_Total";        // ���������� ���� ������
wchar_t readCounterPath[PDH_MAX_COUNTER_PATH];
wchar_t writeCounterPath[PDH_MAX_COUNTER_PATH];
wchar_t LocaleName[LOCALE_NAME_MAX_LENGTH];
const wchar_t* szwAllRun = L"Application alredy run";
const wchar_t* szwWarning = L"Warning!";
const wchar_t* szwUzheRabotaet = L"��������� ��� ��������";
const wchar_t* szwVnimanie = L"��������!";
UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;
HICON hIconIdle, hIconRead, hIconWrite, hIconRW, hIconApp, hIconPause;
NOTIFYICONDATAW nid ={ sizeof(nid) };
HWND window = NULL;
HMENU hMenu, hSubMenu = NULL;
HANDLE monitorThread = NULL;
HANDLE ghExitEvent = NULL;
HANDLE hThis=NULL;
DWORD  dwThreadId = 0;
enum class APP : short { CHECK, UNLOAD, LOAD };
enum class THREAD : short { CHECK, PAUSE, RUN };
static APP CtrlAutoLoad(APP);
static THREAD CtrlThread(THREAD);
bool UserLocale_RU; // ����������� ��� ������� ��� ����������
void ShowContextMenu(HWND hwnd, POINT pt);

#ifdef _DEBUG
    std::wstring nstr = L"";
    void logMessage(const std::wstring& message, const std::wstring& par)
    {
        std::wofstream logFile("LED_SSD.log", std::ios_base::app); // Open log file in append mode
        if (logFile.is_open())
        {
            logFile << message << par << std::endl; // Write message to log file
            logFile.close();
        }
        else {
            std::cerr << "Unable to open log file!" << std::endl; // Error handling
        }
    }
#endif

static inline void InitGDIPlus()
{
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}

static inline void ShutdownGDIPlus()
{
    GdiplusShutdown(gdiplusToken);
}

static inline float scale_05_1(float data ) 
{
    float tmp = (data / 500000000.0) + 0.75;
    return tmp > 1. ? 1 : tmp;
}

static inline float  remove_dc(float  x, float  alpha) // ToDo ���� ������ class
{
    static float  y_prev;  // ���������� �������� ��������
    static float  x_prev;  // ���������� ������� ��������

    float  y = x - x_prev + alpha * y_prev;

    x_prev = x;
    y_prev = y;

    return y;
}

static HICON AdjustIconBrightnessGDIPlus(HICON hIcon, float brightnessFactor) // ToDo ���� ������ class
 {
        Bitmap bmp(hIcon); // ����������� HICON � Bitmap
        static Bitmap result(bmp.GetWidth(), bmp.GetHeight(), PixelFormat32bppARGB); // ������

        Graphics g(&result); // ��������

        // ������� �������
        float b = brightnessFactor; // �������� 0.5-1
        ColorMatrix cm =
        {
            b, 0, 0, 0, 0,
            0, b, 0, 0, 0,
            0, 0, b, 0, 0,
            0, 0, 0, 1, 0,
            0, 0, 0, 0, 1
        };

        ImageAttributes ia;
        ia.SetColorMatrix(&cm, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

        g.DrawImage(&bmp,
            Rect(0, 0, bmp.GetWidth(), bmp.GetHeight()),
            0, 0, bmp.GetWidth(), bmp.GetHeight(),
            UnitPixel, &ia);

        static HICON newIcon; // �������, ����������� ���������� �� �����!
        result.GetHICON(&newIcon);
        return newIcon;
    }




void inline static UpdateTrayIcon(HICON hIcon)
{
    nid.hIcon = hIcon;
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

static DWORD WINAPI MonitorDiskActivity(LPVOID lpParam)
{   // ���������� ���������� ������ ����� �������� ������������������ �������

    PDH_HQUERY hQueryR, hQueryW;
    PDH_HCOUNTER hCounterRead, hCounterWrite;
    PDH_FMT_COUNTERVALUE valueRead, valueWrite;
    float BritesFactorR=0, BritesFactorW=0, BritesFactorRW;

    swprintf_s(readCounterPath, L"%s%s%s", L"\\PhysicalDisk(", szwSelectedDisk, L")\\Disk Read Bytes/sec");
    swprintf_s(writeCounterPath, L"%s%s%s", L"\\PhysicalDisk(", szwSelectedDisk, L")\\Disk Write Bytes/sec");
    
    PdhOpenQuery(NULL, NULL, &hQueryR);
    PdhOpenQuery(NULL, NULL, &hQueryW);
    PdhAddEnglishCounter(hQueryR, readCounterPath, NULL, &hCounterRead);
    PdhAddEnglishCounter(hQueryW, writeCounterPath, NULL, &hCounterWrite);
            
    while (WaitForSingleObject(ghExitEvent, 0) != WAIT_OBJECT_0)
    {
        PdhCollectQueryData(hQueryR);
        PdhCollectQueryData(hQueryW);
        PdhGetFormattedCounterValue(hCounterRead, PDH_FMT_LONG, NULL, &valueRead);
        PdhGetFormattedCounterValue(hCounterWrite, PDH_FMT_LONG, NULL, &valueWrite);

        if (valueRead.longValue > 0 && valueWrite.longValue > 0)    // ������ � �����
        {
            long meanValue = (valueRead.longValue + valueWrite.longValue) / 2;
            BritesFactorRW = scale_05_1(abs(remove_dc(meanValue, 0.995)));
            UpdateTrayIcon(AdjustIconBrightnessGDIPlus(hIconRW, BritesFactorRW));
        }
        else if (valueRead.longValue > 0)                           // ������ ������
            UpdateTrayIcon(hIconRead);
        else if (valueWrite.longValue > 0)                          // ������ �����
            UpdateTrayIcon(hIconWrite);
        else                                                        // �����
            UpdateTrayIcon(hIconIdle);
        Sleep(41);                                                  
    }
    PdhCloseQuery(hQueryR);
    PdhCloseQuery(hQueryW);
    return 0;
}

void ShowContextMenu(HWND hwnd, POINT pt)
{
    if (hMenu)
    {
        hSubMenu = GetSubMenu(hMenu, 0);
        if (hSubMenu)
        {
            // ���� �� �������� ����, ����� ����������� ���� �� ��������
            SetForegroundWindow(hwnd);
            // ������������� ����������� ����
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
                uFlags |= TPM_RIGHTALIGN;
            else
                uFlags |= TPM_LEFTALIGN;
            TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
        }
    }
}

static APP CtrlAutoLoad(APP mode)  
{   // ���������� ������������� ����� ������

    static APP state = APP::UNLOAD;
    static wchar_t szwSubKey[MAX_PATH] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static wchar_t szwPath[MAX_PATH];
    static wchar_t szwKeyValue[MAX_PATH];
    DWORD size = MAX_PATH;
    LSTATUS lResult =0;
    static HKEY hKeyDescriptor = hKey;

    switch (mode)
    {
    case APP::CHECK: // �������� ��������� ������������
        {   // ��������� ����� �������
            lResult = RegOpenKeyEx(hKey, szwSubKey, 0, KEY_READ, &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {   // ���� ������ �������
                lResult = RegGetValue(hKey, szwSubKey, szWindowClass, RRF_RT_REG_SZ, NULL, szwKeyValue, &size);
                if (lResult == ERROR_SUCCESS) // �������� �������� �������
                {   // �������� ������ ���� � �����
                    GetModuleFileName(NULL, szwPath, MAX_PATH);
                    if (wcscmp(szwKeyValue, szwPath) == 0)
                    {   // ������ � ������� ���� � ��������� � ������� ���������� ���������
                        state = APP::LOAD;
                        break;
                    }
                }
                RegCloseKey(hKeyDescriptor);
            }
            state = APP::UNLOAD;
        }
        break;
    case APP::UNLOAD:   // �������� ������ ������������ �������
        lResult = RegOpenKeyEx(hKey, szwSubKey, 0, KEY_ALL_ACCESS, &hKeyDescriptor);
        if (lResult == ERROR_SUCCESS)
        {
            RegDeleteKeyValue(hKey, szwSubKey, szWindowClass);
            RegCloseKey(hKeyDescriptor);
            state = APP::UNLOAD;
        }
        break;
    case APP::LOAD: // �������� ������ ������������ �������
        lResult = RegOpenKeyEx(hKey, szwSubKey, 0, KEY_ALL_ACCESS, &hKeyDescriptor);
        if ( lResult == ERROR_SUCCESS  )
        { 
            RegCreateKeyEx(hKey, szwSubKey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKeyDescriptor, NULL);
            GetModuleFileName(NULL, szwPath, MAX_PATH);
            RegSetValueEx(hKeyDescriptor, szWindowClass, NULL, REG_SZ, (LPBYTE)szwPath, MAX_PATH);
            RegCloseKey(hKeyDescriptor);
            state = APP::LOAD;
        }
        break;
    }
    return state;
}

static THREAD CtrlThread(THREAD mode)
{   // �������� ��������� ����� � �������

    static THREAD state = THREAD::RUN;
    static wchar_t szwSubKey[MAX_PATH] = L"";
    static HKEY hKeyDescriptor = hKey;
    LSTATUS lResult = 0;
    DWORD dwKeyValue=0;
    static DWORD size = sizeof(dwKeyValue);

    // ����� ������� ��������� ��� �����
    swprintf_s(szwSubKey, L"%s%s", L"Software\\", szWindowClass );

    switch (mode)
    {
    case THREAD::CHECK:
        {   // ��������� ����� �������
            lResult = RegOpenKeyEx(hKey, szwSubKey, 0, KEY_READ, &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {   // ���� ������ �������
                lResult = RegGetValue(hKey, szwSubKey, szPause, RRF_RT_DWORD, NULL, &dwKeyValue, &size);
                if (lResult == ERROR_SUCCESS) // �������� �������� �������
                    if (dwKeyValue == 1)
                    {   
                        state = THREAD::PAUSE;
                        break;
                    }
                RegCloseKey(hKeyDescriptor);
            }
            state = THREAD::RUN;
        }
        break;
    case THREAD::PAUSE:
        lResult = RegCreateKeyEx(hKey, szwSubKey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKeyDescriptor, NULL);
        if (lResult == ERROR_SUCCESS)
        {
            lResult = RegOpenKeyEx(hKey, szwSubKey, 0, KEY_ALL_ACCESS, &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {
                dwKeyValue = 1;
                RegSetValueEx(hKeyDescriptor, szPause, NULL, REG_DWORD, (const BYTE*)&dwKeyValue, size);
                RegCloseKey(hKeyDescriptor);
                state = THREAD::PAUSE;
            }
        }
        state = THREAD::RUN;
        break;
    case THREAD::RUN:
        lResult = RegOpenKeyEx(hKey, szwSubKey, 0, KEY_ALL_ACCESS, &hKeyDescriptor);
        if (lResult == ERROR_SUCCESS)
        {
            RegDeleteKeyValue(hKey, szwSubKey, szPause);
            RegCloseKey(hKeyDescriptor);
            state = THREAD::RUN;
        }
        state = THREAD::PAUSE;
        break;
    }
    return state;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND s_hwndFlyout = NULL;
    static BOOL s_fCanShowFlyout = TRUE;

    switch (message)
    {
        case WM_COMMAND:
        {
            int const wmId = LOWORD(wParam);
            // ������ ������� ����:
            switch (wmId)
            {
                case IDM_EXIT:
                    DestroyWindow(hwnd);
                    break;

                case IDM_PAUSE:
                    if (CtrlThread(THREAD::CHECK) == THREAD::RUN)
                    {
                        SuspendThread(monitorThread);
                        CtrlThread(THREAD::PAUSE);
                        SetPriorityClass(hThis, THREAD_MODE_BACKGROUND_BEGIN);
                        CheckMenuItem(hMenu, IDM_PAUSE, MF_CHECKED);
                        lstrcpy(nid.szTip, szTipP);
                        UpdateTrayIcon(hIconPause);
                    }
                    else
                    {
                        lstrcpy(nid.szTip, szTip);
                        ResumeThread(monitorThread);
                        CtrlThread(THREAD::RUN);
                        SetPriorityClass(hThis, THREAD_MODE_BACKGROUND_END);
                        CheckMenuItem(hMenu, IDM_PAUSE, MF_UNCHECKED);
                    }
                    break;

                case IDM_AUTOLOAD:
                    if (CtrlAutoLoad(APP::CHECK) == APP::LOAD)
                    {
                        CtrlAutoLoad(APP::UNLOAD);
                        CheckMenuItem(hMenu, IDM_AUTOLOAD, MF_UNCHECKED);
                    }
                    else
                    {
                        CtrlAutoLoad(APP::LOAD);
                        CheckMenuItem(hMenu, IDM_AUTOLOAD, MF_CHECKED);
                    }
                break;

            default:
                return DefWindowProc(hwnd, message, wParam, lParam);
            }
        }
        break;

        case WMAPP_NOTIFYCALLBACK:
            switch (LOWORD(lParam))
            {
            case NIN_SELECT:
            case WM_CONTEXTMENU:
                {
                    POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
                    ShowContextMenu(hwnd, pt);
                }
                break;
            }
            break;

        case WM_WTSSESSION_CHANGE:
        {
            switch (wParam)
            {
            case WTS_SESSION_LOCK:
                if (CtrlThread(THREAD::CHECK) == THREAD::RUN)
                {
                    SuspendThread(monitorThread);
                    SetPriorityClass(hThis, THREAD_MODE_BACKGROUND_BEGIN);
                }
                break;
            case WTS_SESSION_UNLOCK:
                if (CtrlThread(THREAD::CHECK) != THREAD::PAUSE)
                {
                    ResumeThread(monitorThread);
                    SetPriorityClass(hThis, THREAD_MODE_BACKGROUND_END);
                }
                break;
            }
        }
        break;
        
        case WM_DESTROY:
            ResumeThread(monitorThread);
            SetEvent(ghExitEvent);
            WaitForSingleObject(monitorThread, INFINITE);
            CloseHandle(monitorThread);
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
#ifdef _DEBUG
    try
    {
#endif
        hThis = GetCurrentProcess(); // ����

        // ���� ������������ � �������
        if (GetUserDefaultLocaleName(LocaleName, LOCALE_NAME_MAX_LENGTH) != 0)
            UserLocale_RU = wcscmp(LocaleName, L"ru-RU") == 0 ? TRUE : FALSE;

        // ���������� ������� ������� ���������� ���������
        HANDLE mutex = CreateMutexEx(0, szwMutex, CREATE_MUTEX_INITIAL_OWNER, READ_CONTROL);

        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            if (UserLocale_RU)
                MessageBoxEx(NULL, szwUzheRabotaet, szwVnimanie, MB_OK, 0);
            else
                MessageBoxEx(NULL, szwAllRun, szwWarning, MB_OK, 0);
            return 1;
        }

        // �������� ������ �� ��������
        hIconIdle = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IDLE));
        hIconRead = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_READ));
        hIconWrite = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WRITE));
        hIconRW = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RW));
        hIconApp = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
        hIconPause = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PAUSE));

        // ����������� ������ ����
        WNDCLASSEXW wcex = { sizeof(wcex) };
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_CLASSDC;
        wcex.lpfnWndProc = WindowProc;
        wcex.hInstance = hInstance;
        wcex.hIcon = hIconApp;
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_CONTEXTMENU);
        wcex.lpszClassName = szWindowClass;
        RegisterClassEx(&wcex);

        // �������� ���� �� ��������
        hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDC_CONTEXTMENU));

        // �������� �������� ����
        window = CreateWindowEx(WS_EX_APPWINDOW, szWindowClass, NULL, 0, 0, 0, 0, 0, NULL, hMenu, hInstance, NULL);

        // �������� ������� ���������� ������ �����������
        ghExitEvent = CreateEvent(NULL, TRUE, FALSE, TEXT("ExitEvent"));

        // ������������ ����, ��� ��������� ��������� ������
        WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION);

        // �������� ������ �����������
        if ((monitorThread = CreateThread(NULL, 65536, MonitorDiskActivity, NULL, 0, &dwThreadId)))
            SetPriorityClass(monitorThread, THREAD_PRIORITY_ABOVE_NORMAL );

        // �������� ��������� ��� �����������
        nid.cbSize = sizeof(nid);
        nid.hWnd = window;
        nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
        nid.guidItem = __uuidof(AppIcon);
        nid.dwInfoFlags = NIIF_USER;
        nid.dwState = NIS_SHAREDICON;
        nid.hBalloonIcon = hIconApp;
        nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
        // ������ �����������
        LoadString(hInstance, IDS_ACT, szTip, ARRAYSIZE(szTip));
        LoadString(hInstance, IDS_ACTP, szTipP, ARRAYSIZE(szTipP));
        lstrcpy(nid.szTip, szTip);
        LoadString(hInstance, IDS_ACT, nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle));
        LoadString(hInstance, IDS_INFO, nid.szInfo, ARRAYSIZE(nid.szInfo));
        // ������ ��������
        nid.dwState = NIS_SHAREDICON;
        nid.hIcon = hIconIdle;
        Shell_NotifyIcon(NIM_ADD, &nid);
        nid.dwState = NIS_HIDDEN;
        nid.hIcon = hIconWrite;
        Shell_NotifyIcon(NIM_ADD, &nid);
        nid.hIcon = hIconRW;
        Shell_NotifyIcon(NIM_ADD, &nid);
        nid.hIcon = hIconRead;
        Shell_NotifyIcon(NIM_ADD, &nid);
        nid.hIcon = hIconPause;
        Shell_NotifyIcon(NIM_ADD, &nid);
        // ������ �����������
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIcon(NIM_SETVERSION, &nid);

        if (!UserLocale_RU) // ���� �� �������, ����� ����������
        {
            wchar_t szAutoloadMenu[85], szPauseMenu[85], szExitMenu[85];
            LoadString(hInstance, IDS_AUTOLOAD, szAutoloadMenu, ARRAYSIZE(szAutoloadMenu));
            LoadString(hInstance, IDS_PAUSE, szPauseMenu, ARRAYSIZE(szPauseMenu));
            LoadString(hInstance, IDS_EXIT, szExitMenu, ARRAYSIZE(szExitMenu));
            LoadString(hInstance, IDS_ACTE, szTip, ARRAYSIZE(szTip));
            LoadString(hInstance, IDS_ACTEP, szTipP, ARRAYSIZE(szTipP));
            LoadString(hInstance, IDS_INFOE, nid.szInfo, ARRAYSIZE(nid.szInfo));
            LoadString(hInstance, IDS_APP_DECR_E, nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle));
            ModifyMenu(hMenu, IDM_AUTOLOAD, MF_STRING | MF_ENABLED, IDM_AUTOLOAD, szAutoloadMenu);
            ModifyMenu(hMenu, IDM_PAUSE, MF_STRING | MF_ENABLED, IDM_PAUSE, szPauseMenu);
            ModifyMenu(hMenu, IDM_EXIT, MF_STRING | MF_ENABLED, IDM_EXIT, szExitMenu);
            Shell_NotifyIcon(NIM_MODIFY, &nid);
        }

        // �������� ��������� ������������
        if (CtrlAutoLoad(APP::CHECK) == APP::LOAD)
            CheckMenuItem(hMenu, IDM_AUTOLOAD, MF_CHECKED);
        else
            CheckMenuItem(hMenu, IDM_AUTOLOAD, MF_UNCHECKED);

        // �������� ��������� �����
        if (CtrlThread(THREAD::CHECK) == THREAD::PAUSE)
        {
            if (monitorThread)
            {
                SuspendThread(monitorThread);
                CheckMenuItem(hMenu, IDM_PAUSE, MF_CHECKED);
                lstrcpy(nid.szTip, szTipP);
                UpdateTrayIcon(hIconPause);
                
            }
        }
        else
        {
            lstrcpy(nid.szTip, szTip);
            CheckMenuItem(hMenu, IDM_PAUSE, MF_UNCHECKED);
            
        }

        InitGDIPlus();

        if (window)
        {   // ������� ���� ���������:
            MSG msg;
            while (GetMessage(&msg, 0, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        ShutdownGDIPlus();
        WTSUnRegisterSessionNotification(window);

#ifdef _DEBUG
    }
    catch (...)
    {
        MessageBoxEx(NULL, L"���-�� ����� �� ���...", L"����������!", MB_OK, 0);
        return 1;
    }
#endif
    return 0;
}

// EOF