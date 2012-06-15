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
}

