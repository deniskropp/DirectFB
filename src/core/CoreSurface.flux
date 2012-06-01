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
		name	Flip

		arg {
			name	    swap
			direction   input
			type        enum
			typename    bool
		}
	}


	method {
		name	GetPalette

		arg {
			name	    palette
			direction   output
			type        object
			typename    CorePalette
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


	method {
		name	SetAlphaRamp

		arg {
			name	    a0
			direction   input
			type        int
			typename    u8
		}

		arg {
			name	    a1
			direction   input
			type        int
			typename    u8
		}

		arg {
			name	    a2
			direction   input
			type        int
			typename    u8
		}

		arg {
			name	    a3
			direction   input
			type        int
			typename    u8
		}
	}


	method {
		name	SetField

		arg {
			name	    field
			direction   input
			type        int
			typename    s32
		}
	}


	method {
		name	PreLockBuffer

		arg {
			name	    buffer
			direction   input
			type        object
			typename    CoreSurfaceBuffer
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
			name	    allocation
			direction   output
			type        object
			typename    CoreSurfaceAllocation
		}
	}


	method {
		name	PreLockBuffer2

		arg {
			name	    role
			direction   input
			type        enum
			typename    CoreSurfaceBufferRole
		}

		arg {
			name	    eye
			direction   input
			type        enum
			typename    DFBSurfaceStereoEye
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
			direction   input
			type        int
			typename    bool
		}

		arg {
			name	    allocation
			direction   output
			type        object
			typename    CoreSurfaceAllocation
		}
	}


	method {
		name	PreReadBuffer

		arg {
			name	    buffer
			direction   input
			type        object
			typename    CoreSurfaceBuffer
		}

		arg {
			name	    rect
			direction   input
			type        struct
			typename    DFBRectangle
		}

		arg {
			name	    allocation
			direction   output
			type        object
			typename    CoreSurfaceAllocation
		}
	}


	method {
		name	PreWriteBuffer

		arg {
			name	    buffer
			direction   input
			type        object
			typename    CoreSurfaceBuffer
		}

		arg {
			name	    rect
			direction   input
			type        struct
			typename    DFBRectangle
		}

		arg {
			name	    allocation
			direction   output
			type        object
			typename    CoreSurfaceAllocation
		}
	}


	method {
		name	PreLockBuffer3

		arg {
			name	    role
			direction   input
			type        enum
			typename    CoreSurfaceBufferRole
		}

		arg {
			name	    flip_count
			direction   input
			type        int
			typename    u32
		}

		arg {
			name	    eye
			direction   input
			type        enum
			typename    DFBSurfaceStereoEye
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
			direction   input
			type        int
			typename    bool
		}

		arg {
			name	    allocation
			direction   output
			type        object
			typename    CoreSurfaceAllocation
		}
	}


	method {
		name	CreateClient

		arg {
			name	    client
			direction   output
			type        object
			typename    CoreSurfaceClient
		}
	}


	method {
		name	BackToFrontCopy

		arg {
			name	    eye
			direction   input
			type        enum
			typename    DFBSurfaceStereoEye
		}

		arg {
			name	    left_region
			direction   input
			type        struct
			typename    DFBRegion
			optional    yes
		}

		arg {
			name	    right_region
			direction   input
			type        struct
			typename    DFBRegion
			optional    yes
		}
	}
}

