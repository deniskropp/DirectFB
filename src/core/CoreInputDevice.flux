interface {
	name    IInputDevice
	version 1.0
	object  CoreInputDevice

        method {
                name    SetKeymapEntry

                arg {
                        name        key_code
                        direction   input
                        type        int
                        typename    s32
                }

                arg {
                        name        entry
                        direction   input
                        type        struct
                        typename    DFBInputDeviceKeymapEntry
                }
        }

        method {
                name    ReloadKeymap
        }

        method {
                name    SetConfiguration

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBInputDeviceConfig
                }
        }
}

