interface {
	name        IPalette
	version     1.0
	object      CorePalette


	method {
		name	SetEntries

		arg {
			name	    colors
			direction   input
			type        struct
			typename    DFBColor
			count       num
		}

		arg {
			name	    num
			direction   input
			type        int
			typename    u32
		}

		arg {
			name	    offset
			direction   input
			type        int
			typename    u32
		}
	}

	method {
		name	SetEntriesYUV

		arg {
			name	    colors
			direction   input
			type        struct
			typename    DFBColorYUV
			count       num
		}

		arg {
			name	    num
			direction   input
			type        int
			typename    u32
		}

		arg {
			name	    offset
			direction   input
			type        int
			typename    u32
		}
	}
}

