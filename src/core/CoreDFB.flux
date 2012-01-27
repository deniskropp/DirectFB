interface {
        name        ICore
        version     "1.0"
        object      CoreDFB

	method {
		name	Register

                arg {
                        name        slave_call
                        direction   input
                        type        int
                        typename    u32
                }
	}

        method {
                name    CreateSurface

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    CoreSurfaceConfig
                }

                arg {
                        name        type
                        direction   input
                        type        enum
                        typename    CoreSurfaceTypeFlags
                }

                arg {
                        name        resource_id
                        direction   input
                        type        int
                        typename    u64
                }

                arg {
                        name        palette
                        direction   input
                        type        object
                        typename    CorePalette
                        optional    yes
                }

                arg {
                        name        surface
                        direction   output
                        type        object
                        typename    CoreSurface
                }
        }

        method {
                name    CreatePalette

                arg {
                        name        size
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        palette
                        direction   output
                        type        object
                        typename    CorePalette
                }
        }

        method {
                name    CreateState

                arg {
                        name        state
                        direction   output
                        type        object
                        typename    CoreGraphicsState
                }
        }

        method {
                name    WaitIdle
        }

        method {
                name    CreateImageProvider

                arg {
                        name        buffer_call
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        call
                        direction   output
                        type        int
                        typename    u32
                }
        }

        method {
                name    AllowSurface

                arg {
                        name        surface
                        direction   input
                        type        object
                        typename    CoreSurface
                }

                arg {
                        name        executable
                        direction   input
                        type        int
                        typename    char
			count       executable_length
                }

                arg {
                        name        executable_length
                        direction   input
                        type        int
                        typename    u32
                }
        }

        method {
                name    GetSurface

                arg {
                        name        surface_id
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        surface
                        direction   output
                        type        object
                        typename    CoreSurface
                }
        }
}

