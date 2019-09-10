// Made possible by the Build Engine linking exception.

#if defined _WIN32 || defined __linux__ || defined EDUKE32_OSX
# define VW_ENABLED
#endif

#ifdef VW_ENABLED
# define VOIDWRAP_RUNTIMELINK
# include "voidwrap_steam.h"
#endif

#include "build.h"
#include "communityapi.h"

#ifdef VW_ENABLED

static bool steamworks_enabled;
static VW_LIBHANDLE wrapper_handle;

#ifdef _WIN32
# ifdef _WIN64
static char const wrapper_lib[] = "voidwrap_steam_x64.dll";
# else
static char const wrapper_lib[] = "voidwrap_steam_x86.dll";
# endif
#else
static char const wrapper_lib[] = "libvoidwrap_steam.so.0";
#endif

#ifdef VWSCREENSHOT
static void steam_callback_screenshotrequested()
{
#ifdef VWDEBUG
    initprintf("Voidwrap: Preparing steam screenshot!\n");
#endif
    videoCaptureScreen("steam0000.png", 0);
}

#if 0
static void steam_callback_screenshotready(int32_t result)
{
#ifdef VWDEBUG
    initprintf("Voidwrap: Steam screenshot ready! - Result: %d\n", result);
#endif
}
#endif
#endif

#ifdef VWDEBUG
static void steam_callback_printdebug(char const * str)
{
    initprintf("[DEBUG](%s): %s\n", wrapper_lib, str);
}
#endif

#endif

void communityapiInit()
{
#ifdef VW_ENABLED
    wrapper_handle = Voidwrap_LoadLibrary(wrapper_lib);
    if (wrapper_handle == nullptr)
    {
#ifdef VWDEBUG
#ifdef _WIN32
        initprintf("Voidwrap: %s missing or load failed.\n", wrapper_lib);
#else
        initprintf("Voidwrap: %s dlopen error: %s\n", wrapper_lib, dlerror());
#endif
#endif
        return;
    }

#ifdef VWDEBUG
    initprintf("Voidwrap: %s found!\n", wrapper_lib);
#endif

    Voidwrap_Steam_Init = (VW_BOOL)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_Init");
    Voidwrap_Steam_Shutdown = (VW_VOID)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_Shutdown");
    Voidwrap_Steam_RunCallbacks = (VW_VOID)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_RunCallbacks");

#ifdef VWDEBUG
    Voidwrap_Steam_SetCallback_PrintDebug = (VW_SETCALLBACK_CHARPTR)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SetCallback_PrintDebug");
    Voidwrap_Steam_SetCallback_PrintDebug(steam_callback_printdebug);
#endif

#ifdef VWSCREENSHOT
    Voidwrap_Steam_SendScreenshot = (VW_BOOL_SCREENSHOT)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SendScreenshot");
    Voidwrap_Steam_SetCallback_ScreenshotRequested = (VW_SETCALLBACK_NOPARAM)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SetCallback_ScreenshotRequested");
    Voidwrap_Steam_SetCallback_ScreenshotRequested(steam_callback_screenshotrequested);
#if 0
    Voidwrap_Steam_SetCallback_ScreenshotReady = (VW_SETCALLBACK_INT32)Voidwrap_GetSymbol(wrapper_handle, "Voidwrap_Steam_SetCallback_ScreenshotReady");
    Voidwrap_Steam_SetCallback_ScreenshotReady(steam_callback_screenshotready);
#endif
#endif

    if (Voidwrap_Steam_Init == nullptr || Voidwrap_Steam_RunCallbacks == nullptr)
    {
#ifdef VWDEBUG
        initprintf("Voidwrap: getproc_ failure.\n");
#endif
        return;
    }

    if (!Voidwrap_Steam_Init())
    {
#ifdef VWDEBUG
        initprintf("Voidwrap: Steamworks init failure.\n");
#endif
        return;
    }

#ifdef VWDEBUG
    initprintf("Voidwrap: Steamworks init success!\n");
#endif
    steamworks_enabled = true;
#endif
}

void communityapiShutdown()
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    Voidwrap_Steam_Shutdown();
#endif
}

void communityapiRunCallbacks()
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    Voidwrap_Steam_RunCallbacks();
#endif
}

#ifdef VWSCREENSHOT
void communityapiSendScreenshot(char * filename)
{
#ifdef VW_ENABLED
    if (!steamworks_enabled)
        return;

    char fullpath[BMAX_PATH];
    buildvfs_getcwd(fullpath, sizeof(fullpath));
    Bstrcat(fullpath, "/");
    Bstrcat(fullpath, filename);
#ifdef VWDEBUG
    OSD_Printf("Voidwrap: Steam screenshot full path: %s\n", fullpath);
#endif

    Voidwrap_Steam_SendScreenshot(fullpath, xdim, ydim);
#else
    UNREFERENCED_PARAMETER(filename);
#endif
}
#endif
