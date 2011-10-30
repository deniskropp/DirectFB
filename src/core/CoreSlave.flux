interface {
        name        ICoreSlave
        version     "1.0"
        object      CoreSlave
	dispatch    CoreDFB

        method {
                name    GetData

                arg {
                        name        address
                        direction   input
                        type        int
                        typename    "void*"
                }

                arg {
                        name        bytes
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        data
                        direction   output
                        type        int
                        typename    u8
			count       bytes
			max         bytes
                }
        }

        method {
                name    PutData

                arg {
                        name        address
                        direction   input
                        type        int
                        typename    "void*"
                }

                arg {
                        name        bytes
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        data
                        direction   input
                        type        int
                        typename    u8
			count       bytes
                }
        }
}

