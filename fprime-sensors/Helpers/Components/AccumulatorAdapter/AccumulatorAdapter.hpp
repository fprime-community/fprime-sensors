// ======================================================================
// \title  AccumulatorAdapter.hpp
// \author starchmd
// \brief  hpp file for AccumulatorAdapter component implementation class
// ======================================================================

#ifndef FprimeSensors_AccumulatorAdapter_HPP
#define FprimeSensors_AccumulatorAdapter_HPP

#include "fprime-sensors/Helpers/Components/AccumulatorAdapter/AccumulatorAdapterComponentAc.hpp"

namespace FprimeSensors {

class AccumulatorAdapter final : public AccumulatorAdapterComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct AccumulatorAdapter object
    AccumulatorAdapter(const char* const compName  //!< The component name
    );

    //! Destroy AccumulatorAdapter object
    ~AccumulatorAdapter();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for bufferLikeIn
    //!
    //! Port to receive buffer like data
    void bufferLikeIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& data) override;

    //! Handler implementation for commLikeIn
    //!
    //! Port to receive comm like data
    void commLikeIn_handler(FwIndexType portNum,  //!< The port number
                            Fw::Buffer& data,
                            const ComCfg::FrameContext& context) override;

    //! Handler implementation for byteStreamLikeIn
    //!
    //! Port to receive byte stream like data
    void byteStreamLikeIn_handler(FwIndexType portNum,  //!< The port number
                                  Fw::Buffer& buffer,
                                  const Drv::ByteStreamStatus& status) override;
};

}  // namespace FprimeSensors

#endif
