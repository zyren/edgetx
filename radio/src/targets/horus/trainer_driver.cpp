/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "stm32_pulse_driver.h"

#include "opentx.h"
#include "aux_serial_driver.h"

static_assert((TRAINER_OUT_TIMER_Channel == LL_TIM_CHANNEL_CH1 ||
               TRAINER_OUT_TIMER_Channel == LL_TIM_CHANNEL_CH2) &&
              __STM32_PULSE_IS_TIMER_CHANNEL_SUPPORTED(TRAINER_OUT_TIMER_Channel),
              "Unsupported trainer timer output channel");

static_assert(TRAINER_IN_TIMER_Channel == LL_TIM_CHANNEL_CH1 ||
              TRAINER_IN_TIMER_Channel == LL_TIM_CHANNEL_CH2,
              "Unsupported trainer timer input channel");

static void (*_trainer_timer_isr)();

void init_trainer()
{
#if defined(TRAINER_DETECT_GPIO_PIN)
  LL_GPIO_InitTypeDef pinInit;
  LL_GPIO_StructInit(&pinInit);

  pinInit.Pin = TRAINER_DETECT_GPIO_PIN;
  pinInit.Mode = LL_GPIO_MODE_INPUT;
  pinInit.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(TRAINER_DETECT_GPIO, &pinInit);
#endif  

  _trainer_timer_isr = nullptr;
}

static const stm32_pulse_timer_t trainerOutputTimer = {
  .GPIOx = TRAINER_GPIO,
  .GPIO_Pin = TRAINER_OUT_GPIO_PIN,
  .GPIO_Alternate = TRAINER_GPIO_AF,
  .TIMx = TRAINER_TIMER,
  .TIM_Prescaler = __LL_TIM_CALC_PSC(TRAINER_TIMER_FREQ, 2000000),
  .TIM_Channel = TRAINER_OUT_TIMER_Channel,
  .TIM_IRQn = TRAINER_TIMER_IRQn,
  .DMAx = nullptr,
  .DMA_Stream = 0,
  .DMA_Channel = 0,
  .DMA_IRQn = (IRQn_Type)0,
};

static void trainerSendNextFrame()
{
  stm32_pulse_set_polarity(&trainerOutputTimer, GET_TRAINER_PPM_POLARITY());
  
  // load the first period: next reload when CC compare event triggers
  trainerPulsesData.ppm.ptr = trainerPulsesData.ppm.pulses;
  TRAINER_TIMER->ARR = *(trainerPulsesData.ppm.ptr++);

  switch (trainerOutputTimer.TIM_Channel) {
  case LL_TIM_CHANNEL_CH1:
    LL_TIM_EnableIT_CC1(trainerOutputTimer.TIMx);
    break;
  case LL_TIM_CHANNEL_CH2:
    LL_TIM_EnableIT_CC2(trainerOutputTimer.TIMx);
    break;
  }
}

static void trainer_out_isr();
static void trainer_in_isr();

void init_trainer_ppm()
{
  // set proper ISR handler first
  _trainer_timer_isr = trainer_out_isr;

  stm32_pulse_init(&trainerOutputTimer);
  stm32_pulse_config_output(&trainerOutputTimer, GET_TRAINER_PPM_POLARITY(),
                            LL_TIM_OCMODE_PWM1, GET_TRAINER_PPM_DELAY() * 2);

  setupPulsesPPMTrainer();
  trainerSendNextFrame();

  LL_TIM_EnableCounter(trainerOutputTimer.TIMx);
}

void stop_trainer_ppm()
{
  stm32_pulse_deinit(&trainerOutputTimer);
  _trainer_timer_isr = nullptr;
}

static const stm32_pulse_timer_t trainerInputTimer = {
  .GPIOx = TRAINER_GPIO,
  .GPIO_Pin = TRAINER_IN_GPIO_PIN,
  .GPIO_Alternate = TRAINER_GPIO_AF,
  .TIMx = TRAINER_TIMER,
  .TIM_Prescaler = __LL_TIM_CALC_PSC(TRAINER_TIMER_FREQ, 2000000),
  .TIM_Channel = TRAINER_IN_TIMER_Channel,
  .TIM_IRQn = TRAINER_TIMER_IRQn,
  .DMAx = nullptr,
  .DMA_Stream = 0,
  .DMA_Channel = 0,
  .DMA_IRQn = (IRQn_Type)0,
};

void init_trainer_capture()
{
  // set proper ISR handler first
  _trainer_timer_isr = trainer_in_isr;

  stm32_pulse_init(&trainerInputTimer);
  stm32_pulse_config_input(&trainerInputTimer);

  switch (trainerInputTimer.TIM_Channel) {
  case LL_TIM_CHANNEL_CH1:
    LL_TIM_EnableIT_CC1(trainerInputTimer.TIMx);
    break;
  case LL_TIM_CHANNEL_CH2:
    LL_TIM_EnableIT_CC2(trainerInputTimer.TIMx);
    break;
  }

  LL_TIM_EnableCounter(trainerInputTimer.TIMx);
}

void stop_trainer_capture()
{
  stm32_pulse_deinit(&trainerInputTimer);
  _trainer_timer_isr = nullptr;
}

bool is_trainer_connected()
{
  bool set = LL_GPIO_IsInputPinSet(TRAINER_DETECT_GPIO, TRAINER_DETECT_GPIO_PIN);
#if defined(TRAINER_DETECT_INVERTED)
  return !set;
#else
  return set;
#endif
}

static inline bool trainer_check_isr_flag(const stm32_pulse_timer_t* tim)
{
  switch(tim->TIM_Channel) {
  case LL_TIM_CHANNEL_CH1:
    if (LL_TIM_IsEnabledIT_CC1(tim->TIMx) &&
        LL_TIM_IsActiveFlag_CC1(tim->TIMx)) {
      LL_TIM_ClearFlag_CC1(tim->TIMx);
      return true;
    }
    break;
  case LL_TIM_CHANNEL_CH2:
    if (LL_TIM_IsEnabledIT_CC2(tim->TIMx) &&
        LL_TIM_IsActiveFlag_CC2(tim->TIMx)) {
      LL_TIM_ClearFlag_CC2(tim->TIMx);
      return true;
    }
    break;
  }
  return false;
}

static void trainer_out_isr()
{
  // proceed only if the channel flag was set
  // and the IRQ was enabled
  if (!trainer_check_isr_flag(&trainerOutputTimer))
    return;

  if (*trainerPulsesData.ppm.ptr) {
    // load next period
    LL_TIM_SetAutoReload(trainerOutputTimer.TIMx,
                         *(trainerPulsesData.ppm.ptr++));
  } else {
    setupPulsesPPMTrainer();
    trainerSendNextFrame();
  }  
}

static void trainer_in_isr()
{
  // proceed only if the channel flag was set
  // and the IRQ was enabled
  if (!trainer_check_isr_flag(&trainerOutputTimer))
    return;

  uint16_t capture = 0;
  switch(trainerOutputTimer.TIM_Channel) {
  case LL_TIM_CHANNEL_CH1:
    capture = LL_TIM_IC_GetCaptureCH1(trainerOutputTimer.TIMx);
    break;
  case LL_TIM_CHANNEL_CH2:
    capture = LL_TIM_IC_GetCaptureCH2(trainerOutputTimer.TIMx);
    break;
  default:
    return;
  }

  // avoid spurious pulses in case the cable is not connected
  if (is_trainer_connected())
    captureTrainerPulses(capture);
}

#if !defined(TRAINER_TIMER_IRQHandler)
  #error "Missing TRAINER_TIMER_IRQHandler definition"
#endif
extern "C" void TRAINER_TIMER_IRQHandler()
{
  DEBUG_INTERRUPT(INT_TRAINER);

  if (_trainer_timer_isr)
    _trainer_timer_isr();
}
