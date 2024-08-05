# Air Sensor

The `air_sensor` module provides an interface for interacting with hardware
components that measure atmospheric conditions such as ambient temperature,
barometric pressure, relative humidity, etc.

Implementators of this module have one key method that they must provide:
`AirSensor::DoMeasure`. This method should measure the local conditions and call
`AirSensor::Update` with the collected data.

Measurements make take some time to perform. Consumers of this module have to
approaches to handle this delay:

1. Consumers may provide a `pw::sync::ThreadNotification` to
   `AirSensor::Measure`. The notification will be released when the data is
   ready.
2. Consumers may call `AirSensor::MeasureSync` from a thread that can block.
   This function will not return until the data is ready.