/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   OpenTX - https://github.com/opentx/opentx
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
 *
 * The content of this file partially stems from STM32F4 HAL by STMicroelectronics.
 */

#include "opentx.h"
#include "i2c_driver.h"

/* I2C MSP Initialization
 * This function configures the hardware resources used in this example
 * @param hi2c: I2C handle pointer
 * @retval None
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (I2C_GPIO == GPIOA)
        __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (I2C_GPIO == GPIOB)
        __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (I2C_GPIO == GPIOC)
        __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (I2C_GPIO == GPIOH)
        __HAL_RCC_GPIOH_CLK_ENABLE();
    else
        TRACE("I2C ERROR: HAL_I2C_MspInit() I2C_GPIO misconfiguration");

    GPIO_PinAFConfig(I2C_GPIO, I2C_SCL_GPIO_PinSource, I2C_GPIO_AF);
    GPIO_PinAFConfig(I2C_GPIO, I2C_SDA_GPIO_PinSource, I2C_GPIO_AF);

    GPIO_InitStruct.GPIO_Pin = I2C_SCL_GPIO_PIN | I2C_SDA_GPIO_PIN;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(I2C_GPIO, &GPIO_InitStruct);

    /* Peripheral clock enable */
    if (I2C == I2C1)
        __HAL_RCC_I2C1_CLK_ENABLE();
    else if (I2C == I2C2)
        __HAL_RCC_I2C2_CLK_ENABLE();
    else if (I2C == I2C3)
        __HAL_RCC_I2C3_CLK_ENABLE();
    else
        TRACE("I2C ERROR: HAL_I2C_MspInit() I2C misconfiguration");
}

/* De-initializes the GPIOx peripheral registers to their default reset values.
 * @param  GPIOx where x can be (A..K) to select the GPIO peripheral for STM32F429X device or
 *                      x can be (A..I) to select the GPIO peripheral for STM32F40XX and STM32F427X devices.
 * @param  GPIO_Pin specifies the port bit to be written.
 *          This parameter can be one of GPIO_PIN_x where x can be (0..15).
 * @retval None
 */
void HAL_GPIO_DeInit(GPIO_TypeDef  *GPIOx, uint32_t GPIO_Pin)
{
  uint32_t position;
  uint32_t ioposition = 0x00U;
  uint32_t iocurrent = 0x00U;
  uint32_t tmp = 0x00U;

  /* Check the parameters */
  assert_param(IS_GPIO_ALL_INSTANCE(GPIOx));

  /* Configure the port pins */
  for(position = 0U; position < GPIO_NUMBER; position++)
  {
    /* Get the IO position */
    ioposition = 0x01U << position;
    /* Get the current IO position */
    iocurrent = (GPIO_Pin) & ioposition;

    if(iocurrent == ioposition)
    {
      /*------------------------- EXTI Mode Configuration --------------------*/
      tmp = SYSCFG->EXTICR[position >> 2U];
      tmp &= (0x0FU << (4U * (position & 0x03U)));
      if(tmp == ((uint32_t)(GPIO_GET_INDEX(GPIOx)) << (4U * (position & 0x03U))))
      {
        /* Clear EXTI line configuration */
        EXTI->IMR &= ~((uint32_t)iocurrent);
        EXTI->EMR &= ~((uint32_t)iocurrent);

        /* Clear Rising Falling edge configuration */
        EXTI->RTSR &= ~((uint32_t)iocurrent);
        EXTI->FTSR &= ~((uint32_t)iocurrent);

        /* Configure the External Interrupt or event for the current IO */
        tmp = 0x0FU << (4U * (position & 0x03U));
        SYSCFG->EXTICR[position >> 2U] &= ~tmp;
      }

      /*------------------------- GPIO Mode Configuration --------------------*/
      /* Configure IO Direction in Input Floating Mode */
      GPIOx->MODER &= ~(GPIO_MODER_MODER0 << (position * 2U));

      /* Configure the default Alternate Function in current IO */
      GPIOx->AFR[position >> 3U] &= ~(0xFU << ((uint32_t)(position & 0x07U) * 4U)) ;

      /* Deactivate the Pull-up and Pull-down resistor for the current IO */
      GPIOx->PUPDR &= ~(GPIO_PUPDR_PUPDR0 << (position * 2U));

      /* Configure the default value IO Output Type */
      GPIOx->OTYPER  &= ~(GPIO_OTYPER_OT_0 << position) ;

      /* Configure the default value for IO Speed */
      GPIOx->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR0 << (position * 2U));
    }
  }
}

/* I2C MSP De-Initialization
 * This function freeze the hardware resources used in this example
 * @param hi2c: I2C handle pointer
 * @retval None
 */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
  if(hi2c->Instance==I2C1)
  {
    __HAL_RCC_I2C1_CLK_DISABLE();
  }
  else
  {
      if(hi2c->Instance==I2C2)
      {
          __HAL_RCC_I2C2_CLK_DISABLE();
            if(hi2c->Instance==I2C3)
                __HAL_RCC_I2C3_CLK_DISABLE();
            else
                TRACE("I2C ERROR: HAL_I2C_MspDeInit() I2C misconfiguration");
      }
  }

  GPIO_InitTypeDef GPIO_InitStructure;

  /* Configure the default Alternate Function in current IO */
  GPIO_PinAFConfig(I2C_GPIO, I2C_SCL_GPIO_PinSource, 0);
  GPIO_PinAFConfig(I2C_GPIO, I2C_SDA_GPIO_PinSource, 0);

  GPIO_InitStructure.GPIO_Pin = I2C_SCL_GPIO_PIN | I2C_SDA_GPIO_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;   /* Configure a low value for IO Speed */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN; /* Configure IO Direction in Input Floating Mode */
  GPIO_InitStructure.GPIO_OType = GPIO_OType_OD; /* Leave the configuration to Open Drain */
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; /* Deactivate the Pull-up and Pull-down resistor for the current IO */
  GPIO_Init(I2C_GPIO, &GPIO_InitStructure);
}

/* Initializes the I2C according to the specified parameters
 * in the HAL_I2C_InitTypeDef and initialize the associated handle.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2C.
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *hi2c)
{
  uint32_t freqrange;
  uint32_t pclk1;

  /* Check the I2C handle allocation */
  if (hi2c == NULL)
  {
    return HAL_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_I2C_ALL_INSTANCE(hi2c->Instance));
  assert_param(IS_I2C_CLOCK_SPEED(hi2c->Init.ClockSpeed));
  assert_param(IS_I2C_DUTY_CYCLE(hi2c->Init.DutyCycle));
  assert_param(IS_I2C_OWN_ADDRESS1(hi2c->Init.OwnAddress1));
  assert_param(IS_I2C_ADDRESSING_MODE(hi2c->Init.AddressingMode));
  assert_param(IS_I2C_DUAL_ADDRESS(hi2c->Init.DualAddressMode));
  assert_param(IS_I2C_OWN_ADDRESS2(hi2c->Init.OwnAddress2));
  assert_param(IS_I2C_GENERAL_CALL(hi2c->Init.GeneralCallMode));
  assert_param(IS_I2C_NO_STRETCH(hi2c->Init.NoStretchMode));

  if (hi2c->State == HAL_I2C_STATE_RESET)
  {
    /* Allocate lock resource and initialize it */
    hi2c->Lock = HAL_UNLOCKED;

    /* Init the low level hardware : GPIO, CLOCK, NVIC */
    HAL_I2C_MspInit(hi2c);
  }

  hi2c->State = HAL_I2C_STATE_BUSY;

  /* Disable the selected I2C peripheral */
  __HAL_I2C_DISABLE(hi2c);

  /*Reset I2C*/
  hi2c->Instance->CR1 |= I2C_CR1_SWRST;
  hi2c->Instance->CR1 &= ~I2C_CR1_SWRST;

  /* Get PCLK1 frequency */
  RCC_ClocksTypeDef rccClocks;
  RCC_GetClocksFreq(&rccClocks);
  pclk1 = rccClocks.PCLK1_Frequency;

  /* Check the minimum allowed PCLK1 frequency */
  if (I2C_MIN_PCLK_FREQ(pclk1, hi2c->Init.ClockSpeed) == 1U)
  {
    return HAL_ERROR;
  }

  /* Calculate frequency range */
  freqrange = I2C_FREQRANGE(pclk1);

  /*---------------------------- I2Cx CR2 Configuration ----------------------*/
  /* Configure I2Cx: Frequency range */
  MODIFY_REG(hi2c->Instance->CR2, I2C_CR2_FREQ, freqrange);

  /*---------------------------- I2Cx TRISE Configuration --------------------*/
  /* Configure I2Cx: Rise Time */
  MODIFY_REG(hi2c->Instance->TRISE, I2C_TRISE_TRISE, I2C_RISE_TIME(freqrange, hi2c->Init.ClockSpeed));

  /*---------------------------- I2Cx CCR Configuration ----------------------*/
  /* Configure I2Cx: Speed */
  MODIFY_REG(hi2c->Instance->CCR, (I2C_CCR_FS | I2C_CCR_DUTY | I2C_CCR_CCR), I2C_SPEED(pclk1, hi2c->Init.ClockSpeed, hi2c->Init.DutyCycle));

  /*---------------------------- I2Cx CR1 Configuration ----------------------*/
  /* Configure I2Cx: Generalcall and NoStretch mode */
  MODIFY_REG(hi2c->Instance->CR1, (I2C_CR1_ENGC | I2C_CR1_NOSTRETCH), (hi2c->Init.GeneralCallMode | hi2c->Init.NoStretchMode));

  /*---------------------------- I2Cx OAR1 Configuration ---------------------*/
  /* Configure I2Cx: Own Address1 and addressing mode */
  MODIFY_REG(hi2c->Instance->OAR1, (I2C_OAR1_ADDMODE | I2C_OAR1_ADD8_9 | I2C_OAR1_ADD1_7 | I2C_OAR1_ADD0), (hi2c->Init.AddressingMode | hi2c->Init.OwnAddress1));

  /*---------------------------- I2Cx OAR2 Configuration ---------------------*/
  /* Configure I2Cx: Dual mode and Own Address2 */
  MODIFY_REG(hi2c->Instance->OAR2, (I2C_OAR2_ENDUAL | I2C_OAR2_ADD2), (hi2c->Init.DualAddressMode | hi2c->Init.OwnAddress2));

  /* Enable the selected I2C peripheral */
  __HAL_I2C_ENABLE(hi2c);

  hi2c->ErrorCode = HAL_I2C_ERROR_NONE;
  hi2c->State = HAL_I2C_STATE_READY;
  hi2c->PreviousState = I2C_STATE_NONE;
  hi2c->Mode = HAL_I2C_MODE_NONE;

  return HAL_OK;
}

/* DeInitialize the I2C peripheral.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for the specified I2C.
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *hi2c)
{
  /* Check the I2C handle allocation */
  if (hi2c == NULL)
  {
    return HAL_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_I2C_ALL_INSTANCE(hi2c->Instance));

  hi2c->State = HAL_I2C_STATE_BUSY;

  /* Disable the I2C Peripheral Clock */
  __HAL_I2C_DISABLE(hi2c);

  /* DeInit the low level hardware: GPIO, CLOCK, NVIC */
  HAL_I2C_MspDeInit(hi2c);

  hi2c->ErrorCode     = HAL_I2C_ERROR_NONE;
  hi2c->State         = HAL_I2C_STATE_RESET;
  hi2c->PreviousState = I2C_STATE_NONE;
  hi2c->Mode          = HAL_I2C_MODE_NONE;

  /* Release Lock */
  __HAL_UNLOCK(hi2c);

  return HAL_OK;
}

/* Configures I2C Analog noise filter.
 * @param  hi2c pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2Cx peripheral.
 * @param  AnalogFilter new state of the Analog filter.
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *hi2c, uint32_t AnalogFilter)
{
  /* Check the parameters */
  assert_param(IS_I2C_ALL_INSTANCE(hi2c->Instance));
  assert_param(IS_I2C_ANALOG_FILTER(AnalogFilter));

  if (hi2c->State == HAL_I2C_STATE_READY)
  {
    hi2c->State = HAL_I2C_STATE_BUSY;

    /* Disable the selected I2C peripheral */
    __HAL_I2C_DISABLE(hi2c);

    /* Reset I2Cx ANOFF bit */
    hi2c->Instance->FLTR &= ~(I2C_FLTR_ANOFF);

    /* Disable the analog filter */
    hi2c->Instance->FLTR |= AnalogFilter;

    __HAL_I2C_ENABLE(hi2c);

    hi2c->State = HAL_I2C_STATE_READY;

    return HAL_OK;
  }
  else
  {
    return HAL_BUSY;
  }
}

/* Configures I2C Digital noise filter.
 * @param  hi2c pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2Cx peripheral.
 * @param  DigitalFilter Coefficient of digital noise filter between 0x00 and 0x0F.
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *hi2c, uint32_t DigitalFilter)
{
  uint16_t tmpreg = 0;

  /* Check the parameters */
  assert_param(IS_I2C_ALL_INSTANCE(hi2c->Instance));
  assert_param(IS_I2C_DIGITAL_FILTER(DigitalFilter));

  if (hi2c->State == HAL_I2C_STATE_READY)
  {
    hi2c->State = HAL_I2C_STATE_BUSY;

    /* Disable the selected I2C peripheral */
    __HAL_I2C_DISABLE(hi2c);

    /* Get the old register value */
    tmpreg = hi2c->Instance->FLTR;

    /* Reset I2Cx DNF bit [3:0] */
    tmpreg &= ~(I2C_FLTR_DNF);

    /* Set I2Cx DNF coefficient */
    tmpreg |= DigitalFilter;

    /* Store the new register value */
    hi2c->Instance->FLTR = tmpreg;

    __HAL_I2C_ENABLE(hi2c);

    hi2c->State = HAL_I2C_STATE_READY;

    return HAL_OK;
  }
  else
  {
    return HAL_BUSY;
  }
}

/* This function handles I2C Communication Timeout.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @param  Flag specifies the I2C flag to check.
 * @param  Status The new Flag status (SET or RESET).
 * @param  Timeout Timeout duration
 * @param  Tickstart Tick start value
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_WaitOnFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Flag, FlagStatus Status, uint32_t Timeout, uint32_t Tickstart)
{
  /* Wait until flag is set */
  while (I2C_GetFlagStatus(hi2c->Instance, Flag) == Status)
  {
    /* Check for the Timeout */
    if (Timeout != HAL_MAX_DELAY)
    {
      if (((xTaskGetTickCount() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        hi2c->PreviousState     = I2C_STATE_NONE;
        hi2c->State             = HAL_I2C_STATE_READY;
        hi2c->Mode              = HAL_I2C_MODE_NONE;
        hi2c->ErrorCode         |= HAL_I2C_ERROR_TIMEOUT;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }
    }
  }
  return HAL_OK;
}

/* This function handles I2C Communication Timeout for Master addressing phase.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @param  Flag specifies the I2C flag to check.
 * @param  Timeout Timeout duration
 * @param  Tickstart Tick start value
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_WaitOnMasterAddressFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Flag, uint32_t Timeout, uint32_t Tickstart)
{
  while (I2C_GetFlagStatus(hi2c->Instance, Flag) == RESET)
  {
    if (I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_AF) == SET)
    {
      /* Generate Stop */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);

      /* Clear AF Flag */
      __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF);

      hi2c->PreviousState       = I2C_STATE_NONE;
      hi2c->State               = HAL_I2C_STATE_READY;
      hi2c->Mode                = HAL_I2C_MODE_NONE;
      hi2c->ErrorCode           |= HAL_I2C_ERROR_AF;

      /* Process Unlocked */
      __HAL_UNLOCK(hi2c);

      return HAL_ERROR;
    }

    /* Check for the Timeout */
    if (Timeout != HAL_MAX_DELAY)
    {
      if (((xTaskGetTickCount() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        hi2c->PreviousState       = I2C_STATE_NONE;
        hi2c->State               = HAL_I2C_STATE_READY;
        hi2c->Mode                = HAL_I2C_MODE_NONE;
        hi2c->ErrorCode           |= HAL_I2C_ERROR_TIMEOUT;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }
    }
  }
  return HAL_OK;
}

/* hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @param  DevAddress Target device address: The device 7 bits address value
 *         in datasheet must be shifted to the left before calling the interface
 * @param  Timeout Timeout duration
 * @param  Tickstart Tick start value
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_MasterRequestWrite(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint32_t Timeout, uint32_t Tickstart)
{
  /* Declaration of temporary variable to prevent undefined behavior of volatile usage */
  uint32_t CurrentXferOptions = hi2c->XferOptions;

  /* Generate Start condition if first transfer */
  if ((CurrentXferOptions == I2C_FIRST_AND_LAST_FRAME) || (CurrentXferOptions == I2C_FIRST_FRAME) || (CurrentXferOptions == I2C_NO_OPTION_FRAME))
  {
    /* Generate Start */
    SET_BIT(hi2c->Instance->CR1, I2C_CR1_START);
  }
  else if (hi2c->PreviousState == I2C_STATE_MASTER_BUSY_RX)
  {
    /* Generate ReStart */
    SET_BIT(hi2c->Instance->CR1, I2C_CR1_START);
  }
  else
  {
    /* Do nothing */
  }

  /* Wait until SB flag is set */
  if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_SB, RESET, Timeout, Tickstart) != HAL_OK)
  {
    if (READ_BIT(hi2c->Instance->CR1, I2C_CR1_START) == I2C_CR1_START)
    {
      hi2c->ErrorCode = HAL_I2C_WRONG_START;
    }
    return HAL_TIMEOUT;
  }

  if (hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_7BIT)
  {
    /* Send slave address */
    hi2c->Instance->DR = I2C_7BIT_ADD_WRITE(DevAddress);
  }
  else
  {
    /* Send header of slave address */
    hi2c->Instance->DR = I2C_10BIT_HEADER_WRITE(DevAddress);

    /* Wait until ADD10 flag is set */
    if (I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADD10, Timeout, Tickstart) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* Send slave address */
    hi2c->Instance->DR = I2C_10BIT_ADDRESS(DevAddress);
  }

  /* Wait until ADDR flag is set */
  if (I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, Timeout, Tickstart) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* This function handles Acknowledge failed detection during an I2C Communication.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2C.
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_IsAcknowledgeFailed(I2C_HandleTypeDef *hi2c)
{
  if ( I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_AF) == SET)
  {
    /* Clear NACKF Flag */
    __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF);

    hi2c->PreviousState       = I2C_STATE_NONE;
    hi2c->State               = HAL_I2C_STATE_READY;
    hi2c->Mode                = HAL_I2C_MODE_NONE;
    hi2c->ErrorCode           |= HAL_I2C_ERROR_AF;

    /* Process Unlocked */
    __HAL_UNLOCK(hi2c);

    return HAL_ERROR;
  }
  return HAL_OK;
}

/* This function handles I2C Communication Timeout for specific usage of TXE flag.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2C.
 * @param  Timeout Timeout duration
 * @param  Tickstart Tick start value
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_WaitOnTXEFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  while ( I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_TXE) == RESET)
  {
    /* Check if a NACK is detected */
    if (I2C_IsAcknowledgeFailed(hi2c) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* Check for the Timeout */
    if (Timeout != HAL_MAX_DELAY)
    {
      if (((xTaskGetTickCount() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        hi2c->PreviousState       = I2C_STATE_NONE;
        hi2c->State               = HAL_I2C_STATE_READY;
        hi2c->Mode                = HAL_I2C_MODE_NONE;
        hi2c->ErrorCode           |= HAL_I2C_ERROR_TIMEOUT;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }
    }
  }
  return HAL_OK;
}

/* This function handles I2C Communication Timeout for specific usage of BTF flag.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2C.
 * @param  Timeout Timeout duration
 * @param  Tickstart Tick start value
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_WaitOnBTFFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  while ( I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_BTF) == RESET)
  {
    /* Check if a NACK is detected */
    if (I2C_IsAcknowledgeFailed(hi2c) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* Check for the Timeout */
    if (Timeout != HAL_MAX_DELAY)
    {
      if (((xTaskGetTickCount() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        hi2c->PreviousState       = I2C_STATE_NONE;
        hi2c->State               = HAL_I2C_STATE_READY;
        hi2c->Mode                = HAL_I2C_MODE_NONE;
        hi2c->ErrorCode           |= HAL_I2C_ERROR_TIMEOUT;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }
    }
  }
  return HAL_OK;
}

/* Transmits in master mode an amount of data in blocking mode.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2C.
 * @param  DevAddress Target device address: The device 7 bits address value
 *         in datasheet must be shifted to the left before calling the interface
 * @param  pData Pointer to data buffer
 * @param  Size Amount of data to be sent
 * @param  Timeout Timeout duration
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    /* Init tickstart for timeout management*/
     uint32_t tickstart = xTaskGetTickCount();

     if (hi2c->State == HAL_I2C_STATE_READY)
     {
       /* Wait until BUSY flag is reset */
       if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BUSY, SET, I2C_TIMEOUT_BUSY_FLAG, tickstart) != HAL_OK)
       {
         return HAL_BUSY;
       }

       /* Process Locked */
       __HAL_LOCK(hi2c);

       /* Check if the I2C is already enabled */
       if ((hi2c->Instance->CR1 & I2C_CR1_PE) != I2C_CR1_PE)
       {
         /* Enable I2C peripheral */
         __HAL_I2C_ENABLE(hi2c);
       }

       /* Disable Pos */
       CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_POS);

       hi2c->State       = HAL_I2C_STATE_BUSY_TX;
       hi2c->Mode        = HAL_I2C_MODE_MASTER;
       hi2c->ErrorCode   = HAL_I2C_ERROR_NONE;

       /* Prepare transfer parameters */
       hi2c->pBuffPtr    = pData;
       hi2c->XferCount   = Size;
       hi2c->XferSize    = hi2c->XferCount;
       hi2c->XferOptions = I2C_NO_OPTION_FRAME;

       /* Send Slave Address */
       if (I2C_MasterRequestWrite(hi2c, DevAddress, Timeout, tickstart) != HAL_OK)
       {
         return HAL_ERROR;
       }

       /* Clear ADDR flag */
       __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

       while (hi2c->XferSize > 0U)
       {
         /* Wait until TXE flag is set */
         if (I2C_WaitOnTXEFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)
         {
           if (hi2c->ErrorCode == HAL_I2C_ERROR_AF)
           {
             /* Generate Stop */
             SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
           }
           return HAL_ERROR;
         }

         /* Write data to DR */
         hi2c->Instance->DR = *hi2c->pBuffPtr;

         /* Increment Buffer pointer */
         hi2c->pBuffPtr++;

         /* Update counter */
         hi2c->XferCount--;
         hi2c->XferSize--;

         if (( I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_BTF) == SET) && (hi2c->XferSize != 0U))
         {
           /* Write data to DR */
           hi2c->Instance->DR = *hi2c->pBuffPtr;

           /* Increment Buffer pointer */
           hi2c->pBuffPtr++;

           /* Update counter */
           hi2c->XferCount--;
           hi2c->XferSize--;
         }

         /* Wait until BTF flag is set */
         if (I2C_WaitOnBTFFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)
         {
           if (hi2c->ErrorCode == HAL_I2C_ERROR_AF)
           {
             /* Generate Stop */
             SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
           }
           return HAL_ERROR;
         }
       }

       /* Generate Stop */
       SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);

       hi2c->State = HAL_I2C_STATE_READY;
       hi2c->Mode = HAL_I2C_MODE_NONE;

       /* Process Unlocked */
       __HAL_UNLOCK(hi2c);

       return HAL_OK;
     }
     else
     {
       return HAL_BUSY;
     }
}

/* Master sends target device address for read request.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @param  DevAddress Target device address: The device 7 bits address value
 *         in datasheet must be shifted to the left before calling the interface
 * @param  Timeout Timeout duration
 * @param  Tickstart Tick start value
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_MasterRequestRead(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint32_t Timeout, uint32_t Tickstart)
{
  /* Declaration of temporary variable to prevent undefined behavior of volatile usage */
  uint32_t CurrentXferOptions = hi2c->XferOptions;

  /* Enable Acknowledge */
  SET_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

  /* Generate Start condition if first transfer */
  if ((CurrentXferOptions == I2C_FIRST_AND_LAST_FRAME) || (CurrentXferOptions == I2C_FIRST_FRAME)  || (CurrentXferOptions == I2C_NO_OPTION_FRAME))
  {
    /* Generate Start */
    SET_BIT(hi2c->Instance->CR1, I2C_CR1_START);
  }
  else if (hi2c->PreviousState == I2C_STATE_MASTER_BUSY_TX)
  {
    /* Generate ReStart */
    SET_BIT(hi2c->Instance->CR1, I2C_CR1_START);
  }
  else
  {
    /* Do nothing */
  }

  /* Wait until SB flag is set */
  if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_SB, RESET, Timeout, Tickstart) != HAL_OK)
  {
    if (READ_BIT(hi2c->Instance->CR1, I2C_CR1_START) == I2C_CR1_START)
    {
      hi2c->ErrorCode = HAL_I2C_WRONG_START;
    }
    return HAL_TIMEOUT;
  }

  if (hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_7BIT)
  {
    /* Send slave address */
    hi2c->Instance->DR = I2C_7BIT_ADD_READ(DevAddress);
  }
  else
  {
    /* Send header of slave address */
    hi2c->Instance->DR = I2C_10BIT_HEADER_WRITE(DevAddress);

    /* Wait until ADD10 flag is set */
    if (I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADD10, Timeout, Tickstart) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* Send slave address */
    hi2c->Instance->DR = I2C_10BIT_ADDRESS(DevAddress);

    /* Wait until ADDR flag is set */
    if (I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, Timeout, Tickstart) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* Clear ADDR flag */
    __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

    /* Generate Restart */
    SET_BIT(hi2c->Instance->CR1, I2C_CR1_START);

    /* Wait until SB flag is set */
    if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_SB, RESET, Timeout, Tickstart) != HAL_OK)
    {
      if (READ_BIT(hi2c->Instance->CR1, I2C_CR1_START) == I2C_CR1_START)
      {
        hi2c->ErrorCode = HAL_I2C_WRONG_START;
      }
      return HAL_TIMEOUT;
    }

    /* Send header of slave address */
    hi2c->Instance->DR = I2C_10BIT_HEADER_READ(DevAddress);
  }

  /* Wait until ADDR flag is set */
  if (I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, Timeout, Tickstart) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* This function handles I2C Communication Timeout for specific usage of RXNE flag.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2C.
 * @param  Timeout Timeout duration
 * @param  Tickstart Tick start value
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_WaitOnRXNEFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{

  while ( I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_RXNE) == RESET)
  {
    /* Check if a STOPF is detected */
    if ( I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_STOPF) == SET)
    {
      /* Clear STOP Flag */
      __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);

      hi2c->PreviousState       = I2C_STATE_NONE;
      hi2c->State               = HAL_I2C_STATE_READY;
      hi2c->Mode                = HAL_I2C_MODE_NONE;
      hi2c->ErrorCode           |= HAL_I2C_ERROR_NONE;

      /* Process Unlocked */
      __HAL_UNLOCK(hi2c);

      return HAL_ERROR;
    }

    /* Check for the Timeout */
    if (((xTaskGetTickCount() - Tickstart) > Timeout) || (Timeout == 0U))
    {
      hi2c->PreviousState       = I2C_STATE_NONE;
      hi2c->State               = HAL_I2C_STATE_READY;
      hi2c->Mode                = HAL_I2C_MODE_NONE;
      hi2c->ErrorCode           |= HAL_I2C_ERROR_TIMEOUT;

      /* Process Unlocked */
      __HAL_UNLOCK(hi2c);

      return HAL_ERROR;
    }
  }
  return HAL_OK;
}

/* Receives in master mode an amount of data in blocking mode.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2C.
 * @param  DevAddress Target device address: The device 7 bits address value
 *         in datasheet must be shifted to the left before calling the interface
 * @param  pData Pointer to data buffer
 * @param  Size Amount of data to be sent
 * @param  Timeout Timeout duration
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
      /* Init tickstart for timeout management*/
      uint32_t tickstart = xTaskGetTickCount();

      if (hi2c->State == HAL_I2C_STATE_READY)
      {
        /* Wait until BUSY flag is reset */
        if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BUSY, SET, I2C_TIMEOUT_BUSY_FLAG, tickstart) != HAL_OK)
        {
          return HAL_BUSY;
        }

        /* Process Locked */
        __HAL_LOCK(hi2c);

        /* Check if the I2C is already enabled */
        if ((hi2c->Instance->CR1 & I2C_CR1_PE) != I2C_CR1_PE)
        {
          /* Enable I2C peripheral */
          __HAL_I2C_ENABLE(hi2c);
        }

        /* Disable Pos */
        CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_POS);

        hi2c->State       = HAL_I2C_STATE_BUSY_RX;
        hi2c->Mode        = HAL_I2C_MODE_MASTER;
        hi2c->ErrorCode   = HAL_I2C_ERROR_NONE;

        /* Prepare transfer parameters */
        hi2c->pBuffPtr    = pData;
        hi2c->XferCount   = Size;
        hi2c->XferSize    = hi2c->XferCount;
        hi2c->XferOptions = I2C_NO_OPTION_FRAME;

        /* Send Slave Address */
        if (I2C_MasterRequestRead(hi2c, DevAddress, Timeout, tickstart) != HAL_OK)
        {
          return HAL_ERROR;
        }

        if (hi2c->XferSize == 0U)
        {
          /* Clear ADDR flag */
          __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

          /* Generate Stop */
          SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
        }
        else if (hi2c->XferSize == 1U)
        {
          /* Disable Acknowledge */
          CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

          /* Clear ADDR flag */
          __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

          /* Generate Stop */
          SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
        }
        else if (hi2c->XferSize == 2U)
        {
          /* Disable Acknowledge */
          CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

          /* Enable Pos */
          SET_BIT(hi2c->Instance->CR1, I2C_CR1_POS);

          /* Clear ADDR flag */
          __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
        }
        else
        {
          /* Enable Acknowledge */
          SET_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

          /* Clear ADDR flag */
          __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
        }

        while (hi2c->XferSize > 0U)
        {
          if (hi2c->XferSize <= 3U)
          {
            /* One byte */
            if (hi2c->XferSize == 1U)
            {
              /* Wait until RXNE flag is set */
              if (I2C_WaitOnRXNEFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)
              {
                return HAL_ERROR;
              }

              /* Read data from DR */
              *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

              /* Increment Buffer pointer */
              hi2c->pBuffPtr++;

              /* Update counter */
              hi2c->XferSize--;
              hi2c->XferCount--;
            }
            /* Two bytes */
            else if (hi2c->XferSize == 2U)
            {
              /* Wait until BTF flag is set */
              if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BTF, RESET, Timeout, tickstart) != HAL_OK)
              {
                return HAL_ERROR;
              }

              /* Generate Stop */
              SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);

              /* Read data from DR */
              *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

              /* Increment Buffer pointer */
              hi2c->pBuffPtr++;

              /* Update counter */
              hi2c->XferSize--;
              hi2c->XferCount--;

              /* Read data from DR */
              *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

              /* Increment Buffer pointer */
              hi2c->pBuffPtr++;

              /* Update counter */
              hi2c->XferSize--;
              hi2c->XferCount--;
            }
            /* 3 Last bytes */
            else
            {
              /* Wait until BTF flag is set */
              if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BTF, RESET, Timeout, tickstart) != HAL_OK)
              {
                return HAL_ERROR;
              }

              /* Disable Acknowledge */
              CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

              /* Read data from DR */
              *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

              /* Increment Buffer pointer */
              hi2c->pBuffPtr++;

              /* Update counter */
              hi2c->XferSize--;
              hi2c->XferCount--;

              /* Wait until BTF flag is set */
              if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BTF, RESET, Timeout, tickstart) != HAL_OK)
              {
                return HAL_ERROR;
              }

              /* Generate Stop */
              SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);

              /* Read data from DR */
              *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

              /* Increment Buffer pointer */
              hi2c->pBuffPtr++;

              /* Update counter */
              hi2c->XferSize--;
              hi2c->XferCount--;

              /* Read data from DR */
              *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

              /* Increment Buffer pointer */
              hi2c->pBuffPtr++;

              /* Update counter */
              hi2c->XferSize--;
              hi2c->XferCount--;
            }
          }
          else
          {
            /* Wait until RXNE flag is set */
            if (I2C_WaitOnRXNEFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)
            {
              return HAL_ERROR;
            }

            /* Read data from DR */
            *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

            /* Increment Buffer pointer */
            hi2c->pBuffPtr++;

            /* Update counter */
            hi2c->XferSize--;
            hi2c->XferCount--;

            if ( I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_BTF) == SET)
            {
              /* Read data from DR */
              *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

              /* Increment Buffer pointer */
              hi2c->pBuffPtr++;

              /* Update counter */
              hi2c->XferSize--;
              hi2c->XferCount--;
            }
          }
        }

        hi2c->State = HAL_I2C_STATE_READY;
        hi2c->Mode = HAL_I2C_MODE_NONE;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_OK;
      }
      else
      {
        return HAL_BUSY;
      }
}

/* Slave Tx Transfer completed callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
__weak void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_SlaveTxCpltCallback could be implemented in the user file
   */
}

/* Slave Rx Transfer completed callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
__weak void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_SlaveRxCpltCallback could be implemented in the user file
   */
}

/* I2C error callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
__weak void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_ErrorCallback could be implemented in the user file
   */
}

/* Memory Tx Transfer completed callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
__weak void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_MemTxCpltCallback could be implemented in the user file
   */
}

/* Memory Rx Transfer completed callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
__weak void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_MemRxCpltCallback could be implemented in the user file
   */
}

/* Master Rx Transfer completed callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
__weak void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_MasterRxCpltCallback could be implemented in the user file
   */
}

/* I2C abort callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
__weak void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_AbortCpltCallback could be implemented in the user file
   */
}

/* DMA I2C process complete callback.
 * @param  hdma DMA handle
 * @retval None
 */
static void I2C_DMAXferCplt(DMA_HandleTypeDef *hdma)
{
  I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent; /* Derogation MISRAC2012-Rule-11.5 */

  /* Declaration of temporary variable to prevent undefined behavior of volatile usage */
  HAL_I2C_StateTypeDef CurrentState = hi2c->State;
  HAL_I2C_ModeTypeDef CurrentMode   = hi2c->Mode;
  uint32_t CurrentXferOptions       = hi2c->XferOptions;

  /* Disable EVT and ERR interrupt */
  __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_ERR);

  /* Clear Complete callback */
  if (hi2c->hdmatx != NULL)
  {
    hi2c->hdmatx->XferCpltCallback = NULL;
  }
  if (hi2c->hdmarx != NULL)
  {
    hi2c->hdmarx->XferCpltCallback = NULL;
  }

  if ((((uint32_t)CurrentState & (uint32_t)HAL_I2C_STATE_BUSY_TX) == (uint32_t)HAL_I2C_STATE_BUSY_TX) || ((((uint32_t)CurrentState & (uint32_t)HAL_I2C_STATE_BUSY_RX) == (uint32_t)HAL_I2C_STATE_BUSY_RX) && (CurrentMode == HAL_I2C_MODE_SLAVE)))
  {
    /* Disable DMA Request */
    CLEAR_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN);

    hi2c->XferCount = 0U;

    if (CurrentState == HAL_I2C_STATE_BUSY_TX_LISTEN)
    {
      /* Set state at HAL_I2C_STATE_LISTEN */
      hi2c->PreviousState = I2C_STATE_SLAVE_BUSY_TX;
      hi2c->State = HAL_I2C_STATE_LISTEN;

      /* Call the corresponding callback to inform upper layer of End of Transfer */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->SlaveTxCpltCallback(hi2c);
#else
      HAL_I2C_SlaveTxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
    else if (CurrentState == HAL_I2C_STATE_BUSY_RX_LISTEN)
    {
      /* Set state at HAL_I2C_STATE_LISTEN */
      hi2c->PreviousState = I2C_STATE_SLAVE_BUSY_RX;
      hi2c->State = HAL_I2C_STATE_LISTEN;

      /* Call the corresponding callback to inform upper layer of End of Transfer */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->SlaveRxCpltCallback(hi2c);
#else
      HAL_I2C_SlaveRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
    else
    {
      /* Do nothing */
    }

    /* Enable EVT and ERR interrupt to treat end of transfer in IRQ handler */
    __HAL_I2C_ENABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_ERR);
  }
  /* Check current Mode, in case of treatment DMA handler have been preempted by a prior interrupt */
  else if (hi2c->Mode != HAL_I2C_MODE_NONE)
  {
    if (hi2c->XferCount == (uint16_t)1)
    {
      /* Disable Acknowledge */
      CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);
    }

    /* Disable EVT and ERR interrupt */
    __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_ERR);

    /* Prepare next transfer or stop current transfer */
    if ((CurrentXferOptions == I2C_NO_OPTION_FRAME) || (CurrentXferOptions == I2C_FIRST_AND_LAST_FRAME) || (CurrentXferOptions == I2C_OTHER_AND_LAST_FRAME) || (CurrentXferOptions == I2C_LAST_FRAME))
    {
      /* Generate Stop */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
    }

    /* Disable Last DMA */
    CLEAR_BIT(hi2c->Instance->CR2, I2C_CR2_LAST);

    /* Disable DMA Request */
    CLEAR_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN);

    hi2c->XferCount = 0U;

    /* Check if Errors has been detected during transfer */
    if (hi2c->ErrorCode != HAL_I2C_ERROR_NONE)
    {
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->ErrorCallback(hi2c);
#else
      HAL_I2C_ErrorCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
    else
    {
      hi2c->State = HAL_I2C_STATE_READY;

      if (hi2c->Mode == HAL_I2C_MODE_MEM)
      {
        hi2c->Mode = HAL_I2C_MODE_NONE;
        hi2c->PreviousState = I2C_STATE_NONE;

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
        hi2c->MemRxCpltCallback(hi2c);
#else
        HAL_I2C_MemRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
      }
      else
      {
        hi2c->Mode = HAL_I2C_MODE_NONE;
        hi2c->PreviousState = I2C_STATE_MASTER_BUSY_RX;

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
        hi2c->MasterRxCpltCallback(hi2c);
#else
        HAL_I2C_MasterRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
      }
    }
  }
  else
  {
    /* Do nothing */
  }
}

/* Return the DMA error code
 * @param  hdma  pointer to a DMA_HandleTypeDef structure that contains
 *              the configuration information for the specified DMA Stream.
 * @retval DMA Error Code
 */
uint32_t HAL_DMA_GetError(DMA_HandleTypeDef *hdma)
{
  return hdma->ErrorCode;
}

/* DMA I2C communication error callback.
 * @param  hdma DMA handle
 * @retval None
 */
static void I2C_DMAError(DMA_HandleTypeDef *hdma)
{
  I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent; /* Derogation MISRAC2012-Rule-11.5 */

  /* Clear Complete callback */
  if (hi2c->hdmatx != NULL)
  {
    hi2c->hdmatx->XferCpltCallback = NULL;
  }
  if (hi2c->hdmarx != NULL)
  {
    hi2c->hdmarx->XferCpltCallback = NULL;
  }

  /* Ignore DMA FIFO error */
  if (HAL_DMA_GetError(hdma) != HAL_DMA_ERROR_FE)
  {
    /* Disable Acknowledge */
    hi2c->Instance->CR1 &= ~I2C_CR1_ACK;

    hi2c->XferCount = 0U;

    hi2c->State = HAL_I2C_STATE_READY;
    hi2c->Mode = HAL_I2C_MODE_NONE;

    hi2c->ErrorCode |= HAL_I2C_ERROR_DMA;

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
    hi2c->ErrorCallback(hi2c);
#else
    HAL_I2C_ErrorCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
  }
}

/* Sets the DMA Transfer parameter.
 * @param  hdma       pointer to a DMA_HandleTypeDef structure that contains
 *                     the configuration information for the specified DMA Stream.
 * @param  SrcAddress The source memory Buffer address
 * @param  DstAddress The destination memory Buffer address
 * @param  DataLength The length of data to be transferred from source to destination
 * @retval HAL status
 */
static void DMA_SetConfig(DMA_HandleTypeDef *hdma, uint32_t SrcAddress, uint32_t DstAddress, uint32_t DataLength)
{
  /* Clear DBM bit */
  hdma->Instance->CR &= (uint32_t)(~DMA_SxCR_DBM);

  /* Configure DMA Stream data length */
  hdma->Instance->NDTR = DataLength;

  /* Memory to Peripheral */
  if((hdma->Init.DMA_DIR) == DMA_MEMORY_TO_PERIPH)
  {
    /* Configure DMA Stream destination address */
    hdma->Instance->PAR = DstAddress;

    /* Configure DMA Stream source address */
    hdma->Instance->M0AR = SrcAddress;
  }
  /* Peripheral to Memory */
  else
  {
    /* Configure DMA Stream source address */
    hdma->Instance->PAR = SrcAddress;

    /* Configure DMA Stream destination address */
    hdma->Instance->M0AR = DstAddress;
  }
}

/* Start the DMA Transfer with interrupt enabled.
 * @param  hdma       pointer to a DMA_HandleTypeDef structure that contains
 *                     the configuration information for the specified DMA Stream.
 * @param  SrcAddress The source memory Buffer address
 * @param  DstAddress The destination memory Buffer address
 * @param  DataLength The length of data to be transferred from source to destination
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_DMA_Start_IT(DMA_HandleTypeDef *hdma, uint32_t SrcAddress, uint32_t DstAddress, uint32_t DataLength)
{
  HAL_StatusTypeDef status = HAL_OK;

  /* calculate DMA base and stream number */
  DMA_Base_Registers *regs = (DMA_Base_Registers *)hdma->StreamBaseAddress;

  /* Check the parameters */
  assert_param(IS_DMA_BUFFER_SIZE(DataLength));

  /* Process locked */
  __HAL_LOCK(hdma);

  if(HAL_DMA_STATE_READY == hdma->State)
  {
    /* Change DMA peripheral state */
    hdma->State = HAL_DMA_STATE_BUSY;

    /* Initialize the error code */
    hdma->ErrorCode = HAL_DMA_ERROR_NONE;

    /* Configure the source, destination address and the data length */
    DMA_SetConfig(hdma, SrcAddress, DstAddress, DataLength);

    /* Clear all interrupt flags at correct offset within the register */
    regs->IFCR = 0x3FU << hdma->StreamIndex;

    /* Enable Common interrupts*/
    hdma->Instance->CR  |= DMA_IT_TC | DMA_IT_TE | DMA_IT_DME;

    if(hdma->XferHalfCpltCallback != NULL)
    {
      hdma->Instance->CR  |= DMA_IT_HT;
    }

    /* Enable the Peripheral */
    __HAL_DMA_ENABLE(hdma);
  }
  else
  {
    /* Process unlocked */
    __HAL_UNLOCK(hdma);

    /* Return error status */
    status = HAL_BUSY;
  }

  return status;
}

/* Master sends target device address followed by internal memory address for read request.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @param  DevAddress Target device address: The device 7 bits address value
 *         in datasheet must be shifted to the left before calling the interface
 * @param  MemAddress Internal memory address
 * @param  MemAddSize Size of internal memory address
 * @param  Timeout Timeout duration
 * @param  Tickstart Tick start value
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_RequestMemoryRead(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint32_t Timeout, uint32_t Tickstart)
{
  /* Enable Acknowledge */
  SET_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

  /* Generate Start */
  SET_BIT(hi2c->Instance->CR1, I2C_CR1_START);

  /* Wait until SB flag is set */
  if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_SB, RESET, Timeout, Tickstart) != HAL_OK)
  {
    if (READ_BIT(hi2c->Instance->CR1, I2C_CR1_START) == I2C_CR1_START)
    {
      hi2c->ErrorCode = HAL_I2C_WRONG_START;
    }
    return HAL_TIMEOUT;
  }

  /* Send slave address */
  hi2c->Instance->DR = I2C_7BIT_ADD_WRITE(DevAddress);

  /* Wait until ADDR flag is set */
  if (I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, Timeout, Tickstart) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* Clear ADDR flag */
  __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

  /* Wait until TXE flag is set */
  if (I2C_WaitOnTXEFlagUntilTimeout(hi2c, Timeout, Tickstart) != HAL_OK)
  {
    if (hi2c->ErrorCode == HAL_I2C_ERROR_AF)
    {
      /* Generate Stop */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
    }
    return HAL_ERROR;
  }

  /* If Memory address size is 8Bit */
  if (MemAddSize == I2C_MEMADD_SIZE_8BIT)
  {
    /* Send Memory Address */
    hi2c->Instance->DR = I2C_MEM_ADD_LSB(MemAddress);
  }
  /* If Memory address size is 16Bit */
  else
  {
    /* Send MSB of Memory Address */
    hi2c->Instance->DR = I2C_MEM_ADD_MSB(MemAddress);

    /* Wait until TXE flag is set */
    if (I2C_WaitOnTXEFlagUntilTimeout(hi2c, Timeout, Tickstart) != HAL_OK)
    {
      if (hi2c->ErrorCode == HAL_I2C_ERROR_AF)
      {
        /* Generate Stop */
        SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
      }
      return HAL_ERROR;
    }

    /* Send LSB of Memory Address */
    hi2c->Instance->DR = I2C_MEM_ADD_LSB(MemAddress);
  }

  /* Wait until TXE flag is set */
  if (I2C_WaitOnTXEFlagUntilTimeout(hi2c, Timeout, Tickstart) != HAL_OK)
  {
    if (hi2c->ErrorCode == HAL_I2C_ERROR_AF)
    {
      /* Generate Stop */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
    }
    return HAL_ERROR;
  }

  /* Generate Restart */
  SET_BIT(hi2c->Instance->CR1, I2C_CR1_START);

  /* Wait until SB flag is set */
  if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_SB, RESET, Timeout, Tickstart) != HAL_OK)
  {
    if (READ_BIT(hi2c->Instance->CR1, I2C_CR1_START) == I2C_CR1_START)
    {
      hi2c->ErrorCode = HAL_I2C_WRONG_START;
    }
    return HAL_TIMEOUT;
  }

  /* Send slave address */
  hi2c->Instance->DR = I2C_7BIT_ADD_READ(DevAddress);

  /* Wait until ADDR flag is set */
  if (I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, Timeout, Tickstart) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* Aborts the DMA Transfer in Interrupt mode.
 * @param  hdma   pointer to a DMA_HandleTypeDef structure that contains
 *                 the configuration information for the specified DMA Stream.
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_DMA_Abort_IT(DMA_HandleTypeDef *hdma)
{
  if(hdma->State != HAL_DMA_STATE_BUSY)
  {
    hdma->ErrorCode = HAL_DMA_ERROR_NO_XFER;
    return HAL_ERROR;
  }
  else
  {
    /* Set Abort State  */
    hdma->State = HAL_DMA_STATE_ABORT;

    /* Disable the stream */
    __HAL_DMA_DISABLE(hdma);
  }

  return HAL_OK;
}

/* Master sends target device address followed by internal memory address for write request.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @param  DevAddress Target device address: The device 7 bits address value
 *         in datasheet must be shifted to the left before calling the interface
 * @param  MemAddress Internal memory address
 * @param  MemAddSize Size of internal memory address
 * @param  Timeout Timeout duration
 * @param  Tickstart Tick start value
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_RequestMemoryWrite(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint32_t Timeout, uint32_t Tickstart)
{
  /* Generate Start */
  SET_BIT(hi2c->Instance->CR1, I2C_CR1_START);

  /* Wait until SB flag is set */
  if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_SB, RESET, Timeout, Tickstart) != HAL_OK)
  {
    if (READ_BIT(hi2c->Instance->CR1, I2C_CR1_START) == I2C_CR1_START)
    {
      hi2c->ErrorCode = HAL_I2C_WRONG_START;
    }
    return HAL_TIMEOUT;
  }

  /* Send slave address */
  hi2c->Instance->DR = I2C_7BIT_ADD_WRITE(DevAddress);

  /* Wait until ADDR flag is set */
  if (I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, Timeout, Tickstart) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* Clear ADDR flag */
  __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

  /* Wait until TXE flag is set */
  if (I2C_WaitOnTXEFlagUntilTimeout(hi2c, Timeout, Tickstart) != HAL_OK)
  {
    if (hi2c->ErrorCode == HAL_I2C_ERROR_AF)
    {
      /* Generate Stop */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
    }
    return HAL_ERROR;
  }

  /* If Memory address size is 8Bit */
  if (MemAddSize == I2C_MEMADD_SIZE_8BIT)
  {
    /* Send Memory Address */
    hi2c->Instance->DR = I2C_MEM_ADD_LSB(MemAddress);
  }
  /* If Memory address size is 16Bit */
  else
  {
    /* Send MSB of Memory Address */
    hi2c->Instance->DR = I2C_MEM_ADD_MSB(MemAddress);

    /* Wait until TXE flag is set */
    if (I2C_WaitOnTXEFlagUntilTimeout(hi2c, Timeout, Tickstart) != HAL_OK)
    {
      if (hi2c->ErrorCode == HAL_I2C_ERROR_AF)
      {
        /* Generate Stop */
        SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
      }
      return HAL_ERROR;
    }

    /* Send LSB of Memory Address */
    hi2c->Instance->DR = I2C_MEM_ADD_LSB(MemAddress);
  }

  return HAL_OK;
}

/* Write an amount of data in non-blocking mode with DMA to a specific memory address
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @param  DevAddress Target device address: The device 7 bits address value
 *         in datasheet must be shifted to the left before calling the interface
 * @param  MemAddress Internal memory address
 * @param  MemAddSize Size of internal memory address
 * @param  pData Pointer to data buffer
 * @param  Size Amount of data to be sent
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_I2C_Mem_Write_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size)
{
  __IO uint32_t count = 0U;
  HAL_StatusTypeDef dmaxferstatus;

  /* Init tickstart for timeout management*/
  uint32_t tickstart = xTaskGetTickCount();

  /* Check the parameters */
  assert_param(IS_I2C_MEMADD_SIZE(MemAddSize));

  if (hi2c->State == HAL_I2C_STATE_READY)
  {
    /* Wait until BUSY flag is reset */
    count = I2C_TIMEOUT_BUSY_FLAG * (SystemCoreClock / 25U / 1000U);
    do
    {
      count--;
      if (count == 0U)
      {
        hi2c->PreviousState       = I2C_STATE_NONE;
        hi2c->State               = HAL_I2C_STATE_READY;
        hi2c->Mode                = HAL_I2C_MODE_NONE;
        hi2c->ErrorCode           |= HAL_I2C_ERROR_TIMEOUT;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }
    }
    while (I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_BUSY) != RESET);

    /* Process Locked */
    __HAL_LOCK(hi2c);

    /* Check if the I2C is already enabled */
    if ((hi2c->Instance->CR1 & I2C_CR1_PE) != I2C_CR1_PE)
    {
      /* Enable I2C peripheral */
      __HAL_I2C_ENABLE(hi2c);
    }

    /* Disable Pos */
    CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_POS);

    hi2c->State     = HAL_I2C_STATE_BUSY_TX;
    hi2c->Mode      = HAL_I2C_MODE_MEM;
    hi2c->ErrorCode = HAL_I2C_ERROR_NONE;

    /* Prepare transfer parameters */
    hi2c->pBuffPtr    = pData;
    hi2c->XferCount   = Size;
    hi2c->XferSize    = hi2c->XferCount;
    hi2c->XferOptions = I2C_NO_OPTION_FRAME;

    if (hi2c->XferSize > 0U)
    {
      if (hi2c->hdmatx != NULL)
      {
        /* Set the I2C DMA transfer complete callback */
        hi2c->hdmatx->XferCpltCallback = I2C_DMAXferCplt;

        /* Set the DMA error callback */
        hi2c->hdmatx->XferErrorCallback = I2C_DMAError;

        /* Set the unused DMA callbacks to NULL */
        hi2c->hdmatx->XferHalfCpltCallback = NULL;
        hi2c->hdmatx->XferM1CpltCallback = NULL;
        hi2c->hdmatx->XferM1HalfCpltCallback = NULL;
        hi2c->hdmatx->XferAbortCallback = NULL;

        /* Enable the DMA stream */
        dmaxferstatus = HAL_DMA_Start_IT(hi2c->hdmatx, (uint32_t)hi2c->pBuffPtr, (uint32_t)&hi2c->Instance->DR, hi2c->XferSize);
      }
      else
      {
        /* Update I2C state */
        hi2c->State     = HAL_I2C_STATE_READY;
        hi2c->Mode      = HAL_I2C_MODE_NONE;

        /* Update I2C error code */
        hi2c->ErrorCode |= HAL_I2C_ERROR_DMA_PARAM;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }

      if (dmaxferstatus == HAL_OK)
      {
        /* Send Slave Address and Memory Address */
        if (I2C_RequestMemoryWrite(hi2c, DevAddress, MemAddress, MemAddSize, I2C_TIMEOUT_FLAG, tickstart) != HAL_OK)
        {
          /* Abort the ongoing DMA */
          dmaxferstatus = HAL_DMA_Abort_IT(hi2c->hdmatx);

          /* Prevent unused argument(s) compilation and MISRA warning */
          UNUSED(dmaxferstatus);

          /* Set the unused I2C DMA transfer complete callback to NULL */
          hi2c->hdmatx->XferCpltCallback = NULL;

          /* Disable Acknowledge */
          CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

          hi2c->XferSize = 0U;
          hi2c->XferCount = 0U;

          /* Disable I2C peripheral to prevent dummy data in buffer */
          __HAL_I2C_DISABLE(hi2c);

          return HAL_ERROR;
        }

        /* Clear ADDR flag */
        __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        /* Note : The I2C interrupts must be enabled after unlocking current process
        to avoid the risk of I2C interrupt handle execution before current
        process unlock */
        /* Enable ERR interrupt */
        __HAL_I2C_ENABLE_IT(hi2c, I2C_IT_ERR);

        /* Enable DMA Request */
        SET_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN);

        return HAL_OK;
      }
      else
      {
        /* Update I2C state */
        hi2c->State     = HAL_I2C_STATE_READY;
        hi2c->Mode      = HAL_I2C_MODE_NONE;

        /* Update I2C error code */
        hi2c->ErrorCode |= HAL_I2C_ERROR_DMA;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }
    }
    else
    {
      /* Update I2C state */
      hi2c->State     = HAL_I2C_STATE_READY;
      hi2c->Mode      = HAL_I2C_MODE_NONE;

      /* Update I2C error code */
      hi2c->ErrorCode |= HAL_I2C_ERROR_SIZE;

      /* Process Unlocked */
      __HAL_UNLOCK(hi2c);

      return HAL_ERROR;
    }
  }
  else
  {
    return HAL_BUSY;
  }
}

/* Reads an amount of data in non-blocking mode with DMA from a specific memory address.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *              the configuration information for the specified I2C.
 * @param  DevAddress Target device address: The device 7 bits address value
 *         in datasheet must be shifted to the left before calling the interface
 * @param  MemAddress Internal memory address
 * @param  MemAddSize Size of internal memory address
 * @param  pData Pointer to data buffer
 * @param  Size Amount of data to be read
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_I2C_Mem_Read_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size)
{
  /* Init tickstart for timeout management*/
  uint32_t tickstart = xTaskGetTickCount();
  __IO uint32_t count = 0U;
  HAL_StatusTypeDef dmaxferstatus;

  /* Check the parameters */
  assert_param(IS_I2C_MEMADD_SIZE(MemAddSize));

  if (hi2c->State == HAL_I2C_STATE_READY)
  {
    /* Wait until BUSY flag is reset */
    count = I2C_TIMEOUT_BUSY_FLAG * (SystemCoreClock / 25U / 1000U);
    do
    {
      count--;
      if (count == 0U)
      {
        hi2c->PreviousState       = I2C_STATE_NONE;
        hi2c->State               = HAL_I2C_STATE_READY;
        hi2c->Mode                = HAL_I2C_MODE_NONE;
        hi2c->ErrorCode           |= HAL_I2C_ERROR_TIMEOUT;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }
    }
    while (I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_BUSY) != RESET);

    /* Process Locked */
    __HAL_LOCK(hi2c);

    /* Check if the I2C is already enabled */
    if ((hi2c->Instance->CR1 & I2C_CR1_PE) != I2C_CR1_PE)
    {
      /* Enable I2C peripheral */
      __HAL_I2C_ENABLE(hi2c);
    }

    /* Disable Pos */
    CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_POS);

    hi2c->State     = HAL_I2C_STATE_BUSY_RX;
    hi2c->Mode      = HAL_I2C_MODE_MEM;
    hi2c->ErrorCode = HAL_I2C_ERROR_NONE;

    /* Prepare transfer parameters */
    hi2c->pBuffPtr    = pData;
    hi2c->XferCount   = Size;
    hi2c->XferSize    = hi2c->XferCount;
    hi2c->XferOptions = I2C_NO_OPTION_FRAME;

    if (hi2c->XferSize > 0U)
    {
      if (hi2c->hdmarx != NULL)
      {
        /* Set the I2C DMA transfer complete callback */
        hi2c->hdmarx->XferCpltCallback = I2C_DMAXferCplt;

        /* Set the DMA error callback */
        hi2c->hdmarx->XferErrorCallback = I2C_DMAError;

        /* Set the unused DMA callbacks to NULL */
        hi2c->hdmarx->XferHalfCpltCallback = NULL;
        hi2c->hdmarx->XferM1CpltCallback = NULL;
        hi2c->hdmarx->XferM1HalfCpltCallback = NULL;
        hi2c->hdmarx->XferAbortCallback = NULL;

        /* Enable the DMA stream */
        dmaxferstatus = HAL_DMA_Start_IT(hi2c->hdmarx, (uint32_t)&hi2c->Instance->DR, (uint32_t)hi2c->pBuffPtr, hi2c->XferSize);
      }
      else
      {
        /* Update I2C state */
        hi2c->State     = HAL_I2C_STATE_READY;
        hi2c->Mode      = HAL_I2C_MODE_NONE;

        /* Update I2C error code */
        hi2c->ErrorCode |= HAL_I2C_ERROR_DMA_PARAM;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }

      if (dmaxferstatus == HAL_OK)
      {
        /* Send Slave Address and Memory Address */
        if (I2C_RequestMemoryRead(hi2c, DevAddress, MemAddress, MemAddSize, I2C_TIMEOUT_FLAG, tickstart) != HAL_OK)
        {
          /* Abort the ongoing DMA */
          dmaxferstatus = HAL_DMA_Abort_IT(hi2c->hdmarx);

          /* Prevent unused argument(s) compilation and MISRA warning */
          UNUSED(dmaxferstatus);

          /* Set the unused I2C DMA transfer complete callback to NULL */
          hi2c->hdmarx->XferCpltCallback = NULL;

          /* Disable Acknowledge */
          CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

          hi2c->XferSize = 0U;
          hi2c->XferCount = 0U;

          /* Disable I2C peripheral to prevent dummy data in buffer */
          __HAL_I2C_DISABLE(hi2c);

          return HAL_ERROR;
        }

        if (hi2c->XferSize == 1U)
        {
          /* Disable Acknowledge */
          CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);
        }
        else
        {
          /* Enable Last DMA bit */
          SET_BIT(hi2c->Instance->CR2, I2C_CR2_LAST);
        }

        /* Clear ADDR flag */
        __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        /* Note : The I2C interrupts must be enabled after unlocking current process
        to avoid the risk of I2C interrupt handle execution before current
        process unlock */
        /* Enable ERR interrupt */
        __HAL_I2C_ENABLE_IT(hi2c, I2C_IT_ERR);

        /* Enable DMA Request */
        hi2c->Instance->CR2 |= I2C_CR2_DMAEN;
      }
      else
      {
        /* Update I2C state */
        hi2c->State     = HAL_I2C_STATE_READY;
        hi2c->Mode      = HAL_I2C_MODE_NONE;

        /* Update I2C error code */
        hi2c->ErrorCode |= HAL_I2C_ERROR_DMA;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_ERROR;
      }
    }
    else
    {
      /* Send Slave Address and Memory Address */
      if (I2C_RequestMemoryRead(hi2c, DevAddress, MemAddress, MemAddSize, I2C_TIMEOUT_FLAG, tickstart) != HAL_OK)
      {
        return HAL_ERROR;
      }

      /* Clear ADDR flag */
      __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

      /* Generate Stop */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);

      hi2c->State = HAL_I2C_STATE_READY;

      /* Process Unlocked */
      __HAL_UNLOCK(hi2c);
    }

    return HAL_OK;
  }
  else
  {
    return HAL_BUSY;
  }
}

/* Convert I2Cx OTHER_xxx XferOptions to functional XferOptions.
 * @param  hi2c I2C handle.
 * @retval None
 */
static void I2C_ConvertOtherXferOptions(I2C_HandleTypeDef *hi2c)
{
  /* if user set XferOptions to I2C_OTHER_FRAME            */
  /* it request implicitly to generate a restart condition */
  /* set XferOptions to I2C_FIRST_FRAME                    */
  if (hi2c->XferOptions == I2C_OTHER_FRAME)
  {
    hi2c->XferOptions = I2C_FIRST_FRAME;
  }
  /* else if user set XferOptions to I2C_OTHER_AND_LAST_FRAME */
  /* it request implicitly to generate a restart condition    */
  /* then generate a stop condition at the end of transfer    */
  /* set XferOptions to I2C_FIRST_AND_LAST_FRAME              */
  else if (hi2c->XferOptions == I2C_OTHER_AND_LAST_FRAME)
  {
    hi2c->XferOptions = I2C_FIRST_AND_LAST_FRAME;
  }
  else
  {
    /* Nothing to do */
  }
}

/* Handle SB flag for Master
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_Master_SB(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Mode == HAL_I2C_MODE_MEM)
  {
    if (hi2c->EventCount == 0U)
    {
      /* Send slave address */
      hi2c->Instance->DR = I2C_7BIT_ADD_WRITE(hi2c->Devaddress);
    }
    else
    {
      hi2c->Instance->DR = I2C_7BIT_ADD_READ(hi2c->Devaddress);
    }
  }
  else
  {
    if (hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_7BIT)
    {
      /* Send slave 7 Bits address */
      if (hi2c->State == HAL_I2C_STATE_BUSY_TX)
      {
        hi2c->Instance->DR = I2C_7BIT_ADD_WRITE(hi2c->Devaddress);
      }
      else
      {
        hi2c->Instance->DR = I2C_7BIT_ADD_READ(hi2c->Devaddress);
      }

      if (((hi2c->hdmatx != NULL) && (hi2c->hdmatx->XferCpltCallback != NULL))
          || ((hi2c->hdmarx != NULL) && (hi2c->hdmarx->XferCpltCallback != NULL)))
      {
        /* Enable DMA Request */
        SET_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN);
      }
    }
    else
    {
      if (hi2c->EventCount == 0U)
      {
        /* Send header of slave address */
        hi2c->Instance->DR = I2C_10BIT_HEADER_WRITE(hi2c->Devaddress);
      }
      else if (hi2c->EventCount == 1U)
      {
        /* Send header of slave address */
        hi2c->Instance->DR = I2C_10BIT_HEADER_READ(hi2c->Devaddress);
      }
      else
      {
        /* Do nothing */
      }
    }
  }
}

/* Handle ADD10 flag for Master
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_Master_ADD10(I2C_HandleTypeDef *hi2c)
{
  /* Send slave address */
  hi2c->Instance->DR = I2C_10BIT_ADDRESS(hi2c->Devaddress);

  if (((hi2c->hdmatx != NULL) && (hi2c->hdmatx->XferCpltCallback != NULL))
      || ((hi2c->hdmarx != NULL) && (hi2c->hdmarx->XferCpltCallback != NULL)))
  {
    /* Enable DMA Request */
    SET_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN);
  }
}

/* Handle ADDR flag for Master
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_Master_ADDR(I2C_HandleTypeDef *hi2c)
{
  /* Declaration of temporary variable to prevent undefined behavior of volatile usage */
  HAL_I2C_ModeTypeDef CurrentMode       = hi2c->Mode;
  uint32_t CurrentXferOptions           = hi2c->XferOptions;
  uint32_t Prev_State                   = hi2c->PreviousState;

  if (hi2c->State == HAL_I2C_STATE_BUSY_RX)
  {
    if ((hi2c->EventCount == 0U) && (CurrentMode == HAL_I2C_MODE_MEM))
    {
      /* Clear ADDR flag */
      __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
    }
    else if ((hi2c->EventCount == 0U) && (hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_10BIT))
    {
      /* Clear ADDR flag */
      __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

      /* Generate Restart */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_START);

      hi2c->EventCount++;
    }
    else
    {
      if (hi2c->XferCount == 0U)
      {
        /* Clear ADDR flag */
        __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

        /* Generate Stop */
        SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
      }
      else if (hi2c->XferCount == 1U)
      {
        if (CurrentXferOptions == I2C_NO_OPTION_FRAME)
        {
          /* Disable Acknowledge */
          CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

          if ((hi2c->Instance->CR2 & I2C_CR2_DMAEN) == I2C_CR2_DMAEN)
          {
            /* Disable Acknowledge */
            CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

            /* Clear ADDR flag */
            __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
          }
          else
          {
            /* Clear ADDR flag */
            __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

            /* Generate Stop */
            SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
          }
        }
        /* Prepare next transfer or stop current transfer */
        else if ((CurrentXferOptions != I2C_FIRST_AND_LAST_FRAME) && (CurrentXferOptions != I2C_LAST_FRAME) \
                 && ((Prev_State != I2C_STATE_MASTER_BUSY_RX) || (CurrentXferOptions == I2C_FIRST_FRAME)))
        {
          if ((CurrentXferOptions != I2C_NEXT_FRAME) && (CurrentXferOptions != I2C_FIRST_AND_NEXT_FRAME) && (CurrentXferOptions != I2C_LAST_FRAME_NO_STOP))
          {
            /* Disable Acknowledge */
            CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);
          }
          else
          {
            /* Enable Acknowledge */
            SET_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);
          }

          /* Clear ADDR flag */
          __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
        }
        else
        {
          /* Disable Acknowledge */
          CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

          /* Clear ADDR flag */
          __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

          /* Generate Stop */
          SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
        }
      }
      else if (hi2c->XferCount == 2U)
      {
        if ((CurrentXferOptions != I2C_NEXT_FRAME) && (CurrentXferOptions != I2C_FIRST_AND_NEXT_FRAME) && (CurrentXferOptions != I2C_LAST_FRAME_NO_STOP))
        {
          /* Disable Acknowledge */
          CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

          /* Enable Pos */
          SET_BIT(hi2c->Instance->CR1, I2C_CR1_POS);
        }
        else
        {
          /* Enable Acknowledge */
          SET_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);
        }

        if (((hi2c->Instance->CR2 & I2C_CR2_DMAEN) == I2C_CR2_DMAEN) && ((CurrentXferOptions == I2C_NO_OPTION_FRAME) || (CurrentXferOptions == I2C_FIRST_FRAME) || (CurrentXferOptions == I2C_FIRST_AND_LAST_FRAME) || (CurrentXferOptions == I2C_LAST_FRAME_NO_STOP) || (CurrentXferOptions == I2C_LAST_FRAME)))
        {
          /* Enable Last DMA bit */
          SET_BIT(hi2c->Instance->CR2, I2C_CR2_LAST);
        }

        /* Clear ADDR flag */
        __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
      }
      else
      {
        /* Enable Acknowledge */
        SET_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

        if (((hi2c->Instance->CR2 & I2C_CR2_DMAEN) == I2C_CR2_DMAEN) && ((CurrentXferOptions == I2C_NO_OPTION_FRAME) || (CurrentXferOptions == I2C_FIRST_FRAME) || (CurrentXferOptions == I2C_FIRST_AND_LAST_FRAME) || (CurrentXferOptions == I2C_LAST_FRAME_NO_STOP) || (CurrentXferOptions == I2C_LAST_FRAME)))
        {
          /* Enable Last DMA bit */
          SET_BIT(hi2c->Instance->CR2, I2C_CR2_LAST);
        }

        /* Clear ADDR flag */
        __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
      }

      /* Reset Event counter  */
      hi2c->EventCount = 0U;
    }
  }
  else
  {
    /* Clear ADDR flag */
    __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
  }
}

/* Master Tx Transfer completed callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
__weak void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_MasterTxCpltCallback could be implemented in the user file
   */
}

/* Slave Address Match callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @param  TransferDirection Master request Transfer Direction (Write/Read), value of @ref I2C_XferDirection_definition
 * @param  AddrMatchCode Address Match Code
 * @retval None
 */
__weak void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);
  UNUSED(TransferDirection);
  UNUSED(AddrMatchCode);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_AddrCallback() could be implemented in the user file
   */
}

/* Listen Complete callback.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
__weak void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hi2c);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_I2C_ListenCpltCallback() could be implemented in the user file
  */
}

/* Handle TXE and BTF flag for Memory transmitter
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_MemoryTransmit_TXE_BTF(I2C_HandleTypeDef *hi2c)
{
  /* Declaration of temporary variables to prevent undefined behavior of volatile usage */
  HAL_I2C_StateTypeDef CurrentState = hi2c->State;

  if (hi2c->EventCount == 0U)
  {
    /* If Memory address size is 8Bit */
    if (hi2c->MemaddSize == I2C_MEMADD_SIZE_8BIT)
    {
      /* Send Memory Address */
      hi2c->Instance->DR = I2C_MEM_ADD_LSB(hi2c->Memaddress);

      hi2c->EventCount += 2U;
    }
    /* If Memory address size is 16Bit */
    else
    {
      /* Send MSB of Memory Address */
      hi2c->Instance->DR = I2C_MEM_ADD_MSB(hi2c->Memaddress);

      hi2c->EventCount++;
    }
  }
  else if (hi2c->EventCount == 1U)
  {
    /* Send LSB of Memory Address */
    hi2c->Instance->DR = I2C_MEM_ADD_LSB(hi2c->Memaddress);

    hi2c->EventCount++;
  }
  else if (hi2c->EventCount == 2U)
  {
    if (CurrentState == HAL_I2C_STATE_BUSY_RX)
    {
      /* Generate Restart */
      hi2c->Instance->CR1 |= I2C_CR1_START;
    }
    else if ((hi2c->XferCount > 0U) && (CurrentState == HAL_I2C_STATE_BUSY_TX))
    {
      /* Write data to DR */
      hi2c->Instance->DR = *hi2c->pBuffPtr;

      /* Increment Buffer pointer */
      hi2c->pBuffPtr++;

      /* Update counter */
      hi2c->XferCount--;
    }
    else if ((hi2c->XferCount == 0U) && (CurrentState == HAL_I2C_STATE_BUSY_TX))
    {
      /* Generate Stop condition then Call TxCpltCallback() */
      /* Disable EVT, BUF and ERR interrupt */
      __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);

      /* Generate Stop */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);

      hi2c->PreviousState = I2C_STATE_NONE;
      hi2c->State = HAL_I2C_STATE_READY;
      hi2c->Mode = HAL_I2C_MODE_NONE;
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->MemTxCpltCallback(hi2c);
#else
      HAL_I2C_MemTxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
    else
    {
      /* Do nothing */
    }
  }
  else
  {
    /* Do nothing */
  }
}

/* Handle TXE flag for Master
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_MasterTransmit_TXE(I2C_HandleTypeDef *hi2c)
{
  /* Declaration of temporary variables to prevent undefined behavior of volatile usage */
  HAL_I2C_StateTypeDef CurrentState = hi2c->State;
  HAL_I2C_ModeTypeDef CurrentMode   = hi2c->Mode;
  uint32_t CurrentXferOptions       = hi2c->XferOptions;

  if ((hi2c->XferSize == 0U) && (CurrentState == HAL_I2C_STATE_BUSY_TX))
  {
    /* Call TxCpltCallback() directly if no stop mode is set */
    if ((CurrentXferOptions != I2C_FIRST_AND_LAST_FRAME) && (CurrentXferOptions != I2C_LAST_FRAME) && (CurrentXferOptions != I2C_NO_OPTION_FRAME))
    {
      __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);

      hi2c->PreviousState = I2C_STATE_MASTER_BUSY_TX;
      hi2c->Mode = HAL_I2C_MODE_NONE;
      hi2c->State = HAL_I2C_STATE_READY;

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->MasterTxCpltCallback(hi2c);
#else
      HAL_I2C_MasterTxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
    else /* Generate Stop condition then Call TxCpltCallback() */
    {
      /* Disable EVT, BUF and ERR interrupt */
      __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);

      /* Generate Stop */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);

      hi2c->PreviousState = I2C_STATE_NONE;
      hi2c->State = HAL_I2C_STATE_READY;

      if (hi2c->Mode == HAL_I2C_MODE_MEM)
      {
        hi2c->Mode = HAL_I2C_MODE_NONE;
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
        hi2c->MemTxCpltCallback(hi2c);
#else
        HAL_I2C_MemTxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
      }
      else
      {
        hi2c->Mode = HAL_I2C_MODE_NONE;
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
        hi2c->MasterTxCpltCallback(hi2c);
#else
        HAL_I2C_MasterTxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
      }
    }
  }
  else if ((CurrentState == HAL_I2C_STATE_BUSY_TX) || \
           ((CurrentMode == HAL_I2C_MODE_MEM) && (CurrentState == HAL_I2C_STATE_BUSY_RX)))
  {
    if (hi2c->XferCount == 0U)
    {
      /* Disable BUF interrupt */
      __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_BUF);
    }
    else
    {
      if (hi2c->Mode == HAL_I2C_MODE_MEM)
      {
        I2C_MemoryTransmit_TXE_BTF(hi2c);
      }
      else
      {
        /* Write data to DR */
        hi2c->Instance->DR = *hi2c->pBuffPtr;

        /* Increment Buffer pointer */
        hi2c->pBuffPtr++;

        /* Update counter */
        hi2c->XferCount--;
      }
    }
  }
  else
  {
    /* Do nothing */
  }
}

/* Handle BTF flag for Master transmitter
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_MasterTransmit_BTF(I2C_HandleTypeDef *hi2c)
{
  /* Declaration of temporary variables to prevent undefined behavior of volatile usage */
  uint32_t CurrentXferOptions = hi2c->XferOptions;

  if (hi2c->State == HAL_I2C_STATE_BUSY_TX)
  {
    if (hi2c->XferCount != 0U)
    {
      /* Write data to DR */
      hi2c->Instance->DR = *hi2c->pBuffPtr;

      /* Increment Buffer pointer */
      hi2c->pBuffPtr++;

      /* Update counter */
      hi2c->XferCount--;
    }
    else
    {
      /* Call TxCpltCallback() directly if no stop mode is set */
      if ((CurrentXferOptions != I2C_FIRST_AND_LAST_FRAME) && (CurrentXferOptions != I2C_LAST_FRAME) && (CurrentXferOptions != I2C_NO_OPTION_FRAME))
      {
        __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);

        hi2c->PreviousState = I2C_STATE_MASTER_BUSY_TX;
        hi2c->Mode = HAL_I2C_MODE_NONE;
        hi2c->State = HAL_I2C_STATE_READY;

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
        hi2c->MasterTxCpltCallback(hi2c);
#else
        HAL_I2C_MasterTxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
      }
      else /* Generate Stop condition then Call TxCpltCallback() */
      {
        /* Disable EVT, BUF and ERR interrupt */
        __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);

        /* Generate Stop */
        SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);

        hi2c->PreviousState = I2C_STATE_NONE;
        hi2c->State = HAL_I2C_STATE_READY;
        hi2c->Mode = HAL_I2C_MODE_NONE;

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
        hi2c->MasterTxCpltCallback(hi2c);
#else
        HAL_I2C_MasterTxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
      }
    }
  }
  else
  {
    /* Do nothing */
  }
}

/* This function handles I2C Communication Timeout for specific usage of STOP request through Interrupt.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval HAL status
 */
static HAL_StatusTypeDef I2C_WaitOnSTOPRequestThroughIT(I2C_HandleTypeDef *hi2c)
{
  __IO uint32_t count = 0U;

  /* Wait until STOP flag is reset */
  count = I2C_TIMEOUT_STOP_FLAG * (SystemCoreClock / 25U / 1000U);
  do
  {
    count--;
    if (count == 0U)
    {
      hi2c->ErrorCode           |= HAL_I2C_ERROR_TIMEOUT;

      return HAL_ERROR;
    }
  }
  while (READ_BIT(hi2c->Instance->CR1, I2C_CR1_STOP) == I2C_CR1_STOP);

  return HAL_OK;
}

/* Handle RXNE flag for Master
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_MasterReceive_RXNE(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->State == HAL_I2C_STATE_BUSY_RX)
  {
    uint32_t tmp;

    tmp = hi2c->XferCount;
    if (tmp > 3U)
    {
      /* Read data from DR */
      *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

      /* Increment Buffer pointer */
      hi2c->pBuffPtr++;

      /* Update counter */
      hi2c->XferCount--;

      if (hi2c->XferCount == (uint16_t)3)
      {
        /* Disable BUF interrupt, this help to treat correctly the last 4 bytes
        on BTF subroutine */
        /* Disable BUF interrupt */
        __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_BUF);
      }
    }
    else if ((hi2c->XferOptions != I2C_FIRST_AND_NEXT_FRAME) && ((tmp == 1U) || (tmp == 0U)))
    {
      if (I2C_WaitOnSTOPRequestThroughIT(hi2c) == HAL_OK)
      {
        /* Disable Acknowledge */
        CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

        /* Disable EVT, BUF and ERR interrupt */
        __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);

        /* Read data from DR */
        *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

        /* Increment Buffer pointer */
        hi2c->pBuffPtr++;

        /* Update counter */
        hi2c->XferCount--;

        hi2c->State = HAL_I2C_STATE_READY;

        if (hi2c->Mode == HAL_I2C_MODE_MEM)
        {
          hi2c->Mode = HAL_I2C_MODE_NONE;
          hi2c->PreviousState = I2C_STATE_NONE;

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
          hi2c->MemRxCpltCallback(hi2c);
#else
          HAL_I2C_MemRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
        }
        else
        {
          hi2c->Mode = HAL_I2C_MODE_NONE;
          hi2c->PreviousState = I2C_STATE_MASTER_BUSY_RX;

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
          hi2c->MasterRxCpltCallback(hi2c);
#else
          HAL_I2C_MasterRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
        }
      }
      else
      {
        /* Disable EVT, BUF and ERR interrupt */
        __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);

        /* Read data from DR */
        *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

        /* Increment Buffer pointer */
        hi2c->pBuffPtr++;

        /* Update counter */
        hi2c->XferCount--;

        hi2c->State = HAL_I2C_STATE_READY;
        hi2c->Mode = HAL_I2C_MODE_NONE;

        /* Call user error callback */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
        hi2c->ErrorCallback(hi2c);
#else
        HAL_I2C_ErrorCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
      }
    }
    else
    {
      /* Do nothing */
    }
  }
}

/* Handle BTF flag for Master receiver
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_MasterReceive_BTF(I2C_HandleTypeDef *hi2c)
{
  /* Declaration of temporary variables to prevent undefined behavior of volatile usage */
  uint32_t CurrentXferOptions = hi2c->XferOptions;

  if (hi2c->XferCount == 4U)
  {
    /* Disable BUF interrupt, this help to treat correctly the last 2 bytes
       on BTF subroutine if there is a reception delay between N-1 and N byte */
    __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_BUF);

    /* Read data from DR */
    *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

    /* Increment Buffer pointer */
    hi2c->pBuffPtr++;

    /* Update counter */
    hi2c->XferCount--;
  }
  else if (hi2c->XferCount == 3U)
  {
    /* Disable BUF interrupt, this help to treat correctly the last 2 bytes
       on BTF subroutine if there is a reception delay between N-1 and N byte */
    __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_BUF);

    if ((CurrentXferOptions != I2C_NEXT_FRAME) && (CurrentXferOptions != I2C_FIRST_AND_NEXT_FRAME))
    {
      /* Disable Acknowledge */
      CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);
    }

    /* Read data from DR */
    *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

    /* Increment Buffer pointer */
    hi2c->pBuffPtr++;

    /* Update counter */
    hi2c->XferCount--;
  }
  else if (hi2c->XferCount == 2U)
  {
    /* Prepare next transfer or stop current transfer */
    if ((CurrentXferOptions == I2C_FIRST_FRAME) || (CurrentXferOptions == I2C_LAST_FRAME_NO_STOP))
    {
      /* Disable Acknowledge */
      CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);
    }
    else if ((CurrentXferOptions == I2C_NEXT_FRAME) || (CurrentXferOptions == I2C_FIRST_AND_NEXT_FRAME))
    {
      /* Enable Acknowledge */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);
    }
    else if (CurrentXferOptions != I2C_LAST_FRAME_NO_STOP)
    {
      /* Generate Stop */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_STOP);
    }
    else
    {
      /* Do nothing */
    }

    /* Read data from DR */
    *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

    /* Increment Buffer pointer */
    hi2c->pBuffPtr++;

    /* Update counter */
    hi2c->XferCount--;

    /* Read data from DR */
    *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

    /* Increment Buffer pointer */
    hi2c->pBuffPtr++;

    /* Update counter */
    hi2c->XferCount--;

    /* Disable EVT and ERR interrupt */
    __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_ERR);

    hi2c->State = HAL_I2C_STATE_READY;
    if (hi2c->Mode == HAL_I2C_MODE_MEM)
    {
      hi2c->Mode = HAL_I2C_MODE_NONE;
      hi2c->PreviousState = I2C_STATE_NONE;
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->MemRxCpltCallback(hi2c);
#else
      HAL_I2C_MemRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
    else
    {
      hi2c->Mode = HAL_I2C_MODE_NONE;
      hi2c->PreviousState = I2C_STATE_MASTER_BUSY_RX;
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->MasterRxCpltCallback(hi2c);
#else
      HAL_I2C_MasterRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
  }
  else
  {
    /* Read data from DR */
    *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

    /* Increment Buffer pointer */
    hi2c->pBuffPtr++;

    /* Update counter */
    hi2c->XferCount--;
  }
}

/* Handle ADD flag for Slave
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @param  IT2Flags Interrupt2 flags to handle.
 * @retval None
 */
static void I2C_Slave_ADDR(I2C_HandleTypeDef *hi2c, uint32_t IT2Flags)
{
  uint8_t TransferDirection = I2C_DIRECTION_RECEIVE;
  uint16_t SlaveAddrCode;

  if (((uint32_t)hi2c->State & (uint32_t)HAL_I2C_STATE_LISTEN) == (uint32_t)HAL_I2C_STATE_LISTEN)
  {
    /* Disable BUF interrupt, BUF enabling is manage through slave specific interface */
    __HAL_I2C_DISABLE_IT(hi2c, (I2C_IT_BUF));

    /* Transfer Direction requested by Master */
    if (I2C_CHECK_FLAG(IT2Flags, I2C_FLAG_TRA) == RESET)
    {
      TransferDirection = I2C_DIRECTION_TRANSMIT;
    }

    if (I2C_CHECK_FLAG(IT2Flags, I2C_FLAG_DUALF) == RESET)
    {
      SlaveAddrCode = (uint16_t)hi2c->Init.OwnAddress1;
    }
    else
    {
      SlaveAddrCode = (uint16_t)hi2c->Init.OwnAddress2;
    }

    /* Process Unlocked */
    __HAL_UNLOCK(hi2c);

    /* Call Slave Addr callback */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
    hi2c->AddrCallback(hi2c, TransferDirection, SlaveAddrCode);
#else
    HAL_I2C_AddrCallback(hi2c, TransferDirection, SlaveAddrCode);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
  }
  else
  {
    /* Clear ADDR flag */
    __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_ADDR);

    /* Process Unlocked */
    __HAL_UNLOCK(hi2c);
  }
}

/* DMA I2C communication abort callback
 *        (To be called at end of DMA Abort procedure).
 * @param hdma DMA handle.
 * @retval None
 */
static void I2C_DMAAbort(DMA_HandleTypeDef *hdma)
{
  __IO uint32_t count = 0U;
  I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent; /* Derogation MISRAC2012-Rule-11.5 */

  /* Declaration of temporary variable to prevent undefined behavior of volatile usage */
  HAL_I2C_StateTypeDef CurrentState = hi2c->State;

  /* During abort treatment, check that there is no pending STOP request */
  /* Wait until STOP flag is reset */
  count = I2C_TIMEOUT_FLAG * (SystemCoreClock / 25U / 1000U);
  do
  {
    if (count == 0U)
    {
      hi2c->ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
      break;
    }
    count--;
  }
  while (READ_BIT(hi2c->Instance->CR1, I2C_CR1_STOP) == I2C_CR1_STOP);

  /* Clear Complete callback */
  if (hi2c->hdmatx != NULL)
  {
    hi2c->hdmatx->XferCpltCallback = NULL;
  }
  if (hi2c->hdmarx != NULL)
  {
    hi2c->hdmarx->XferCpltCallback = NULL;
  }

  /* Disable Acknowledge */
  CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

  hi2c->XferCount = 0U;

  /* Reset XferAbortCallback */
  if (hi2c->hdmatx != NULL)
  {
    hi2c->hdmatx->XferAbortCallback = NULL;
  }
  if (hi2c->hdmarx != NULL)
  {
    hi2c->hdmarx->XferAbortCallback = NULL;
  }

  /* Disable I2C peripheral to prevent dummy data in buffer */
  __HAL_I2C_DISABLE(hi2c);

  /* Check if come from abort from user */
  if (hi2c->State == HAL_I2C_STATE_ABORT)
  {
    hi2c->State         = HAL_I2C_STATE_READY;
    hi2c->Mode          = HAL_I2C_MODE_NONE;
    hi2c->ErrorCode     = HAL_I2C_ERROR_NONE;

    /* Call the corresponding callback to inform upper layer of End of Transfer */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
    hi2c->AbortCpltCallback(hi2c);
#else
    HAL_I2C_AbortCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
  }
  else
  {
    if (((uint32_t)CurrentState & (uint32_t)HAL_I2C_STATE_LISTEN) == (uint32_t)HAL_I2C_STATE_LISTEN)
    {
      /* Renable I2C peripheral */
      __HAL_I2C_ENABLE(hi2c);

      /* Enable Acknowledge */
      SET_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

      /* keep HAL_I2C_STATE_LISTEN */
      hi2c->PreviousState = I2C_STATE_NONE;
      hi2c->State = HAL_I2C_STATE_LISTEN;
    }
    else
    {
      hi2c->State = HAL_I2C_STATE_READY;
      hi2c->Mode = HAL_I2C_MODE_NONE;
    }

    /* Call the corresponding callback to inform upper layer of End of Transfer */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
    hi2c->ErrorCallback(hi2c);
#else
    HAL_I2C_ErrorCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
  }
}

/* I2C interrupts error process
 * @param  hi2c I2C handle.
 * @retval None
 */
static void I2C_ITError(I2C_HandleTypeDef *hi2c)
{
  /* Declaration of temporary variable to prevent undefined behavior of volatile usage */
  HAL_I2C_StateTypeDef CurrentState = hi2c->State;
  HAL_I2C_ModeTypeDef CurrentMode = hi2c->Mode;
  uint32_t CurrentError;

  if (((CurrentMode == HAL_I2C_MODE_MASTER) || (CurrentMode == HAL_I2C_MODE_MEM)) && (CurrentState == HAL_I2C_STATE_BUSY_RX))
  {
    /* Disable Pos bit in I2C CR1 when error occurred in Master/Mem Receive IT Process */
    hi2c->Instance->CR1 &= ~I2C_CR1_POS;
  }

  if (((uint32_t)CurrentState & (uint32_t)HAL_I2C_STATE_LISTEN) == (uint32_t)HAL_I2C_STATE_LISTEN)
  {
    /* keep HAL_I2C_STATE_LISTEN */
    hi2c->PreviousState = I2C_STATE_NONE;
    hi2c->State = HAL_I2C_STATE_LISTEN;
  }
  else
  {
    /* If state is an abort treatment on going, don't change state */
    /* This change will be do later */
    if ((READ_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN) != I2C_CR2_DMAEN) && (CurrentState != HAL_I2C_STATE_ABORT))
    {
      hi2c->State = HAL_I2C_STATE_READY;
      hi2c->Mode = HAL_I2C_MODE_NONE;
    }
    hi2c->PreviousState = I2C_STATE_NONE;
  }

  /* Abort DMA transfer */
  if (READ_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN) == I2C_CR2_DMAEN)
  {
    hi2c->Instance->CR2 &= ~I2C_CR2_DMAEN;

    if (hi2c->hdmatx->State != HAL_DMA_STATE_READY)
    {
      /* Set the DMA Abort callback :
      will lead to call HAL_I2C_ErrorCallback() at end of DMA abort procedure */
      hi2c->hdmatx->XferAbortCallback = I2C_DMAAbort;

      if (HAL_DMA_Abort_IT(hi2c->hdmatx) != HAL_OK)
      {
        /* Disable I2C peripheral to prevent dummy data in buffer */
        __HAL_I2C_DISABLE(hi2c);

        hi2c->State = HAL_I2C_STATE_READY;

        /* Call Directly XferAbortCallback function in case of error */
        hi2c->hdmatx->XferAbortCallback(hi2c->hdmatx);
      }
    }
    else
    {
      /* Set the DMA Abort callback :
      will lead to call HAL_I2C_ErrorCallback() at end of DMA abort procedure */
      hi2c->hdmarx->XferAbortCallback = I2C_DMAAbort;

      if (HAL_DMA_Abort_IT(hi2c->hdmarx) != HAL_OK)
      {
        /* Store Last receive data if any */
        if (I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_RXNE) == SET)
        {
          /* Read data from DR */
          *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

          /* Increment Buffer pointer */
          hi2c->pBuffPtr++;
        }

        /* Disable I2C peripheral to prevent dummy data in buffer */
        __HAL_I2C_DISABLE(hi2c);

        hi2c->State = HAL_I2C_STATE_READY;

        /* Call Directly hi2c->hdmarx->XferAbortCallback function in case of error */
        hi2c->hdmarx->XferAbortCallback(hi2c->hdmarx);
      }
    }
  }
  else if (hi2c->State == HAL_I2C_STATE_ABORT)
  {
    hi2c->State = HAL_I2C_STATE_READY;
    hi2c->ErrorCode = HAL_I2C_ERROR_NONE;

    /* Store Last receive data if any */
    if (I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_RXNE) == SET)
    {
      /* Read data from DR */
      *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

      /* Increment Buffer pointer */
      hi2c->pBuffPtr++;
    }

    /* Disable I2C peripheral to prevent dummy data in buffer */
    __HAL_I2C_DISABLE(hi2c);

    /* Call the corresponding callback to inform upper layer of End of Transfer */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
    hi2c->AbortCpltCallback(hi2c);
#else
    HAL_I2C_AbortCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
  }
  else
  {
    /* Store Last receive data if any */
    if (I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_RXNE) == SET)
    {
      /* Read data from DR */
      *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

      /* Increment Buffer pointer */
      hi2c->pBuffPtr++;
    }

    /* Call user error callback */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
    hi2c->ErrorCallback(hi2c);
#else
    HAL_I2C_ErrorCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
  }

  /* STOP Flag is not set after a NACK reception, BusError, ArbitrationLost, OverRun */
  CurrentError = hi2c->ErrorCode;

  if (((CurrentError & HAL_I2C_ERROR_BERR) == HAL_I2C_ERROR_BERR) || \
      ((CurrentError & HAL_I2C_ERROR_ARLO) == HAL_I2C_ERROR_ARLO) || \
      ((CurrentError & HAL_I2C_ERROR_AF) == HAL_I2C_ERROR_AF)     || \
      ((CurrentError & HAL_I2C_ERROR_OVR) == HAL_I2C_ERROR_OVR))
  {
    /* Disable EVT, BUF and ERR interrupt */
    __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);
  }

  /* So may inform upper layer that listen phase is stopped */
  /* during NACK error treatment */
  CurrentState = hi2c->State;
  if (((hi2c->ErrorCode & HAL_I2C_ERROR_AF) == HAL_I2C_ERROR_AF) && (CurrentState == HAL_I2C_STATE_LISTEN))
  {
    hi2c->XferOptions   = I2C_NO_OPTION_FRAME;
    hi2c->PreviousState = I2C_STATE_NONE;
    hi2c->State         = HAL_I2C_STATE_READY;
    hi2c->Mode          = HAL_I2C_MODE_NONE;

    /* Call the Listen Complete callback, to inform upper layer of the end of Listen usecase */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
    hi2c->ListenCpltCallback(hi2c);
#else
    HAL_I2C_ListenCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
  }
}

/* Handle STOPF flag for Slave
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_Slave_STOPF(I2C_HandleTypeDef *hi2c)
{
  /* Declaration of temporary variable to prevent undefined behavior of volatile usage */
  HAL_I2C_StateTypeDef CurrentState = hi2c->State;

  /* Disable EVT, BUF and ERR interrupt */
  __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);

  /* Clear STOPF flag */
  __HAL_I2C_CLEAR_STOPFLAG(hi2c);

  /* Disable Acknowledge */
  CLEAR_BIT(hi2c->Instance->CR1, I2C_CR1_ACK);

  /* If a DMA is ongoing, Update handle size context */
  if ((hi2c->Instance->CR2 & I2C_CR2_DMAEN) == I2C_CR2_DMAEN)
  {
    if ((CurrentState == HAL_I2C_STATE_BUSY_RX) || (CurrentState == HAL_I2C_STATE_BUSY_RX_LISTEN))
    {
      hi2c->XferCount = (uint16_t)(__HAL_DMA_GET_COUNTER(hi2c->hdmarx));

      if (hi2c->XferCount != 0U)
      {
        /* Set ErrorCode corresponding to a Non-Acknowledge */
        hi2c->ErrorCode |= HAL_I2C_ERROR_AF;
      }

      /* Disable, stop the current DMA */
      CLEAR_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN);

      /* Abort DMA Xfer if any */
      if (HAL_DMA_GetState(hi2c->hdmarx) != HAL_DMA_STATE_READY)
      {
        /* Set the I2C DMA Abort callback :
        will lead to call HAL_I2C_ErrorCallback() at end of DMA abort procedure */
        hi2c->hdmarx->XferAbortCallback = I2C_DMAAbort;

        /* Abort DMA RX */
        if (HAL_DMA_Abort_IT(hi2c->hdmarx) != HAL_OK)
        {
          /* Call Directly XferAbortCallback function in case of error */
          hi2c->hdmarx->XferAbortCallback(hi2c->hdmarx);
        }
      }
    }
    else
    {
      hi2c->XferCount = (uint16_t)(__HAL_DMA_GET_COUNTER(hi2c->hdmatx));

      if (hi2c->XferCount != 0U)
      {
        /* Set ErrorCode corresponding to a Non-Acknowledge */
        hi2c->ErrorCode |= HAL_I2C_ERROR_AF;
      }

      /* Disable, stop the current DMA */
      CLEAR_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN);

      /* Abort DMA Xfer if any */
      if (HAL_DMA_GetState(hi2c->hdmatx) != HAL_DMA_STATE_READY)
      {
        /* Set the I2C DMA Abort callback :
        will lead to call HAL_I2C_ErrorCallback() at end of DMA abort procedure */
        hi2c->hdmatx->XferAbortCallback = I2C_DMAAbort;

        /* Abort DMA TX */
        if (HAL_DMA_Abort_IT(hi2c->hdmatx) != HAL_OK)
        {
          /* Call Directly XferAbortCallback function in case of error */
          hi2c->hdmatx->XferAbortCallback(hi2c->hdmatx);
        }
      }
    }
  }

  /* All data are not transferred, so set error code accordingly */
  if (hi2c->XferCount != 0U)
  {
    /* Store Last receive data if any */
    if (I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_BTF) == SET)
    {
      /* Read data from DR */
      *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

      /* Increment Buffer pointer */
      hi2c->pBuffPtr++;

      /* Update counter */
      hi2c->XferCount--;
    }

    /* Store Last receive data if any */
    if (I2C_GetFlagStatus(hi2c->Instance, I2C_FLAG_RXNE) == SET)
    {
      /* Read data from DR */
      *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

      /* Increment Buffer pointer */
      hi2c->pBuffPtr++;

      /* Update counter */
      hi2c->XferCount--;
    }

    if (hi2c->XferCount != 0U)
    {
      /* Set ErrorCode corresponding to a Non-Acknowledge */
      hi2c->ErrorCode |= HAL_I2C_ERROR_AF;
    }
  }

  if (hi2c->ErrorCode != HAL_I2C_ERROR_NONE)
  {
    /* Call the corresponding callback to inform upper layer of End of Transfer */
    I2C_ITError(hi2c);
  }
  else
  {
    if (CurrentState == HAL_I2C_STATE_BUSY_RX_LISTEN)
    {
      /* Set state at HAL_I2C_STATE_LISTEN */
      hi2c->PreviousState = I2C_STATE_NONE;
      hi2c->State = HAL_I2C_STATE_LISTEN;

      /* Call the corresponding callback to inform upper layer of End of Transfer */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->SlaveRxCpltCallback(hi2c);
#else
      HAL_I2C_SlaveRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }

    if (hi2c->State == HAL_I2C_STATE_LISTEN)
    {
      hi2c->XferOptions = I2C_NO_OPTION_FRAME;
      hi2c->PreviousState = I2C_STATE_NONE;
      hi2c->State = HAL_I2C_STATE_READY;
      hi2c->Mode = HAL_I2C_MODE_NONE;

      /* Call the Listen Complete callback, to inform upper layer of the end of Listen usecase */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->ListenCpltCallback(hi2c);
#else
      HAL_I2C_ListenCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
    else
    {
      if ((hi2c->PreviousState  == I2C_STATE_SLAVE_BUSY_RX) || (CurrentState == HAL_I2C_STATE_BUSY_RX))
      {
        hi2c->PreviousState = I2C_STATE_NONE;
        hi2c->State = HAL_I2C_STATE_READY;
        hi2c->Mode = HAL_I2C_MODE_NONE;

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
        hi2c->SlaveRxCpltCallback(hi2c);
#else
        HAL_I2C_SlaveRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
      }
    }
  }
}

/* Handle TXE flag for Slave
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_SlaveTransmit_TXE(I2C_HandleTypeDef *hi2c)
{
  /* Declaration of temporary variables to prevent undefined behavior of volatile usage */
  HAL_I2C_StateTypeDef CurrentState = hi2c->State;

  if (hi2c->XferCount != 0U)
  {
    /* Write data to DR */
    hi2c->Instance->DR = *hi2c->pBuffPtr;

    /* Increment Buffer pointer */
    hi2c->pBuffPtr++;

    /* Update counter */
    hi2c->XferCount--;

    if ((hi2c->XferCount == 0U) && (CurrentState == HAL_I2C_STATE_BUSY_TX_LISTEN))
    {
      /* Last Byte is received, disable Interrupt */
      __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_BUF);

      /* Set state at HAL_I2C_STATE_LISTEN */
      hi2c->PreviousState = I2C_STATE_SLAVE_BUSY_TX;
      hi2c->State = HAL_I2C_STATE_LISTEN;

      /* Call the corresponding callback to inform upper layer of End of Transfer */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->SlaveTxCpltCallback(hi2c);
#else
      HAL_I2C_SlaveTxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
  }
}

/* Handle BTF flag for Slave transmitter
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_SlaveTransmit_BTF(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->XferCount != 0U)
  {
    /* Write data to DR */
    hi2c->Instance->DR = *hi2c->pBuffPtr;

    /* Increment Buffer pointer */
    hi2c->pBuffPtr++;

    /* Update counter */
    hi2c->XferCount--;
  }
}

/* Handle RXNE flag for Slave
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_SlaveReceive_RXNE(I2C_HandleTypeDef *hi2c)
{
  /* Declaration of temporary variables to prevent undefined behavior of volatile usage */
  HAL_I2C_StateTypeDef CurrentState = hi2c->State;

  if (hi2c->XferCount != 0U)
  {
    /* Read data from DR */
    *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

    /* Increment Buffer pointer */
    hi2c->pBuffPtr++;

    /* Update counter */
    hi2c->XferCount--;

    if ((hi2c->XferCount == 0U) && (CurrentState == HAL_I2C_STATE_BUSY_RX_LISTEN))
    {
      /* Last Byte is received, disable Interrupt */
      __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_BUF);

      /* Set state at HAL_I2C_STATE_LISTEN */
      hi2c->PreviousState = I2C_STATE_SLAVE_BUSY_RX;
      hi2c->State = HAL_I2C_STATE_LISTEN;

      /* Call the corresponding callback to inform upper layer of End of Transfer */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1)
      hi2c->SlaveRxCpltCallback(hi2c);
#else
      HAL_I2C_SlaveRxCpltCallback(hi2c);
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
    }
  }
}

/* Handle BTF flag for Slave receiver
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *         the configuration information for I2C module
 * @retval None
 */
static void I2C_SlaveReceive_BTF(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->XferCount != 0U)
  {
    /* Read data from DR */
    *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->DR;

    /* Increment Buffer pointer */
    hi2c->pBuffPtr++;

    /* Update counter */
    hi2c->XferCount--;
  }
}

/* This function handles I2C event interrupt request.
 * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
 *                the configuration information for the specified I2C.
 * @retval None
 */
void HAL_I2C_EV_IRQHandler(I2C_HandleTypeDef *hi2c)
{
  uint32_t sr1itflags;
  uint32_t sr2itflags               = 0U;
  uint32_t itsources                = READ_REG(hi2c->Instance->CR2);
  uint32_t CurrentXferOptions       = hi2c->XferOptions;
  HAL_I2C_ModeTypeDef CurrentMode   = hi2c->Mode;
  HAL_I2C_StateTypeDef CurrentState = hi2c->State;

  /* Master or Memory mode selected */
  if ((CurrentMode == HAL_I2C_MODE_MASTER) || (CurrentMode == HAL_I2C_MODE_MEM))
  {
    sr2itflags   = READ_REG(hi2c->Instance->SR2);
    sr1itflags   = READ_REG(hi2c->Instance->SR1);

    /* Exit IRQ event until Start Bit detected in case of Other frame requested */
    if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_SB) == RESET) && (IS_I2C_TRANSFER_OTHER_OPTIONS_REQUEST(CurrentXferOptions) == 1U))
    {
      return;
    }

    /* SB Set ----------------------------------------------------------------*/
    if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_SB) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_EVT) != RESET))
    {
      /* Convert OTHER_xxx XferOptions if any */
      I2C_ConvertOtherXferOptions(hi2c);

      I2C_Master_SB(hi2c);
    }
    /* ADD10 Set -------------------------------------------------------------*/
    else if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_ADD10) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_EVT) != RESET))
    {
      I2C_Master_ADD10(hi2c);
    }
    /* ADDR Set --------------------------------------------------------------*/
    else if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_ADDR) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_EVT) != RESET))
    {
      I2C_Master_ADDR(hi2c);
    }
    /* I2C in mode Transmitter -----------------------------------------------*/
    else if (I2C_CHECK_FLAG(sr2itflags, I2C_FLAG_TRA) != RESET)
    {
      /* Do not check buffer and BTF flag if a Xfer DMA is on going */
      if (READ_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN) != I2C_CR2_DMAEN)
      {
        /* TXE set and BTF reset -----------------------------------------------*/
        if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_TXE) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_BUF) != RESET) && (I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_BTF) == RESET))
        {
          I2C_MasterTransmit_TXE(hi2c);
        }
        /* BTF set -------------------------------------------------------------*/
        else if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_BTF) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_EVT) != RESET))
        {
          if (CurrentMode == HAL_I2C_MODE_MASTER)
          {
            I2C_MasterTransmit_BTF(hi2c);
          }
          else /* HAL_I2C_MODE_MEM */
          {
            I2C_MemoryTransmit_TXE_BTF(hi2c);
          }
        }
        else
        {
          /* Do nothing */
        }
      }
    }
    /* I2C in mode Receiver --------------------------------------------------*/
    else
    {
      /* Do not check buffer and BTF flag if a Xfer DMA is on going */
      if (READ_BIT(hi2c->Instance->CR2, I2C_CR2_DMAEN) != I2C_CR2_DMAEN)
      {
        /* RXNE set and BTF reset -----------------------------------------------*/
        if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_RXNE) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_BUF) != RESET) && (I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_BTF) == RESET))
        {
          I2C_MasterReceive_RXNE(hi2c);
        }
        /* BTF set -------------------------------------------------------------*/
        else if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_BTF) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_EVT) != RESET))
        {
          I2C_MasterReceive_BTF(hi2c);
        }
        else
        {
          /* Do nothing */
        }
      }
    }
  }
  /* Slave mode selected */
  else
  {
    /* If an error is detected, read only SR1 register to prevent */
    /* a clear of ADDR flags by reading SR2 after reading SR1 in Error treatment */
    if (hi2c->ErrorCode != HAL_I2C_ERROR_NONE)
    {
      sr1itflags   = READ_REG(hi2c->Instance->SR1);
    }
    else
    {
      sr2itflags   = READ_REG(hi2c->Instance->SR2);
      sr1itflags   = READ_REG(hi2c->Instance->SR1);
    }

    /* ADDR set --------------------------------------------------------------*/
    if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_ADDR) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_EVT) != RESET))
    {
      /* Now time to read SR2, this will clear ADDR flag automatically */
      if (hi2c->ErrorCode != HAL_I2C_ERROR_NONE)
      {
        sr2itflags   = READ_REG(hi2c->Instance->SR2);
      }
      I2C_Slave_ADDR(hi2c, sr2itflags);
    }
    /* STOPF set --------------------------------------------------------------*/
    else if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_STOPF) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_EVT) != RESET))
    {
      I2C_Slave_STOPF(hi2c);
    }
    /* I2C in mode Transmitter -----------------------------------------------*/
    else if ((CurrentState == HAL_I2C_STATE_BUSY_TX) || (CurrentState == HAL_I2C_STATE_BUSY_TX_LISTEN))
    {
      /* TXE set and BTF reset -----------------------------------------------*/
      if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_TXE) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_BUF) != RESET) && (I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_BTF) == RESET))
      {
        I2C_SlaveTransmit_TXE(hi2c);
      }
      /* BTF set -------------------------------------------------------------*/
      else if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_BTF) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_EVT) != RESET))
      {
        I2C_SlaveTransmit_BTF(hi2c);
      }
      else
      {
        /* Do nothing */
      }
    }
    /* I2C in mode Receiver --------------------------------------------------*/
    else
    {
      /* RXNE set and BTF reset ----------------------------------------------*/
      if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_RXNE) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_BUF) != RESET) && (I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_BTF) == RESET))
      {
        I2C_SlaveReceive_RXNE(hi2c);
      }
      /* BTF set -------------------------------------------------------------*/
      else if ((I2C_CHECK_FLAG(sr1itflags, I2C_FLAG_BTF) != RESET) && (I2C_CHECK_IT_SOURCE(itsources, I2C_IT_EVT) != RESET))
      {
        I2C_SlaveReceive_BTF(hi2c);
      }
      else
      {
        /* Do nothing */
      }
    }
  }
}
