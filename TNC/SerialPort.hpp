// Copyright 2016 Rob Riggs <rob@mobilinkd.com>
// All rights reserved.

#ifndef MOBILINKD__TNC__SERIAL_PORT_HPP_
#define MOBILINKD__TNC__SERIAL_PORT_HPP_

#include "PortInterface.hpp"
#include "cmsis_os.h"

namespace mobilinkd { namespace tnc {

/**
 * This interface defines the semi-asynchronous interface used for reading
 * and writing
 */
struct SerialPort : PortInterface
{
    virtual ~SerialPort() {}
    virtual bool open();
    virtual bool isOpen() const { return open_; }
    virtual void close();
    virtual osMessageQId queue() const { return queue_; }
    virtual bool write(const uint8_t* data, uint32_t size, uint8_t type,
        uint32_t timeout);
    virtual bool write(const uint8_t* data, uint32_t size, uint32_t timeout);
    virtual bool write(hdlc::IoFrame* frame, uint32_t timeout = osWaitForever);

    void init();

    bool open_{false};                  // opened/closed
    osMutexId mutex_{0};                // TX Mutex
    osMessageQId queue_{0};             // ISR read queue
    osThreadId serialTaskHandle_{0};
};

SerialPort* getSerialPort();

}} // mobilinkd::tnc

#endif // MOBILINKD__TNC__SERIAL_PORT_HPP_
