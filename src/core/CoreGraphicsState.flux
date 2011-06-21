interface {
	name    IGraphicsState
	version 1.0
	object  CoreGraphicsState

	method {
		name      SetDrawingFlags

		arg {
			name      flags
			direction input
			type      enum
			typename  DFBSurfaceDrawingFlags
		}
	}

	method {
		name      SetBlittingFlags

		arg {
			name      flags
			direction input
			type      enum
			typename  DFBSurfaceBlittingFlags
		}
	}

	method {
		name      SetClip

		arg {
			name      region
			direction input
			type      struct
			typename  DFBRegion
		}
	}

	method {
		name      SetColor

		arg {
			name      color
			direction input
			type      struct
			typename  DFBColor
		}
	}

	method {
		name      SetSrcBlend

		arg {
			name      function
			direction input
			type      enum
			typename  DFBSurfaceBlendFunction
		}
	}

	method {
		name      SetDstBlend

		arg {
			name      function
			direction input
			type      enum
			typename  DFBSurfaceBlendFunction
		}
	}

	method {
		name      SetSrcColorKey

		arg {
			name      key
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      SetDstColorKey

		arg {
			name      key
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      SetDestination

		arg {
			name      surface
			direction input
			type      object
			typename  CoreSurface
		}
	}

	method {
		name      SetSource

		arg {
			name      surface
			direction input
			type      object
			typename  CoreSurface
		}
	}

	method {
		name      SetSourceMask

		arg {
			name      surface
			direction input
			type      object
			typename  CoreSurface
		}
	}

	method {
		name      SetSourceMaskVals

		arg {
			name      offset
			direction input
			type      struct
			typename  DFBPoint
		}

		arg {
			name      flags
			direction input
			type      enum
			typename  DFBSurfaceMaskFlags
		}
	}

	method {
		name      SetIndexTranslation

		arg {
			name      indices
			direction input
			type      struct
			typename  u32
			count     num
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      SetColorKey

		arg {
			name      key
			direction input
			type      struct
			typename  DFBColorKey
		}
	}

	method {
		name      SetRenderOptions

		arg {
			name      options
			direction input
			type      enum
			typename  DFBSurfaceRenderOptions
		}
	}

	method {
		name      SetMatrix

		arg {
			name      values
			direction input
			type      struct
			typename  u32
			count     9
		}
	}

	method {
		name      SetSource2

		arg {
			name      surface
			direction input
			type      object
			typename  CoreSurface
		}
	}




	method {
		name      FillRectangles

		arg {
			name      rects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      Blit

		arg {
			name      rects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
		}

		arg {
			name      points
			direction input
			type      struct
			typename  DFBPoint
			count     num
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      StretchBlit

		arg {
			name      srects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
		}

		arg {
			name      drects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}
}

