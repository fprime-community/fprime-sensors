module MpuImu {
    @ Define ImuSM State Machine
    state machine ImuStateMachine {
        @ Initial state: reset the device
        initial enter RESET

        @ Rate-group driven signal
        signal tick

        @ Reconfigure signal
        signal reconfigure

        @ Current state passed successfully
        signal success

        @ Current state erred
        signal error

        @ Perform reset commands
        action doReset

        @ Check reset took place
        action checkReset

        @ Perform enable commands
        action doEnable

        @ Perform configure commands
        action doConfigure

        @ Read the IMU
        action doRead

        @ Reset the Imu
        state RESET {
            on tick do { doReset }
            on success enter WAIT_RESET
        }

        state WAIT_RESET {
            on tick do { checkReset }
            on success enter ENABLE
            on error enter RESET
        }

        @ Enable Imu data flows
        state ENABLE {
            on tick do { doEnable }
            on success enter CONFIGURE
            on error enter RESET
        }

        @ Configure Imu
        state CONFIGURE {
            on tick do { doConfigure }
            on success enter RUN
            on error enter RESET
        }

        @ Run the Imu
        state RUN {
            on tick do { doRead }
            on reconfigure enter CONFIGURE
            on error enter RESET
        }
    }
}