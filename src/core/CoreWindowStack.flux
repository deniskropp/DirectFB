interface {
	name    IWindowStack
	version 1.0
	object  CoreWindowStack


	method {
		name RepaintAll
	}


	method {
		name GetInsets

		arg {
			name		window
			direction	input
			type		object
			typename	CoreWindow
		}

		arg {
			name		insets
			direction	output
			type		struct
			typename	DFBInsets
		}
	}

	method {
		name CursorEnable

		arg {
			name		enable
			direction	input
			type		enum
			typename	bool
		}
	}

	method {
		name CursorSetShape

		arg {
			name		shape
			direction	input
			type		object
			typename	CoreSurface
		}

		arg {
			name		hotspot
			direction	input
			type		struct
			typename	DFBPoint
		}
	}

	method {
		name CursorSetOpacity

		arg {
			name		opacity
			direction	input
			type		int
			typename	u8
		}
	}

	method {
		name CursorSetAcceleration

		arg {
			name		numerator
			direction	input
			type		int
			typename	u32
		}

		arg {
			name		denominator
			direction	input
			type		int
			typename	u32
		}

		arg {
			name		threshold
			direction	input
			type		int
			typename	u32
		}
	}

	method {
		name CursorWarp

		arg {
			name		position
			direction	input
			type		struct
			typename	DFBPoint
		}
	}

	method {
		name CursorGetPosition

		arg {
			name		position
			direction	output
			type		struct
			typename	DFBPoint
		}
	}


	method {
		name BackgroundSetMode

		arg {
			name		mode
			direction	input
			type		enum
			typename	DFBDisplayLayerBackgroundMode
		}
	}

	method {
		name BackgroundSetImage

		arg {
			name		image
			direction	input
			type		object
			typename	CoreSurface
		}
	}

	method {
		name BackgroundSetColor

		arg {
			name		color
			direction	input
			type		struct
			typename	DFBColor
		}
	}

	method {
		name BackgroundSetColorIndex

		arg {
			name		index
			direction	input
			type		int
			typename	s32
		}
	}
}

