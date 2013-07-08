/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/


#include <config.h>

extern "C" {
#include <direct/mem.h>

#include <voodoo/play.h>
}


#if 0  // FIXME: only get 255.255.255.255 and 0.0.0.0

DirectResult
voodoo_play_get_broadcast( VoodooPlayAddress **ret_addr,
                           size_t             *ret_num )
{
     SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
     if (sd == SOCKET_ERROR) {
          D_ERROR( "Voodoo/Win32: WSASocket() failed -> %u!\n", WSAGetLastError() );
          return DR_FAILURE;
     }

     INTERFACE_INFO InterfaceList[20];
     unsigned long nBytesReturned;
     if (WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
                  sizeof(InterfaceList), &nBytesReturned, 0, 0) == SOCKET_ERROR)
     {
          D_ERROR( "Voodoo/Win32: WSAIoctl() failed -> %u!\n", WSAGetLastError() );
          return DR_FAILURE;
     }

     int                nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);
     VoodooPlayAddress *addr;

     addr = (VoodooPlayAddress*) D_CALLOC( nNumInterfaces, sizeof(VoodooPlayAddress) );
     if (!addr)
          return D_OOM();

     for (int i = 0; i<nNumInterfaces; ++i) {
          struct sockaddr_in *pAddress = (struct sockaddr_in *) & (InterfaceList[i].iiBroadcastAddress);

		  printf("addr 0x%08x\n",pAddress->sin_addr.s_addr);
          voodoo_play_from_inet_addr( &addr[i], pAddress->sin_addr.s_addr );
     }

     *ret_addr = addr;
     *ret_num  = nNumInterfaces;

     return DR_OK;
}

#endif







#if 1

DirectResult
voodoo_play_get_broadcast( VoodooPlayAddress **ret_addr,
                           size_t             *ret_num )
{
     VoodooPlayAddress *addr;

     // Get local host name
     char szHostName[128] = "";

     if (::gethostname(szHostName, sizeof(szHostName))) {
          D_ERROR( "Voodoo/Win32: gethostname() failed!\n" );
          // Error handling -> call 'WSAGetLastError()'
          return DR_FAILURE;
     }

     // Get local IP addresses
     struct hostent *pHost = 0;

     pHost = ::gethostbyname(szHostName);
     if (!pHost) {
          D_ERROR( "Voodoo/Win32: gethostbyname('%s') failed!\n", szHostName );
          // Error handling -> call 'WSAGetLastError()'
          return DR_FAILURE;
     }


     size_t iCnt, iTotal = 0;

     for (iCnt = 0; pHost->h_addr_list[iCnt]; ++iCnt) {
          iTotal++;
     }


     addr = (VoodooPlayAddress*) D_CALLOC( iTotal, sizeof(VoodooPlayAddress) );
     if (!addr)
          return D_OOM();

     for (iCnt = 0; pHost->h_addr_list[iCnt]; ++iCnt) {
          struct sockaddr_in SocketAddress;

          memcpy(&SocketAddress.sin_addr, pHost->h_addr_list[iCnt], pHost->h_length);

          voodoo_play_from_inet_addr( &addr[iCnt], SocketAddress.sin_addr.s_addr | htonl(0xff) );  // FIXME
     }

     *ret_addr = addr;
     *ret_num  = iTotal;

     return DR_OK;
}








#endif

















#if 0

#define _WIN32_DCOM
#include <iostream>
using namespace std;
#include <comdef.h>
#include <Wbemidl.h>

# pragma comment(lib, "wbemuuid.lib")


extern "C" {
     int get_interfaces();
}

int
get_interfaces()
{
     HRESULT hres;

     // Step 1: --------------------------------------------------
     // Initialize COM. ------------------------------------------

     hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
     if (FAILED(hres)) {
          cout << "Failed to initialize COM library. Error code = 0x" 
          << hex << hres << endl;
          return 1;                  // Program has failed.
     }

     // Step 2: --------------------------------------------------
     // Set general COM security levels --------------------------
     // Note: If you are using Windows 2000, you need to specify -
     // the default authentication credentials for a user by using
     // a SOLE_AUTHENTICATION_LIST structure in the pAuthList ----
     // parameter of CoInitializeSecurity ------------------------

     hres =  CoInitializeSecurity(
                                 NULL, 
                                 -1,                          // COM authentication
                                 NULL,                        // Authentication services
                                 NULL,                        // Reserved
                                 RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
                                 RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
                                 NULL,                        // Authentication info
                                 EOAC_NONE,                   // Additional capabilities 
                                 NULL                         // Reserved
                                 );


     if (FAILED(hres)) {
          cout << "Failed to initialize security. Error code = 0x" 
          << hex << hres << endl;
          CoUninitialize();
          return 1;                    // Program has failed.
     }

     // Step 3: ---------------------------------------------------
     // Obtain the initial locator to WMI -------------------------

     IWbemLocator *pLoc = NULL;

     hres = CoCreateInstance(
                            CLSID_WbemLocator,             
                            0, 
                            CLSCTX_INPROC_SERVER, 
                            IID_IWbemLocator, (LPVOID *) &pLoc);

     if (FAILED(hres)) {
          cout << "Failed to create IWbemLocator object."
          << " Err code = 0x"
          << hex << hres << endl;
          CoUninitialize();
          return 1;                 // Program has failed.
     }

     // Step 4: -----------------------------------------------------
     // Connect to WMI through the IWbemLocator::ConnectServer method

     IWbemServices *pSvc = NULL;

     // Connect to the root\cimv2 namespace with
     // the current user and obtain pointer pSvc
     // to make IWbemServices calls.
     hres = pLoc->ConnectServer(
                               _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
                               NULL,                    // User name. NULL = current user
                               NULL,                    // User password. NULL = current
                               0,                       // Locale. NULL indicates current
                               NULL,                    // Security flags.
                               0,                       // Authority (e.g. Kerberos)
                               0,                       // Context object 
                               &pSvc                    // pointer to IWbemServices proxy
                               );

     if (FAILED(hres)) {
          cout << "Could not connect. Error code = 0x" 
          << hex << hres << endl;
          pLoc->Release();     
          CoUninitialize();
          return 1;                // Program has failed.
     }

     //cout << "Connected to ROOT\\CIMV2 WMI namespace" << endl;


     // Step 5: --------------------------------------------------
     // Set security levels on the proxy -------------------------

     hres = CoSetProxyBlanket(
                             pSvc,                        // Indicates the proxy to set
                             RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
                             RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
                             NULL,                        // Server principal name 
                             RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
                             RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
                             NULL,                        // client identity
                             EOAC_NONE                    // proxy capabilities 
                             );

     if (FAILED(hres)) {
          cout << "Could not set proxy blanket. Error code = 0x" 
          << hex << hres << endl;
          pSvc->Release();
          pLoc->Release();     
          CoUninitialize();
          return 1;               // Program has failed.
     }

     // Step 6: --------------------------------------------------
     // Use the IWbemServices pointer to make requests of WMI ----

     // For example, get the name of the operating system
     IEnumWbemClassObject* pEnumerator = NULL;
     hres = pSvc->ExecQuery(
                           bstr_t("WQL"), 
                           bstr_t("SELECT * FROM Win32_NetworkAdapterConfiguration WHERE IPEnabled = 'TRUE'"),
                           WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
                           NULL,
                           &pEnumerator);

     if (FAILED(hres)) {
          cout << "Query for operating system name failed."
          << " Error code = 0x" 
          << hex << hres << endl;
          pSvc->Release();
          pLoc->Release();
          CoUninitialize();
          return 1;               // Program has failed.
     }

     // Step 7: -------------------------------------------------
     // Get the data from the query in step 6 -------------------

     IWbemClassObject *pclsObj;
     ULONG uReturn = 0;

     while (pEnumerator) {
          HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, 
                                         &pclsObj, &uReturn);

          if (0 == uReturn) {
               break;
          }

          VARIANT vtProp;

          // Get the value of the Name property
          hr = pclsObj->Get(L"IPAddress", 0, &vtProp, 0, 0);
          wcout << " IP Address : " << vtProp.uintVal << endl;
          VariantClear(&vtProp);

          pclsObj->Release();
     }

     // Cleanup
     // ========

     pSvc->Release();
     pLoc->Release();
     pEnumerator->Release();
     pclsObj->Release();
     CoUninitialize();

     return 0;   // Program successfully completed.

}


#endif

