// ======================================================================
// \title  GpsManager.cpp
// \author starchmd
// \brief  cpp file for GpsManager component implementation class
// ======================================================================

#include "fprime-sensors/NmeaGps/Components/GpsManager/GpsManager.hpp"
#include "Fw/Types/StringBase.hpp"
#include <cstdio> // Needed for sscanf
#include <cinttypes>

namespace NmeaGps {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

GpsManager ::GpsManager(const char* const compName) : GpsManagerComponentBase(compName) {}

GpsManager ::~GpsManager() {}

F64 GpsManager ::ddmmmmmm_to_degrees(F64 ddmmmmmm, char direction) {
    // Convert DDmm.mmmm to degrees
    U8 degrees = ddmmmmmm / 100;
    double minutes = ddmmmmmm - (degrees * 100.0);
    return ((direction == 'N' || direction == 'E') ? 1 : -1) * (degrees + (minutes / 60.0));
}

void GpsManager ::parse_gga_message(const char* data, Fw::StringBase& messageHeader) {
    GpsManager::GgaMessage gga;
    GpsData reading;

    // Scan the GGA message
    int fields = ::sscanf(data, GpsManager::GGA_FORMAT,
                          &gga.time, &gga.latitude, &gga.latitudeDirection,
                          &gga.longitude, &gga.longitudeDirection, &gga.fixType,
                          &gga.numSatellites, &gga.horizontalDilution, &gga.altitude,
                          &gga.altitudeUnits, &gga.undulation, &gga.undulationUnits,
                          &gga.age, &gga.stationId);
    // Read at least 13 fields from the GGA message
    if (fields < 12) {
        this->log_WARNING_LO_MalformedMessage(messageHeader, static_cast<U8>((fields < 0) ? 0 : fields));
    }
    // Check if the reading is invalid
    else if (gga.fixType == 0) {
        this->log_WARNING_LO_InvalidData(messageHeader);
    }
    // Perform conversion
    else {
        reading.set_latitude(this->ddmmmmmm_to_degrees(gga.latitude, gga.latitudeDirection));
        reading.set_longitude(this->ddmmmmmm_to_degrees(gga.longitude, gga.longitudeDirection));
        reading.set_altitude(gga.altitude * ((gga.altitudeUnits == 'F') ? 0.3048 : 1.0)); // Convert to meters if in feet
        this->tlmWrite_Reading(reading);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void GpsManager ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data) {
    char messageTypeBuffer[5 + 1] = "?????"; // Header length - 1 ($) + null terminator
    Fw::ExternalString messageHeader(messageTypeBuffer, sizeof(messageTypeBuffer));
    // Offset type by 2 to skip BD, GB, GA, GP, and GL prefixes
    Fw::ExternalString messageType(messageTypeBuffer + 2, sizeof(messageTypeBuffer) - 2);

    char* messagePointer = reinterpret_cast<char*>(data.getData());
    messagePointer[data.getSize() - 1] = '\0'; // Ensure null termination

    // Parse the message header
    int fields = ::sscanf(messagePointer, GpsManager::HEADER_FORMAT, messageTypeBuffer);
    if (fields < 1) {
        this->log_WARNING_LO_MalformedMessage(messageHeader, 0);
    }
    // Beidou, Galileo, GPS, and GLONASS messages
    else if (messageType == "GGA") {
        this->parse_gga_message(messagePointer + 1 + sizeof(messageTypeBuffer), messageHeader);
    }

    // Always return the data
    this->dataReturnOut_out(portNum, data);
}

}  // namespace NmeaGps
