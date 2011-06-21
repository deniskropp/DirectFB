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
                name    ActivateContext

                arg {
                        name        context
                        direction   input
                        type        object
                        typename    CoreLayerContext
                }
        }
}

