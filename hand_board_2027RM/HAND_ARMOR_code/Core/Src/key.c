#include "key.h"
#include "main.h"
#include "app_config.h"

static bool s_last_raw_pressed = false;
static bool s_stable_pressed = false;

static uint32_t s_raw_change_tick = 0U;
static uint32_t s_press_tick = 0U;

static bool s_long_started = false;

static bool Key_ReadPhysicalPressed(void)
{
    /*
     * KEY硬件外部上拉。
     *
     * GPIO HIGH = 松开
     * GPIO LOW  = 按下
     */
    return (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET);
}

void Key_Init(void)
{
    uint32_t now = HAL_GetTick();
    bool pressed = Key_ReadPhysicalPressed();

    s_last_raw_pressed = pressed;
    s_stable_pressed = pressed;

    s_raw_change_tick = now;
    s_press_tick = now;

    s_long_started = false;
}

Key_Event_t Key_Task(void)
{
    uint32_t now = HAL_GetTick();
    bool raw_pressed = Key_ReadPhysicalPressed();
    Key_Event_t event = KEY_EVENT_NONE;

    /*
     * 原始电平发生变化时重新开始消抖计时。
     */
    if (raw_pressed != s_last_raw_pressed)
    {
        s_last_raw_pressed = raw_pressed;
        s_raw_change_tick = now;
    }

    /*
     * 原始状态连续稳定达到消抖时间，·
     */
    if ((raw_pressed != s_stable_pressed) &&
        ((uint32_t)(now - s_raw_change_tick) >= APP_KEY_DEBOUNCE_MS))
    {
        s_stable_pressed = raw_pressed;

        if (s_stable_pressed)
        {
            /*
             * 新的一次有效按下。
             */
            s_press_tick = now;
            s_long_started = false;
        }
        else
        {
            /*
             * 有效松开。
             *
             * 如果本次已经进入Long模式：
             * 只能产生LONG_RELEASE。
             *
             * 绝对不能再产生SHORT。
             */
            if (s_long_started)
            {
                event = KEY_EVENT_LONG_RELEASE;
            }
            else
            {
                event = KEY_EVENT_SHORT_PRESS;
            }
        }
    }

    /*
     * 稳定处于按下状态并持续达到600ms以后，
     * 仅产生一次LONG_START。
     */
    if (s_stable_pressed &&
        (!s_long_started) &&
        ((uint32_t)(now - s_press_tick) >= APP_KEY_LONG_PRESS_MS))
    {
        s_long_started = true;
        event = KEY_EVENT_LONG_START;
    }

    return event;
}

bool Key_IsPressed(void)
{
    return s_stable_pressed;
}
