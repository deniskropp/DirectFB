interface {
	name        IWindow
	version     1.0
	object      CoreWindow


	method {
		name	Repaint

		arg {
			name	    left
			direction   input
			type        struct
			typename    DFBRegion
		}

		arg {
			name	    right
			direction   input
			type        struct
			typename    DFBRegion
		}

		arg {
			name	    flags
			direction   input
			type        enum
			typename    DFBSurfaceFlipFlags
		}
	}


	method {
		name	Restack

		arg {
			name	    relative
			direction   input
			type        object
			typename    CoreWindow
			optional    yes
		}

		arg {
			name	    relation
			direction   input
			type        int
			typename    int
		}
	}


	method {
		name	SetConfig

		arg {
			name	    config
			direction   input
			type        struct
			typename    CoreWindowConfig
		}

		arg {
			name	    flags
			direction   input
			type        enum
			typename    CoreWindowConfigFlags
		}
	}


	method {
		name	Bind

		arg {
			name	    source
			direction   input
			type        object
			typename    CoreWindow
		}

		arg {
			name	    x
			direction   input
			type        int
			typename    int
		}

		arg {
			name	    y
			direction   input
			type        int
			typename    int
		}
	}


	method {
		name	Unbind

		arg {
			name	    source
			direction   input
			type        object
			typename    CoreWindow
		}
	}


	method {
		name	RequestFocus
	}


	method {
		name	GrabKey

		arg {
			name	    symbol
			direction   input
			type        enum
			typename    DFBInputDeviceKeySymbol
		}

		arg {
			name	    modifiers
			direction   input
			type        enum
			typename    DFBInputDeviceModifierMask
		}
	}


	method {
		name	UngrabKey

		arg {
			name	    symbol
			direction   input
			type        enum
			typename    DFBInputDeviceKeySymbol
		}

		arg {
			name	    modifiers
			direction   input
			type        enum
			typename    DFBInputDeviceModifierMask
		}
	}
}

