/*
 * Copyright (C) EdgeTx
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

#pragma once

#include <stdint.h>
#include "stm32_hal_ll.h"

#define __STM32_DMA_IS_STREAM_SUPPORTED(stream)       \
  ((stream) == LL_DMA_STREAM_1 || (stream) == LL_DMA_STREAM_3 || \
   (stream) == LL_DMA_STREAM_5 || (stream) == LL_DMA_STREAM_6 || \
   (stream) == LL_DMA_STREAM_7)

inline static bool stm32_dma_check_tc_flag(DMA_TypeDef* DMAx, uint32_t DMA_Stream)
{
  if (!LL_DMA_IsEnabledIT_TC(DMAx, DMA_Stream))
    return false;

  switch(DMA_Stream) {
  case LL_DMA_STREAM_1:
    if (!LL_DMA_IsActiveFlag_TC1(DMAx)) return false;
    LL_DMA_ClearFlag_TC1(DMAx);
    break;
  case LL_DMA_STREAM_3:
    if (!LL_DMA_IsActiveFlag_TC3(DMAx)) return false;
    LL_DMA_ClearFlag_TC3(DMAx);
    break;
  case LL_DMA_STREAM_5:
    if (!LL_DMA_IsActiveFlag_TC5(DMAx)) return false;
    LL_DMA_ClearFlag_TC5(DMAx);
    break;
  case LL_DMA_STREAM_6:
    if (!LL_DMA_IsActiveFlag_TC6(DMAx)) return false;
    LL_DMA_ClearFlag_TC6(DMAx);
    break;
  case LL_DMA_STREAM_7:
    if (!LL_DMA_IsActiveFlag_TC7(DMAx)) return false;
    LL_DMA_ClearFlag_TC7(DMAx);
    break;
  }

  return true;
}

inline static bool stm32_dma_check_ht_flag(DMA_TypeDef* DMAx, uint32_t DMA_Stream)
{
  if (!LL_DMA_IsEnabledIT_HT(DMAx, DMA_Stream))
    return false;

  switch(DMA_Stream) {
  case LL_DMA_STREAM_1:
    if (!LL_DMA_IsActiveFlag_HT1(DMAx)) return false;
    LL_DMA_ClearFlag_HT1(DMAx);
    break;
  case LL_DMA_STREAM_3:
    if (!LL_DMA_IsActiveFlag_HT3(DMAx)) return false;
    LL_DMA_ClearFlag_HT3(DMAx);
    break;
  case LL_DMA_STREAM_5:
    if (!LL_DMA_IsActiveFlag_HT5(DMAx)) return false;
    LL_DMA_ClearFlag_HT5(DMAx);
    break;
  case LL_DMA_STREAM_6:
    if (!LL_DMA_IsActiveFlag_HT6(DMAx)) return false;
    LL_DMA_ClearFlag_HT6(DMAx);
    break;
  case LL_DMA_STREAM_7:
    if (!LL_DMA_IsActiveFlag_HT7(DMAx)) return false;
    LL_DMA_ClearFlag_HT7(DMAx);
    break;
  }

  return true;
}
