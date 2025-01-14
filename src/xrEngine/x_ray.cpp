//-----------------------------------------------------------------------------
// File: x_ray.cpp
//
// Programmers:
// Oles - Oles Shishkovtsov
// AlexMX - Alexander Maksimchuk
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "igame_level.h"
#include "igame_persistent.h"
#include "../xrNetServer/NET_AuthCheck.h"

#include "xr_input.h"
#include "xr_ioconsole.h"
#include "x_ray.h"
#include "std_classes.h"
#include "GameFont.h"
#include "resource.h"
#include "LightAnimLibrary.h"
#include "../xrcdb/ispatial.h"
#include <process.h>
#include <locale.h>

//---------------------------------------------------------------------
ENGINE_API CInifile* pGameIni = NULL;
BOOL g_bIntroFinished = FALSE;
extern void Intro(void* fn);
extern void Intro_DSHOW(void* fn);
extern int PASCAL IntroDSHOW_wnd(HINSTANCE hInstC, HINSTANCE hInstP, LPSTR lpCmdLine, int nCmdShow);

const TCHAR* c_szSplashClass = _T("SplashWindow");

// computing build id
XRCORE_API LPCSTR build_date;
XRCORE_API u32 build_id;

#ifdef MASTER_GOLD
# define NO_MULTI_INSTANCES
#endif // #ifdef MASTER_GOLD

static LPSTR month_id[12] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int days_in_month[12] =
{
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int start_day = 10; // 31
static int start_month = 2; // January
static int start_year = 2019; // 1999

// binary hash, mainly for copy-protection
#include "md5.h"
#include <ctype.h>

#include <thread>

#define DEFAULT_MODULE_HASH "3CAABCFCFF6F3A810019C6A72180F166"
static char szEngineHash[33] = DEFAULT_MODULE_HASH;

char* ComputeModuleHash(char* pszHash)
{
    char szModuleFileName[MAX_PATH];
    HANDLE hModuleHandle = NULL, hFileMapping = NULL;
    LPVOID lpvMapping = NULL;
    MEMORY_BASIC_INFORMATION MemoryBasicInformation;

    if (!GetModuleFileName(NULL, szModuleFileName, MAX_PATH))
        return pszHash;

    hModuleHandle = CreateFile(szModuleFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (hModuleHandle == INVALID_HANDLE_VALUE)
        return pszHash;

    hFileMapping = CreateFileMapping(hModuleHandle, NULL, PAGE_READONLY, 0, 0, NULL);

    if (hFileMapping == NULL)
    {
        CloseHandle(hModuleHandle);
        return pszHash;
    }

    lpvMapping = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);

    if (lpvMapping == NULL)
    {
        CloseHandle(hFileMapping);
        CloseHandle(hModuleHandle);
        return pszHash;
    }

    ZeroMemory(&MemoryBasicInformation, sizeof(MEMORY_BASIC_INFORMATION));

    VirtualQuery(lpvMapping, &MemoryBasicInformation, sizeof(MEMORY_BASIC_INFORMATION));

    if (MemoryBasicInformation.RegionSize)
    {
        char szHash[33];
        MD5Digest((unsigned char*)lpvMapping, (unsigned int)MemoryBasicInformation.RegionSize, szHash);
        MD5Digest((unsigned char*)szHash, 32, pszHash);
        for (int i = 0; i < 32; ++i)
            pszHash[i] = (char)toupper(pszHash[i]);
    }

    UnmapViewOfFile(lpvMapping);
    CloseHandle(hFileMapping);
    CloseHandle(hModuleHandle);

    return pszHash;
}

void compute_build_id()
{
    build_date = __DATE__;

    int days;
    int months = 0;
    int years;
    string16 month;
    string256 buffer;
    xr_strcpy(buffer, __DATE__);
    sscanf(buffer, "%s %d %d", month, &days, &years);

    for (int i = 0; i < 12; i++)
    {
        if (_stricmp(month_id[i], month))
            continue;

        months = i;
        break;
    }

    build_id = (years - start_year) * 365 + days - start_day;

    for (int i = 0; i < months; ++i)
        build_id += days_in_month[i];

    for (int i = 0; i < start_month - 1; ++i)
        build_id -= days_in_month[i];
}

//---------------------------------------------------------------------
// 2446363
// umbt@ukr.net
//////////////////////////////////////////////////////////////////////////
struct _SoundProcessor : public pureFrame
{
    virtual void _BCL OnFrame()
    {
        Device.Statistic->Sound.Begin();
        ::Sound->update(Device.vCameraPosition, Device.vCameraDirection, Device.vCameraTop);
        Device.Statistic->Sound.End();
    }
} 
SoundProcessor;

//////////////////////////////////////////////////////////////////////////
// global variables
ENGINE_API CApplication* pApp = NULL;
static HWND logoWindow = NULL;

ENGINE_API string512 g_sLaunchOnExit_params;
ENGINE_API string512 g_sLaunchOnExit_app;
ENGINE_API string_path g_sLaunchWorkingFolder;
// -------------------------------------------
// startup point
void InitEngine()
{
    Engine.Initialize();
    while (!g_bIntroFinished) Sleep(100);
    Device.Initialize();
}

struct path_excluder_predicate
{
    explicit path_excluder_predicate(xr_auth_strings_t const* ignore) : m_ignore(ignore) {}
    bool xr_stdcall is_allow_include(LPCSTR path)
    {
        if (!m_ignore)
            return true;

        return allow_to_include_path(*m_ignore, path);
    }
    xr_auth_strings_t const* m_ignore;
};

template <typename T>
void InitConfig(T& config, pcstr name, bool fatal = true, bool readOnly = true, bool loadAtStart = true, bool saveAtEnd = true, u32 sectCount = 0, const CInifile::allow_include_func_t& allowIncludeFunc = nullptr)
{
	string_path fname;
	FS.update_path(fname, "$game_config$", name);
	config = new CInifile(fname, readOnly, loadAtStart, saveAtEnd, sectCount, allowIncludeFunc);

	CHECK_OR_EXIT(config->section_count() || !fatal, make_string("Cannot find file %s.\nReinstalling application may fix this problem.", fname));
}

void InitSettings()
{
    Msg("EH: %s\n", ComputeModuleHash(szEngineHash));

    string_path fname;
    FS.update_path(fname, "$game_config$", "system.ltx");
    pSettings = xr_new<CInifile>(fname, TRUE);
    CHECK_OR_EXIT(0 != pSettings->section_count(), make_string("Cannot find file %s.\nReinstalling application may fix this problem.", fname));

    xr_auth_strings_t tmp_ignore_pathes, tmp_check_pathes;
    fill_auth_check_params(tmp_ignore_pathes, tmp_check_pathes);
    path_excluder_predicate tmp_excluder(&tmp_ignore_pathes);
    CInifile::allow_include_func_t includeFilter;
	includeFilter.bind(&tmp_excluder, &path_excluder_predicate::is_allow_include);

	InitConfig(pSettings, "system.ltx");
	InitConfig(pSettingsAuth, "system.ltx", true, true, true, false, 0, includeFilter);
	InitConfig(pFFSettings, "stcop_config.ltx", false, true, true, false);
	InitConfig(pGameIni, "game.ltx");
}

void InitConsole()
{
    Console = xr_new<CConsole>();
    Console->Initialize();

    xr_strcpy(Console->ConfigFile, "user.ltx");
    if (strstr(Core.Params, "-ltx "))
    {
        string64 c_name;
        sscanf(strstr(Core.Params, "-ltx ") + 5, "%[^ ] ", c_name);
        xr_strcpy(Console->ConfigFile, c_name);
    }
}

void InitInput()
{
    BOOL bCaptureInput = !strstr(Core.Params, "-i");
    pInput = xr_new<CInput>(bCaptureInput);
}

void destroyInput()
{
    xr_delete(pInput);
}

void InitSound1()
{
    CSound_manager_interface::_create(0);
}

void InitSound2()
{
    CSound_manager_interface::_create(1);
}

void destroySound()
{
    CSound_manager_interface::_destroy();
}

void destroySettings()
{
	auto s = const_cast<CInifile**>(&pSettings);
	xr_delete(*s);
	auto sa = const_cast<CInifile**>(&pSettingsAuth);
	xr_delete(*sa);
	xr_delete(pGameIni);
}

void destroyConsole()
{
    Console->Execute("cfg_save");
    Console->Destroy();
    xr_delete(Console);
}

void destroyEngine()
{
    Device.Destroy();
    Engine.Destroy();
}

void execUserScript()
{
    Console->Execute("default_controls");
    Console->ExecuteScript(Console->ConfigFile);
}

void slowdownthread(void*)
{
    for (;;)
    {
        if (Device.Statistic->fFPS < 30) 
            Sleep(1);
        if (Device.mt_bMustExit) 
            return;
        if (0 == pSettings) 
            return;
        if (0 == Console) 
            return;
        if (0 == pInput) 
            return;
        if (0 == pApp)
            return;
    }
}

void CheckPrivilegySlowdown()
{
#ifdef DEBUG
    if (strstr(Core.Params, "-slowdown"))
    {
        thread_spawn(slowdownthread, "slowdown", 0, 0);
    }
    if (strstr(Core.Params, "-slowdown2x"))
    {
        thread_spawn(slowdownthread, "slowdown", 0, 0);
        thread_spawn(slowdownthread, "slowdown", 0, 0);
    }
#endif // DEBUG
}

void Startup()
{
    InitSound1();
    execUserScript();
    InitSound2();

    // ...command line for auto start
    {
        LPCSTR pStartup = strstr(Core.Params, "-start ");
        if (pStartup) 
            Console->Execute(pStartup + 1);
    }
    {
        LPCSTR pStartup = strstr(Core.Params, "-load ");
        if (pStartup)
            Console->Execute(pStartup + 1);
    }

    // Initialize APP
    ShowWindow(Device.m_hWnd, SW_SHOWNORMAL);
    Device.Create();

    LALib.OnCreate();
    pApp = xr_new<CApplication>();
    g_pGamePersistent = (IGame_Persistent*)NEW_INSTANCE(CLSID_GAME_PERSISTANT);
    g_SpatialSpace = xr_new<ISpatial_DB>();
    g_SpatialSpacePhysic = xr_new<ISpatial_DB>();

    // Destroy LOGO
    DestroyWindow(logoWindow);
    logoWindow = NULL;

    // Main cycle
    Memory.mem_usage();
    Device.Run();

    // Destroy APP
    xr_delete(g_SpatialSpacePhysic);
    xr_delete(g_SpatialSpace);
    DEL_INSTANCE(g_pGamePersistent);
    xr_delete(pApp);
    Engine.Event.Dump();

    // Destroying
    destroyInput();

    destroySettings();

    LALib.OnDestroy();

    destroyConsole();

    destroySound();

    destroyEngine();
}

IStream* CreateStreamOnResource(LPCTSTR lpName, LPCTSTR lpType)
{
    IStream* ipStream = NULL;

    HRSRC hrsrc = FindResource(NULL, lpName, lpType);
    if (hrsrc == NULL)
        goto Return;

    DWORD dwResourceSize = SizeofResource(NULL, hrsrc);
    HGLOBAL hglbImage = LoadResource(NULL, hrsrc);
    if (hglbImage == NULL)
        goto Return;

    LPVOID pvSourceResourceData = LockResource(hglbImage);
    if (pvSourceResourceData == NULL)
        goto Return;

    HGLOBAL hgblResourceData = GlobalAlloc(GMEM_MOVEABLE, dwResourceSize);
    if (hgblResourceData == NULL)
        goto Return;

    LPVOID pvResourceData = GlobalLock(hgblResourceData);

    if (pvResourceData == NULL)
        goto FreeData;

    CopyMemory(pvResourceData, pvSourceResourceData, dwResourceSize);

    GlobalUnlock(hgblResourceData);

    if (SUCCEEDED(CreateStreamOnHGlobal(hgblResourceData, TRUE, &ipStream)))
        goto Return;

FreeData:
    GlobalFree(hgblResourceData);

Return:
    return ipStream;
}

void RegisterWindowClass(HINSTANCE hInst)
{
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = c_szSplashClass;
    RegisterClass(&wc);
}

HWND CreateSplashWindow(HINSTANCE hInst)
{
    HWND hwndOwner = CreateWindow(c_szSplashClass, NULL, WS_POPUP,
        0, 0, 0, 0, NULL, NULL, hInst, NULL);
    return CreateWindowEx(WS_EX_LAYERED, c_szSplashClass, NULL, WS_POPUP | WS_VISIBLE,
        0, 0, 0, 0, hwndOwner, NULL, hInst, NULL);
}

HWND WINAPI ShowSplash(HINSTANCE hInstance, int nCmdShow)
{
    MSG msg;
    HWND hWnd;

    //image
    CImage img;                             //объект изображения

    WCHAR path[MAX_PATH];

    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring ws(path);

    std::string splash_path(ws.begin(), ws.end());
    splash_path = splash_path.erase(splash_path.find_last_of('\\'), splash_path.size() - 1);
    splash_path += "\\splash.png";

    img.Load(splash_path.c_str());              //загрузка сплеша

    int splashWidth = img.GetWidth();            //фиксируем ширину картинки
    int splashHeight = img.GetHeight();            //фиксируем высоту картинки

    if (splashWidth == 0 || splashHeight == 0)  //если картинки нет на диске, то грузим из ресурсов
    {
        img.Destroy();
        img.Load(CreateStreamOnResource(MAKEINTRESOURCE(IDB_PNG1), _T("PNG")));//загружаем сплеш
        splashWidth = img.GetWidth();
        splashHeight = img.GetHeight();
    }

    int scr_x = GetSystemMetrics(SM_CXSCREEN);
    int scr_y = GetSystemMetrics(SM_CYSCREEN);

    int pos_x = (scr_x / 2) - (splashWidth / 2);
    int pos_y = (scr_y / 2) - (splashHeight / 2);

    hWnd = CreateSplashWindow(hInstance);

    if (!hWnd) 
        return FALSE;

    HDC hdcScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hdcScreen);

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, splashWidth, splashHeight);
    HBITMAP hBmpOld = (HBITMAP)SelectObject(hDC, hBmp);

	BLENDFUNCTION blend = { 0 };
	blend.SourceConstantAlpha = 255;

    //рисуем картиночку
	if (img.GetBPP() == 32)
	{
		blend.AlphaFormat = AC_SRC_ALPHA;
		blend.BlendOp = AC_SRC_OVER;

		for (int i = 0; i < img.GetWidth(); i++)
		{
			for (int j = 0; j < img.GetHeight(); j++)
			{
				BYTE* ptr = (BYTE*)img.GetPixelAddress(i, j);
				ptr[0] = ((ptr[0] * ptr[3]) + 127) / 255;
				ptr[1] = ((ptr[1] * ptr[3]) + 127) / 255;
				ptr[2] = ((ptr[2] * ptr[3]) + 127) / 255;
			}
		}
	}

    img.AlphaBlend(hDC, 0, 0, splashWidth, splashHeight, 0, 0, splashWidth, splashHeight);

    POINT ptPos = { 0, 0 };
    SIZE sizeWnd = { splashWidth, splashHeight };
    POINT ptSrc = { 0, 0 };
    HWND hDT = GetDesktopWindow();

    if (hDT)
    {
        RECT rcDT;
        GetWindowRect(hDT, &rcDT);
        ptPos.x = (rcDT.right - splashWidth) / 2;
        ptPos.y = (rcDT.bottom - splashHeight) / 2;
    }

    UpdateLayeredWindow(hWnd, hdcScreen, &ptPos, &sizeWnd, hDC, &ptSrc, 0, &blend, ULW_ALPHA);

    return hWnd;
}

static INT_PTR CALLBACK logDlgProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_DESTROY:
        break;
    case WM_CLOSE:
        DestroyWindow(hw);
        break;
    case WM_COMMAND:
        if (LOWORD(wp) == IDCANCEL)
            DestroyWindow(hw);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

#define dwStickyKeysStructSize sizeof( STICKYKEYS )
#define dwFilterKeysStructSize sizeof( FILTERKEYS )
#define dwToggleKeysStructSize sizeof( TOGGLEKEYS )

struct damn_keys_filter
{
    BOOL bScreenSaverState;

    // Sticky & Filter & Toggle keys
    STICKYKEYS StickyKeysStruct;
    FILTERKEYS FilterKeysStruct;
    TOGGLEKEYS ToggleKeysStruct;

    DWORD dwStickyKeysFlags;
    DWORD dwFilterKeysFlags;
    DWORD dwToggleKeysFlags;

    damn_keys_filter()
    {
        // Screen saver stuff
        bScreenSaverState = FALSE;

        // Saveing current state
        SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, (PVOID)&bScreenSaverState, 0);

        // Disable screensaver
        if (bScreenSaverState)
            SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);

        dwStickyKeysFlags = 0;
        dwFilterKeysFlags = 0;
        dwToggleKeysFlags = 0;

        ZeroMemory(&StickyKeysStruct, dwStickyKeysStructSize);
        ZeroMemory(&FilterKeysStruct, dwFilterKeysStructSize);
        ZeroMemory(&ToggleKeysStruct, dwToggleKeysStructSize);

        StickyKeysStruct.cbSize = dwStickyKeysStructSize;
        FilterKeysStruct.cbSize = dwFilterKeysStructSize;
        ToggleKeysStruct.cbSize = dwToggleKeysStructSize;

        // Saving current state
        SystemParametersInfo(SPI_GETSTICKYKEYS, dwStickyKeysStructSize, (PVOID)&StickyKeysStruct, 0);
        SystemParametersInfo(SPI_GETFILTERKEYS, dwFilterKeysStructSize, (PVOID)&FilterKeysStruct, 0);
        SystemParametersInfo(SPI_GETTOGGLEKEYS, dwToggleKeysStructSize, (PVOID)&ToggleKeysStruct, 0);

        if (StickyKeysStruct.dwFlags & SKF_AVAILABLE)
        {
            // Disable StickyKeys feature
            dwStickyKeysFlags = StickyKeysStruct.dwFlags;
            StickyKeysStruct.dwFlags = 0;
            SystemParametersInfo(SPI_SETSTICKYKEYS, dwStickyKeysStructSize, (PVOID)&StickyKeysStruct, 0);
        }

        if (FilterKeysStruct.dwFlags & FKF_AVAILABLE)
        {
            // Disable FilterKeys feature
            dwFilterKeysFlags = FilterKeysStruct.dwFlags;
            FilterKeysStruct.dwFlags = 0;
            SystemParametersInfo(SPI_SETFILTERKEYS, dwFilterKeysStructSize, (PVOID)&FilterKeysStruct, 0);
        }

        if (ToggleKeysStruct.dwFlags & TKF_AVAILABLE)
        {
            // Disable FilterKeys feature
            dwToggleKeysFlags = ToggleKeysStruct.dwFlags;
            ToggleKeysStruct.dwFlags = 0;
            SystemParametersInfo(SPI_SETTOGGLEKEYS, dwToggleKeysStructSize, (PVOID)&ToggleKeysStruct, 0);
        }
    }

    ~damn_keys_filter()
    {
        if (bScreenSaverState)
            // Restoring screen saver
            SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE, NULL, 0);

        if (dwStickyKeysFlags)
        {
            // Restore StickyKeys feature
            StickyKeysStruct.dwFlags = dwStickyKeysFlags;
            SystemParametersInfo(SPI_SETSTICKYKEYS, dwStickyKeysStructSize, (PVOID)&StickyKeysStruct, 0);
        }

        if (dwFilterKeysFlags)
        {
            // Restore FilterKeys feature
            FilterKeysStruct.dwFlags = dwFilterKeysFlags;
            SystemParametersInfo(SPI_SETFILTERKEYS, dwFilterKeysStructSize, (PVOID)&FilterKeysStruct, 0);
        }

        if (dwToggleKeysFlags)
        {
            // Restore FilterKeys feature
            ToggleKeysStruct.dwFlags = dwToggleKeysFlags;
            SystemParametersInfo(SPI_SETTOGGLEKEYS, dwToggleKeysStructSize, (PVOID)&ToggleKeysStruct, 0);
        }
    }
};

#undef dwStickyKeysStructSize
#undef dwFilterKeysStructSize
#undef dwToggleKeysStructSize

#include "xr_ioc_cmd.h"

//typedef void DUMMY_STUFF (const void*,const u32&,void*);
//XRCORE_API DUMMY_STUFF *g_temporary_stuff;

//#define TRIVIAL_ENCRYPTOR_DECODER
//#include "trivial_encryptor.h"

//#define RUSSIAN_BUILD

// forward declaration for Parental Control checks
BOOL IsPCAccessAllowed();

int APIENTRY WinMain_impl(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nCmdShow)
{
    Debug._initialize(false);
    
    // Check for another instance
#ifdef NO_MULTI_INSTANCES
#define STALKER_PRESENCE_MUTEX "Local\\STALKER-COP"

    HANDLE hCheckPresenceMutex = INVALID_HANDLE_VALUE;
    hCheckPresenceMutex = OpenMutex(READ_CONTROL, FALSE, STALKER_PRESENCE_MUTEX);
    if (hCheckPresenceMutex == NULL)
    {
        // New mutex
        hCheckPresenceMutex = CreateMutex(NULL, FALSE, STALKER_PRESENCE_MUTEX);
        if (hCheckPresenceMutex == NULL)
            // Shit happens
            return 2;
    }
    else
    {
        // Already running
        CloseHandle(hCheckPresenceMutex);
        return 1;
    }
#endif

    RegisterWindowClass(hInstance);
    logoWindow = ShowSplash(hInstance, nCmdShow);

    SendMessage(logoWindow, WM_DESTROY, 0, 0);

    g_bIntroFinished = TRUE;

    g_sLaunchOnExit_app[0] = NULL;
    g_sLaunchOnExit_params[0] = NULL;

    LPCSTR fsgame_ltx_name = "-fsltx ";
    string_path fsgame = "";
    if (strstr(lpCmdLine, fsgame_ltx_name))
    {
        int sz = xr_strlen(fsgame_ltx_name);
        sscanf(strstr(lpCmdLine, fsgame_ltx_name) + sz, "%[^ ] ", fsgame);
    }

    compute_build_id();
    Core._initialize("xray", NULL, TRUE, fsgame[0] ? fsgame : NULL);

    InitSettings();

    // Adjust player & computer name for Asian
    if (pSettings->line_exist("string_table", "no_native_input"))
    {
        xr_strcpy(Core.UserName, sizeof(Core.UserName), "Player");
        xr_strcpy(Core.CompName, sizeof(Core.CompName), "Computer");
    }

    {
        damn_keys_filter filter;
        (void)filter;

        FPU::m24r();
        InitEngine();

        InitInput();

        InitConsole();

        Engine.External.CreateRendererList();

        Msg("command line %s", lpCmdLine);

        if (strstr(Core.Params, "-r2a"))
            Console->Execute("renderer renderer_r2a");
        else if (strstr(Core.Params, "-r2"))
            Console->Execute("renderer renderer_r2");
        else
        {
            CCC_LoadCFG_custom* pTmp = xr_new<CCC_LoadCFG_custom>("renderer ");
            pTmp->Execute(Console->ConfigFile);
            xr_delete(pTmp);
        }

        Engine.External.Initialize();
        Console->Execute("stat_memory");

        Startup();
        Core._destroy();

        // check for need to execute something external
        if (xr_strlen(g_sLaunchOnExit_app))
        {
            //CreateProcess need to return results to next two structures
            STARTUPINFO si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            //We use CreateProcess to setup working folder
            char const* temp_wf = (xr_strlen(g_sLaunchWorkingFolder) > 0) ? g_sLaunchWorkingFolder : NULL;
            CreateProcess(g_sLaunchOnExit_app, g_sLaunchOnExit_params, NULL, NULL, FALSE, 0, NULL, temp_wf, &si, &pi);

        }
#ifdef NO_MULTI_INSTANCES
        // Delete application presence mutex
        CloseHandle(hCheckPresenceMutex);
#endif
    }

    // here damn_keys_filter class instanse will be destroyed
    return 0;
}

int stack_overflow_exception_filter(int exception_code)
{
    if (exception_code == EXCEPTION_STACK_OVERFLOW)
    {
        // Do not call _resetstkoflw here, because
        // at this point, the stack is not yet unwound.
        // Instead, signal that the handler (the __except block)
        // is to be executed.
        return EXCEPTION_EXECUTE_HANDLER;
    }
    else
        return EXCEPTION_CONTINUE_SEARCH;
}

#include <boost/crc.hpp>

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nCmdShow)
{
    __try
    {
        WinMain_impl(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
    }
    __except (stack_overflow_exception_filter(GetExceptionCode()))
    {
        _resetstkoflw();
        FATAL("stack overflow");
    }

    return (0);
}

LPCSTR _GetFontTexName(LPCSTR section)
{
    static char* tex_names[] = {"texture800", "texture", "texture1600"};
    int def_idx = 1;//default 1024x768
    int idx = def_idx;

    u32 h = Device.dwHeight;

    if (h <= 600) 
        idx = 0;
    else if (h < 1024) 
        idx = 1;
    else 
        idx = 2;

    while (idx >= 0)
    {
        if (pSettings->line_exist(section, tex_names[idx]))
            return pSettings->r_string(section, tex_names[idx]);
        --idx;
    }
    return pSettings->r_string(section, tex_names[def_idx]);
}

void _InitializeFont(CGameFont*& F, LPCSTR section, u32 flags)
{
    LPCSTR font_tex_name = _GetFontTexName(section);
    R_ASSERT(font_tex_name);

    LPCSTR sh_name = pSettings->r_string(section, "shader");
    if (!F)
    {
        F = xr_new<CGameFont>(sh_name, font_tex_name, flags);
    }
    else
        F->Initialize(sh_name, font_tex_name);

    if (pSettings->line_exist(section, "size"))
    {
        float sz = pSettings->r_float(section, "size");
        if (flags&CGameFont::fsDeviceIndependent) F->SetHeightI(sz);
        else F->SetHeight(sz);
    }
    if (pSettings->line_exist(section, "interval"))
        F->SetInterval(pSettings->r_fvector2(section, "interval"));
}

CApplication::CApplication()
{
    ll_dwReference = 0;

    max_load_stage = 0;

    // events
    eQuit = Engine.Event.Handler_Attach("KERNEL:quit", this);
    eStart = Engine.Event.Handler_Attach("KERNEL:start", this);
    eStartLoad = Engine.Event.Handler_Attach("KERNEL:load", this);
    eDisconnect = Engine.Event.Handler_Attach("KERNEL:disconnect", this);
    eConsole = Engine.Event.Handler_Attach("KERNEL:console", this);
    eStartMPDemo = Engine.Event.Handler_Attach("KERNEL:start_mp_demo", this);

    // levels
    Level_Current = u32(-1);
    Level_Scan();

    // Font
    pFontSystem = NULL;

    // Register us
    Device.seqFrame.Add(this, REG_PRIORITY_HIGH + 1000);

    if (psDeviceFlags.test(mtSound)) 
        Device.seqFrameMT.Add(&SoundProcessor);
    else 
        Device.seqFrame.Add(&SoundProcessor);

    Console->Show();

    // App Title
    ls_header[0] = '\0';
    ls_tip_number[0] = '\0';
    ls_tip[0] = '\0';
}

CApplication::~CApplication()
{
    Console->Hide();

    // font
    xr_delete(pFontSystem);

    Device.seqFrameMT.Remove(&SoundProcessor);
    Device.seqFrame.Remove(&SoundProcessor);
    Device.seqFrame.Remove(this);

    // events
    Engine.Event.Handler_Detach(eConsole, this);
    Engine.Event.Handler_Detach(eDisconnect, this);
    Engine.Event.Handler_Detach(eStartLoad, this);
    Engine.Event.Handler_Detach(eStart, this);
    Engine.Event.Handler_Detach(eQuit, this);
    Engine.Event.Handler_Detach(eStartMPDemo, this);
}

extern CRenderDevice Device;

void CApplication::OnEvent(EVENT E, u64 P1, u64 P2)
{
    if (E == eQuit)
    {
        PostQuitMessage(0);

        for (u32 i = 0; i < Levels.size(); i++)
        {
            xr_free(Levels[i].folder);
            xr_free(Levels[i].name);
        }
    }
    else if (E == eStart)
    {
        LPSTR op_server = LPSTR(P1);
        LPSTR op_client = LPSTR(P2);
        Level_Current = u32(-1);
        R_ASSERT(0 == g_pGameLevel);
        R_ASSERT(0 != g_pGamePersistent);
        Console->Execute("main_menu off");
        Console->Hide();
        g_pGamePersistent->PreStart(op_server);
        g_pGameLevel = (IGame_Level*)NEW_INSTANCE(CLSID_GAME_LEVEL);
        pApp->LoadBegin();
        g_pGamePersistent->Start(op_server);
        g_pGameLevel->net_Start(op_server, op_client);
        pApp->LoadEnd();
        xr_free(op_server);
        xr_free(op_client);
    }
    else if (E == eDisconnect)
    {
        ls_header[0] = '\0';
        ls_tip_number[0] = '\0';
        ls_tip[0] = '\0';

        if (g_pGameLevel)
        {
            Console->Hide();
            g_pGameLevel->net_Stop();
            DEL_INSTANCE(g_pGameLevel);
            Console->Show();

            if ((FALSE == Engine.Event.Peek("KERNEL:quit")) && (FALSE == Engine.Event.Peek("KERNEL:start")))
            {
                Console->Execute("main_menu off");
                Console->Execute("main_menu on");
            }
        }
        R_ASSERT(0 != g_pGamePersistent);
        g_pGamePersistent->Disconnect();
    }
    else if (E == eConsole)
    {
        LPSTR command = (LPSTR)P1;
        Console->ExecuteCommand(command, false);
        xr_free(command);
    }
    else if (E == eStartMPDemo)
    {
        LPSTR demo_file = LPSTR(P1);

        R_ASSERT(0 == g_pGameLevel);
        R_ASSERT(0 != g_pGamePersistent);

        Console->Execute("main_menu off");
        Console->Hide();
        Device.Reset(false);

        g_pGameLevel = (IGame_Level*)NEW_INSTANCE(CLSID_GAME_LEVEL);
        shared_str server_options = g_pGameLevel->OpenDemoFile(demo_file);

        //-----------------------------------------------------------
        g_pGamePersistent->PreStart(server_options.c_str());
        //-----------------------------------------------------------

        pApp->LoadBegin();
        g_pGamePersistent->Start("");
        g_pGameLevel->net_StartPlayDemo();
        pApp->LoadEnd();

        xr_free(demo_file);
    }
}

static CTimer phase_timer;
extern ENGINE_API BOOL g_appLoaded = FALSE;

void CApplication::LoadBegin()
{
    ll_dwReference++;
    if (1 == ll_dwReference)
    {
        g_appLoaded = FALSE;
        _InitializeFont(pFontSystem, "ui_font_letterica18_russian", 0);
        m_pRender->LoadBegin();
        phase_timer.Start();
        load_stage = 0;
    }
}

void CApplication::LoadEnd()
{
    ll_dwReference--;
    if (0 == ll_dwReference)
    {
        Msg("* phase time: %d ms", phase_timer.GetElapsed_ms());
		Msg("* phase cmem: %lld K", Memory.mem_usage() / 1024);
        Console->Execute("stat_memory");
        g_appLoaded = TRUE;
    }
}

void CApplication::destroy_loading_shaders()
{
    m_pRender->destroy_loading_shaders();
}

#include "Render.h"

void CApplication::LoadDraw()
{
    if (g_appLoaded) 
        return;

    Device.dwFrame += 1;

	Render->firstViewPort = MAIN_VIEWPORT;
	Render->lastViewPort = MAIN_VIEWPORT;
	Render->currentViewPort = MAIN_VIEWPORT;
	Render->needPresenting = true;

    if (!Device.Begin()) 
        return;

    load_draw_internal();

    Device.End();
}

void CApplication::LoadTitleInt(LPCSTR str1, LPCSTR str2, LPCSTR str3)
{
    xr_strcpy(ls_header, str1);
    xr_strcpy(ls_tip_number, str2);
    xr_strcpy(ls_tip, str3);
}

void CApplication::LoadStage()
{
    load_stage++;
    VERIFY(ll_dwReference);
    Msg("* phase time: %d ms", phase_timer.GetElapsed_ms());
    phase_timer.Start();
	Msg("* phase cmem: %lld K", Memory.mem_usage() / 1024);

    if (g_pGamePersistent->GameType() == 1 && strstr(Core.Params, "alife"))
        max_load_stage = 17;
    else
        max_load_stage = 14;
    LoadDraw();
}

void CApplication::LoadSwitch()
{}

// Sequential
void CApplication::OnFrame()
{
    Engine.Event.OnFrame();
    g_SpatialSpace->update();
    g_SpatialSpacePhysic->update();
    if (g_pGameLevel)
        g_pGameLevel->SoundEvent_Dispatch();
}

void CApplication::Level_Append(LPCSTR folder)
{
    string_path N1, N2, N3, N4;
    strconcat(sizeof(N1), N1, folder, "level");
    strconcat(sizeof(N2), N2, folder, "level.ltx");
    strconcat(sizeof(N3), N3, folder, "level.geom");
    strconcat(sizeof(N4), N4, folder, "level.cform");
    if (FS.exist("$game_levels$", N1) && FS.exist("$game_levels$", N2) && FS.exist("$game_levels$", N3) && FS.exist("$game_levels$", N4))
    {
        sLevelInfo LI;
        LI.folder = xr_strdup(folder);
        LI.name = 0;
        Levels.push_back(LI);
    }
}

void CApplication::Level_Scan()
{
    for (u32 i = 0; i < Levels.size(); i++)
    {
        xr_free(Levels[i].folder);
        xr_free(Levels[i].name);
    }

    Levels.clear();

    xr_vector<char*>* folder = FS.file_list_open("$game_levels$", FS_ListFolders | FS_RootOnly);

    for (u32 i = 0; i < folder->size(); ++i)
        Level_Append((*folder)[i]);

    FS.file_list_close(folder);
}

void gen_logo_name(string_path& dest, LPCSTR level_name, int num)
{
    strconcat(sizeof(dest), dest, "intro\\intro_", level_name);

    u32 len = xr_strlen(dest);
    if (dest[len - 1] == '\\')
        dest[len - 1] = 0;

    string16 buff;
    xr_strcat(dest, sizeof(dest), "_");
    xr_strcat(dest, sizeof(dest), itoa(num + 1, buff, 10));
}

void CApplication::Level_Set(u32 L)
{
    if (L >= Levels.size()) return;
    FS.get_path("$level$")->_set(Levels[L].folder);

    static string_path path;

    if (Level_Current != L)
    {
        path[0] = 0;

        Level_Current = L;

        int count = 0;
        while (true)
        {
            string_path temp2;
            gen_logo_name(path, Levels[L].folder, count);
            if (FS.exist(temp2, "$game_textures$", path, ".dds") || FS.exist(temp2, "$level$", path, ".dds"))
                count++;
            else
                break;
        }

        if (count)
        {
            int num = ::Random.randI(count);
            gen_logo_name(path, Levels[L].folder, num);
        }
    }

    if (path[0])
        m_pRender->setLevelLogo(path);
}

int CApplication::Level_ID(LPCSTR name, LPCSTR ver, bool bSet)
{
    int result = -1;

    CLocatorAPI::archives_it it = FS.m_archives.begin();
    CLocatorAPI::archives_it it_e = FS.m_archives.end();
    bool arch_res = false;

    for (; it != it_e; ++it)
    {
        CLocatorAPI::archive& A = *it;
        if (A.hSrcFile == NULL)
        {
            LPCSTR ln = A.header->r_string("header", "level_name");
            LPCSTR lv = A.header->r_string("header", "level_ver");
            if (0 == stricmp(ln, name) && 0 == stricmp(lv, ver))
            {
                FS.LoadArchive(A);
                arch_res = true;
            }
        }
    }

    if (arch_res)
        Level_Scan();

    string256 buffer;
    strconcat(sizeof(buffer), buffer, name, "\\");
    for (u32 I = 0; I < Levels.size(); ++I)
    {
        if (0 == stricmp(buffer, Levels[I].folder))
        {
            result = int(I);
            break;
        }
    }

    if (bSet && result != -1)
        Level_Set(result);

    if (arch_res)
        g_pGamePersistent->OnAssetsChanged();

    return result;
}

CInifile* CApplication::GetArchiveHeader(LPCSTR name, LPCSTR ver)
{
    CLocatorAPI::archives_it it = FS.m_archives.begin();
    CLocatorAPI::archives_it it_e = FS.m_archives.end();

    for (; it != it_e; ++it)
    {
        CLocatorAPI::archive& A = *it;

        LPCSTR ln = A.header->r_string("header", "level_name");
        LPCSTR lv = A.header->r_string("header", "level_ver");
        if (0 == stricmp(ln, name) && 0 == stricmp(lv, ver))
        {
            return A.header;
        }
    }
    return NULL;
}

void CApplication::LoadAllArchives()
{
    if (FS.load_all_unloaded_archives())
    {
        Level_Scan();
        g_pGamePersistent->OnAssetsChanged();
    }
}

// Parential control for Vista and upper
typedef BOOL(*PCCPROC)(CHAR*);

BOOL IsPCAccessAllowed()
{
    CHAR szPCtrlChk[MAX_PATH], szGDF[MAX_PATH], *pszLastSlash;
    HINSTANCE hPCtrlChk = NULL;
    PCCPROC pctrlchk = NULL;
    BOOL bAllowed = TRUE;

    if (!GetModuleFileName(NULL, szPCtrlChk, MAX_PATH))
        return TRUE;

    if ((pszLastSlash = strrchr(szPCtrlChk, '\\')) == NULL)
        return TRUE;

    *pszLastSlash = '\0';

    strcpy_s(szGDF, szPCtrlChk);

    strcat_s(szPCtrlChk, "\\pctrlchk.dll");
    if (GetFileAttributes(szPCtrlChk) == INVALID_FILE_ATTRIBUTES)
        return TRUE;

    if ((pszLastSlash = strrchr(szGDF, '\\')) == NULL)
        return TRUE;

    *pszLastSlash = '\0';

    strcat_s(szGDF, "\\Stalker-COP.exe");
    if (GetFileAttributes(szGDF) == INVALID_FILE_ATTRIBUTES)
        return TRUE;

    if ((hPCtrlChk = LoadLibrary(szPCtrlChk)) == NULL)
        return TRUE;

    if ((pctrlchk = (PCCPROC)GetProcAddress(hPCtrlChk, "pctrlchk")) == NULL)
    {
        FreeLibrary(hPCtrlChk);
        return TRUE;
    }

    bAllowed = pctrlchk(szGDF);

    FreeLibrary(hPCtrlChk);

    return bAllowed;
}

#pragma optimize("g", off)
void CApplication::load_draw_internal()
{
    m_pRender->load_draw_internal(*this);
}