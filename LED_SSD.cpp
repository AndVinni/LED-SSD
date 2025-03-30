// Утилита индикации чтения и записи дисковой подсистемы Windows
//                  
//                          LED-SSD
//                           
//          W7 и младше, x32, x64, C++ 23, RU EN unicode             
//                          FPS 24,39 
// "Вспомнить всё" = 30 лет паузы в проограммировании для Windows на C++
//                  (C) Vinni, Апрель 2025 г.
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
#include <string>

#pragma comment(lib, "pdh.lib") // Работа со счётчиками



#define hKey HKEY_CURRENT_USER

// GUID - уникальный идентификатор иконки
class __declspec(uuid("8a002844-4745-4336-a9a1-98ff80bce4c2")) AppIcon;
// GUID - уникальный идентификатор мьютекса
const wchar_t *szwMutex = L"36д85б51e72д4504997ф28е0е243101с";
const wchar_t *szWindowClass = L"LED-SSD";
const wchar_t* szPause = L"Pause";
const wchar_t* szwSelectedDisk = L"_Total";        // Активность всех дисков
wchar_t readCounterPath[PDH_MAX_COUNTER_PATH];
wchar_t writeCounterPath[PDH_MAX_COUNTER_PATH];
wchar_t LocaleName[LOCALE_NAME_MAX_LENGTH];
const wchar_t* szwAllRun = L"Application alredy run";
const wchar_t* szwWarning = L"Warning!";
const wchar_t* szwUzheRabotaet = L"Программа уже запущена";
const wchar_t* szwVnimanie = L"Внимание!";
UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;
HICON hIconIdle, hIconRead, hIconWrite, hIconRW, hIconApp, hIconPause;
NOTIFYICONDATA  nid = { sizeof(nid) };
HWND window = NULL;
HMENU hMenu, hSubMenu = NULL;
HANDLE monitorThread = NULL;
HANDLE ghExitEvent = NULL;
DWORD  dwThreadId = 0;
enum class APP : short { CHECK, UNLOAD, LOAD };
enum class THREAD : short { CHECK, PAUSE, RUN };
APP CtrlAutoLoad(APP);
THREAD CtrlPause(THREAD);
bool UserLocale_RU; // Локализация или русская или английская
void ShowContextMenu(HWND hwnd, POINT pt);

void inline static UpdateTrayIcon(HICON hIcon)
{
    nid.hIcon = hIcon;
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

static DWORD WINAPI MonitorDiskActivity(LPVOID lpParam)
{   // Мониторинг активности дисков через счётчики производительности системы

    PDH_HQUERY hQueryR, hQueryW;
    PDH_HCOUNTER hCounterRead, hCounterWrite;
    PDH_FMT_COUNTERVALUE valueRead, valueWrite;

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

        if (valueRead.longValue > 0 && valueWrite.longValue > 0)    // Читает и пишет
            UpdateTrayIcon(hIconRW);
        else if (valueRead.longValue > 0)                           // Только читает
            UpdateTrayIcon(hIconRead);
        else if (valueWrite.longValue > 0)                          // Только пишет
            UpdateTrayIcon(hIconWrite);
        else                                                        // Курит
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
        HMENU hSubMenu = GetSubMenu(hMenu, 0);
        if (hSubMenu)
        {
            // Окно на передний план, иначе контекстное меню не исчезнет
            SetForegroundWindow(hwnd);
            // Выравниевание выпадающего меню
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
                uFlags |= TPM_RIGHTALIGN;
            else
                uFlags |= TPM_LEFTALIGN;
            TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
        }
    }
}

APP CtrlAutoLoad(APP mode)  
{   // Управление автозагрузкой через реестр

    static APP state = APP::UNLOAD;
    static wchar_t szwSubKey[MAX_PATH] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static wchar_t szwPath[MAX_PATH];
    static wchar_t szwKeyValue[MAX_PATH];
    DWORD size = MAX_PATH;
    LSTATUS lResult =0;
    static HKEY hKeyDescriptor = hKey;

    switch (mode)
    {
    case APP::CHECK: // Проверка состояния автозагрузки
        {   // Открываем ветвь реестра
            lResult = RegOpenKeyEx(hKey, szwSubKey, 0, KEY_READ, &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {   // Есть запись реестра
                lResult = RegGetValue(hKey, szwSubKey, szWindowClass, RRF_RT_REG_SZ, NULL, szwKeyValue, &size);
                if (lResult == ERROR_SUCCESS) // Получили значение реестра
                {   // Получаем полный путь к файлу
                    GetModuleFileName(NULL, szwPath, MAX_PATH);
                    if (wcscmp(szwKeyValue, szwPath) == 0)
                    {   // Запись в реестре есть и совпадает с текущим положением программы
                        state = APP::LOAD;
                        break;
                    }
                }
                RegCloseKey(hKeyDescriptor);
            }
            state = APP::UNLOAD;
        }
        break;
    case APP::UNLOAD:   // Удаление записи автозагрузки реестра
        lResult = RegOpenKeyEx(hKey, szwSubKey, 0, KEY_ALL_ACCESS, &hKeyDescriptor);
        if (lResult == ERROR_SUCCESS)
        {
            RegDeleteKeyValue(hKey, szwSubKey, szWindowClass);
            RegCloseKey(hKeyDescriptor);
            state = APP::UNLOAD;
        }
        break;
    case APP::LOAD: // Создание записи автозагрузки реестра
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

static THREAD CtrlPause(THREAD mode)
{   // Фиксация состояния паузы в реестре

    static THREAD state = THREAD::RUN;
    static wchar_t szwSubKey[MAX_PATH] = L"";
    static HKEY hKeyDescriptor = hKey;
    LSTATUS lResult = 0;
    DWORD dwKeyValue=0;
    static DWORD size = sizeof(dwKeyValue);

    // Ветвь реестра параметра для паузы
    swprintf_s(szwSubKey, L"%s%s", L"Software\\", szWindowClass );

    switch (mode)
    {
    case THREAD::CHECK:
        {   // Открываем ветвь реестра
            lResult = RegOpenKeyEx(hKey, szwSubKey, 0, KEY_READ, &hKeyDescriptor);
            if (lResult == ERROR_SUCCESS)
            {   // Есть запись реестра
                lResult = RegGetValue(hKey, szwSubKey, szPause, RRF_RT_DWORD, NULL, &dwKeyValue, &size);
                if (lResult == ERROR_SUCCESS) // Получили значение реестра
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
            // Анализ пунктов меню:
            switch (wmId)
            {
                case IDM_EXIT:
                    DestroyWindow(hwnd);
                    break;

                case IDM_PAUSE:
                    if (CtrlPause(THREAD::CHECK) == THREAD::RUN)
                    {
                        SuspendThread(monitorThread);
                        CtrlPause(THREAD::PAUSE);
                        CheckMenuItem(hMenu, IDM_PAUSE, MF_CHECKED);
                        UpdateTrayIcon(hIconPause);
                    }
                    else
                    {
                        ResumeThread(monitorThread);
                        CtrlPause(THREAD::RUN);
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
    try
    {
        // Язык пользователя в системе
        GetUserDefaultLocaleName(LocaleName, LOCALE_NAME_MAX_LENGTH);
        UserLocale_RU = wcscmp(LocaleName, L"ru-RU") == 0 ? TRUE : FALSE;

        // Блокировка запуска второго экземпляра программы
        HANDLE mutex = CreateMutexEx(0, szwMutex, CREATE_MUTEX_INITIAL_OWNER, READ_CONTROL);
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            if (UserLocale_RU)
                MessageBoxEx(NULL, szwUzheRabotaet, szwVnimanie, MB_OK, 0);
            else
                MessageBoxEx(NULL, szwAllRun, szwWarning, MB_OK, 0);
            return 1;
        }

        // Загрузка иконок из ресурсов
        hIconIdle = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IDLE));
        hIconRead = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_READ));
        hIconWrite = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WRITE));
        hIconRW = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RW));
        hIconApp = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
        hIconPause = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PAUSE));

        // Регистрация класса окна
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

        // Загрузка меню из ресурсов
        hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDC_CONTEXTMENU));

        // Создание главного окна
        window = CreateWindowEx(WS_EX_APPWINDOW, szWindowClass, NULL, 0, 0, 0, 0, 0, NULL, hMenu, hInstance, NULL);

        // Создание события завершения потока мониторинга
        ghExitEvent = CreateEvent(NULL, TRUE, FALSE, TEXT("ExitEvent"));

        // Создание потока мониторинга
        if (monitorThread = CreateThread(NULL, 65536, MonitorDiskActivity, NULL, 0, &dwThreadId))
            SetPriorityClass(monitorThread, THREAD_PRIORITY_ABOVE_NORMAL);

        // Создание контекста для нотификации
        nid.hWnd = window;
        nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
        nid.hIcon = hIconApp;
        nid.guidItem = __uuidof(AppIcon);
        nid.dwState = NIS_SHAREDICON;
        nid.dwInfoFlags = NIIF_USER | NIIF_ICON_MASK;
        nid.hBalloonIcon = hIconApp;
        nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
        LoadString(hInstance, IDS_ACT, nid.szTip, ARRAYSIZE(nid.szInfoTitle));
        LoadString(hInstance, IDS_INFO, nid.szInfo, ARRAYSIZE(nid.szInfoTitle));
        Shell_NotifyIcon(NIM_ADD, &nid);
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIcon(NIM_SETVERSION, &nid);

        if (!UserLocale_RU) // Если не русский, тогда английский
        {
            wchar_t szAutoload[85], szPause[85], szExit[85];
            LoadString(hInstance, IDS_AUTOLOAD, szAutoload, ARRAYSIZE(szAutoload));
            LoadString(hInstance, IDS_PAUSE, szPause, ARRAYSIZE(szPause));
            LoadString(hInstance, IDS_EXIT, szExit, ARRAYSIZE(szExit));
            ModifyMenu(hMenu, IDM_AUTOLOAD, MF_STRING | MF_ENABLED, IDM_AUTOLOAD, szAutoload);
            ModifyMenu(hMenu, IDM_PAUSE, MF_STRING | MF_ENABLED, IDM_PAUSE, szPause);
            ModifyMenu(hMenu, IDM_EXIT, MF_STRING | MF_ENABLED, IDM_EXIT, szExit);
            LoadString(hInstance, IDS_ACTE, nid.szTip, ARRAYSIZE(nid.szInfoTitle));
        }

        // Проверка состояния автозагрузки
        if (CtrlAutoLoad(APP::CHECK) == APP::LOAD)
            CheckMenuItem(hMenu, IDM_AUTOLOAD, MF_CHECKED);
        else
            CheckMenuItem(hMenu, IDM_AUTOLOAD, MF_UNCHECKED);

        // Проверка состояния паузы
        if (CtrlPause(THREAD::CHECK) == THREAD::PAUSE)
        {
            if (monitorThread)
            {
                SuspendThread(monitorThread);
                CheckMenuItem(hMenu, IDM_PAUSE, MF_CHECKED);
                UpdateTrayIcon(hIconPause);
            }
        }
        else
            CheckMenuItem(hMenu, IDM_PAUSE, MF_UNCHECKED);

        if (window)
        {   // Главный цикл сообщений:
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    catch (...)
    {
        MessageBoxEx(NULL, L"Что-то пошло не так...", L"Исключение!", MB_OK, 0);
        return 1;
    }
    return 0;
}

// EOF