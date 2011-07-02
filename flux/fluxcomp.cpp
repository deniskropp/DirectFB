/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <list>
#include <map>
#include <string>
#include <vector>

extern "C" {
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
}


D_DEBUG_DOMAIN( fluxcomp, "fluxcomp", "Flux Compression Tool" );

/**********************************************************************************************************************/

static const char *license =
"/*\n"
"   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)\n"
"   (c) Copyright 2000-2004  Convergence (integrated media) GmbH\n"
"\n"
"   All rights reserved.\n"
"\n"
"   Written by Denis Oliver Kropp <dok@directfb.org>,\n"
"              Andreas Hundt <andi@fischlustig.de>,\n"
"              Sven Neumann <neo@directfb.org>,\n"
"              Ville Syrjälä <syrjala@sci.fi> and\n"
"              Claudio Ciccani <klan@users.sf.net>.\n"
"\n"
"   This library is free software; you can redistribute it and/or\n"
"   modify it under the terms of the GNU Lesser General Public\n"
"   License as published by the Free Software Foundation; either\n"
"   version 2 of the License, or (at your option) any later version.\n"
"\n"
"   This library is distributed in the hope that it will be useful,\n"
"   but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
"   Lesser General Public License for more details.\n"
"\n"
"   You should have received a copy of the GNU Lesser General Public\n"
"   License along with this library; if not, write to the\n"
"   Free Software Foundation, Inc., 59 Temple Place - Suite 330,\n"
"   Boston, MA 02111-1307, USA.\n"
"*/\n";

static const char *filename;

/**********************************************************************************************************************/

static void
print_usage (const char *prg_name)
{
     fprintf (stderr, "\nFlux Compression Tool (version %s)\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -h, --help                     Show this help message\n");
     fprintf (stderr, "   -v, --version                  Print version information\n");
     fprintf (stderr, "\n");
}

static bool
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return false;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "fluxcomp version %s\n", DIRECTFB_VERSION);
               return false;
          }

          if (filename || access( arg, R_OK )) {
               print_usage (argv[0]);
               return false;
          }

          filename = arg;
     }

     if (!filename) {
          print_usage (argv[0]);
          return false;
     }

     return true;
}

/**********************************************************************************************************************/

class Entity
{
public:
     Entity()
     {
     }

     typedef std::list<Entity*>   list;
     typedef std::vector<Entity*> vector;

     typedef enum {
          ENTITY_NONE,

          ENTITY_INTERFACE,
          ENTITY_METHOD,
          ENTITY_ARG
     } Type;


     virtual Type GetType() const = 0;


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );

private:
     const char    *buf;
     size_t         length;

public:
     Entity::vector entities;

     void Open( const char *buf ) {
          this->buf = buf;
     }

     void Close( size_t length ) {
          this->length = length;

          GetEntities( buf, length, entities );
     }

     static void GetEntities( const char     *buf,
                              size_t          length,
                              Entity::vector &out_vector );

     static DirectResult GetEntities( const char     *filename,
                                      Entity::vector &out_vector );
};

class Interface : public Entity
{
public:
     Interface()
          :
          Entity()
     {
     }


     virtual Type GetType() const { return ENTITY_INTERFACE; }


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );


     std::string              name;
     std::string              version;
     std::string              object;
};

class Method : public Entity
{
public:
     Method()
          :
          Entity(),
          async( false )
     {
     }


     virtual Type GetType() const { return ENTITY_METHOD; }


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );


     std::string              name;
     bool                     async;


public:
     static std::string
     PrintParam( std::string string_buffer,
                 std::string type,
                 std::string ptr,
                 std::string name,
                 bool        first )
     {
          char buf[1000];

          snprintf( buf, sizeof(buf), "%s                    %-40s %2s%s", first ? "" : ",\n", type.c_str(), ptr.c_str(), name.c_str() );

          return string_buffer + buf;
     }

     static std::string
     PrintMember( std::string string_buffer,
                          std::string type,
                          std::string ptr,
                          std::string name )
     {
          char buf[1000];

          snprintf( buf, sizeof(buf), "    %-40s %2s%s;\n", type.c_str(), ptr.c_str(), name.c_str() );

          return string_buffer + buf;
     }

     std::string ArgumentsAsParamDecl() const;
     std::string ArgumentsAsMemberDecl() const;
     std::string ArgumentsOutputAsMemberDecl() const;
     std::string ArgumentsAsMemberParams() const;

     std::string ArgumentsInputAssignments() const;
     std::string ArgumentsOutputAssignments() const;

     std::string ArgumentsAssertions() const;

     std::string ArgumentsOutputObjectDecl() const;
     std::string ArgumentsInputObjectDecl() const;

     std::string ArgumentsOutputObjectCatch() const;
     std::string ArgumentsOutputObjectThrow() const;
     std::string ArgumentsInoutReturn() const;

     std::string ArgumentsInputObjectLookup() const;
     std::string ArgumentsInputObjectUnref() const;

     std::string ArgumentsNames() const;
     std::string ArgumentsSize( const Interface *face ) const;
};

class Arg : public Entity
{
public:
     Arg()
          :
          Entity(),
          optional( false ),
          array( false )
     {
     }


     virtual Type GetType() const { return ENTITY_ARG; }


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );


     std::string              name;
     std::string              direction;
     std::string              type;
     std::string              type_name;
     bool                     optional;

     bool                     array;
     std::string              count;


public:
     std::string param_name() const
     {
          if (direction == "output")
               return std::string("ret_") + name;

          return name;
     }

     std::string size( bool use_args ) const
     {
          if (array) {
               if (use_args)
                    return std::string("sizeof(") + type_name + ") * args->" + count;
               else
                    return std::string("sizeof(") + type_name + ") * " + count;
          }

          return std::string("sizeof(") + type_name + ")";
     }

     std::string offset( const Method *method, bool use_args ) const
     {
          D_ASSERT( array == true );

          std::string result;

          for (Entity::vector::const_iterator iter = method->entities.begin(); iter != method->entities.end(); iter++) {
               const Arg *arg = dynamic_cast<const Arg*>( *iter );

               if (!arg->array)
                    continue;

               D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

               if (arg == this)
                    break;

               if (arg->direction == "input" || arg->direction == "inout")
                    result += std::string(" + ") + arg->size( use_args );
          }

          return result;
     }
};

/**********************************************************************************************************************/

void
Entity::Dump() const
{
     direct_log_printf( NULL, "\n" );
     direct_log_printf( NULL, "Entity (TYPE %d)\n", GetType() );
     direct_log_printf( NULL, "  Buffer at        %p [%zu]\n", buf, length );
}

void
Interface::Dump() const
{
     Entity::Dump();

     direct_log_printf( NULL, "  Name             %s\n", name.c_str() );
     direct_log_printf( NULL, "  Version          %s\n", version.c_str() );
     direct_log_printf( NULL, "  Object           %s\n", object.c_str() );
}

void
Method::Dump() const
{
     Entity::Dump();

     direct_log_printf( NULL, "  Name             %s\n", name.c_str() );
}

void
Arg::Dump() const
{
     Entity::Dump();

     direct_log_printf( NULL, "  Name             %s\n", name.c_str() );
     direct_log_printf( NULL, "  Direction        %s\n", direction.c_str() );
     direct_log_printf( NULL, "  Type             %s\n", type.c_str() );
     direct_log_printf( NULL, "  Typename         %s\n", type_name.c_str() );
}

/**********************************************************************************************************************/

void
Entity::SetProperty( const std::string &name,
                     const std::string &value )
{
}

void
Interface::SetProperty( const std::string &name,
                        const std::string &value )
{
     if (name == "name") {
          this->name = value;
          return;
     }

     if (name == "version") {
          version = value;
          return;
     }

     if (name == "object") {
          object = value;
          return;
     }
}

void
Method::SetProperty( const std::string &name,
                     const std::string &value )
{
     if (name == "name") {
          this->name = value;
          return;
     }

     if (name == "async") {
          async = value == "yes";
          return;
     }
}

void
Arg::SetProperty( const std::string &name,
                  const std::string &value )
{
     if (name == "name") {
          this->name = value;
          return;
     }

     if (name == "direction") {
          direction = value;
          return;
     }

     if (name == "type") {
          type = value;
          return;
     }

     if (name == "typename") {
          type_name = value;
          return;
     }

     if (name == "optional") {
          optional = value == "yes";
          return;
     }

     if (name == "count") {
          array = true;
          count = value;
          return;
     }
}

/**********************************************************************************************************************/

void
Entity::GetEntities( const char     *buf,
                     size_t          length,
                     Entity::vector &out_vector )
{
     size_t       i;
     unsigned int level   = 0;
     bool         quote   = false;
     bool         comment = false;

     std::string                        name;
     std::map<unsigned int,std::string> names;

     Entity *entity = NULL;

     D_DEBUG_AT( fluxcomp, "%s( buf %p, length %zu )\n", __func__, buf, length );

     for (i=0; i<length; i++) {
          D_DEBUG_AT( fluxcomp, "%*s[%u]  -> '%c' <-\n", level*2, "", level, buf[i] );

          if (comment) {
               switch (buf[i]) {
                    case '\n':
                         comment = false;
                         break;

                    default:
                         break;
               }
          }
          else if (quote) {
               switch (buf[i]) {
                    // TODO: implement escaped quotes in strings
                    case '"':
                         quote = false;
                         break;

                    default:
                         name += buf[i];
               }
          }
          else {
               switch (buf[i]) {
                    case '"':
                         quote = true;
                         break;

                    case '#':
                         comment = true;
                         break;

                    case '.':
                    case '-':
                    case '_':
                    case 'a' ... 'z':
                    case 'A' ... 'Z':
                    case '0' ... '9':
                         name += buf[i];
                         break;

                    default:
                         if (!name.empty()) {
                              D_DEBUG_AT( fluxcomp, "%*s=-> name = '%s'\n", level*2, "", name.c_str() );

                              if (!names[level].empty()) {
                                   switch (level) {
                                        case 1:
                                             D_DEBUG_AT( fluxcomp, "%*s#### setting property '%s' = '%s'\n",
                                                         level*2, "", names[level].c_str(), name.c_str() );

                                             D_ASSERT( entity != NULL );

                                             entity->SetProperty( names[level], name );
                                             break;

                                        default:
                                             break;
                                   }

                                   name = "";
                              }

                              names[level] = name;
                              name         = "";
                         }

                         switch (buf[i]) {
                              case '{':
                              case '}':
                                   switch (buf[i]) {
                                        case '{':
                                             switch (level) {
                                                  case 0:
                                                       if (names[level] == "interface") {
                                                            D_ASSERT( entity == NULL );

                                                            entity = new Interface();

                                                            entity->Open( &buf[i + 1] );

                                                            D_DEBUG_AT( fluxcomp, "%*s#### open entity %p (Interface)\n", level*2, "", entity );
                                                       }
                                                       if (names[level] == "method") {
                                                            D_ASSERT( entity == NULL );

                                                            entity = new Method();

                                                            entity->Open( &buf[i + 1] );

                                                            D_DEBUG_AT( fluxcomp, "%*s#### open entity %p (Method)\n", level*2, "", entity );
                                                       }
                                                       if (names[level] == "arg") {
                                                            D_ASSERT( entity == NULL );

                                                            entity = new Arg();

                                                            entity->Open( &buf[i + 1] );

                                                            D_DEBUG_AT( fluxcomp, "%*s#### open entity %p (Arg)\n", level*2, "", entity );
                                                       }
                                                       break;

                                                  default:
                                                       break;
                                             }

                                             names[level] = "";

                                             level++;
                                             break;

                                        case '}':
                                             D_ASSERT( names[level].empty() );

                                             level--;

                                             switch (level) {
                                                  case 0:
                                                       D_DEBUG_AT( fluxcomp, "%*s#### close entity %p\n", level*2, "", entity );

                                                       D_ASSERT( entity != NULL );

                                                       entity->Close( &buf[i-1] - entity->buf );

                                                       out_vector.push_back( entity );

                                                       entity = NULL;
                                                       break;

                                                  case 1:
                                                       break;

                                                  default:
                                                       break;
                                             }
                                             break;
                                   }

                                   D_DEBUG_AT( fluxcomp, "%*s=-> level => %u\n", level*2, "", level );
                                   break;

                              case ' ':
                              case '\t':
                              case '\n':
                              case '\r':
                                   break;

                              default:
                                   break;
                         }
                         break;
               }
          }
     }
}

DirectResult
Entity::GetEntities( const char     *filename,
                     Entity::vector &out_vector )
{
     int          ret = DR_OK;
     int          fd;
     struct stat  stat;
     void        *ptr = MAP_FAILED;

     /* Open the file. */
     fd = open( filename, O_RDONLY );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "GetEntities: Failure during open() of '%s'!\n", filename );
          return (DirectResult) ret;
     }

     /* Query file size etc. */
     if (fstat( fd, &stat ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "GetEntities: Failure during fstat() of '%s'!\n", filename );
          goto out;
     }

     /* Memory map the file. */
     ptr = mmap( NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0 );
     if (ptr == MAP_FAILED) {
          ret = errno2result( errno );
          D_PERROR( "GetEntities: Failure during mmap() of '%s'!\n", filename );
          goto out;
     }


     Entity::GetEntities( (const char*) ptr, stat.st_size, out_vector );


out:
     if (ptr != MAP_FAILED)
          munmap( ptr, stat.st_size );

     close( fd );

     return (DirectResult) ret;
}

/**********************************************************************************************************************/

std::string
Method::ArgumentsAsParamDecl() const
{
     std::string result;
     bool        first = true;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "struct") {
               if (arg->direction == "input")
                    result = PrintParam( result, std::string("const ") + arg->type_name, "*", arg->param_name(), first );
               else
                    result = PrintParam( result, arg->type_name, "*", arg->param_name(), first );
          }
          else if (arg->type == "enum" || arg->type == "int") {
               if (arg->direction == "input") {
                    if (arg->array)
                         result = PrintParam( result, std::string("const ") + arg->type_name, "*", arg->param_name(), first );
                    else
                         result = PrintParam( result, arg->type_name, "", arg->param_name(), first );
               }
               else
                    result = PrintParam( result, arg->type_name, "*", arg->param_name(), first );
          }
          else if (arg->type == "object") {
               if (arg->direction == "input")
                    result = PrintParam( result, arg->type_name, "*", arg->param_name(), first );
               else
                    result = PrintParam( result, arg->type_name, "**", arg->param_name(), first );
          }

          if (first)
               first = false;
     }

     return result;
}

std::string
Method::ArgumentsAsMemberDecl() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->array)
               continue;

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result = PrintMember( result, "bool", "", arg->name + "_set" );

               if (arg->type == "struct")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "enum")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "int")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "object")
                    result = PrintMember( result, "u32", "", arg->name + "_id" );
          }
     }

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          char       buf[300];
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (!arg->array)
               continue;

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result = PrintMember( result, "bool", "", arg->name + "_set" );

               snprintf( buf, sizeof(buf), "    /* '%s' %s follow (%s) */\n", arg->count.c_str(), arg->type_name.c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsOutputAsMemberDecl() const
{
     std::string result;

     result = PrintMember( result, "DFBResult", "", "result" );

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "output" || arg->direction == "inout") {
               if (arg->type == "struct")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "enum")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "int")
                    result = PrintMember( result, arg->type_name, "", arg->name );
               else if (arg->type == "object")
                    result = PrintMember( result, "u32", "", arg->name + "_id" );
          }
     }

     return result;
}

std::string
Method::ArgumentsAsMemberParams() const
{
     std::string result;
     bool        first = true;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (first)
               first = false;
          else
               result += ", ";

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result += std::string("args->") + arg->name + "_set ? ";

               if (arg->array) {
                    if (arg->type == "struct" || arg->type == "enum" || arg->type == "int")
                         result += std::string("(") + arg->type_name + "*) ((char*)(args + 1)" + arg->offset( this, true ) + ")";
                    else if (arg->type == "object")
                         D_UNIMPLEMENTED();
               }
               else {
                    if (arg->type == "struct")
                         result += std::string("&args->") + arg->name;
                    else if (arg->type == "enum")
                         result += std::string("args->") + arg->name;
                    else if (arg->type == "int")
                         result += std::string("args->") + arg->name;
                    else if (arg->type == "object")
                         result += arg->name;
               }

               if (arg->optional)
                    result += std::string(" : NULL");
          }

          if (arg->direction == "output") {
               if (arg->type == "struct")
                    result += std::string("&return_args->") + arg->name;
               else if (arg->type == "enum")
                    result += std::string("&return_args->") + arg->name;
               else if (arg->type == "int")
                    result += std::string("&return_args->") + arg->name;
               else if (arg->type == "object")
                    result += std::string("&") + arg->name;
          }
     }

     return result;
}

std::string
Method::ArgumentsInputAssignments() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (arg->array)
               continue;

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result += std::string("  if (") + arg->name + ") {\n";

               if (arg->type == "struct")
                    result += std::string("    block->") + arg->name + " = *" + arg->name + ";\n";
               else if (arg->type == "enum" || arg->type == "int")
                    result += std::string("    block->") + arg->name + " = " + arg->name + ";\n";
               else if (arg->type == "object")
                    result += std::string("    block->") + arg->name + "_id = " + arg->name + "->object.id;\n";

               if (arg->optional) {
                    result += std::string("    block->") + arg->name + "_set = true;\n";
                    result += std::string("  }\n");
                    result += std::string("  else\n");
                    result += std::string("    block->") + arg->name + "_set = false;\n";
               }
          }
     }

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (!arg->array)
               continue;

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout") {
               if (arg->optional)
                    result += std::string("  if (") + arg->name + ") {\n";

               if (arg->type == "struct" || arg->type == "enum" || arg->type == "int")
                    result += std::string("    direct_memcpy( (char*) (block + 1)") + arg->offset( this, false ) + ", " + arg->name + ", " + arg->size( false ) + " );\n";
               else if (arg->type == "object")
                    D_UNIMPLEMENTED();

               if (arg->optional) {
                    result += std::string("    block->") + arg->name + "_set = true;\n";
                    result += std::string("  }\n");
                    result += std::string("  else {\n");
                    result += std::string("    block->") + arg->name + "_set = false;\n";
                    result += std::string("    ") + arg->name + " = 0;\n"; // FIXME: this sets num to 0 to avoid dispatch errors, but what if num is before this?
                    result += std::string("  }\n");
               }
          }
     }

     return result;
}

std::string
Method::ArgumentsOutputAssignments() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "output" || arg->direction == "inout") {
               if (arg->type == "struct" || arg->type == "enum" || arg->type == "int")
                    result += std::string("    *") + arg->param_name() + " = return_block." + arg->name + ";\n";
          }
     }

     return result;
}

std::string
Method::ArgumentsAssertions() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if ((arg->type == "struct" || arg->type == "object") && !arg->optional)
               result += std::string("    D_ASSERT( ") + arg->param_name() + " != NULL );\n";
     }

     return result;
}

std::string
Method::ArgumentsOutputObjectDecl() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "output")
               result += std::string("    ") + arg->type_name + " *" + arg->name + " = NULL;\n";
     }

     return result;
}

std::string
Method::ArgumentsInputObjectDecl() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "input")
               result += std::string("    ") + arg->type_name + " *" + arg->name + " = NULL;\n";
     }

     return result;
}

std::string
Method::ArgumentsOutputObjectCatch() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "output") {
               char buf[1000];

               snprintf( buf, sizeof(buf),
                         "    ret = (DFBResult) %s_Catch( core, return_block.%s_id, &%s );\n"
                         "    if (ret) {\n"
                         "         D_DERROR( ret, \"%%s: Catching %s by ID %%u failed!\\n\", __FUNCTION__, return_block.%s_id );\n"
                         "         return ret;\n"
                         "    }\n"
                         "\n"
                         "    *%s = %s;\n"
                         "\n",
                         arg->type_name.c_str(), arg->name.c_str(), arg->name.c_str(),
                         arg->name.c_str(), arg->name.c_str(),
                         arg->param_name().c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsOutputObjectThrow() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "output") {
               char buf[1000];

               snprintf( buf, sizeof(buf),
                         "                %s_Throw( %s, caller, &return_args->%s_id );\n",
                         arg->type_name.c_str(), arg->name.c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsInoutReturn() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "inout") {
               char buf[1000];

               snprintf( buf, sizeof(buf),
                         "                return_args->%s = args->%s;\n",
                         arg->name.c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsInputObjectLookup() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "input") {
               char buf[1000];

               if (arg->optional) {
                    snprintf( buf, sizeof(buf),
                              "            if (args->%s_set) {\n"
                              "                ret = (DFBResult) %s_Lookup( core, args->%s_id, &%s );\n"
                              "                if (ret) {\n"
                              "                     D_DERROR( ret, \"%%s: Looking up %s by ID %%u failed!\\n\", __FUNCTION__, args->%s_id );\n"
                              "%s"
                              "                     return DFB_OK;\n"
                              "                }\n"
                              "            }\n"
                              "\n",
                              arg->name.c_str(),
                              arg->type_name.c_str(), arg->name.c_str(), arg->name.c_str(),
                              arg->name.c_str(), arg->name.c_str(),
                              async ? "" : "                     return_args->result = ret;\n" );
               }
               else {
                    snprintf( buf, sizeof(buf),
                              "            ret = (DFBResult) %s_Lookup( core, args->%s_id, &%s );\n"
                              "            if (ret) {\n"
                              "                 D_DERROR( ret, \"%%s: Looking up %s by ID %%u failed!\\n\", __FUNCTION__, args->%s_id );\n"
                              "%s"
                              "                 return DFB_OK;\n"
                              "            }\n"
                              "\n",
                              arg->type_name.c_str(), arg->name.c_str(), arg->name.c_str(),
                              arg->name.c_str(), arg->name.c_str(),
                              async ? "" : "                 return_args->result = ret;\n" );
               }

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsInputObjectUnref() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->type == "object" && arg->direction == "input") {
               char buf[1000];

               snprintf( buf, sizeof(buf),
                         "            if (%s)\n"
                         "                %s_Unref( %s );\n"
                         "\n",
                         arg->name.c_str(), arg->type_name.c_str(), arg->name.c_str() );

               result += buf;
          }
     }

     return result;
}

std::string
Method::ArgumentsNames() const
{
     std::string result;

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg  = dynamic_cast<const Arg*>( *iter );
          bool       last = arg == entities[entities.size()-1];

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          result += arg->param_name();

          if (!last)
               result += ", ";
     }

     return result;
}

std::string
Method::ArgumentsSize( const Interface *face ) const
{
     std::string result = "sizeof(";

     result += face->object + name + ")";

     for (Entity::vector::const_iterator iter = entities.begin(); iter != entities.end(); iter++) {
          const Arg *arg = dynamic_cast<const Arg*>( *iter );

          if (!arg->array)
               continue;

          D_DEBUG_AT( fluxcomp, "%s( %p )\n", __FUNCTION__, arg );

          if (arg->direction == "input" || arg->direction == "inout")
               result += " + " + arg->size( false );
     }

     return result;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

class FluxComp
{
public:
     void GenerateHeader( const Interface   *face );

     void GenerateSource( const Interface   *face );

     void PrintInterface( FILE              *file,
                          const Interface   *face,
                          const std::string &name,
                          const std::string &super,
                          bool               abstract );
};

/**********************************************************************************************************************/

void
FluxComp::GenerateHeader( const Interface *face )
{
     FILE        *file;
     std::string  filename = face->object + ".h";

     file = fopen( filename.c_str(), "w" );
     if (!file) {
          D_PERROR( "FluxComp: fopen( '%s' ) failed!\n", filename.c_str() );
          return;
     }

     fprintf( file, "%s\n"
                    "#ifndef ___%s__H___\n"
                    "#define ___%s__H___\n"
                    "\n"
                    "#include \"%s_includes.h\"\n"
                    "\n"
                    "/**********************************************************************************************************************\n"
                    " * %s\n"
                    " */\n"
                    "\n"
                    "#ifdef __cplusplus\n"
                    "#include <core/Interface.h>\n"
                    "\n"
                    "extern \"C\" {\n"
                    "#endif\n"
                    "\n"
                    "\n",
              license, face->object.c_str(), face->object.c_str(), face->object.c_str(), face->object.c_str() );

     /* C Wrappers */

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "DFBResult %s_%s(\n"
                         "                    %-40s  *obj%s\n"
                         "%s"
                         ");\n"
                         "\n",
                   face->object.c_str(), method->name.c_str(),
                   face->object.c_str(), method->entities.empty() ? "" : ",",
                   method->ArgumentsAsParamDecl().c_str() );
     }


     fprintf( file, "\n"
                    "void *%s_Init_Dispatch(\n"
                    "                    CoreDFB              *core,\n"
                    "                    %-20s *obj,\n"
                    "                    FusionCall           *call\n"
                    ");\n"
                    "\n",
              face->object.c_str(), face->object.c_str() );

     fprintf( file, "void  %s_Deinit_Dispatch(\n"
                    "                    void                 *dispatch\n"
                    ");\n"
                    "\n",
              face->object.c_str() );


     fprintf( file, "\n"
                    "#ifdef __cplusplus\n"
                    "}\n"
                    "\n"
                    "\n"
                    "namespace DirectFB {\n"
                    "\n"
                    "\n"
                    "/*\n"
                    " * %s Calls\n"
                    " */\n"
                    "typedef enum {\n",
              face->object.c_str() );

     /* Method IDs */

     int index = 1;

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "    %s_%s = %d,\n",
                   face->object.c_str(), method->name.c_str(), index++ );
     }

     fprintf( file, "} %sCall;\n"
                    "\n",
              face->object.c_str() );


     /* Method Argument Structures */

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "/*\n"
                         " * %s_%s\n"
                         " */\n"
                         "typedef struct {\n"
                         "%s"
                         "} %s%s;\n"
                         "\n"
                         "typedef struct {\n"
                         "%s"
                         "} %s%sReturn;\n"
                         "\n"
                         "\n",
                   face->object.c_str(), method->name.c_str(),
                   method->ArgumentsAsMemberDecl().c_str(),
                   face->object.c_str(), method->name.c_str(),
                   method->ArgumentsOutputAsMemberDecl().c_str(),
                   face->object.c_str(), method->name.c_str() );
     }


     /* Abstract Interface */

     PrintInterface( file, face, face->name, "Interface", true );


     /* Real Interface */

     fprintf( file, "\n"
                    "\n"
                    "\n"
                    "class %s_Real : public %s\n"
                    "{\n"
                    "private:\n"
                    "    %s *obj;\n"
                    "\n"
                    "public:\n"
                    "    %s_Real( CoreDFB *core, %s *obj )\n"
                    "        :\n"
                    "        %s( core ),\n"
                    "        obj( obj )\n"
                    "    {\n"
                    "    }\n"
                    "\n"
                    "public:\n",
              face->name.c_str(), face->name.c_str(), face->object.c_str(),
              face->name.c_str(), face->object.c_str(), face->name.c_str() );

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "    virtual DFBResult %s(\n"
                         "%s\n"
                         "    );\n"
                         "\n",
                   method->name.c_str(),
                   method->ArgumentsAsParamDecl().c_str() );
     }

     fprintf( file, "};\n" );


     /* Requestor Interface */

     fprintf( file, "\n"
                    "\n"
                    "\n"
                    "class %s_Requestor : public %s\n"
                    "{\n"
                    "private:\n"
                    "    %s *obj;\n"
                    "\n"
                    "public:\n"
                    "    %s_Requestor( CoreDFB *core, %s *obj )\n"
                    "        :\n"
                    "        %s( core ),\n"
                    "        obj( obj )\n"
                    "    {\n"
                    "    }\n"
                    "\n"
                    "public:\n",
              face->name.c_str(), face->name.c_str(), face->object.c_str(),
              face->name.c_str(), face->object.c_str(), face->name.c_str() );

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "    virtual DFBResult %s(\n"
                         "%s\n"
                         "    );\n"
                         "\n",
                   method->name.c_str(),
                   method->ArgumentsAsParamDecl().c_str() );
     }

     fprintf( file, "};\n" );


     /* Dispatch Object */

     fprintf( file, "\n"
                    "\n"
                    "\n"
                    "class %sDispatch\n"
                    "{\n"
                    "\n"
                    "public:\n"
                    "    %sDispatch( CoreDFB *core, %s *real )\n"
                    "        :\n"
                    "        core( core ),\n"
                    "        real( real )\n"
                    "    {\n"
                    "    }\n"
                    "\n"
                    "\n"
                    "    virtual DFBResult Dispatch( FusionID      caller,\n"
                    "                                int           method,\n"
                    "                                void         *ptr,\n"
                    "                                unsigned int  length,\n"
                    "                                void         *ret_ptr,\n"
                    "                                unsigned int  ret_size,\n"
                    "                                unsigned int *ret_length );\n"
                    "\n"
                    "private:\n"
                    "    CoreDFB              *core;\n"
                    "    %-20s *real;\n"
                    "};\n",
              face->object.c_str(), face->object.c_str(), face->name.c_str(), face->name.c_str() );


     fprintf( file, "\n"
                    "\n"
                    "}\n"
                    "\n"
                    "#endif\n"
                    "\n"
                    "#endif\n" );

     fclose( file );
}

void
FluxComp::GenerateSource( const Interface *face )
{
     FILE        *file;
     std::string  filename = face->object + ".cpp";

     file = fopen( filename.c_str(), "w" );
     if (!file) {
          D_PERROR( "FluxComp: fopen( '%s' ) failed!\n", filename.c_str() );
          return;
     }

     fprintf( file, "%s\n"
                    "#include <config.h>\n"
                    "\n"
                    "#include \"%s.h\"\n"
                    "\n"
                    "extern \"C\" {\n"
                    "#include <directfb_util.h>\n"
                    "\n"
                    "#include <direct/debug.h>\n"
                    "#include <direct/mem.h>\n"
                    "#include <direct/memcpy.h>\n"
                    "#include <direct/messages.h>\n"
                    "\n"
                    "#include <core/core.h>\n"
                    "}\n"
                    "\n"
                    "D_DEBUG_DOMAIN( DirectFB_%s, \"DirectFB/%s\", \"DirectFB %s\" );\n"
                    "\n"
                    "/*********************************************************************************************************************/\n"
                    "\n",
              license, face->object.c_str(), face->object.c_str(), face->object.c_str(), face->object.c_str() );

     /* C Wrappers */

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "DFBResult\n"
                         "%s_%s(\n"
                         "                    %-40s  *obj%s\n"
                         "%s\n"
                         ")\n"
                         "{\n"
                         "    if (dfb_core_is_master( core_dfb )) {\n"
                         "        DirectFB::%s_Real real( core_dfb, obj );\n"
                         "\n"
                         "        return real.%s( %s );\n"
                         "    }\n"
                         "\n"
                         "    DirectFB::%s_Requestor requestor( core_dfb, obj );\n"
                         "\n"
                         "    return requestor.%s( %s );\n"
                         "}\n"
                         "\n",
                   face->object.c_str(), method->name.c_str(),
                   face->object.c_str(), method->entities.empty() ? "" : ",",
                   method->ArgumentsAsParamDecl().c_str(),
                   face->name.c_str(),
                   method->name.c_str(), method->ArgumentsNames().c_str(),
                   face->name.c_str(),
                   method->name.c_str(), method->ArgumentsNames().c_str() );
     }


     fprintf( file, "/*********************************************************************************************************************/\n"
                    "\n"
                    "static FusionCallHandlerResult\n"
                    "%s_Dispatch( int           caller,   /* fusion id of the caller */\n"
                    "                     int           call_arg, /* optional call parameter */\n"
                    "                     void         *ptr, /* optional call parameter */\n"
                    "                     unsigned int  length,\n"
                    "                     void         *ctx,      /* optional handler context */\n"
                    "                     unsigned int  serial,\n"
                    "                     void         *ret_ptr,\n"
                    "                     unsigned int  ret_size,\n"
                    "                     unsigned int *ret_length )\n"
                    "{\n"
                    "    DirectFB::%sDispatch *dispatch = (DirectFB::%sDispatch*) ctx;\n"
                    "\n"
                    "    dispatch->Dispatch( caller, call_arg, ptr, length, ret_ptr, ret_size, ret_length );\n"
                    "\n"
                    "    return FCHR_RETURN;\n"
                    "}\n"
                    "\n",
              face->object.c_str(), face->object.c_str(), face->object.c_str() );

     fprintf( file, "void *%s_Init_Dispatch(\n"
                    "                    CoreDFB              *core,\n"
                    "                    %-20s *obj,\n"
                    "                    FusionCall           *call\n"
                    ")\n"
                    "{\n"
                    "    DirectFB::%sDispatch *dispatch = new DirectFB::%sDispatch( core, new DirectFB::%s_Real(core, obj) );\n"
                    "\n"
                    "    fusion_call_init3( call, %s_Dispatch, dispatch, core->world );\n"
                    "\n"
                    "    return dispatch;\n"
                    "}\n"
                    "\n",
              face->object.c_str(), face->object.c_str(), face->object.c_str(), face->object.c_str(), face->name.c_str(), face->object.c_str() );

     fprintf( file, "void  %s_Deinit_Dispatch(\n"
                    "                    void                 *_dispatch\n"
                    ")\n"
                    "{\n"
                    "    DirectFB::%sDispatch *dispatch = (DirectFB::%sDispatch*) _dispatch;\n"
                    "\n"
                    "    delete dispatch;\n"
                    "}\n"
                    "\n",
              face->object.c_str(), face->object.c_str(), face->object.c_str() );


     fprintf( file, "/*********************************************************************************************************************/\n"
                    "\n"
                    "namespace DirectFB {\n"
                    "\n"
                    "\n" );

     /* Requestor Methods */

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          if (method->async) {
               fprintf( file, "\n"
                              "DFBResult\n"
                              "%s_Requestor::%s(\n"
                              "%s\n"
                              ")\n"
                              "{\n"
                              "    DFBResult           ret;\n"
                              "%s"
                              "    %s%s       *block = (%s%s*) alloca( %s );\n"
                              "\n"
                              "    D_DEBUG_AT( DirectFB_%s, \"%s_Requestor::%%s()\\n\", __FUNCTION__ );\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "\n"
                              "    ret = (DFBResult) %s_Call( obj, FCEF_ONEWAY, %s_%s, block, %s, NULL, 0, NULL );\n"
                              "    if (ret) {\n"
                              "        D_DERROR( ret, \"%%s: %s_Call( %s_%s ) failed!\\n\", __FUNCTION__ );\n"
                              "        return ret;\n"
                              "    }\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "    return DFB_OK;\n"
                              "}\n"
                              "\n",
                        face->name.c_str(), method->name.c_str(),
                        method->ArgumentsAsParamDecl().c_str(),
                        method->ArgumentsOutputObjectDecl().c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(), method->ArgumentsSize( face ).c_str(),
                        face->object.c_str(), face->name.c_str(),
                        method->ArgumentsAssertions().c_str(),
                        method->ArgumentsInputAssignments().c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(), method->ArgumentsSize( face ).c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(),
                        method->ArgumentsOutputAssignments().c_str(),
                        method->ArgumentsOutputObjectCatch().c_str() );
          }
          else {
               fprintf( file, "\n"
                              "DFBResult\n"
                              "%s_Requestor::%s(\n"
                              "%s\n"
                              ")\n"
                              "{\n"
                              "    DFBResult           ret;\n"
                              "%s"
                              "    %s%s       *block = (%s%s*) alloca( %s );\n"
                              "    %s%sReturn  return_block;\n"
                              "\n"
                              "    D_DEBUG_AT( DirectFB_%s, \"%s_Requestor::%%s()\\n\", __FUNCTION__ );\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "\n"
                              "    ret = (DFBResult) %s_Call( obj, FCEF_NONE, %s_%s, block, %s, &return_block, sizeof(return_block), NULL );\n"
                              "    if (ret) {\n"
                              "        D_DERROR( ret, \"%%s: %s_Call( %s_%s ) failed!\\n\", __FUNCTION__ );\n"
                              "        return ret;\n"
                              "    }\n"
                              "\n"
                              "    if (return_block.result) {\n"
                              "         D_DERROR( return_block.result, \"%%s: %s_%s failed!\\n\", __FUNCTION__ );\n"
                              "         return return_block.result;\n"
                              "    }\n"
                              "\n"
                              "%s"
                              "\n"
                              "%s"
                              "    return DFB_OK;\n"
                              "}\n"
                              "\n",
                        face->name.c_str(), method->name.c_str(),
                        method->ArgumentsAsParamDecl().c_str(),
                        method->ArgumentsOutputObjectDecl().c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(), method->ArgumentsSize( face ).c_str(),
                        face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), face->name.c_str(),
                        method->ArgumentsAssertions().c_str(),
                        method->ArgumentsInputAssignments().c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(), method->ArgumentsSize( face ).c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), method->name.c_str(),
                        method->ArgumentsOutputAssignments().c_str(),
                        method->ArgumentsOutputObjectCatch().c_str() );
          }
     }

     /* Dispatch Object */

     fprintf( file, "/*********************************************************************************************************************/\n"
                    "\n"
                    "DFBResult\n"
                    "%sDispatch::Dispatch( FusionID      caller,\n"
                    "                                int           method,\n"
                    "                                void         *ptr,\n"
                    "                                unsigned int  length,\n"
                    "                                void         *ret_ptr,\n"
                    "                                unsigned int  ret_size,\n"
                    "                                unsigned int *ret_length )\n"
                    "{\n"
                    "    D_UNUSED\n"
                    "    DFBResult ret;\n"
                    "\n"
                    "    D_DEBUG_AT( DirectFB_%s, \"%sDispatch::%%s()\\n\", __FUNCTION__ );\n"
                    "\n"
                    "    switch (method) {\n",
              face->object.c_str(), face->object.c_str(), face->object.c_str() );

     /* Dispatch Methods */

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          if (method->async) {
               fprintf( file, "        case %s_%s: {\n"
                              "%s"
                              "%s"
                              "            D_UNUSED\n"
                              "            %s%s       *args        = (%s%s *) ptr;\n"
                              "\n"
                              "            D_DEBUG_AT( DirectFB_%s, \"=-> %s_%s\\n\" );\n"
                              "\n"
                              "%s"
                              "            real->%s( %s );\n"
                              "\n"
                              "%s"
                              "            return DFB_OK;\n"
                              "        }\n"
                              "\n",
                        face->object.c_str(), method->name.c_str(),
                        method->ArgumentsInputObjectDecl().c_str(),
                        method->ArgumentsOutputObjectDecl().c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(),
                        method->ArgumentsInputObjectLookup().c_str(),
                        method->name.c_str(), method->ArgumentsAsMemberParams().c_str(),
                        method->ArgumentsInputObjectUnref().c_str() );
          }
          else {
               fprintf( file, "        case %s_%s: {\n"
                              "%s"
                              "%s"
                              "            D_UNUSED\n"
                              "            %s%s       *args        = (%s%s *) ptr;\n"
                              "            %s%sReturn *return_args = (%s%sReturn *) ret_ptr;\n"
                              "\n"
                              "            D_DEBUG_AT( DirectFB_%s, \"=-> %s_%s\\n\" );\n"
                              "\n"
                              "%s"
                              "            return_args->result = real->%s( %s );\n"
                              "            if (return_args->result == DFB_OK) {\n"
                              "%s"
                              "%s"
                              "            }\n"
                              "\n"
                              "            *ret_length = sizeof(%s%sReturn);\n"
                              "\n"
                              "%s"
                              "            return DFB_OK;\n"
                              "        }\n"
                              "\n",
                        face->object.c_str(), method->name.c_str(),
                        method->ArgumentsInputObjectDecl().c_str(),
                        method->ArgumentsOutputObjectDecl().c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), method->name.c_str(), face->object.c_str(), method->name.c_str(),
                        face->object.c_str(), face->object.c_str(), method->name.c_str(),
                        method->ArgumentsInputObjectLookup().c_str(),
                        method->name.c_str(), method->ArgumentsAsMemberParams().c_str(),
                        method->ArgumentsOutputObjectThrow().c_str(),
                        method->ArgumentsInoutReturn().c_str(),
                        face->object.c_str(), method->name.c_str(),
                        method->ArgumentsInputObjectUnref().c_str() );
          }
     }

     fprintf( file, "    }\n"
                    "\n"
                    "    return DFB_NOSUCHMETHOD;\n"
                    "}\n" );


     fprintf( file, "\n"
                    "}\n" );

     fclose( file );
}

void
FluxComp::PrintInterface( FILE              *file,
                          const Interface   *face,
                          const std::string &name,
                          const std::string &super,
                          bool               abstract )
{
     fprintf( file, "\n"
                    "\n"
                    "\n"
                    "class %s : public %s\n"
                    "{\n"
                    "public:\n"
                    "    %s( CoreDFB *core )\n"
                    "        :\n"
                    "        %s( core )\n"
                    "    {\n"
                    "    }\n"
                    "\n"
                    "public:\n",
              name.c_str(), super.c_str(), name.c_str(), super.c_str() );

     for (Entity::vector::const_iterator iter = face->entities.begin(); iter != face->entities.end(); iter++) {
          const Method *method = dynamic_cast<const Method*>( *iter );

          fprintf( file, "    virtual DFBResult %s(\n"
                         "%s\n"
                         "    )%s;\n"
                         "\n",
                   method->name.c_str(),
                   method->ArgumentsAsParamDecl().c_str(),
                   abstract ? " = 0" : "" );
     }

     fprintf( file, "};\n" );
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DirectResult   ret;
     Entity::vector faces;

     direct_initialize();

//     direct_debug_config_domain( "fluxcomp", true );

     direct_config->debug    = true;
     direct_config->debugmem = true;

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -1;


     ret = Entity::GetEntities( filename, faces );
     if (ret == DR_OK) {
          FluxComp fc;

          for (Entity::vector::const_iterator iter = faces.begin(); iter != faces.end(); iter++) {
               const Interface *face = dynamic_cast<const Interface*>( *iter );

               fc.GenerateHeader( face );
               fc.GenerateSource( face );
          }
     }


     direct_print_memleaks();

     direct_shutdown();

     return ret;
}

