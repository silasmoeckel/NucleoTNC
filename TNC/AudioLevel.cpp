// Copyright 2017 Rob Riggs <rob@mobilinkd.com>
// All rights reserved.

#include "AudioLevel.hpp"
#include "AudioInput.hpp"
#include "ModulatorTask.hpp"
#include "KissHardware.hpp"
#include "GPIO.hpp"
#include "Led.h"

#include "main.h"
#include "stm32l4xx_hal.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>

#include <inttypes.h>
#include <Log.h>

extern OPAMP_HandleTypeDef hopamp1;
extern DAC_HandleTypeDef hdac1;

namespace mobilinkd { namespace tnc { namespace audio {

uint16_t virtual_ground;

void setAudioPins(void) {
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void set_input_gain(int level)
{
    uint32_t dc_offset{};

    if (HAL_OPAMP_DeInit(&hopamp1) != HAL_OK)
    {
        Error_Handler();
    }


    switch (level) {
    case 0: // 0dB
        hopamp1.Init.Mode = OPAMP_FOLLOWER_MODE;
        dc_offset = 2048;
        break;
    case 1: // 6dB
        hopamp1.Init.Mode = OPAMP_PGA_MODE;
        hopamp1.Init.PgaGain = OPAMP_PGA_GAIN_2;
        dc_offset = 1024;
        break;
    case 2: // 12dB
        hopamp1.Init.Mode = OPAMP_PGA_MODE;
        hopamp1.Init.PgaGain = OPAMP_PGA_GAIN_4;
        dc_offset = 512;
        break;
    case 3: // 18dB
        hopamp1.Init.Mode = OPAMP_PGA_MODE;
        hopamp1.Init.PgaGain = OPAMP_PGA_GAIN_8;
        dc_offset = 256;
        break;
    case 4: // 24dB
        hopamp1.Init.Mode = OPAMP_PGA_MODE;
        hopamp1.Init.PgaGain = OPAMP_PGA_GAIN_16;
        dc_offset = 128;
        break;
    default:
        Error_Handler();
    }

    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, dc_offset);

    if (HAL_OPAMP_Init(&hopamp1) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_OPAMP_Start(&hopamp1);
    osDelay(100);
}

int adjust_input_gain() __attribute__((noinline));

int adjust_input_gain() {

    INFO("Adjusting input gain...");

    int gain{0};

    while (true) {
        set_input_gain(gain);

        auto [vpp, vavg, vmin, vmax] = readLevels(AUDIO_IN);

        INFO("Vpp = %" PRIu16 ", Vavg = %" PRIu16, vpp, vavg);
        INFO("Vmin = %" PRIu16 ", Vmax = %" PRIu16 ", setting = %d", vmin, vmax, gain);

        while (gain == 0 and (vmax > 4090 or vmin < 5)) {

            gpio::LED_OTHER::toggle();
            std::tie(vpp, vavg, vmin, vmax) = readLevels(AUDIO_IN);

            INFO("Vpp = %" PRIu16 ", Vavg = %" PRIu16 ", Vmin = %" PRIu16
                ", Vmax = %" PRIu16 ", setting = %d", vpp, vavg, vmin, vmax, gain);

        }
        gpio::LED_OTHER::off();

        virtual_ground = vavg;

        if (vpp > 2048) break;
        if (gain == 4) break;
        ++gain;
    }
    gpio::LED_OTHER::on();

    return gain;
}

void autoAudioInputLevel()
{

    led_dcd_on();
    led_tx_on();

    INFO("autoInputLevel");

    mobilinkd::tnc::kiss::settings().input_gain = adjust_input_gain();

    int rx_twist = readTwist() + 0.5f;
    if (rx_twist < -3) rx_twist = -3;
    else if (rx_twist > 9) rx_twist = 9;
    INFO("TWIST = %ddB", rx_twist);
    mobilinkd::tnc::kiss::settings().rx_twist = rx_twist;
    mobilinkd::tnc::kiss::settings().update_crc();

    //mobilinkd::tnc::kiss::settings().store();

    led_tx_off();
    led_dcd_off();
}

/**
 * Set the audio input levels from the values stored in EEPROM.
 */
void setAudioInputLevels()
{
    // setAudioPins();
    INFO("Setting input gain: %d", kiss::settings().input_gain);
    set_input_gain(kiss::settings().input_gain);
}

std::array<int16_t, 128> log_volume;

void init_log_volume()
{
    int16_t level = 256;
    float gain = 1.0f;
    float factor = 1.02207f;

    for (auto& i : log_volume) {
        i = int16_t(roundf(level * gain));
        gain *= factor;
    }
}

std::tuple<int16_t, int16_t> computeLogAudioLevel(int16_t level)
{
  int16_t l = level & 0x80 ? 1 : 0;
  int16_t r = log_volume[(level & 0x7F)];

  return std::make_tuple(l, r);
}

/**
 * Set the audio output level from the values stored in EEPROM.
 */
void setAudioOutputLevel()
{
  auto [l, r] = computeLogAudioLevel(kiss::settings().output_gain);

  INFO("Setting output gain: %" PRIi16 " (log %" PRIi16 " + %" PRIi16 ")", kiss::settings().output_gain, l, r);

  if (l) {
      gpio::AUDIO_OUT_ATTEN::on();
  } else {
      gpio::AUDIO_OUT_ATTEN::off();
  }
  getModulator().set_volume(r);
}

}}} // mobilinkd::tnc::audio