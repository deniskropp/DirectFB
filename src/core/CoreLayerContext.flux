interface {
	name    ILayerContext
	version 1.0
	object  CoreLayerContext

	method {
		name CreateWindow

		arg {
			name      description
			direction input
			type      struct
			typename  DFBWindowDescription
		}

		arg {
			name      window
			direction output
			type      object
			typename  CoreWindow
		}
	}

	method {
		name SetConfiguration

		arg {
			name      config
			direction input
			type      struct
			typename  DFBDisplayLayerConfig
		}
	}
}

