interface {
	name        ISurfaceClient
	version     1.0
	object      CoreSurfaceClient


	method {
		name	FrameAck
		async	yes
		
		arg {
			name	    flip_count
			direction   input
			type        int
			typename    u32
		}
	}


        method {
                     name	SetFrameTimeConfig
                     async	yes

                     arg {
                             name	 config
                             direction   input
                             type        struct
                             typename    DFBFrameTimeConfig
                     }
             }
}

