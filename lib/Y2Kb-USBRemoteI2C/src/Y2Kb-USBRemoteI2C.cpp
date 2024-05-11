#include "Y2Kb-USBRemoteI2C.h"
#include <Arduino.h>
#include <Wire.h>

#define I2C_REGISTER_POWER_STATE 0x01
#define I2C_REGISTER_INTIAL_STATE 0x02
#define I2C_REGISTER_RESET 0x06

// Constructor
Y2KB_USBRemoteI2C::Y2KB_USBRemoteI2C(TwoWire &wire)
    : _address(0x00), _initialized(false), _wire(wire)
{
}

void Y2KB_USBRemoteI2C::begin(unsigned char address)
{
    _address = address;
    _initialized = true;
}

void Y2KB_USBRemoteI2C::reset()
{
    Serial.printf("Resetting USB Remote I2C address: 0x%02X\n", _address);
    _wire.beginTransmission(_address);

    _wire.write(I2C_REGISTER_RESET);
    _wire.write(0xFF);

    _wire.endTransmission();
}

void Y2KB_USBRemoteI2C::end()
{
    _address = 0x00;
}

void Y2KB_USBRemoteI2C::on()
{
    _wire.beginTransmission(_address);

    _wire.write(I2C_REGISTER_POWER_STATE);
    _wire.write(0x01);

    _wire.endTransmission();
}

void Y2KB_USBRemoteI2C::off()
{
    _wire.beginTransmission(_address);

    _wire.write(I2C_REGISTER_POWER_STATE);
    _wire.write(0x00);

    _wire.endTransmission();
}

int Y2KB_USBRemoteI2C::read()
{
    _wire.beginTransmission(_address);
    _wire.write(I2C_REGISTER_POWER_STATE);
    _wire.endTransmission();
    _wire.requestFrom(_address, 1, true);
    if (_wire.available())
    {
        return _wire.read();
    }

    return -1;
}

void Y2KB_USBRemoteI2C::updateInitialState(int state)
{
    _wire.beginTransmission(_address);

    _wire.write(I2C_REGISTER_INTIAL_STATE);
    _wire.write(state);

    _wire.endTransmission();
}

Y2KB_USBRemoteI2C USBRemoteI2C;