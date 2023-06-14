/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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

#ifndef ___Direct__Utils__H___
#define ___Direct__Utils__H___

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/os/mutex.h>
#include <direct/os/waitqueue.h>

#include <direct/debug.h>
#include <direct/modules.h>


const char *D_Demangle( const char *symbol );


#ifdef __cplusplus
}

#include <direct/Types++.h>

#include <direct/String.hxx>
#include <direct/ToString.h>

#include <map>
#include <memory>
#include <string>

#include <typeinfo>

#include <cxxabi.h>


namespace Direct {


template <typename _Map>
class MapLookup
{
public:
     operator bool() const {
          return success;
     }

protected:
     bool success;

public:
     MapLookup( const _Map                 &map,
                typename _Map::key_type    &key,
                typename _Map::mapped_type &val )
     {
          typename _Map::const_iterator it = map.find( key );

          if (it != map.end()) {
               val = (*it).second;

               success = true;
          }
          else
               success = false;
     }
};


template <typename _T>
class TypeID : public ToString<std::type_info>
{
public:
     TypeID()
          :
          ToString( typeid(_T) )
     {
     }
};


class Demangle : public Direct::String
{
public:
     Demangle( const char *symbol )
     {
#if defined(__x86_64__) || defined(__arm__)
          int   status;
          char *realname;

          realname = abi::__cxa_demangle(symbol, 0, 0, &status);

          switch (status) {
               case -2:
                    PrintF( "%s", symbol );
                    break;

               case 0:
                    PrintF( "%s", realname );
                    free( realname );
                    break;

               default:
                    PrintF( "DEMANGLE-ERROR-(%d) <- [%s]", status, symbol );
                    break;
          }
#else
          PrintF( "%s", symbol );
#endif
     }
};


template<class singleType>
class Singleton
{
public:
    static std::shared_ptr<singleType> GetInstance()
    {
         static std::weak_ptr<singleType> singleObject;

         std::shared_ptr<singleType> shareObject = singleObject.lock();

         if (!shareObject) {
              shareObject.reset( new singleType );
              singleObject = shareObject;
         }

         return shareObject;
    }
};



class Modules;

class Module
{
public:
     Module( Modules &modules );
     virtual ~Module();

     virtual const Direct::String &GetName() const = 0;

     virtual DirectResult Initialise() { return DR_OK; };
     virtual DirectResult Finalise() { return DR_OK; };

protected:
     void Register();

private:
     friend class Modules;

     Modules        &modules;
     Direct::String  registered;
};


typedef std::map<DirectModuleEntry*,Module*> ModuleMap;

class Modules
{
public:
     Modules( const Direct::String &directory )
          :
          directory( directory )
     {
          memset( &modules, 0, sizeof(modules) );

          modules.path = directory.buffer();
     }

     DirectResult Load();

private:
     friend class Module;

     DirectModuleDir     modules;
     Direct::String      directory;
     ModuleMap           map;

     void Register( Module *module );
     void Unregister( Module *module );
};



}


#endif // __cplusplus

#endif

