// A utility for indicating the reading and writing of the Windows disk subsystem.
// Replacing the hardware LED, for rapid evaluation of disk loading.
// The indicator is located on the taskbar.
// Green - reading, yellow - reading and writing, red - writing.
// Each color has 3 brightness gradations, depending on the speed change.
//                  
//       ██████    █████████ ████████           ████ █   ████ █ ████████   
//        ▓██▓▓▓    ▓██▓▓▓▓█▓ ▓██▓▓▓██         ██▓▓▓██▓ ██▓▓▓██▓ ▓██▓▓▓██   
//         ██▓▒▒▒    ██▓▒▒▒█▓▒ ██▓▒▒▒██        ██▓▒▒▒█▓▒██▓▒▒▒█▓▒ ██▓▒▒▒██   
//         ██▓▒░░░   ██▓▒█░░▓▒░██▓▒░░░██       ███▒░░█▓▒███▒░░█▓▒░██▓▒░░░██   
//         ██▓▒░     ██▓▒█▓  ▒░██▓▒░  ██▓       ███░  ▓▒░███░  ▓▒░██▓▒░  ██▓  
//         ██▓▒░     █████▓▒  ░██▓▒░  ██▓▒       ███   ▒░ ███   ▒░██▓▒░  ██▓▒ 
//         ██▓▒░     ██▓▓█▓▒░  ██▓▒░  ██▓▒░       ███   ░  ███   ░██▓▒░  ██▓▒░
//         ██▓▒░     ██▓▒█▓▒░  ██▓▒░  ██▓████  █   ███  █   ███   ██▓▒░  ██▓▒░
//         ██▓▒░ █   ██▓▒░▓█░  ██▓▒░ ██▓▓▒▓▓▓▓ █▓   ██▓ █▓   ██▓  ██▓▒░ ██▓▓▒░
//         ██▓▒░ █▓  ██▓▒░ █▓  ██▓▒░██▓▓▒▒░▒▒▒▒██▒  ██▓▒██▒  ██▓▒ ██▓▒░██▓▓▒▒░
//       █████████▓█████████▓████████▓▓▒▒░░ ░░░█▓████▓▓▒█▓████▓▓████████▓▓▒▒░░
//        ▓▓▓▓▓▓▓▓▓▒▓▓▓▓▓▓▓▓▓▒▓▓▓▓▓▓▓▓▒▒░░      ▓▒▓▓▓▓▒▒░▓▒▓▓▓▓▒▒▓▓▓▓▓▓▓▓▒▒░░ 
//         ▒▒▒▒▒▒▒▒▒░▒▒▒▒▒▒▒▒▒░▒▒▒▒▒▒▒▒░░        ▒░▒▒▒▒░░ ▒░▒▒▒▒░░▒▒▒▒▒▒▒▒░░  
//          ░░░░░░░░░ ░░░░░░░░░ ░░░░░░░░          ░ ░░░░   ░ ░░░░  ░░░░░░░░   

//                           
//          WIN7 and greater, x32, x64, C/C++ 17, RU, EN, unicode             
//                          
//   "Total Recall" = 30 years of pause in programming for Windows in C++
//                  (C) Vinni, April 2025
//

#define WINVER _WIN32_WINNT_WIN7
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define NTDDI_VERSION NTDDI_WIN7
#define WIN32_LEAN_AND_MEAN
#define NOCOMM


#include "resource.h"
#include <windows.h>
#include <pdh.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <stdio.h>
#include <cstdlib>
#include <VersionHelpers.h>



#ifdef _DEBUG
#include <fstream>
#include <iostream>
#include <string>
#endif
#pragma comment(lib, "pdh.lib")         // Working with counters
#pragma comment(lib, "Wtsapi32.lib" )   // Working with the session

#define hKey HKEY_CURRENT_USER

// ID is the unique identifier of the icon.
constexpr UINT MY_TRAY_ICON_ID = 48231;
// Name of the mutex
const wchar_t* szwMutex = L"36д85б51e72д4504997ф28е0е243101с";
const wchar_t* szWindowClass = L"LED-SSD";
const wchar_t* szPause = L"Pause";
wchar_t  szTip[128] = L"";
wchar_t  szTipP[128] = L"";
const wchar_t* szwSelectedDisk = L"_Total";        // Activity of all disks
wchar_t readCounterPath[PDH_MAX_COUNTER_PATH];
wchar_t writeCounterPath[PDH_MAX_COUNTER_PATH];
wchar_t LocaleName[LOCALE_NAME_MAX_LENGTH];
const wchar_t* szwWarning = L"Warning!";
const wchar_t* szwVnimanie = L"Внимание!";
UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;
HICON hIconIdle , hIconApp , hIconPause , hIconReadD , hIconRead , hIconReadB ,
hIconWriteD , hIconWrite , hIconWriteB ,
hIconRWd , hIconRW , hIconRWb;

const float MIN_AC_RANGE = -10.f; // gb/sec
const float MAX_AC_RANGE = 10.f;

NOTIFYICONDATAW nid = { sizeof(nid) };
HWND   window = NULL;
HMENU  hMenu , hSubMenu = NULL;
HANDLE monitorThread = NULL;
HANDLE ghExitEvent = NULL;
HANDLE hThis = NULL;
DWORD  dwThreadId = 0;
typedef enum class APP : short { Check , Unload , Load } Application;
typedef enum class THREAD : short { Check , Pause , Run } Thread;
static Application CtrlAutoLoad(Application);
static Thread CtrlThread(Thread);
bool UserLocale_RU;                         // Localization is either Russian or English
void ShowContextMenu(HWND hwnd , POINT pt);

// Структура для хранения информации о версии Windows
struct WindowsVersionInfo
{
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
};

#ifdef _DEBUG
std::wstring nstr = L"";
static void logMessage(const std::wstring& message , const std::wstring& par)
{
    std::wofstream logFile("LED_SSD.log" , std::ios_base::app); // Open log file in append mode
    if (logFile.is_open())
    {
        logFile << message << par << std::endl; // Write message to log file
        logFile.close();
    }
    else
    {
        std::cerr << "Unable to open log file!" << std::endl; // Error handling
    }
}
#endif

class IconBright
{
    HICON hID;
    HICON hIN;
    HICON hIB;
    const float rmin;
    const float rmax;
    const float darkRange;
    const float brightRange;

public:

    IconBright(HICON hIconD , HICON hIconN , HICON hIconB) :
        rmin(MIN_AC_RANGE) , rmax(MAX_AC_RANGE) , darkRange(MIN_AC_RANGE / 100) , brightRange(MAX_AC_RANGE / 100)
    {
        hID = hIconD;
        hIN = hIconN;
        hIB = hIconB;
    }

    HICON IconSelector(float brightnessFactor) const;
};

HICON IconBright::IconSelector(float brightnessFactor) const
{
    if (brightnessFactor < rmin)
        return hID;  // Removing brightness beyond the minimum range

    if (brightnessFactor > rmax)
        return hIB;  // Removing brightness beyond the maximum range

    if (brightnessFactor < darkRange)
        return hID;  // Brightness in the dark area

    if (brightnessFactor < brightRange)
        return hIN; // Brightness in the middle area

    return hIB;  // Brightness in the light area
}

class Normalizator
{
    // Filter status
    float x_prev;
    float y_prev;
    float scaled;

    // Removing the DC component
    inline float remove_dc(float x , float alpha)
    {
        float y = x - x_prev + alpha * y_prev;
        x_prev = x;
        y_prev = y;
        return y;
    }

public:
    Normalizator() : x_prev(0.0f) , y_prev(0.0f) , scaled(0.0f)
    {}

    // DC removal
    inline float Preparation(float value , float alpha)
    {
        return scaled = remove_dc((value < 0.0f ? 0.0f : value) , alpha);
    }

    operator float() { return scaled; }
};

void inline static UpdateTrayIcon(HICON hIcon)
{
    nid.hIcon = hIcon;
    Shell_NotifyIcon(NIM_MODIFY , &nid);
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100)  // we suppress the C1400 only here
#endif

static DWORD WINAPI MonitorDiskActivity(LPVOID lpParam)
{   // Monitoring disk activity via system disk performance counters

    static Normalizator levelR , levelW , levelRW;
    static IconBright Green(hIconReadD , hIconRead , hIconReadB) ,
        Red(hIconWriteD , hIconWrite , hIconWriteB) ,
        Yellow(hIconRWd , hIconRW , hIconRWb);
    static float vRead = 0.f , vWrite = 0.f;
    static PDH_HQUERY hQueryR , hQueryW;
    static PDH_HCOUNTER hCounterRead , hCounterWrite;
    static PDH_FMT_COUNTERVALUE valueRead , valueWrite;

    swprintf_s(readCounterPath , L"%s%s%s" , L"\\PhysicalDisk(" , szwSelectedDisk , L")\\Disk Read Bytes/sec");
    swprintf_s(writeCounterPath , L"%s%s%s" , L"\\PhysicalDisk(" , szwSelectedDisk , L")\\Disk Write Bytes/sec");

    PdhOpenQuery(NULL , NULL , &hQueryR);
    PdhOpenQuery(NULL , NULL , &hQueryW);
    PdhAddEnglishCounter(hQueryR , readCounterPath , NULL , &hCounterRead);
    PdhAddEnglishCounter(hQueryW , writeCounterPath , NULL , &hCounterWrite);

    PdhCollectQueryData(hQueryR);
    PdhCollectQueryData(hQueryW);

    while (WaitForSingleObject(ghExitEvent , 0) != WAIT_OBJECT_0)
    {
        PdhCollectQueryData(hQueryR);
        PdhCollectQueryData(hQueryW);
        PdhGetFormattedCounterValue(hCounterRead , PDH_FMT_DOUBLE , NULL , &valueRead);
        PdhGetFormattedCounterValue(hCounterWrite , PDH_FMT_DOUBLE , NULL , &valueWrite);

        vRead = (float)(valueRead.doubleValue / 1073741824.0); // gb/sec;
        vWrite = (float)(valueWrite.doubleValue / 1073741824.0);

        if (vRead > 0.f && vWrite > 0.f)                    // Reads and writes
        {
            float meanValueRW = std::abs(vRead) > std::abs(vWrite) ? vRead : vWrite;
            levelRW.Preparation(meanValueRW , 0.001f);
            UpdateTrayIcon(Yellow.IconSelector(levelRW));
        }
        else if (vRead > 0.f)                               // Only reads
        {
            levelR.Preparation(vRead , 0.001f);
            UpdateTrayIcon(Green.IconSelector(levelR));
            //#ifdef _DEBUG
            //    #pragma message( "-> A log file will be created!")
            //    logMessage(std::to_wstring(levelR), nstr);
            //#endif
        }
        else if (vWrite > 0.f)                              // Only writes
        {
            levelW.Preparation(vWrite , 0.001f);
            UpdateTrayIcon(Red.IconSelector(levelW));
        }
        else                                                // Smokes
            UpdateTrayIcon(hIconIdle);
        Sleep(41);                                          // 24.39 Hz                                
    }
    PdhCloseQuery(hQueryR);
    PdhCloseQuery(hQueryW);
    return 0;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

void ShowContextMenu(HWND hwnd , POINT pt)
{
    if (hMenu)
    {
        hSubMenu = GetSubMenu(hMenu , 0);
        if (hSubMenu)
        {   // Window to the foreground, otherwise the context menu will not disappear
            SetForegroundWindow(hwnd);
            // Alignment of the drop-down menu
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
                uFlags |= TPM_RIGHTALIGN;
            else
                uFlags |= TPM_LEFTALIGN;
            TrackPopupMenuEx(hSubMenu , uFlags , pt.x , pt.y , hwnd , NULL);
        }
    }
}

static Application CtrlAutoLoad(Application mode)
{   // Auto-upload management via the registry

    static Application state = Application::Unload;
    static wchar_t szwSubKey[MAX_PATH] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static wchar_t szwPath[MAX_PATH];
    static wchar_t szwKeyValue[MAX_PATH];
    DWORD size = MAX_PATH;
    LSTATUS lResult = 0;
    static HKEY hKeyDescriptor = hKey;

    switch (mode)
    {
        case Application::Check: // Checking the auto-upload status
        {   // Opening the registry branch
            lResult = RegOpenKeyEx(hKey , szwSubKey , 0 , KEY_READ , &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {   // There is a registry entry
                lResult = RegGetValue(hKey , szwSubKey , szWindowClass , RRF_RT_REG_SZ , NULL , szwKeyValue , &size);
                if (lResult == ERROR_SUCCESS) // Got the registry value
                {   // Getting the full path to the file
                    GetModuleFileName(NULL , szwPath , MAX_PATH);
                    if (wcscmp(szwKeyValue , szwPath) == 0)
                    {   // There is an entry in the registry and it matches the current position of the program
                        state = Application::Load;
                        break;
                    }
                }
                RegCloseKey(hKeyDescriptor);
            }
            state = Application::Unload;
        }
        break;
        case Application::Unload:   // Deleting Registry startup entry
            lResult = RegOpenKeyEx(hKey , szwSubKey , 0 , KEY_ALL_ACCESS , &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {
                RegDeleteKeyValue(hKey , szwSubKey , szWindowClass);
                RegCloseKey(hKeyDescriptor);
                state = Application::Unload;
            }
            break;
        case Application::Load: // Creating a registry startup entry
            lResult = RegOpenKeyEx(hKey , szwSubKey , 0 , KEY_ALL_ACCESS , &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {
                RegCreateKeyEx(hKey , szwSubKey , 0 , NULL , 0 , KEY_ALL_ACCESS , NULL , &hKeyDescriptor , NULL);
                GetModuleFileName(NULL , szwPath , MAX_PATH);
                RegSetValueEx(hKeyDescriptor , szWindowClass , NULL , REG_SZ , (LPBYTE)szwPath , MAX_PATH);
                RegCloseKey(hKeyDescriptor);
                state = Application::Load;
            }
            break;
    }
    return state;
}

static Thread CtrlThread(Thread mode)
{   // Fixing the pause status in the registry

    static Thread state = Thread::Run;
    static wchar_t szwSubKey[MAX_PATH] = L"";
    static HKEY hKeyDescriptor = hKey;
    LSTATUS lResult = 0;
    DWORD dwKeyValue = 0;
    static DWORD size = sizeof(dwKeyValue);

    // Registry branch of the pause parameter
    swprintf_s(szwSubKey , L"%s%s" , L"Software\\" , szWindowClass);

    switch (mode)
    {
        case Thread::Check:
        {   // Opening the registry branch
            lResult = RegOpenKeyEx(hKey , szwSubKey , 0 , KEY_READ , &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {   // There is a registry entry
                lResult = RegGetValue(hKey , szwSubKey , szPause , RRF_RT_DWORD , NULL , &dwKeyValue , &size);
                if (lResult == ERROR_SUCCESS) // Got the registry value
                    if (dwKeyValue == 1)
                    {
                        state = Thread::Pause;
                        break;                  
                    }
                RegCloseKey(hKeyDescriptor);
            }
            state = Thread::Run;                
        }
        break;
        case Thread::Pause:
            lResult = RegCreateKeyEx(hKey , szwSubKey , 0 , NULL , 0 , KEY_ALL_ACCESS , NULL , &hKeyDescriptor , NULL);
            if (lResult == ERROR_SUCCESS)
            {
                lResult = RegOpenKeyEx(hKey , szwSubKey , 0 , KEY_ALL_ACCESS , &hKeyDescriptor);
                if (lResult == ERROR_SUCCESS)
                {
                    dwKeyValue = 1;
                    RegSetValueEx(hKeyDescriptor , szPause , NULL , REG_DWORD , (const BYTE*)&dwKeyValue , size);
                    RegCloseKey(hKeyDescriptor);
                    state = Thread::Pause;
                    break;
                }
            }
            state = Thread::Run;
            break;
        case Thread::Run:
            lResult = RegOpenKeyEx(hKey , szwSubKey , 0 , KEY_ALL_ACCESS , &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {
                RegDeleteKeyValue(hKey , szwSubKey , szPause);
                RegCloseKey(hKeyDescriptor);
                state = Thread::Run;
                break;
            }
            state = Thread::Pause;
            break;
    }
    return state;
}

static LRESULT CALLBACK WindowProc(HWND hwnd , UINT message , WPARAM wParam , LPARAM lParam)
{
    static HWND s_hwndFlyout = NULL;
    static BOOL s_fCanShowFlyout = TRUE;

    switch (message)
    {
        case WM_COMMAND:
        {
            int const wmId = LOWORD(wParam);
            // Analysis of menu items:
            switch (wmId)
            {
                case IDM_EXIT:
                    DestroyWindow(hwnd);
                    break;

                case IDM_PAUSE:
                    if (CtrlThread(Thread::Check) == Thread::Run)
                    {
                        SuspendThread(monitorThread);
                        CtrlThread(Thread::Pause);
                        SetPriorityClass(hThis , THREAD_MODE_BACKGROUND_BEGIN);
                        CheckMenuItem(hMenu , IDM_PAUSE , MF_CHECKED);
                        lstrcpy(nid.szTip , szTipP);
                        UpdateTrayIcon(hIconPause);
                    }
                    else
                    {
                        lstrcpy(nid.szTip , szTip);
                        ResumeThread(monitorThread);
                        CtrlThread(Thread::Run);
                        SetPriorityClass(hThis , THREAD_MODE_BACKGROUND_END);
                        CheckMenuItem(hMenu , IDM_PAUSE , MF_UNCHECKED);
                    }
                    break;

                case IDM_AUTOLOAD:
                    if (CtrlAutoLoad(Application::Check) == Application::Load)
                    {
                        CtrlAutoLoad(Application::Unload);
                        CheckMenuItem(hMenu , IDM_AUTOLOAD , MF_UNCHECKED);
                    }
                    else
                    {
                        CtrlAutoLoad(Application::Load);
                        CheckMenuItem(hMenu , IDM_AUTOLOAD , MF_CHECKED);
                    }
                    break;

                default:
                    return DefWindowProc(hwnd , message , wParam , lParam);
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
                    ShowContextMenu(hwnd , pt);
                }
                break;
            }
            break;

        case WM_WTSSESSION_CHANGE:
        {
            switch (wParam)
            {
                case WTS_SESSION_LOCK:
                    if (CtrlThread(Thread::Check) == Thread::Run)
                    {
                        SuspendThread(monitorThread);
                        SetPriorityClass(hThis , THREAD_MODE_BACKGROUND_BEGIN);
                    }
                    break;
                case WTS_SESSION_UNLOCK:
                    if (CtrlThread(Thread::Check) != Thread::Pause)
                    {
                        ResumeThread(monitorThread);
                        SetPriorityClass(hThis , THREAD_MODE_BACKGROUND_END);
                    }
                    break;
            }
        }
        break;

        case WM_DESTROY:
            ResumeThread(monitorThread);
            SetEvent(ghExitEvent);
            WaitForSingleObject(monitorThread , INFINITE);
            CloseHandle(monitorThread);
            Shell_NotifyIcon(NIM_DELETE , &nid);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd , message , wParam , lParam);
    }
    return 0;
}

// Windows version constants
#define WIN_VISTA       0x060000    // 6.0
#define WIN_7           0x060001    // 6.1
#define WIN_8           0x060002    // 6.2
#define WIN_8_1         0x060003    // 6.3
#define WIN_10          0x0A0000    // 10.0

// Macro to create a version from major.minor
#define MAKE_VERSION(major, minor) (((major) << 16) | (minor))

// Check for compatibility with NIS_SHAREDICON
static bool IsNISSharedIconCompatible()
{
    // Check for Embedded/IoT edition
    DWORD dwType = 0;
    if (SUCCEEDED(GetProductInfo(6, 0, 0, 0, &dwType))) {
        switch (dwType)
        {
            case PRODUCT_IOTUAP: case PRODUCT_IOTOS: case PRODUCT_IOTEDGEOS:
            case PRODUCT_IOTENTERPRISE: case PRODUCT_IOTENTERPRISES:
            case PRODUCT_EMBEDDED: case PRODUCT_EMBEDDED_A: case PRODUCT_EMBEDDED_E:
            case PRODUCT_EMBEDDED_INDUSTRY: case PRODUCT_EMBEDDED_INDUSTRY_A:
            case PRODUCT_EMBEDDED_INDUSTRY_E: case PRODUCT_EMBEDDED_INDUSTRY_A_E:
            case PRODUCT_EMBEDDED_EVAL: case PRODUCT_EMBEDDED_E_EVAL:
            case PRODUCT_EMBEDDED_INDUSTRY_EVAL: case PRODUCT_EMBEDDED_INDUSTRY_E_EVAL:
            case PRODUCT_EMBEDDED_AUTOMOTIVE: case PRODUCT_THINPC:
            case PRODUCT_SOLUTION_EMBEDDEDSERVER: case PRODUCT_SOLUTION_EMBEDDEDSERVER_CORE:
            return false;
        }
    }
        
    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if (!GetVersionEx(&osvi))
        return false; // Unable to get version - safe choice

    DWORD version = MAKE_VERSION(osvi.dwMajorVersion, osvi.dwMinorVersion);

    switch (version)
    {
        case WIN_VISTA:
            return false; // Known issues

        case WIN_7:
            // RTM(7600) problematic, SP1(7601 + ) works
            return osvi.dwBuildNumber >= 7601;

        case WIN_8:
        case WIN_8_1:
            return false; // Periodic problems

        case WIN_10:
            // Early versions are problematic, Anniversary Update (14393+) is stable
            return osvi.dwBuildNumber >= 14393;

    default:
        // Versions earlier than Vista or unknown
        return version < WIN_VISTA;
    }
}

static void ThisWindowsVersionNotSupported(bool ru)
{
    if (ru)
        MessageBoxEx(NULL , L"Эта версия Windows не поддерживает расширенные функции области уведомлений панели задач" , szwVnimanie , MB_OK , 0);
    else
        MessageBoxEx(NULL , L"This version of Windows does not support advanced functions of the Taskbar Notification Area" , szwWarning , MB_OK , 0);
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100)  // we suppress the C1400 only here
#endif

int APIENTRY wWinMain(_In_ HINSTANCE hInstance ,
    _In_opt_ HINSTANCE hPrevInstance ,
    _In_ LPWSTR    lpCmdLine ,
    _In_ int       nCmdShow)
{

#ifdef _DEBUG
    try
    {
#endif

        // The user's language in the system
        if (GetUserDefaultLocaleName(LocaleName , LOCALE_NAME_MAX_LENGTH) != 0)
            UserLocale_RU = wcscmp(LocaleName , L"ru-RU") == 0 ? TRUE : FALSE;

        // Blocking the launch of the second instance of the program
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4189)  //"local variable is initialized but not referenced"
#endif
        HANDLE mutex = CreateMutexEx(0 , szwMutex , CREATE_MUTEX_INITIAL_OWNER , READ_CONTROL);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            if (UserLocale_RU)
                MessageBoxEx(NULL , L"Программа уже запущена" , szwVnimanie , MB_OK , 0);
            else
                MessageBoxEx(NULL , L"Application alredy run" , szwWarning , MB_OK , 0);
            return 1;
        }

        // Loading icons from resources
        hIconApp = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_APP));
        hIconIdle = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_IDLE));
        hIconPause = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_PAUSE));
        hIconReadD = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_READD));
        hIconRead = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_READ));
        hIconReadB = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_READB));
        hIconWriteD = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_WRITED));
        hIconWrite = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_WRITE));
        hIconWriteB = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_WRITEB));
        hIconRWd = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_RWD));
        hIconRW = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_RW));
        hIconRWb = LoadIcon(hInstance , MAKEINTRESOURCE(IDI_RWB));

        // Window class registration
        WNDCLASSEXW wcex = { sizeof(wcex) };
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_CLASSDC;
        wcex.lpfnWndProc = WindowProc;
        wcex.hInstance = hInstance;
        wcex.hIcon = hIconApp;
        wcex.hCursor = LoadCursor(NULL , IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_CONTEXTMENU);
        wcex.lpszClassName = szWindowClass;
        RegisterClassEx(&wcex);

        // Loading the menu from resources
        hMenu = LoadMenu(hInstance , MAKEINTRESOURCE(IDC_CONTEXTMENU));

        // Creating the main window
        window = CreateWindowEx(WS_EX_APPWINDOW , szWindowClass , NULL , 0 , 0 , 0 , 0 , 0 , NULL , hMenu , hInstance , NULL);
        if (!window)
            return 1;
        // Creating a monitoring flow termination event
        ghExitEvent = CreateEvent(NULL , TRUE , FALSE , TEXT("ExitEvent"));

        // Register a window to receive session messages
        WTSRegisterSessionNotification(window , NOTIFY_FOR_THIS_SESSION);

        // Creating a notification context
        nid.cbSize = sizeof(nid);
        nid.hWnd = window;
        nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
        nid.uID = MY_TRAY_ICON_ID;
        nid.dwInfoFlags = NIIF_USER;
        nid.hBalloonIcon = hIconApp;
        nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
        // Notification texts
        LoadString(hInstance , IDS_ACT , szTip , ARRAYSIZE(szTip));
        LoadString(hInstance , IDS_ACTP , szTipP , ARRAYSIZE(szTipP));
        lstrcpy(nid.szTip , szTip);
        LoadString(hInstance , IDS_ACT , nid.szInfoTitle , ARRAYSIZE(nid.szInfoTitle));
        LoadString(hInstance , IDS_INFO , nid.szInfo , ARRAYSIZE(nid.szInfo));
        // Permanent animation icons
        nid.hIcon = hIconIdle;
        if (IsNISSharedIconCompatible())
        {
            nid.uFlags |= NIF_STATE;
            nid.dwState |= NIS_SHAREDICON;
            nid.dwStateMask |= NIS_SHAREDICON;
        }
        if (!Shell_NotifyIcon(NIM_ADD , &nid))
        {
            ThisWindowsVersionNotSupported(UserLocale_RU);
            return 1;
        }

        nid.uVersion = NOTIFYICON_VERSION_4;
        if (!Shell_NotifyIcon(NIM_SETVERSION , &nid))
        {
            ThisWindowsVersionNotSupported(UserLocale_RU);
            return 1;
        }

        if (!UserLocale_RU) // If not Russian, then English
        {
            wchar_t szAutoloadMenu[85] , szPauseMenu[85] , szExitMenu[85];
            LoadString(hInstance , IDS_AUTOLOAD , szAutoloadMenu , ARRAYSIZE(szAutoloadMenu));
            LoadString(hInstance , IDS_PAUSE , szPauseMenu , ARRAYSIZE(szPauseMenu));
            LoadString(hInstance , IDS_EXIT , szExitMenu , ARRAYSIZE(szExitMenu));
            LoadString(hInstance , IDS_ACTE , szTip , ARRAYSIZE(szTip));
            LoadString(hInstance , IDS_ACTEP , szTipP , ARRAYSIZE(szTipP));
            LoadString(hInstance , IDS_INFOE , nid.szInfo , ARRAYSIZE(nid.szInfo));
            LoadString(hInstance , IDS_APP_DECR_E , nid.szInfoTitle , ARRAYSIZE(nid.szInfoTitle));
            ModifyMenu(hMenu , IDM_AUTOLOAD , MF_STRING | MF_ENABLED , IDM_AUTOLOAD , szAutoloadMenu);
            ModifyMenu(hMenu , IDM_PAUSE , MF_STRING | MF_ENABLED , IDM_PAUSE , szPauseMenu);
            ModifyMenu(hMenu , IDM_EXIT , MF_STRING | MF_ENABLED , IDM_EXIT , szExitMenu);
            Shell_NotifyIcon(NIM_MODIFY , &nid);
        }

        // Creating a monitoring flow
        if ((monitorThread = CreateThread(NULL , 65536 , MonitorDiskActivity , NULL , 0 , &dwThreadId)))
            SetPriorityClass(monitorThread , THREAD_PRIORITY_ABOVE_NORMAL);

        // Checking the auto-upload status
        if (CtrlAutoLoad(Application::Check) == Application::Load)
            CheckMenuItem(hMenu , IDM_AUTOLOAD , MF_CHECKED);
        else
            CheckMenuItem(hMenu , IDM_AUTOLOAD , MF_UNCHECKED);

        // Checking the pause status
        if (CtrlThread(Thread::Check) == Thread::Pause)
        {
            if (monitorThread)
            {
                SuspendThread(monitorThread);
                CheckMenuItem(hMenu , IDM_PAUSE , MF_CHECKED);
                lstrcpy(nid.szTip , szTipP);
                UpdateTrayIcon(hIconPause);

            }
        }
        else
        {
            lstrcpy(nid.szTip , szTip);
            CheckMenuItem(hMenu , IDM_PAUSE , MF_UNCHECKED);
        }

        if (window)
        {   // Main message loop:
            MSG msg;
            while (GetMessage(&msg , 0 , 0 , 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        WTSUnRegisterSessionNotification(window);

#ifdef _DEBUG
    }
    catch (...)
    {
        MessageBoxEx(NULL , L"Something went wrong..." , L"Houston, we have a problem!" , MB_OK , 0);
        return 1;
    }
#endif
    return 0;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// EOF