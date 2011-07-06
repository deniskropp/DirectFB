interface {
	name    ILayerRegion
	version 1.0
	object  CoreLayerRegion

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
}

