/**
  ******************************************************************************
  * @file    stm32h5xx_util_i3c.c
  * @author  MCD Application Team
  * @brief   This utility help to calculate the different I3C Timing.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32h5xx_util_i3c.h"

/** @addtogroup I3C_Utility
  * @{
  */

/* Private macros ------------------------------------------------------------*/
#define DIV_ROUND_CLOSEST(x, d) (((x) + ((d) / 2U)) / (d))

/* Private Constants ---------------------------------------------------------*/
#define SEC210PSEC              (uint64_t)100000000000 /*!< 10ps, to take two decimal float of ns calculation */
#define TI3CH_MIN               3200U    /*!< Open drain & push pull SCL high min, 32ns */
#define TI3CH_OD_MAX            4100U    /*!< Open drain SCL high max, 41 ns */
#define TI3CL_OD_MIN            20000U   /*!< Open drain SCL low min, 200 ns */
#define TFMPL_OD_MIN            50000U   /*!< Fast Mode Plus Open drain SCL low min, 500 ns */
#define TFML_OD_MIN             130000U  /*!< Fast Mode Open drain SCL low min, 1300 ns */
#define TFM_MIN                 250000U  /*!< Fast Mode, period min for ti3cclk, 2.5us */
#define TSM_MIN                 1000000U /*!< Standard Mode, period min for ti3cclk, 10us */
#define TI3C_CAS_MIN            3840U    /*!< time SCL after START min, 38.4 ns */
#define TCAPA                   35000U   /*!< capacitor effect Value measure on Nucleo around 350ns */

#define BUS_I2Cx_FREQUENCY      100000U /*!< Frequency of I2Cn = 100 KHz*/
/* Private Types -------------------------------------------------------------*/
/* Private Private Constants -------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
/**
  * @brief  Calculate the I3C Controller timing according current I3C clock source and required I3C bus clock.
  * @param  pConfig             : [OUT]  Pointer to an LL_I3C_CtrlBusConfTypeDef structure that contains
  *                                      the configuration information for the specified I3C.
  * @param  clockSrcFreq        : [IN] I3C clock source in Hz.
  * @param  i3cFreq             : [IN] I3C required bus clock in Hz.
  * @param  i2cFreq             : [IN] I2C required bus clock in Hz.
  * @param  dutyCycle           : [IN] I3C duty cycle for Pure I3C bus or I2C duty cycle for Mixed bus in purcent
  *                                    Duty cycle must be lower or equal 50 purcent.
  * @param  busType             : [IN] Bus configuration type. It can be one value of @ref I3C_BUS_TYPE.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Timing calculation successfully
  *          - ERROR: Parameters or timing calculation error
  */
ErrorStatus I3C_CtrlTimingComputation(LL_I3C_CtrlBusConfTypeDef *pConfig, uint32_t clockSrcFreq, uint32_t i3cFreq,
                                      uint32_t i2cFreq, uint32_t dutyCycle, uint32_t busType)
{
  ErrorStatus status = SUCCESS;

  /* MIPI Standard constants */
  /* I3C: Open drain & push pull SCL high min, tDIG_H & tDIG_H_MIXED: 32 ns */
  uint32_t ti3ch_min       = TI3CH_MIN;

  /* I3C: Open drain SCL high max, t_HIGH: 41 ns */
  uint32_t ti3ch_od_max   = TI3CH_OD_MAX;

  /* I3C: Open drain SCL high max, tHIGH: 41 ns (Ti3ch_od_max= 410)
      I3C (pure bus): Open drain SCL low min, tLOW_OD: 200 ns */
  uint32_t ti3cl_od_min    = TI3CL_OD_MIN;

  /* I3C (mixed bus): Open drain SCL low min,
     tLOW: 500 ns (FM+ I2C on the bus)
     tLOW: 1300 ns (FM I2C on the bus) */
  uint32_t tfmpl_od_min    = TFMPL_OD_MIN;
  uint32_t tfml_od_min     = TFML_OD_MIN;

  /* I2C: min ti3cclk
          fSCL: 1 MHz (FM+)
          fSCL: 100 kHz (SM) */
  uint32_t tfm_min         = TFM_MIN;
  uint32_t tsm_min         = TSM_MIN;

  /* I3C: time SCL after START min, Tcas: 38,4 ns */
  uint32_t ti3c_cas_min    = TI3C_CAS_MIN;

  /* Period Clock source */
  uint32_t ti3cclk = 0U;

  /* I3C: Push pull period */
  uint32_t ti3c_pp_min = 0U;

  /* I2C: Open drain period */
  uint32_t ti2c_od_min = 0U;

  /* Time for SDA rise to 70% VDD from GND, capacitor effect */
  /* Value measure on Nucleo around 350ns */
  uint32_t tcapa = TCAPA;

  /* Compute variable */
  uint32_t sclhi3c;
  uint32_t scllpp;
  uint32_t scllod;
  uint32_t sclhi2c;
  uint32_t oneus;
  uint32_t free;
  uint32_t sdahold;

  /* Verify Parameters */
  if (((clockSrcFreq == 0U) || (i3cFreq == 0U)) && (busType == I3C_PURE_I3C_BUS))
  {
    status = ERROR;
  }

  if (((clockSrcFreq == 0U) || (i3cFreq == 0U) || (i2cFreq == 0U)) && (busType == I3C_MIXED_BUS))
  {
    status = ERROR;
  }

  if (status == SUCCESS)
  {
    /* Period Clock source */
    ti3cclk = (uint32_t)((SEC210PSEC + ((uint64_t)clockSrcFreq / (uint64_t)2)) / (uint64_t)clockSrcFreq);

    if ((dutyCycle > 50U) || (ti3cclk == 0U))
    {
      status = ERROR;
    }
  }

  if ((status == SUCCESS) && (ti3cclk != 0U))
  {
    /* I3C: Push pull period */
    ti3c_pp_min = (uint32_t)((SEC210PSEC + ((uint64_t)i3cFreq / (uint64_t)2)) / (uint64_t)i3cFreq);

    /* I2C: Open drain period */
    ti2c_od_min = (uint32_t)((SEC210PSEC + ((uint64_t)i2cFreq / (uint64_t)2)) / (uint64_t)i2cFreq);

    if ((busType != I3C_PURE_I3C_BUS) && (ti2c_od_min > tsm_min))
    {
      status = ERROR;
    }
  }

  /* SCL Computation */
  if ((status == SUCCESS) && (ti3cclk != 0U))
  {
    /* I3C SCL high level (push-pull & open drain) */
    if (busType == I3C_PURE_I3C_BUS)
    {
      sclhi3c = DIV_ROUND_CLOSEST(DIV_ROUND_CLOSEST(ti3c_pp_min * dutyCycle, ti3cclk), 100U) - 1U;

      /* Check if sclhi3c < ti3ch_min, in that case calculate sclhi3c based on ti3ch_min */
      if (((sclhi3c + 1U) * ti3cclk) < ti3ch_min)
      {
        sclhi3c = DIV_ROUND_CLOSEST(ti3ch_min, ti3cclk) - 1U;

        /* Check if sclhi3c < ti3ch_min */
        if (((sclhi3c + 1U) * ti3cclk) < ti3ch_min)
        {
          sclhi3c += 1U;
        }

        scllpp = DIV_ROUND_CLOSEST(ti3c_pp_min, ti3cclk) - (sclhi3c + 1U) - 1U;
      }
      else
      {
        sclhi3c = DIV_ROUND_CLOSEST(DIV_ROUND_CLOSEST(ti3c_pp_min * dutyCycle, ti3cclk), 100U) - 1U;

        /* Check if sclhi3c < ti3ch_min */
        if (((sclhi3c + 1U) * ti3cclk) < ti3ch_min)
        {
          sclhi3c += 1U;
        }

        scllpp  = DIV_ROUND_CLOSEST(DIV_ROUND_CLOSEST(ti3c_pp_min * (100U - dutyCycle), ti3cclk), 100U) - 1U;
      }

    }
    else
    {
      /* Warning: (sclhi3c + 1) * ti3cclk > Ti3ch_od_max expected */
      sclhi3c = DIV_ROUND_CLOSEST(ti3ch_od_max, ti3cclk) - 1U;

      if (((sclhi3c + 1U) * ti3cclk) < ti3ch_min)
      {
        sclhi3c += 1U;
      }
      else if (((sclhi3c + 1U) * ti3cclk) > ti3ch_od_max)
      {
        sclhi3c = (ti3ch_od_max / ti3cclk);
      }
      else
      {
        /* Do nothing, keep sclhi3c as previously calculated */
      }

      /* I3C SCL low level (push-pull) */
      /* tscllpp = (scllpp + 1) x ti3cclk */
      scllpp  = DIV_ROUND_CLOSEST((ti3c_pp_min - ((sclhi3c + 1U) * ti3cclk)), ti3cclk) - 1U;
    }

    /* Check if scllpp is superior at (ti3c_pp_min + 1/2 clock source cycle) */
    /* Goal is to choice the scllpp approach lowest, to have a value frequency highest approach as possible */
    uint32_t ideal_scllpp = (ti3c_pp_min - ((sclhi3c + 1U) * ti3cclk));
    if (((scllpp + 1U) * ti3cclk) >= (ideal_scllpp + (ti3cclk / 2U) + 1U))
    {
      scllpp -= 1U;
    }

    /* Check if scllpp + sclhi3c is inferior at (ti3c_pp_min + 1/2 clock source cycle) */
    /* Goal is to increase the scllpp, to have a value frequency not out of the clock request */
    if (((scllpp + sclhi3c + 1U + 1U) * ti3cclk) < (ideal_scllpp + (ti3cclk / 2U) + 1U))
    {
      scllpp += 1U;
    }

    /* I3C SCL low level (pure I3C bus) */
    if (busType == I3C_PURE_I3C_BUS)
    {
      if (ti3c_pp_min < ti3cl_od_min)
      {
        scllod  = DIV_ROUND_CLOSEST(ti3cl_od_min, ti3cclk) - 1U;

        if (((scllod + 1U) * ti3cclk) < ti3cl_od_min)
        {
          scllod += 1U;
        }
      }
      else
      {
        scllod = scllpp;
      }

      /* Verify that SCL Open drain Low duration is superior as SDA rise time 70% */
      if (((scllod + 1U) * ti3cclk) < tcapa)
      {
        scllod = DIV_ROUND_CLOSEST(tcapa, ti3cclk) + 1U;
      }

      sclhi2c = 0U; /* I2C SCL not used in pure I3C bus */
    }
    /* SCL low level on mixed bus (open-drain) */
    /* I2C SCL high level (mixed bus with I2C) */
    else
    {
      scllod  = DIV_ROUND_CLOSEST(DIV_ROUND_CLOSEST(ti2c_od_min * (100U - dutyCycle), ti3cclk), 100U) - 1U;

      /* Mix Bus Fast Mode plus */
      if (ti2c_od_min < tfm_min)
      {
        if (((scllod + 1U) * ti3cclk) < tfmpl_od_min)
        {
          scllod  = DIV_ROUND_CLOSEST(tfmpl_od_min, ti3cclk) - 1U;
        }
      }
      /* Mix Bus Fast Mode */
      else
      {
        if (((scllod + 1U) * ti3cclk) < tfml_od_min)
        {
          scllod  = DIV_ROUND_CLOSEST(tfml_od_min, ti3cclk) - 1U;
        }
      }

      sclhi2c = DIV_ROUND_CLOSEST((ti2c_od_min - ((scllod + 1U) * ti3cclk)), ti3cclk) - 1U;
    }

    /* Clock After Start computation */

    /* I3C pure bus: (Tcas + tcapa)/2 */
    if (busType == I3C_PURE_I3C_BUS)
    {
      free = DIV_ROUND_CLOSEST((ti3c_cas_min + tcapa), (2U * ti3cclk)) + 1U;
    }
    /* I3C, I2C mixed: (scllod + tcapa)/2 */
    else
    {
      free = DIV_ROUND_CLOSEST((((scllod + 1U) * ti3cclk) + tcapa), (2U * ti3cclk));
    }

    /* One cycle hold time addition */
    /* By default 1/2 cycle: must be > 3 ns */
    if (ti3cclk > 600U)
    {
      sdahold = 0U;
    }
    else
    {
      sdahold = 1U;
    }

    /* 1 microsecond reference */
    oneus = DIV_ROUND_CLOSEST(100000U, ti3cclk) - 2U;

    if ((scllpp > 0xFFU) || (sclhi3c > 0xFFU) || (scllod > 0xFFU) || (sclhi2c > 0xFFU) || \
        (free > 0xFFU) || (oneus > 0xFFU))
    {
      /* Case of value is over 8bits, issue may be due to clocksource have a rate too high for bus clock request */
      /* Update the return status */
      status = ERROR;
    }
    else
    {
      /* SCL configuration */
      pConfig->SCLPPLowDuration = (uint8_t)scllpp;
      pConfig->SCLI3CHighDuration = (uint8_t)sclhi3c;
      pConfig->SCLODLowDuration = (uint8_t)scllod;
      pConfig->SCLI2CHighDuration = (uint8_t)sclhi2c;

      /* Free, Idle and SDA hold time configuration */
      pConfig->BusFreeDuration = (uint8_t)free;
      pConfig->BusIdleDuration = (uint8_t)oneus;
      pConfig->SDAHoldTime = (uint32_t)(sdahold << I3C_TIMINGR1_SDA_HD_Pos);
    }
  }

  return status;
}

/**
  * @brief  Calculate the I3C Controller timing according current I3C clock source and required I3C bus clock.
  * @param  pConfig             : [OUT]  Pointer to an LL_I3C_TgtBusConfTypeDef structure that contains
  *                                      the configuration information for the specified I3C.
  * @param  clockSrcFreq        : [IN] I3C clock source in Hz.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Timing calculation successfully
  *          - ERROR: Parameters or timing calculation error
  */
ErrorStatus I3C_TgtTimingComputation(LL_I3C_TgtBusConfTypeDef *pConfig, uint32_t clockSrcFreq)
{
  ErrorStatus status = SUCCESS;
  uint32_t oneus;
  uint32_t ti3cclk = 0U;

  /* Verify Parameters */
  if (clockSrcFreq == 0U)
  {
    status = ERROR;
  }

  if (status == SUCCESS)
  {
    /* Period Clock source */
    ti3cclk = (uint32_t)((SEC210PSEC + ((uint64_t)clockSrcFreq / (uint64_t)2)) / (uint64_t)clockSrcFreq);

    /* Verify Parameters */
    if (ti3cclk == 0U)
    {
      status = ERROR;
    }
  }

  if ((status == SUCCESS) && (ti3cclk != 0U))
  {
    /* 1 microsecond reference */
    oneus =  DIV_ROUND_CLOSEST(100000U, ti3cclk) - 2U;

    /* Bus available time configuration */
    pConfig->BusAvailableDuration = (uint8_t)oneus;
  }

  return status;
}
/* Private functions ---------------------------------------------------------*/
/**
  * @}
  */
