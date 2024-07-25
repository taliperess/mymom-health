// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#define PW_LOG_MODULE_NAME "BME688"
#define PW_LOG_LEVEL PW_LOG_LEVEL_WARN

#include "device/bme688.h"

#include <chrono>
#include <cstdint>

#include "bme68x.h"
#include "pw_assert/check.h"
#include "pw_bytes/endian.h"
#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_log/log.h"
#include "pw_span/span.h"
#include "pw_status/try.h"
#include "pw_thread/sleep.h"

namespace sense {

static constexpr pw::i2c::Address kAddress =
    pw::i2c::Address::SevenBit<BME68X_I2C_ADDR_HIGH>();
static constexpr uint16_t kHeaterTemperature = 300;
static constexpr uint16_t kHeaterDuration = 100;
static constexpr auto kTimeout =
    pw::chrono::SystemClock::for_at_least(std::chrono::minutes(2));

static int8_t Write(uint8_t reg_address,
                    const uint8_t* data,
                    uint32_t length,
                    void* context) {
  PW_LOG_INFO("Write(reg_address=0x%02x, data=%p, length=%u, context=%p)",
              reg_address,
              (const void*)data,
              length,
              context);
  auto i2c_device = static_cast<pw::i2c::RegisterDevice*>(context);

  std::array<std::byte, 16> write_buffer;
  pw::span<const uint8_t> bytes(data, length);
  auto status =
      i2c_device->WriteRegisters8(reg_address, bytes, write_buffer, kTimeout);
  PW_LOG_INFO("WriteRegisters8 returned %s", pw_StatusString(status));
  return status.ok() ? 0 : 1;
}

static int8_t Read(uint8_t reg_address,
                   uint8_t* data,
                   uint32_t length,
                   void* context) {
  PW_LOG_INFO("Read(reg_address=0x%02x, data=%p, length=%u, context=%p)",
              reg_address,
              (const void*)data,
              length,
              context);
  auto i2c_device = static_cast<pw::i2c::RegisterDevice*>(context);

  pw::span<uint8_t> read_buffer(data, length);
  auto status = i2c_device->ReadRegisters8(reg_address, read_buffer, kTimeout);
  PW_LOG_INFO("ReadRegisters8 returned %s", pw_StatusString(status));
  return status.ok() ? 0 : 1;
}

static void Delay(uint32_t interval_us, void* context) {
  PW_LOG_INFO("Delay(interval_us=%u, context=%p)", interval_us, context);
  auto interval = pw::chrono::SystemClock::for_at_least(
      std::chrono::microseconds(interval_us));
  pw::this_thread::sleep_for(interval);
}

Bme688::Bme688(pw::i2c::Initiator& initiator, Worker& worker)
    : worker_(worker),
      i2c_device_(initiator,
                  kAddress,
                  pw::endian::native,
                  pw::i2c::RegisterAddressSize::k1Byte),
      get_data_(pw::bind_member<&Bme688::GetDataCallback>(this)) {}

pw::Status Bme688::DoInit() {
  bme688_.intf_ptr = &i2c_device_;
  bme688_.intf = bme68x_intf::BME68X_I2C_INTF;
  bme688_.read = Read;
  bme688_.write = Write;
  bme688_.delay_us = Delay;
  bme688_.amb_temp = 21;  // Celsius, approximately 70 degrees Fahrenheit.

  PW_LOG_INFO("bme68x_init");
  auto init_status = Check(bme68x_init(&bme688_));
  if (!init_status.ok()) {
    return init_status;
  }

  PW_LOG_INFO("bme68x_get_conf");
  auto get_conf_status = Check(bme68x_get_conf(&config_, &bme688_));
  if (!get_conf_status.ok()) {
    return get_conf_status;
  }

  config_.filter = BME68X_FILTER_OFF;
  config_.odr = BME68X_ODR_NONE;
  config_.os_hum = BME68X_OS_16X;
  config_.os_pres = BME68X_OS_1X;
  config_.os_temp = BME68X_OS_2X;

  PW_LOG_INFO("bme68x_set_conf");
  auto set_conf_status = Check(bme68x_set_conf(&config_, &bme688_));
  if (!set_conf_status.ok()) {
    return set_conf_status;
  }

  return pw::OkStatus();
}

pw::Status Bme688::DoMeasure(pw::sync::ThreadNotification& notification) {
  get_data_.Cancel();
  if (notification_ != nullptr) {
    notification_->release();
  }
  notification_ = &notification;

  heater_.enable = BME68X_ENABLE;
  heater_.heatr_temp = kHeaterTemperature;
  heater_.heatr_dur = kHeaterDuration;
  PW_TRY(Check(bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heater_, &bme688_)));
  PW_TRY(Check(bme68x_set_op_mode(BME68X_FORCED_MODE, &bme688_)));

  worker_.RunOnce([this]() {
    uint32_t delay_us =
        bme68x_get_meas_dur(BME68X_FORCED_MODE, &config_, &bme688_);
    delay_us += (heater_.heatr_dur * 1000);
    auto delay = pw::chrono::SystemClock::for_at_least(
        std::chrono::microseconds(delay_us));
    get_data_.InvokeAfter(delay);
  });
  return pw::OkStatus();
}

void Bme688::GetDataCallback(pw::chrono::SystemClock::time_point) {
  bme68x_data data;
  uint8_t n;
  if (Check(bme68x_get_data(BME68X_FORCED_MODE, &data, &n, &bme688_)).ok() &&
      n != 0) {
    Update(data.temperature, data.pressure, data.humidity, data.gas_resistance);
  }
  notification_->release();
  notification_ = nullptr;
}

pw::Status Bme688::Check(int8_t result) {
  switch (result) {
    case BME68X_OK:
      return pw::OkStatus();

    case BME68X_E_NULL_PTR:
      PW_LOG_ERROR("Null pointer");
      return pw::Status::InvalidArgument();

    case BME68X_E_COM_FAIL:
      PW_LOG_ERROR("Communication failure");
      return pw::Status::Unavailable();

    case BME68X_E_INVALID_LENGTH:
      PW_LOG_ERROR("Incorrect length parameter");
      return pw::Status::OutOfRange();

    case BME68X_E_DEV_NOT_FOUND:
      PW_LOG_ERROR("Device not found");
      return pw::Status::NotFound();

    case BME68X_E_SELF_TEST:
      PW_LOG_ERROR("Self test error");
      return pw::Status::FailedPrecondition();

    case BME68X_W_NO_NEW_DATA:
      PW_LOG_WARN("No new data found");
      return pw::OkStatus();

    default:
      PW_LOG_ERROR("Unknown error code: %d", result);
      return pw::Status::Unknown();
  }
}

}  // namespace sense
