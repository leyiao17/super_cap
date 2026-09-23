#include "app.h"

#include "main.h"
#include "app_config.h"
#include "rgb.h"
#include "key.h"
#include "power.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_STATE_STARTUP = 0,
    APP_STATE_RUNNING
} App_State_t;

static App_State_t s_app_state = APP_STATE_STARTUP;

static uint32_t s_startup_tick = 0U;

static bool s_brightness_adjusting = false;
static uint32_t s_brightness_step_tick = 0U;

/*
 * -1 = 当前长按向暗调整
 * +1 = 当前长按向亮调整
 *
 * 开机第一次长按必须向暗。
 */
static int8_t s_brightness_direction = -1;

static void App_HandleShortPress(void);
static void App_HandleLongStart(uint32_t now);
static void App_HandleLongRelease(void);
static void App_HandleBrightnessAdjustment(uint32_t now);

void App_Init(void)
{
    /*
     * 初始化顺序：
     *
     * 1. Power先确保BOOST关闭
     * 2. RGB启动PWM但是CCR全部为0
     * 3. 初始化按键状态
     * 4. 打开BOOST
     * 5. App进入STARTUP等待20ms
     * 6. STARTUP结束后显示默认红色100%
     */

    Power_Init();
    RGB_Init();
    Key_Init();

    s_brightness_adjusting = false;
    s_brightness_direction = -1;

    Power_BoostOn();

    s_startup_tick = HAL_GetTick();
    s_app_state = APP_STATE_STARTUP;
}

void App_Task(void)
{
    uint32_t now = HAL_GetTick();

    if (s_app_state == APP_STATE_STARTUP)
    {
        /*
         * 非阻塞等待BOOST输出稳定。
         */
        (void)Key_Task();

        if ((uint32_t)(now - s_startup_tick) >=
            APP_BOOST_STARTUP_DELAY_MS)
        {
            /*
             * 默认：
             * 红色
             * 最大亮度
             */
            RGB_SetBrightness(APP_BRIGHTNESS_MAX_PERCENT);
            RGB_SetColor(RGB_COLOR_BLUE);

            /*
             * 重新初始化按键状态，
             * 避免启动等待期间产生的旧事件进入RUNNING。
             */
            Key_Init();

            s_app_state = APP_STATE_RUNNING;
        }

        return;
    }

    if (s_app_state == APP_STATE_RUNNING)
    {
        Key_Event_t event = Key_Task();

        switch (event)
        {
            case KEY_EVENT_SHORT_PRESS:
                App_HandleShortPress();
                break;

            case KEY_EVENT_LONG_START:
                App_HandleLongStart(now);
                break;

            case KEY_EVENT_LONG_RELEASE:
                App_HandleLongRelease();
                break;

            case KEY_EVENT_NONE:
            default:
                break;
        }

        App_HandleBrightnessAdjustment(now);
    }
}

static void App_HandleShortPress(void)
{
    RGB_Color_t color = RGB_GetColor();

    color = (RGB_Color_t)((uint32_t)color + 1U);

    if (color >= RGB_COLOR_COUNT)
    {
        color = RGB_COLOR_RED;
    }

    /*
     * 只改变颜色。
     * 当前Brightness保持不变。
     */
    RGB_SetColor(color);
}

static void App_HandleLongStart(uint32_t now)
{
    /*
     * 进入连续亮度调整。
     *
     * 不改变颜色。
     */
    s_brightness_adjusting = true;

    /*
     * 记录现在时间。
     * 第一次Brightness变化将在30ms后发生。
     */
    s_brightness_step_tick = now;
}

static void App_HandleLongRelease(void)
{
    if (!s_brightness_adjusting)
    {
        return;
    }

    s_brightness_adjusting = false;

    /*
     * 一次长按结束后才改变下次方向。
     *
     * 本次向暗 -> 下次向亮
     * 本次向亮 -> 下次向暗
     */
    s_brightness_direction =
        (int8_t)(-s_brightness_direction);
}

static void App_HandleBrightnessAdjustment(uint32_t now)
{
    uint8_t brightness;
    uint8_t new_brightness;

    if (!s_brightness_adjusting)
    {
        return;
    }

    /*
     * 必须仍然处于经过消抖后的按下状态。
     */
    if (!Key_IsPressed())
    {
        return;
    }

    if ((uint32_t)(now - s_brightness_step_tick) <
        APP_BRIGHTNESS_STEP_INTERVAL_MS)
    {
        return;
    }

    s_brightness_step_tick = now;

    brightness = RGB_GetBrightness();
    new_brightness = brightness;

    if (s_brightness_direction < 0)
    {
        /*
         * 向暗调整。
         */
        if (brightness > APP_BRIGHTNESS_MIN_PERCENT)
        {
            uint8_t remaining =
                (uint8_t)(brightness -
                          APP_BRIGHTNESS_MIN_PERCENT);

            if (remaining <= APP_BRIGHTNESS_STEP_PERCENT)
            {
                new_brightness =
                    APP_BRIGHTNESS_MIN_PERCENT;
            }
            else
            {
                new_brightness =
                    (uint8_t)(brightness -
                              APP_BRIGHTNESS_STEP_PERCENT);
            }
        }
    }
    else
    {
        /*
         * 向亮调整。
         */
        if (brightness < APP_BRIGHTNESS_MAX_PERCENT)
        {
            uint8_t remaining =
                (uint8_t)(APP_BRIGHTNESS_MAX_PERCENT -
                          brightness);

            if (remaining <= APP_BRIGHTNESS_STEP_PERCENT)
            {
                new_brightness =
                    APP_BRIGHTNESS_MAX_PERCENT;
            }
            else
            {
                new_brightness =
                    (uint8_t)(brightness +
                              APP_BRIGHTNESS_STEP_PERCENT);
            }
        }
    }

    /*
     * 只有数值真的发生变化才更新PWM。
     *
     * 到10%或100%以后保持当前值，
     * 当前长按绝对不自动改变方向。
     */
    if (new_brightness != brightness)
    {
        RGB_SetBrightness(new_brightness);
    }
}
