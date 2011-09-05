interface {
	name    ILayerRegion
	version 1.0
	object  CoreLayerRegion

	method {
		name GetSurface

		arg {
			name      surface
			direction output
			type      object
			typename  CoreSurface
		}
	}

	method {
		name FlipUpdate

		arg {
			name      update
			direction input
			type      struct
			typename  DFBRegion
			optional  yes
		}

		arg {
			name      flags
			direction input
			type      enum
			typename  DFBSurfaceFlipFlags
		}
	}

	method {
		name FlipUpdateStereo

		arg {
			name      left
			direction input
			type      struct
			typename  DFBRegion
			optional  yes
		}

		arg {
			name      right
			direction input
			type      struct
			typename  DFBRegion
			optional  yes
		}

		arg {
			name      flags
			direction input
			type      enum
			typename  DFBSurfaceFlipFlags
		}
	}
}

