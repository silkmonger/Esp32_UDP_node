#include "stdint.h"

#ifndef __SHARED_VARS_H
#define __SHARED_VARS_H

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// 'firmware version'
const uint8_t  SKETCH_INTRADAY_REV = 2;
const uint16_t SKETCH_YEAR_MONTH_DAY[] = {2023, 4, 16};
const uint16_t SKETCH_16BIT_DATE_CODE = (SKETCH_YEAR_MONTH_DAY[0]-2020)*12*31 + (SKETCH_YEAR_MONTH_DAY[1]*31) + SKETCH_YEAR_MONTH_DAY[2];

const uint8_t STRING_VARS_COUNT=8;
const uint8_t STRING_VARS_SIZE=32;

// const uint8_t PUSH_NOTIFICATION_STRING_SIZE=16;

const uint32_t OK_STATUS_32b = 0;
const uint32_t NOTOK_STATUS_32b = 1;

#endif