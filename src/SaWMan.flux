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
}

