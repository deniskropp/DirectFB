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

#include <config.h>

#define __CL_ENABLE_EXCEPTIONS

#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <algorithm>

const char * helloStr  = "__kernel void "
                         "hello(void) "
                         "{ "
                         "  "
                         "} ";

int
main(void)
{
     cl_int err = CL_SUCCESS;

     try {
          std::vector<cl::Platform> platforms;

          cl::Platform::get(&platforms);

          if (platforms.size() == 0) {
               std::cout << "Platform size 0\n";
               return -1;
          }

          std::for_each( platforms.begin(), platforms.end(), [](cl::Platform &p){ std::cout << "Platform: " << p.getInfo<CL_PLATFORM_NAME>() << std::endl; } );


          std::vector<cl::Device> devices;

          platforms[0].getDevices( CL_DEVICE_TYPE_CPU, &devices );

          std::for_each( devices.begin(), devices.end(), [](cl::Device &d){ std::cout << "Device: " << d.getInfo<CL_DEVICE_NAME>() << std::endl; } );


//          cl_context_properties properties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)(platforms[0])(), 0};
//          cl::Context context(CL_DEVICE_TYPE_ALL, properties);

          cl::Context context( devices );



          cl::Program::Sources source(1,
                                      std::make_pair(helloStr,strlen(helloStr)));

          cl::Program program_ = cl::Program(context, source);

          program_.build(devices);

          cl::Kernel       kernel(program_, "hello", &err);
          cl::Event        event;
          cl::CommandQueue queue(context, devices[0], 0, &err);

          queue.enqueueNDRangeKernel(
                                    kernel, 
                                    cl::NullRange, 
                                    cl::NDRange(4,4),
                                    cl::NullRange,
                                    NULL,
                                    &event); 

          event.wait();
     }
     catch (cl::Error err) {
          std::cerr 
          << "ERROR: "
          << err.what()
          << "("
          << err.err()
          << ")"
          << std::endl;
     }

     return EXIT_SUCCESS;
}

