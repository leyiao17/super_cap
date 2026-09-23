#include "rgb.h"
#include "tim.h"
#include "main.h"
#include "app_config.h"

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB_Value_t;

static const RGB_Value_t s_color_table[RGB_COLOR_COUNT] =
{
    [RGB_COLOR_RED]     = {255U,   0U,   0U},
    [RGB_COLOR_GREEN]   = {  0U, 255U,   0U},
    [RGB_COLOR_BLUE]    = {  0U,   0U, 255U},
    [RGB_COLOR_YELLOW]  = {255U, 255U,   0U},
    [RGB_COLOR_CYAN]    = {  0U, 255U, 255U},
    [RGB_COLOR_MAGENTA] = {255U,   0U, 255U},
    [RGB_COLOR_WHITE]   = {255U, 255U, 255U}
};

static RGB_Color_t s_current_color = RGB_COLOR_RED;
static uint8_t s_brightness_percent = APP_BRIGHTNESS_MAX_PERCENT;

static uint32_t RGB_ComponentToCompare(uint8_t component);
static void RGB_ApplyCurrent(void);
static void RGB_WriteCompare(uint32_t red,
                             uint32_t green,
                             uint32_t blue);

void RGB_Init(void)
{
    /*
     * 先确保三个CCR为0，再启动PWM。
     * PWM启动以后整个程序生命周期内不再Stop。
     */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0U);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0U);

    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4) != HAL_OK)
    {
        Error_Handler();
    }

    s_current_color = RGB_COLOR_RED;
    s_brightness_percent = APP_BRIGHTNESS_MAX_PERCENT;

    /*
     * 初始化阶段保持灯灭。
     * 等BOOST稳定以后由App层正式显示默认红色。
     */
    RGB_Off();
}

void RGB_SetColor(RGB_Color_t color)
{
    if (color >= RGB_COLOR_COUNT)
    {
        return;
    }

    s_current_color = color;
    RGB_ApplyCurrent();
}

RGB_Color_t RGB_GetColor(void)
{
    return s_current_color;
}

void RGB_SetBrightness(uint8_t percent)
{
    if (percent > 100U)
    {
        percent = 100U;
    }

    s_brightness_percent = percent;
    RGB_ApplyCurrent();
}

uint8_t RGB_GetBrightness(void)
{
    return s_brightness_percent;
}

void RGB_Off(void)
{
    /*
     * RGB_Off只关闭当前输出，
     * 不修改当前颜色和Brightness记录。
     */
    RGB_WriteCompare(0U, 0U, 0U);
}

static uint32_t RGB_ComponentToCompare(uint8_t component)
{
    uint32_t pwm_full_scale;
    uint32_t numerator;
    uint32_t denominator;

    /*
     * ARR=999时：
     *
     * pwm_full_scale = ARR + 1 = 1000
     *
     * PWM Mode 1下CCR=1000，
     * Counter只会运行0~999，
     * 因而得到真正100%常高。
     */
    pwm_full_scale = __HAL_TIM_GET_AUTORELOAD(&htim3) + 1U;

    /*
     * component范围：0~255
     * brightness范围：0~100
     *
     * 最大乘积：
     * 1000 * 255 * 100 = 25,500,000
     *
     * uint32_t完全足够。
     */
    numerator =
        pwm_full_scale *
        (uint32_t)component *
        (uint32_t)s_brightness_percent;

    denominator = 255U * 100U;

    /*
     * 加 denominator/2 实现整数四舍五入。
     */
    return (numerator + (denominator / 2U)) / denominator;
}

static void RGB_ApplyCurrent(void)
{
    const RGB_Value_t *color = &s_color_table[s_current_color];

    uint32_t red_compare   = RGB_ComponentToCompare(color->r);
    uint32_t green_compare = RGB_ComponentToCompare(color->g);
    uint32_t blue_compare  = RGB_ComponentToCompare(color->b);

    RGB_WriteCompare(red_compare,
                     green_compare,
                     blue_compare);
}

static void RGB_WriteCompare(uint32_t red,
                             uint32_t green,
                             uint32_t blue)
{
    /*
     * Hardware mapping:
     *
     * RED   = TIM3_CH1 = PA6
     * GREEN = TIM3_CH2 = PA7
     * BLUE  = TIM3_CH4 = PB1
     */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, red);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, green);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, blue);
}
