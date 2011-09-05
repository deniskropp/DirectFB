interface {
	name    IScreen
	version 1.0
	object  CoreScreen

        method {
                name    SetPowerMode

                arg {
                        name        mode
                        direction   input
                        type        enum
                        typename    DFBScreenPowerMode
                }
        }

        method {
                name    WaitVSync
        }

        method {
                name    GetVSyncCount

                arg {
                        name        count
                        direction   output
                        type        int
                        typename    u64
                }
        }

        method {
                name    TestMixerConfig

                arg {
                        name        mixer
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenMixerConfig
                }

                arg {
                        name        failed
                        direction   output
                        type        enum
                        typename    DFBScreenMixerConfigFlags
                }
        }

        method {
                name    SetMixerConfig

                arg {
                        name        mixer
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenMixerConfig
                }
        }

        method {
                name    TestEncoderConfig

                arg {
                        name        encoder
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenEncoderConfig
                }

                arg {
                        name        failed
                        direction   output
                        type        enum
                        typename    DFBScreenEncoderConfigFlags
                }
        }

        method {
                name    SetEncoderConfig

                arg {
                        name        encoder
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenEncoderConfig
                }
        }

        method {
                name    TestOutputConfig

                arg {
                        name        output
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenOutputConfig
                }

                arg {
                        name        failed
                        direction   output
                        type        enum
                        typename    DFBScreenOutputConfigFlags
                }
        }

        method {
                name    SetOutputConfig

                arg {
                        name        output
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenOutputConfig
                }
        }

        method {
                name    GetScreenSize

                arg {
                        name        size
                        direction   output
                        type        struct
                        typename    DFBDimension
                }
        }

        method {
                name    GetLayerDimension

                arg {
                        name        layer
                        direction   input
                        type        object
                        typename    CoreLayer
                }

                arg {
                        name        size
                        direction   output
                        type        struct
                        typename    DFBDimension
                }
        }
}

