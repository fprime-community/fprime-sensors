// ======================================================================
// \title  XBeeManager.cpp
// \author mstarch
// \brief  cpp file for XBeeManager component implementation class
// ======================================================================

#include <fprime-sensors/XBee/Components/XBeeManager/XBeeManager.hpp>
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/BasicTypes.hpp"

namespace XBee {

constexpr XBeeManager::RadioCommand XBeeManager::ENTER_COMMAND_MODE;
constexpr XBeeManager::RadioCommand XBeeManager::NODE_IDENTIFIER;
constexpr XBeeManager::RadioCommand XBeeManager::ENERGY_DENSITY;
constexpr XBeeManager::RadioCommand XBeeManager::EXIT_COMMAND_MODE;

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

XBeeManager ::XBeeManager(const char* const compName)
    : XBeeManagerComponentBase(compName),
      m_circular(m_data, sizeof m_data),
      m_state(PASSTHROUGH),
      m_opCode(0),
      m_cmdSeq(0),
      m_timeoutCount(0),
      m_reinit(true) {}

XBeeManager ::~XBeeManager() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void XBeeManager ::dataReturnIn_handler(FwIndexType portNum,
                                        Fw::Buffer& fwBuffer,
                                        const ComCfg::FrameContext& context) {
    this->drvReceiveReturnOut_out(0, fwBuffer);
}

void XBeeManager ::drvConnected_handler(const FwIndexType portNum) {
    report_ready();
}

void XBeeManager ::drvReceiveIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    m_lock.lock();
    const ComState current_state = this->m_state;
    m_lock.unlock();
    // On a good read when not in transparent mode, we process data
    if (status == Drv::ByteStreamStatus::OP_OK && current_state != PASSTHROUGH && current_state != ERROR_TIMEOUT &&
        current_state != QUIET_RADIO) {
        m_circular.serialize(buffer.getData(), buffer.getSize());
        state_machine();
        // return buffer ownership after processing
        this->drvReceiveReturnOut_out(0, buffer);
    }
    // Otherwise pass data to the deframer
    else if (current_state == PASSTHROUGH || current_state == QUIET_RADIO) {
        ComCfg::FrameContext context;
        dataOut_out(0, buffer, context);
    } else {
        // In error timeout or other state, just deallocate the buffer
        this->drvReceiveReturnOut_out(0, buffer);
    }
}

void XBeeManager ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    Fw::Success radioReady = Fw::Success::FAILURE;
    m_lock.lock();
    // Only attempt to send data when in transparent mode. Sending data in command mode is an error
    if (PASSTHROUGH == m_state) {
        Drv::ByteStreamStatus driverStatus = Drv::ByteStreamStatus::SEND_RETRY;
        for (FwIndexType i = 0; driverStatus == Drv::ByteStreamStatus::SEND_RETRY && i < retryLimit; i++) {
            driverStatus = drvSendOut_out(0, const_cast<Fw::Buffer&>(data));
        }
        radioReady = (driverStatus.e == Drv::ByteStreamStatus::OP_OK) ? Fw::Success::SUCCESS : Fw::Success::FAILURE;
    } else {
        m_reinit = true;
    }
    m_lock.unlock();
    // Only send ready if the port is connected
    if (isConnected_comStatusOut_OutputPort(0)) {
        comStatusOut_out(0, radioReady);
    }
    dataReturnOut_out(0, data, context);  // return ownership of the buffer
}

void XBeeManager ::run_handler(const FwIndexType portNum, U32 context) {
    m_lock.lock();
    if (m_state != PASSTHROUGH) {
        m_timeoutCount = m_timeoutCount + 1;
        // Check if ready to enable command mode
        if ((m_timeoutCount > QUIET_TICKS_1HZ) && (m_state == QUIET_RADIO)) {
            initiate_command();
        }
        // Check if timing out of command mode
        else if (m_timeoutCount > TIMEOUT_TICKS_1HZ) {
            deinitiate_command(Fw::CmdResponse::EXECUTION_ERROR);
            m_state = PASSTHROUGH;
        }
    }
    m_lock.unlock();
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void XBeeManager ::ReportNodeIdentifier_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {
    m_lock.lock();
    stage_command(XBeeManager::NODE_IDENTIFIER, opCode, cmdSeq);
    m_lock.unlock();
}

void XBeeManager ::EnergyDensityScan_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {
    m_lock.lock();
    stage_command(XBeeManager::ENERGY_DENSITY, opCode, cmdSeq);
    m_lock.unlock();
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void XBeeManager ::stage_command(const RadioCommand& command, const FwOpcodeType opCode, const U32 cmdSeq) {
    if ((m_currentCommand != nullptr) || (m_state != PASSTHROUGH)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
    } else {
        FW_ASSERT(m_currentCommand == nullptr);
        FW_ASSERT(m_state == PASSTHROUGH);
        m_state = QUIET_RADIO;
        m_opCode = opCode;
        m_cmdSeq = cmdSeq;
        m_currentCommand = &command;
    }
}

void XBeeManager ::initiate_command() {
    // Attempt to enter command mode, otherwise immediately fallback to passthrough
    if (send_radio_command(XBeeManager::ENTER_COMMAND_MODE)) {
        m_state = AWAIT_COMMAND_MODE;
    } else {
        deinitiate_command(Fw::CmdResponse::EXECUTION_ERROR);
    }
}

bool XBeeManager ::deinitiate_command(const Fw::CmdResponse& response) {
    report_ready();
    this->cmdResponse_out(m_opCode, m_cmdSeq, response);
    m_currentCommand = nullptr;
    m_timeoutCount = 0;
    m_opCode = 0;
    m_cmdSeq = 0;
    return true;
}

bool XBeeManager ::send_radio_command(const RadioCommand& command) {
    // Send command, wait 1 second per command mode default guidelines
    Drv::ByteStreamStatus driverStatus = Drv::ByteStreamStatus::SEND_RETRY;
    Fw::Buffer buffer(reinterpret_cast<U8*>(const_cast<char*>(command.command)), command.length);
    for (FwIndexType i = 0; driverStatus == Drv::ByteStreamStatus::SEND_RETRY && i < retryLimit; i++) {
        driverStatus = drvSendOut_out(0, buffer);
    }
    m_timeoutCount = (driverStatus.e == Drv::ByteStreamStatus::OP_OK) ? m_timeoutCount : 0;
    return (buffer.getSize() >= command.length) && (driverStatus.e == Drv::ByteStreamStatus::OP_OK);
}

void XBeeManager ::state_machine() {
    XBeeManager::ProcessResponse response = this->process_response();
    if (response == XBeeManager::ProcessResponse::PROCESSED_ERROR) {
        m_state = ERROR_TIMEOUT;
    } else if (response == XBeeManager::ProcessResponse::PROCESSED_GOOD) {
        bool success = true;
        switch (m_state) {
            case AWAIT_COMMAND_MODE:
                m_lock.lock();
                FW_ASSERT(m_currentCommand != nullptr);
                success = send_radio_command(*m_currentCommand);
                m_lock.unlock();
                break;
            case AWAIT_COMMAND_RESPONSE:
                m_lock.lock();
                success = send_radio_command(EXIT_COMMAND_MODE);
                m_lock.unlock();
                break;
            case AWAIT_PASSTHROUGH:
                m_lock.lock();
                success = deinitiate_command(Fw::CmdResponse::OK);
                m_lock.unlock();
                break;
            default:
                FW_ASSERT(0);
                break;
        }
        m_lock.lock();
        m_state = success ? static_cast<ComState>(m_state + 1) : ERROR_TIMEOUT;
        FW_ASSERT(m_state >= ERROR_TIMEOUT && m_state <= PASSTHROUGH, m_state);
        m_lock.unlock();
    }
}

void XBeeManager ::report_ready() {
    if (m_reinit) {
        Fw::Success radioReady = Fw::Success::SUCCESS;
        if (isConnected_comStatusOut_OutputPort(0)) {
            comStatusOut_out(0, radioReady);
        }
        m_reinit = false;
    }
}

// ----------------------------------------------------------------------
// Helper functions: XBeeManager command response processing functions
// ----------------------------------------------------------------------

XBeeManager::ProcessResponse XBeeManager ::process_response() {
    XBeeManager::ProcessResponse response = MORE_NEEDED;
    U8 last_byte = 0;
    m_circular.peek(last_byte, m_circular.get_allocated_size() - 1);
    // Looking of OK\r or ERROR\r
    if (last_byte == '\r') {
        if (m_state != AWAIT_COMMAND_RESPONSE) {
            response = process_ok_or_error();
        } else if (m_currentCommand == &XBeeManager::NODE_IDENTIFIER) {
            response = process_node_identifier();
        } else if (m_currentCommand == &XBeeManager::ENERGY_DENSITY) {
            response = process_energy_density();
        }
        // Clear the existing data in the circular buffer
        m_circular.rotate(m_circular.get_allocated_size());
    }
    return response;
}

XBeeManager::ProcessResponse XBeeManager ::process_ok_or_error() {
    CHAR ok[3];
    const U32 peek_size = FW_MIN(sizeof(ok), m_circular.get_allocated_size());
    const U32 peek_offset = FW_MAX(m_circular.get_allocated_size() - peek_size, 0);
    m_circular.peek(reinterpret_cast<U8*>(ok), peek_size, peek_offset);
    // OK was received that is a successful response
    if (ok[0] == 'O' && ok[1] == 'K' && ok[2] == '\r') {
        return XBeeManager::ProcessResponse::PROCESSED_GOOD;
    }
    // Something else produces error
    return XBeeManager::ProcessResponse::PROCESSED_ERROR;
}

XBeeManager::ProcessResponse XBeeManager ::process_node_identifier() {
    Fw::LogStringArg identifier;
    const U32 peek_size = FW_MIN(FW_MIN(identifier.getCapacity(), m_currentCommand->response_length + 1),
                                 m_circular.get_allocated_size());
    const U32 peek_offset = FW_MAX(m_circular.get_allocated_size() - peek_size, 0);
    char* peak_buffer = const_cast<char*>(identifier.toChar());

    m_circular.peek(reinterpret_cast<U8*>(peak_buffer), peek_size, peek_offset);
    peak_buffer[peek_size - 1] = '\0';
    this->log_ACTIVITY_HI_RadioNodeIdentifier(identifier);
    // Always return true
    return XBeeManager::ProcessResponse::PROCESSED_GOOD;
}

XBeeManager::ProcessResponse XBeeManager ::process_energy_density() {
    XBeeManager::ProcessResponse response = XBeeManager::PROCESSED_GOOD;
    EnergyDensityType density;
    for (U32 i = 0; i < density.SIZE; i++) {
        U8 reading[3];
        if ((m_circular.peek(reading, sizeof(reading)) == Fw::SerializeStatus::FW_SERIALIZE_OK) &&
            (reading[sizeof(reading) - 1] == ',')) {
            density[i] = static_cast<U8>(convert_char(reading[0]) << 4 | convert_char(reading[1]));
        } else {
            response = XBeeManager::PROCESSED_ERROR;
            break;
        }
        m_circular.rotate(sizeof(reading));
    }
    // Successful read, send telemetry
    if (response == XBeeManager::PROCESSED_GOOD) {
        tlmWrite_EnergyDensity(density);
    }
    return response;
}

}  // namespace XBee
