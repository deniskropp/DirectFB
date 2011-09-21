#include <init.h>

#include <windows.h>

extern void IDirectFB_Requestor_ctor(void);
extern void IDirectFBDataBuffer_Requestor_ctor(void);
extern void IDirectFBDisplayLayer_Requestor_ctor(void);
extern void IDirectFBEventBuffer_Requestor_ctor(void);
extern void IDirectFBFont_Requestor_ctor(void);
extern void IDirectFBImageProvider_Requestor_ctor(void);
extern void IDirectFBInputDevice_Requestor_ctor(void);
extern void IDirectFBPalette_Requestor_ctor(void);
extern void IDirectFBScreen_Requestor_ctor(void);
extern void IDirectFBSurface_Requestor_ctor(void);
extern void IDirectFBVideoProvider_Requestor_ctor(void);
extern void IDirectFBWindow_Requestor_ctor(void);

extern void IDirectFB_Dispatcher_ctor(void);
extern void IDirectFBDataBuffer_Dispatcher_ctor(void);
extern void IDirectFBDisplayLayer_Dispatcher_ctor(void);
extern void IDirectFBEventBuffer_Dispatcher_ctor(void);
extern void IDirectFBFont_Dispatcher_ctor(void);
extern void IDirectFBImageProvider_Dispatcher_ctor(void);
extern void IDirectFBInputDevice_Dispatcher_ctor(void);
extern void IDirectFBPalette_Dispatcher_ctor(void);
extern void IDirectFBScreen_Dispatcher_ctor(void);
extern void IDirectFBSurface_Dispatcher_ctor(void);
extern void IDirectFBVideoProvider_Dispatcher_ctor(void);
extern void IDirectFBWindow_Dispatcher_ctor(void);

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpReserved )  // reserved
{
    // Perform actions based on the reason for calling.
    switch( fdwReason ) 
    { 
        case DLL_PROCESS_ATTACH:
         // Initialize once for each new process.
         // Return FALSE to fail DLL load.
               __DFB_init_all();

               IDirectFB_Requestor_ctor();
               IDirectFBDataBuffer_Requestor_ctor();
               IDirectFBDisplayLayer_Requestor_ctor();
               IDirectFBEventBuffer_Requestor_ctor();
               IDirectFBFont_Requestor_ctor();
               IDirectFBImageProvider_Requestor_ctor();
               IDirectFBInputDevice_Requestor_ctor();
               IDirectFBPalette_Requestor_ctor();
               IDirectFBScreen_Requestor_ctor();
               IDirectFBSurface_Requestor_ctor();
               //IDirectFBVideoProvider_Requestor_ctor();
               IDirectFBWindow_Requestor_ctor();

               //IDirectFB_Dispatcher_ctor();
               IDirectFBDataBuffer_Dispatcher_ctor();
               //IDirectFBDisplayLayer_Dispatcher_ctor();
               IDirectFBEventBuffer_Dispatcher_ctor();
               //IDirectFBFont_Dispatcher_ctor();
               //IDirectFBImageProvider_Dispatcher_ctor();
               //IDirectFBInputDevice_Dispatcher_ctor();
               //IDirectFBPalette_Dispatcher_ctor();
               //IDirectFBScreen_Dispatcher_ctor();
               //IDirectFBSurface_Dispatcher_ctor();
               //IDirectFBVideoProvider_Dispatcher_ctor();
               //IDirectFBWindow_Dispatcher_ctor();
            break;

        case DLL_THREAD_ATTACH:
         // Do thread-specific initialization.
            break;

        case DLL_THREAD_DETACH:
         // Do thread-specific cleanup.
            break;

        case DLL_PROCESS_DETACH:
         // Perform any necessary cleanup.
            __DFB_deinit_all();
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}