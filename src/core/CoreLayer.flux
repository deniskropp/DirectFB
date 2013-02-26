interface {
	name    ILayer
	version 1.0
	object  CoreLayer

        method {
                name    CreateContext

                arg {
                        name        context
                        direction   output
                        type        object
                        typename    CoreLayerContext
                }
        }

        method {
                name    GetPrimaryContext

                arg {
                        name        activate
                        direction   input
                        type        enum
                        typename    DFBBoolean
                }

                arg {
                        name        context
                        direction   output
                        type        object
                        typename    CoreLayerContext
                }
        }

        method {
                name    ActivateContext

                arg {
                        name        context
                        direction   input
                        type        object
                        typename    CoreLayerContext
                }
        }

        method {
                name    GetCurrentOutputField

                arg {
                        name        field
                        direction   output
                        type        int
                        typename    s32
                }
        }

        method {
                name    SetLevel

                arg {
                        name        level
                        direction   input
                        type        int
                        typename    s32
                }
        }

        method {
                name    WaitVSync
        }
}

