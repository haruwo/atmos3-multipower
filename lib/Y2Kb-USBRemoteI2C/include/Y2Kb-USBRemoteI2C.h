#ifndef _Y2KB_USBREMOTEI2C_H_
#define _Y2KB_USBREMOTEI2C_H_

#include <Wire.h>

class Y2KB_USBRemoteI2C
{
public:
    Y2KB_USBRemoteI2C(TwoWire &wire = Wire);
    void begin(unsigned char address = 0x15);
    void end();
    void reset();

    // change state to on
    void on();

    // change state to off
    void off();

    // return 0: off, 1: on, -1: error
    int read();

    // update initial state, 0: off, 1: on
    void updateInitialState(int);

private:
    unsigned char _address;
    bool _initialized;
    TwoWire &_wire;
};

extern Y2KB_USBRemoteI2C USBRemoteI2C;

#endif // _Y2KB_USBREMOTEI2C_H_
