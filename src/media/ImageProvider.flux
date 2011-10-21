interface {
        name        IImageProvider
        version     "1.0"
        object      ImageProvider
        dispatch    ImageProviderDispatch

        method {
                name    Dispose
        }

        method {
                name    GetSurfaceDescription

                arg {
                        name        description
                        direction   output
                        type        struct
                        typename    DFBSurfaceDescription
                }
        }

        method {
                name    GetImageDescription

                arg {
                        name        description
                        direction   output
                        type        struct
                        typename    DFBImageDescription
                }
        }

        method {
                name    RenderTo

                arg {
                        name        destination
                        direction   input
                        type        object
                        typename    CoreSurface
                }

                arg {
                        name        rect
                        direction   input
                        type        struct
                        typename    DFBRectangle
			optional    yes
                }
        }
}

