interface {
        name        ISaWManWM
        version     "1.0"
        object      SaWMan

        method {
                name    RegisterProcess

                arg {
                        name        flags
                        direction   input
                        type        enum
                        typename    SaWManProcessFlags
                }

                arg {
                        name        pid
                        direction   input
                        type        int
                        typename    s32
                }

                arg {
                        name        fusion_id
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        process
                        direction   output
                        type        object
                        typename    SaWManProcess
                }
        }

        method {
                name    Start

                arg {
                        name        name
                        direction   input
                        type        int
                        typename    u8
			count	    name_len
                }

                arg {
                        name        name_len
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        pid
                        direction   output
                        type        int
                        typename    s32
                }
        }

        method {
                name    Stop

                arg {
                        name        pid
                        direction   input
                        type        int
                        typename    s32
                }
        }

        method {
                name    GetPerformance

                arg {
                        name        stacking
                        direction   input
                        type        enum
                        typename    DFBWindowStackingClass
                }

                arg {
                        name        reset
                        direction   input
                        type        enum
                        typename    DFBBoolean
                }

                arg {
                        name        updates
                        direction   output
                        type        int
                        typename    u32
                }

                arg {
                        name        pixels
                        direction   output
                        type        int
                        typename    u64
                }

                arg {
                        name        duration
                        direction   output
                        type        int
                        typename    s64
                }
        }
}

