interface {
	name        ISurface
	version     1.0
	object      CoreSurface


	method {
		name	SetConfig
		
		arg {
			name	    config
			direction   input
			type        struct
			typename    CoreSurfaceConfig
		}
	}


	method {
		name	LockBuffer

		arg {
			name	    role
			direction   input
			type        enum
			typename    CoreSurfaceBufferRole
		}

		arg {
			name	    accessor
			direction   input
			type        enum
			typename    CoreSurfaceAccessorID
		}

		arg {
			name	    access
			direction   input
			type        enum
			typename    CoreSurfaceAccessFlags
		}

		arg {
			name	    lock
			direction   output
			type        struct
			typename    CoreSurfaceBufferLock
		}
	}


	method {
		name	UnlockBuffer

		arg {
			name	    lock
			direction   inout
			type        struct
			typename    CoreSurfaceBufferLock
		}
	}


	method {
		name	Flip

		arg {
			name	    swap
			direction   input
			type        enum
			typename    bool
		}
	}


	method {
		name	SetPalette

		arg {
			name	    palette
			direction   input
			type        object
			typename    CorePalette
		}
	}
}

