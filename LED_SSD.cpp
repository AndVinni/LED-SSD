// Утилита индикации чтения и записи дисковой подсистемы Windows
//                  
//                          LED-SSD
//                           
//          WIN7 и младше, x32, x64, C++ 14, RU EN unicode             
//                          
// "Вспомнить всё" = 30 лет паузы в проограммировании для Windows на C++
//                  (C) Vinni, Апрель 2025 г.
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
#include <optional>
#include <cmath>



#ifdef _DEBUG
    #include <fstream>
    #include <iostream>
    import std;
#endif
#pragma comment(lib, "pdh.lib")         // Работа со счётчиками
#pragma comment(lib, "Wtsapi32.lib" )   // Работа с сеансом
#pragma comment(lib, "gdiplus.lib")     // Модификация иконки в трее

#define hKey HKEY_CURRENT_USER

ULONG_PTR gdiplusToken;
// GUID - уникальный идентификатор иконки
class __declspec(uuid("8a002844-4745-4336-a9a1-98ff80bce4c2")) AppIcon;
// Имя мьютекса
const wchar_t *szwMutex = L"36д85б51e72д4504997ф28е0е243101с";
const wchar_t *szWindowClass = L"LED-SSD";
const wchar_t *szPause = L"Pause";
      wchar_t  szTip[128] = L"";
      wchar_t  szTipP[128] = L"";
const wchar_t* szwSelectedDisk = L"_Total";        // Активность всех дисков
wchar_t readCounterPath[PDH_MAX_COUNTER_PATH];
wchar_t writeCounterPath[PDH_MAX_COUNTER_PATH];
wchar_t LocaleName[LOCALE_NAME_MAX_LENGTH];
const wchar_t* szwAllRun = L"Application alredy run";
const wchar_t* szwWarning = L"Warning!";
const wchar_t* szwUzheRabotaet = L"Программа уже запущена";
const wchar_t* szwVnimanie = L"Внимание!";
UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;
HICON hIconIdle, hIconApp, hIconPause, hIconRead, hIconWrite, hIconRW;
std::optional<HICON> pIconRead, pIconWrite, pIconRW;
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
bool UserLocale_RU; // Локализация или русская или английская
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


class IconBright
{
    std::optional<Bitmap> bmp;
    std::optional<Bitmap> result;

public:

    IconBright(HICON hIcon) { bmp.emplace(hIcon);
                              result.emplace(bmp->GetWidth(), bmp->GetHeight(), PixelFormat32bppARGB);
                            } 
    bool AdjustIconBrightness(HICON hIcon, HICON *dstIcon, float brightnessFactor);
};


bool IconBright::AdjustIconBrightness(HICON hIcon, HICON* dstIcon, float brightnessFactor)
 {
        bmp.emplace(hIcon); // Преобразуем HICON в Bitmap;
        result.emplace(bmp->GetWidth(), bmp->GetHeight(), PixelFormat32bppARGB); // Пустая
       
        Graphics g(&*result); // Художник

        // Матрица яркости
        float b = brightnessFactor; // Диапазон 0.5-1.5 проверен на глаз
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

        g.DrawImage(&*bmp,
            Rect(0, 0, bmp->GetWidth(), bmp->GetHeight()),
            0, 0, bmp->GetWidth(), bmp->GetHeight(),
            UnitPixel, &ia);

        result->GetHICON(dstIcon);
        return true;
    }

class Normalizator
{
public:
    Normalizator()
        : x_prev(0.0f), y_prev(0.0f),
        input_min(0.0f), input_max(10.0f),
        output_min(0.75f), output_max(1.2f),
        log_scaled(0.0f),
        k(5.0f)
    {
    }

    // Основная функция: удаление DC + линейное масштабирование + логарифм
    inline float Preparation(float value, float alpha)
    {
        float gb = value / 1073741824.; // gb/sec
        float scaled = scale_linear(gb);
        float no_dc = remove_dc(no_dc, alpha);
        log_scaled = scale_logarithmic(scaled);
        return log_scaled;
    }

    operator float() { return log_scaled; }

private:
    // Состояния фильтра
    float x_prev;
    float y_prev;

    // Диапазоны масштабирования
    float input_min;
    float input_max;
    float output_min;
    float output_max;
    float log_scaled;

    // Коэффициент логарифмического масштабирования
    float k;

    // Линейное масштабирование из [-X, X] в [0.75, 1.2]
    inline float scale_linear(float value)
    {
        float normalized = (value - input_min) / (input_max - input_min);
        return output_min + normalized * (output_max - output_min);
    }

    // Удаление постоянной составляющей
    inline float remove_dc(float x, float alpha)
    {
        float y = x - x_prev + alpha * y_prev;
        x_prev = x;
        y_prev = y;
        return y;
    }

    // Логарифмическое масштабирование в диапазоне [ output_min, output_max]
    inline float scale_logarithmic(float value)
    {
        if (value <= output_min) return output_min;
        if (value >= output_max) return output_max;

        float normalized_input = value - output_min;
        float input_range = output_max - output_min;

        float numerator = std::log1p(k * normalized_input);
        float denominator = std::log1p(k * input_range);

        float factor = numerator / denominator;
        return output_min + factor * (output_max - output_min);
    }
    /*
    inline float scale_logarithmic(float value)
    {
        const float reference = output_min; // то есть 0.5

        if (value <= 0.0f) return -100.0f; // защита от логарифма 0 или отрицательных значений

        return 20.0f * std::log10(value / reference);
    }*/
};



void inline static UpdateTrayIcon(HICON hIcon)
{
    nid.hIcon = hIcon;
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

static DWORD WINAPI MonitorDiskActivity(LPVOID lpParam)
{   // Мониторинг активности дисков через счётчики производительности системы

    static Normalizator levelR, levelW, levelRW;
    static IconBright Green(hIconRead), Red(hIconWrite), Yellow(hIconRW);
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
        PdhGetFormattedCounterValue(hCounterRead, PDH_FMT_DOUBLE, NULL, &valueRead);
        PdhGetFormattedCounterValue(hCounterWrite, PDH_FMT_DOUBLE, NULL, &valueWrite);

        if (valueRead.largeValue > 0ll && valueWrite.largeValue > 0ll)    // Читает и пишет
        {
            float meanValueRW = (valueRead.largeValue + valueWrite.doubleValue) / 2;
            levelRW.Preparation(meanValueRW, 0.001);
            Yellow.AdjustIconBrightness(hIconRW, &*pIconRW, levelRW);
            UpdateTrayIcon(*pIconRW);
        }
        else if (valueRead.largeValue > 0ll)                           // Только читает
        {
            float meanValueR = (valueRead.doubleValue);
            levelR.Preparation(meanValueR, 0.001);
            Green.AdjustIconBrightness(hIconRead, &*pIconRead, levelR);
            UpdateTrayIcon(*pIconRead);
        }
        else if (valueWrite.largeValue > 0ll)                          // Только пишет
        {
            float meanValueW = (valueWrite.doubleValue);
            levelW.Preparation(meanValueW, 0.001);
            Red.AdjustIconBrightness(hIconWrite, &*pIconWrite, levelW);
            UpdateTrayIcon(*pIconWrite);
        }
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
        hSubMenu = GetSubMenu(hMenu, 0);
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

static APP CtrlAutoLoad(APP mode)  
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

static THREAD CtrlThread(THREAD mode)
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
        hThis = GetCurrentProcess(); // Этот

        // Язык пользователя в системе
        if (GetUserDefaultLocaleName(LocaleName, LOCALE_NAME_MAX_LENGTH) != 0)
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
        hIconApp = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
        hIconIdle = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IDLE));
        hIconPause = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PAUSE));
        hIconRead = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_READ));
        hIconWrite = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WRITE));
        hIconRW = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RW));

        pIconRead.emplace(hIconRead);
        pIconWrite.emplace(hIconWrite);
        pIconRW.emplace(hIconRW);

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

        // Регистрируем окно, для получения сообщений сессии
        WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION);

        // Создание потока мониторинга
        if ((monitorThread = CreateThread(NULL, 65536, MonitorDiskActivity, NULL, 0, &dwThreadId)))
            SetPriorityClass(monitorThread, THREAD_PRIORITY_ABOVE_NORMAL );

        // Создание контекста для нотификации
        nid.cbSize = sizeof(nid);
        nid.hWnd = window;
        nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
        nid.guidItem = __uuidof(AppIcon);
        nid.dwInfoFlags = NIIF_USER;
        nid.dwState = NIS_SHAREDICON;
        nid.hBalloonIcon = hIconApp;
        nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
        // Тексты нотификации
        LoadString(hInstance, IDS_ACT, szTip, ARRAYSIZE(szTip));
        LoadString(hInstance, IDS_ACTP, szTipP, ARRAYSIZE(szTipP));
        lstrcpy(nid.szTip, szTip);
        LoadString(hInstance, IDS_ACT, nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle));
        LoadString(hInstance, IDS_INFO, nid.szInfo, ARRAYSIZE(nid.szInfo));
        // Постоянные иконки анимации
        nid.dwState = NIS_SHAREDICON;
        nid.hIcon = hIconIdle;
        Shell_NotifyIcon(NIM_ADD, &nid);
        nid.dwState = NIS_HIDDEN;
        nid.hIcon = hIconPause;
        Shell_NotifyIcon(NIM_ADD, &nid);
        // Изменяемые иконки анимации
        nid.hIcon = *pIconRead;
        Shell_NotifyIcon(NIM_ADD, &nid);
        nid.hIcon = *pIconWrite;
        Shell_NotifyIcon(NIM_ADD, &nid);
        nid.hIcon = *pIconRW;
        Shell_NotifyIcon(NIM_ADD, &nid);
        // Версия нотификации
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIcon(NIM_SETVERSION, &nid);

        if (!UserLocale_RU) // Если не русский, тогда английский
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

        // Проверка состояния автозагрузки
        if (CtrlAutoLoad(APP::CHECK) == APP::LOAD)
            CheckMenuItem(hMenu, IDM_AUTOLOAD, MF_CHECKED);
        else
            CheckMenuItem(hMenu, IDM_AUTOLOAD, MF_UNCHECKED);

        // Проверка состояния паузы
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
        {   // Главный цикл сообщений:
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
        MessageBoxEx(NULL, L"Что-то пошло не так...", L"Исключение!", MB_OK, 0);
        return 1;
    }
#endif
    return 0;
}

// EOF