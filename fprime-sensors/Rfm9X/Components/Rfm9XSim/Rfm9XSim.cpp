// ======================================================================
// \title  Rfm9XSim.cpp
// \brief  cpp file for Rfm9XSim component implementation class
// ======================================================================

#include "fprime-sensors/Rfm9X/Components/Rfm9XSim/Rfm9XSim.hpp"
#include <cstring>

namespace Rfm9X {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

Rfm9XSim ::Rfm9XSim(const char* const compName)
    : Rfm9XSimComponentBase(compName),
      m_model(),
      m_pendingCompletionValid(false),
      m_pendingPortNum(0),
      m_pendingWriteBuffer(),
      m_pendingReadBuffer(),
      m_pendingStatus(Drv::SpiStatus::SPI_OK) {}

Rfm9XSim ::~Rfm9XSim() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Rfm9XSim ::spiWriteReadAsync_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    Drv::SpiStatus status = Drv::SpiStatus::SPI_OK;
    if ((writeBuffer.getSize() == 0) || (readBuffer.getSize() < writeBuffer.getSize())) {
        status = Drv::SpiStatus::SPI_MISMATCH_ERR;
    } else {
        this->m_model.spiTransaction(writeBuffer.getData(), readBuffer.getData(), writeBuffer.getSize());
        this->drainTransmittedPackets();
    }
    // Deferred to run(), not delivered inline -- see this port's doc comment
    // in Rfm9XSim.fpp.
    FW_ASSERT(!this->m_pendingCompletionValid);
    this->m_pendingCompletionValid = true;
    this->m_pendingPortNum = portNum;
    this->m_pendingWriteBuffer = writeBuffer;
    this->m_pendingReadBuffer = readBuffer;
    this->m_pendingStatus = status;
}

void Rfm9XSim ::run_handler(FwIndexType portNum, U32 context) {
    if (!this->m_pendingCompletionValid) {
        return;
    }
    this->m_pendingCompletionValid = false;
    this->spiWriteReadAsyncComplete_out(this->m_pendingPortNum, this->m_pendingWriteBuffer, this->m_pendingReadBuffer,
                                        this->m_pendingStatus);
}

void Rfm9XSim ::airDataIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    if (status == Drv::ByteStreamStatus::OP_OK) {
        this->m_model.injectAirData(buffer.getData(), buffer.getSize());
    }
    this->airDataReturnOut_out(0, buffer);
}

void Rfm9XSim ::airReady_handler(FwIndexType portNum) {}

void Rfm9XSim ::drainTransmittedPackets() {
    U8 payload[MAX_PACKET_PAYLOAD + 1];
    // Each retrievePacket consumes one queue entry, so one full pass over the
    // fixed-depth queue drains everything the model has to offer.
    for (FwSizeType packet = 0; packet < Rfm9XSimModel::TX_QUEUE_DEPTH; packet++) {
        const FwSizeType size = this->m_model.retrievePacket(payload, sizeof payload);
        if (size == 0) {
            break;
        }
        if (!this->isConnected_airDataOut_OutputPort(0)) {
            continue;
        }
        Fw::Buffer buffer = this->allocate_out(0, size);
        if (buffer.getSize() < size) {
            this->log_WARNING_HI_SimAllocationFailed();
            this->deallocate_out(0, buffer);
            continue;
        }
        (void)::memcpy(buffer.getData(), payload, size);
        buffer.setSize(size);
        (void)this->airDataOut_out(0, buffer);
        this->deallocate_out(0, buffer);
    }
}

}  // namespace Rfm9X
