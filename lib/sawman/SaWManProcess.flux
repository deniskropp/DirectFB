interface {
        name        ISaWManProcess
        version     "1.0"
        object      SaWManProcess

        method {
                name    SetExiting
        }

        method {
                name    RegisterManager

                arg {
                        name        data
                        direction   input
                        type        struct
                        typename    SaWManRegisterManagerData
                }

                arg {
                        name        manager
                        direction   output
                        type        object
                        typename    SaWManManager
                }
        }
}

