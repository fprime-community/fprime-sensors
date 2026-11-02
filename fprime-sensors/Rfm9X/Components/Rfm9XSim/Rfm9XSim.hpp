// ======================================================================
// \title  Rfm9XSim.hpp
// \brief  hpp file for Rfm9XSim component implementation class
// ======================================================================

#ifndef Rfm9X_Rfm9XSim_HPP
#define Rfm9X_Rfm9XSim_HPP

#include "fprime-sensors/Rfm9X/Components/Rfm9XSim/Rfm9XSimComponentAc.hpp"
#include "fprime-sensors/Rfm9X/Components/Rfm9XSim/Rfm9XSimModel.hpp"

namespace Rfm9X {

class Rfm9XSim final : public Rfm9XSimComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Rfm9XSim object
    Rfm9XSim(const char* const compName  //!< The component name
    );

    //! Destroy Rfm9XSim object
    ~Rfm9XSim();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for SpiWriteRead
    Drv::SpiStatus SpiWriteRead_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) override;

    //! Handler implementation for SpiReadWrite (deprecated Drv.Spi port)
    void SpiReadWrite_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) override;

    //! Handler implementation for airDataIn: bytes from the simulated air interface
    void airDataIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) override;

    //! Handler implementation for airReady: air interface driver ready (no-op)
    void airReady_handler(FwIndexType portNum) override;

    //! Send any transmitted packets out the air interface
    void drainTransmittedPackets();

    Rfm9XSimModel m_model;  //!< Register-level radio model
};

}  // namespace Rfm9X

#endif  // Rfm9X_Rfm9XSim_HPP
