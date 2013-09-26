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



#ifndef ___Direct__LockWQ__H___
#define ___Direct__LockWQ__H___

#include <direct/Types++.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/os/mutex.h>
#include <direct/os/waitqueue.h>


#ifdef __cplusplus
}



namespace Direct {


class LockWQ
{
public:
     LockWQ()
     {
          direct_mutex_init( &mutex );
          direct_waitqueue_init( &wq );
     }

     ~LockWQ()
     {
          direct_mutex_deinit( &mutex );
          direct_waitqueue_deinit( &wq );
     }

     void lock()
     {
          direct_mutex_lock( &mutex );
     }

     void unlock()
     {
          direct_mutex_unlock( &mutex );
     }

     DirectResult wait( unsigned long timeout_us = 0 )
     {
          if (timeout_us > 0)
               return direct_waitqueue_wait_timeout( &wq, &mutex, timeout_us );

          return direct_waitqueue_wait( &wq, &mutex );
     }

     void notify()
     {
          direct_waitqueue_signal( &wq );
     }

     void notifyAll()
     {
          direct_waitqueue_broadcast( &wq );
     }

     class Lock {
     public:
          Lock( LockWQ &lwq )
               :
               lwq( lwq ),
               unlocked( false )
          {
               lwq.lock();
          }

          ~Lock()
          {
               if (!unlocked)
                    lwq.unlock();
          }

          DirectResult wait( unsigned long timeout_us = 0 )
          {
               return lwq.wait( timeout_us );
          }

          void unlock()
          {
               unlocked = true;

               lwq.unlock();
          }

     private:
          LockWQ &lwq;
          bool    unlocked;
     };

private:
     DirectMutex     mutex;
     DirectWaitQueue wq;
};


}


#endif // __cplusplus

#endif

