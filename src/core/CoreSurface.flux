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
			name	    buffer_index
			direction   input
			type        int
			typename    u32
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
			name	    allocation_index
			direction   output
			type        int
			typename    u32
		}
	}


	method {
		name	PreReadBuffer

		arg {
			name	    buffer_index
			direction   input
			type        int
			typename    u32
		}

		arg {
			name	    rect
			direction   input
			type        struct
			typename    DFBRectangle
		}

		arg {
			name	    allocation_index
			direction   output
			type        int
			typename    u32
		}
	}


	method {
		name	PreWriteBuffer

		arg {
			name	    buffer_index
			direction   input
			type        int
			typename    u32
		}

		arg {
			name	    rect
			direction   input
			type        struct
			typename    DFBRectangle
		}

		arg {
			name	    allocation_index
			direction   output
			type        int
			typename    u32
		}
	}
}

