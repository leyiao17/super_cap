#include "power.h"
#include "main.h"

static bool s_boost_on = false;

void Power_Init(void)
{
    /*
     * BOOST_CMD硬件定义：
     * HIGH = Enable
     * LOW  = Disable
     */
    HAL_GPIO_WritePin(BOOST_CMD_GPIO_Port,
                      BOOST_CMD_Pin,
                      GPIO_PIN_RESET);

    s_boost_on = false;
}

void Power_BoostOn(void)
{
    HAL_GPIO_WritePin(BOOST_CMD_GPIO_Port,
                      BOOST_CMD_Pin,
                      GPIO_PIN_SET);

    s_boost_on = true;
}

void Power_BoostOff(void)
{
    HAL_GPIO_WritePin(BOOST_CMD_GPIO_Port,
                      BOOST_CMD_Pin,
                      GPIO_PIN_RESET);

    s_boost_on = false;
}

bool Power_IsBoostOn(void)
{
    return s_boost_on;
}
