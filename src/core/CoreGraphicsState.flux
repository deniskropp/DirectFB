interface {
	name    IGraphicsState
	version 1.0
	object  CoreGraphicsState

	method {
		name      SetDrawingFlags
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      flags
			direction input
			type      enum
			typename  DFBSurfaceDrawingFlags
		}
	}

	method {
		name      SetBlittingFlags
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      flags
			direction input
			type      enum
			typename  DFBSurfaceBlittingFlags
		}
	}

	method {
		name      SetClip
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      region
			direction input
			type      struct
			typename  DFBRegion
		}
	}

	method {
		name      SetColor
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      color
			direction input
			type      struct
			typename  DFBColor
		}
	}

	method {
               name      SetColorAndIndex
               async     yes
               queue     yes
		buffer    yes

               arg {
                       name      color
                       direction input
                       type      struct
                       typename  DFBColor
               }

               arg {
                       name      index
                       direction input
                       type      int
                       typename  u32
               }
       }

	method {
		name      SetSrcBlend
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      function
			direction input
			type      enum
			typename  DFBSurfaceBlendFunction
		}
	}

	method {
		name      SetDstBlend
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      function
			direction input
			type      enum
			typename  DFBSurfaceBlendFunction
		}
	}

	method {
		name      SetSrcColorKey
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      key
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      SetDstColorKey
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      key
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      SetDestination
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      surface
			direction input
			type      object
			typename  CoreSurface
		}
	}

	method {
		name      SetSource
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      surface
			direction input
			type      object
			typename  CoreSurface
		}
	}

	method {
		name      SetSourceMask
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      surface
			direction input
			type      object
			typename  CoreSurface
		}
	}

	method {
		name      SetSourceMaskVals
		async  	  yes
		queue     yes
		buffer    yes

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
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      indices
			direction input
			type      struct
			typename  s32
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
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      key
			direction input
			type      struct
			typename  DFBColorKey
		}
	}

	method {
		name      SetRenderOptions
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      options
			direction input
			type      enum
			typename  DFBSurfaceRenderOptions
		}
	}

	method {
		name      SetMatrix
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      values
			direction input
			type      struct
			typename  s32
			count     9
		}
	}

	method {
		name      SetSource2
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      surface
			direction input
			type      object
			typename  CoreSurface
		}
	}

	method {
		name      SetFrom
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      role
			direction input
			type      enum
			typename  CoreSurfaceBufferRole
		}

		arg {
			name      eye
			direction input
			type      enum
			typename  DFBSurfaceStereoEye
		}
	}

	method {
		name      SetTo
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      role
			direction input
			type      enum
			typename  CoreSurfaceBufferRole
		}

		arg {
			name      eye
			direction input
			type      enum
			typename  DFBSurfaceStereoEye
		}
	}




	method {
		name      DrawRectangles
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      rects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
                        split     yes
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      DrawLines
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      lines
			direction input
			type      struct
			typename  DFBRegion
			count     num
                        split     yes
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      FillRectangles
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      rects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
                        split     yes
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      FillTriangles
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      triangles
			direction input
			type      struct
			typename  DFBTriangle
			count     num
                        split     yes
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      FillTrapezoids
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      trapezoids
			direction input
			type      struct
			typename  DFBTrapezoid
			count     num
                        split     yes
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      FillSpans
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      y
			direction input
			type      int
			typename  s32
		}

		arg {
			name      spans
			direction input
			type      struct
			typename  DFBSpan
			count     num
                        split     yes
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
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      rects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
                        split     yes
		}

		arg {
			name      points
			direction input
			type      struct
			typename  DFBPoint
			count     num
                        split     yes
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      Blit2
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      rects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
                        split     yes
		}

		arg {
			name      points1
			direction input
			type      struct
			typename  DFBPoint
			count     num
                        split     yes
		}

		arg {
			name      points2
			direction input
			type      struct
			typename  DFBPoint
			count     num
                        split     yes
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
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      srects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
                        split     yes
		}

		arg {
			name      drects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
                        split     yes
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      TileBlit
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      rects
			direction input
			type      struct
			typename  DFBRectangle
			count     num
                        split     yes
		}

		arg {
			name      points1
			direction input
			type      struct
			typename  DFBPoint
			count     num
                        split     yes
		}

		arg {
			name      points2
			direction input
			type      struct
			typename  DFBPoint
			count     num
                        split     yes
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}
	}

	method {
		name      TextureTriangles
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      vertices
			direction input
			type      struct
			typename  DFBVertex
			count     num
                        split     yes
		}

		arg {
			name      num
			direction input
			type      int
			typename  u32
		}

		arg {
			name      formation
			direction input
			type      enum
			typename  DFBTriangleFormation
		}
	}

	method {
		name      Flush
		async  	  yes
		queue     yes
	}

	method {
		name      ReleaseSource
		async  	  yes
		queue     yes
		buffer    yes
	}

	method {
		name      SetSrcConvolution
		async  	  yes
		queue     yes
		buffer    yes

		arg {
			name      filter
			direction input
			type      struct
			typename  DFBConvolutionFilter
		}
	}
}

