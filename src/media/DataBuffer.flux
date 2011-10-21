interface {
        name        IDataBuffer
        version     "1.0"
        object      DataBuffer
        dispatch    IDirectFBDataBuffer

	method {
		name	Flush
	}

	method {
		name	Finish
	}

        method {
                name    SeekTo

                arg {
                        name        offset
                        direction   input
                        type        int
                        typename    u64
                }
        }

        method {
                name    GetPosition

                arg {
                        name        offset
                        direction   output
                        type        int
                        typename    u64
                }
        }

        method {
                name    GetLength

                arg {
                        name        length
                        direction   output
                        type        int
                        typename    u64
                }
        }

        method {
                name    WaitForData

                arg {
                        name        length
                        direction   input
                        type        int
                        typename    u64
                }
        }

        method {
                name    WaitForDataWithTimeout

                arg {
                        name        length
                        direction   input
                        type        int
                        typename    u64
                }

                arg {
                        name        timeout_ms
                        direction   input
                        type        int
                        typename    u64
                }
        }

        method {
                name    GetData

                arg {
                        name        length
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        data
                        direction   output
                        type        int
                        typename    u8
			count       read
                        max         length
                }

                arg {
                        name        read
                        direction   output
                        type        int
                        typename    u32
                }
        }

        method {
                name    PeekData

                arg {
                        name        length
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        offset
                        direction   input
                        type        int
                        typename    s64
                }

                arg {
                        name        data
                        direction   output
                        type        int
                        typename    u8
			count       read
                        max         length
                }

                arg {
                        name        read
                        direction   output
                        type        int
                        typename    u32
                }
        }

        method {
                name    HasData
        }

        method {
                name    PutData

                arg {
                        name        data
                        direction   input
                        type        int
                        typename    u8
			count       length
                }

                arg {
                        name        length
                        direction   input
                        type        int
                        typename    u32
                }
        }
}

