interface {
	name        ISurfaceAllocation
	version     1.0
	object      CoreSurfaceAllocation


        method {
               name	Update

               arg {
                       name        region
                       direction   input
                       type        struct
                       typename    DFBRegion
                       optional    yes
               }
        }


        method {
               name	Updated

               arg {
                       name        updates
                       direction   input
                       type        struct
                       typename    DFBBox
                       optional    yes
                       count       num_updates
               }

               arg {
                       name        num_updates
                       direction   input
                       type        int
                       typename    u32
               }
        }
}

