interface {
        name        ISaWManManager
        version     "1.0"
        object      SaWManManager

        method {
                name    Activate
        }

        method {
                name    QueueUpdate
		async   yes

                arg {
                        name        stacking
                        direction   input
                        type        enum
                        typename    DFBWindowStackingClass
                }

                arg {
                        name        update
                        direction   input
                        type        struct
                        typename    DFBRegion
			optional    yes
                }
        }

        method {
                name    ProcessUpdates
		async   yes

                arg {
                        name        flags
                        direction   input
                        type        enum
                        typename    DFBSurfaceFlipFlags
                }
        }

        method {
                name    CloseWindow
		async   yes

                arg {
                        name        window
                        direction   input
                        type        object
                        typename    SaWManWindow
                }
        }

        method {
                name    InsertWindow
		async   yes

                arg {
                        name        window
                        direction   input
                        type        object
                        typename    SaWManWindow
                }

                arg {
                        name        relative
                        direction   input
                        type        object
                        typename    SaWManWindow
			optional    yes
                }

                arg {
                        name        relation
                        direction   input
                        type        enum
                        typename    SaWManWindowRelation
                }
        }

        method {
                name    RemoveWindow
		async   yes

                arg {
                        name        window
                        direction   input
                        type        object
                        typename    SaWManWindow
                }
        }

        method {
                name    SwitchFocus
		async   yes

                arg {
                        name        window
                        direction   input
                        type        object
                        typename    SaWManWindow
                }
        }

        method {
                name    SetScalingMode
		async   yes

                arg {
                        name        mode
                        direction   input
                        type        enum
                        typename    SaWManScalingMode
                }
        }

        method {
                name    SetWindowConfig
		async   yes

                arg {
                        name        window
                        direction   input
                        type        object
                        typename    SaWManWindow
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    SaWManWindowConfig
                }

                arg {
                        name        flags
                        direction   input
                        type        enum
                        typename    SaWManWindowConfigFlags
                }
        }

        method {
                name    IsShowingWindow

                arg {
                        name        window
                        direction   input
                        type        object
                        typename    SaWManWindow
                }

                arg {
                        name        showing
                        direction   output
                        type        enum
                        typename    DFBBoolean
                }
        }
}

