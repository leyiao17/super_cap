/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : RS485通信终极修复版 - 解决接收方死锁
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {STATE_OFF=0, STATE_RED, STATE_BLUE} LightState_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// 设备ID：编译时修改 (1,2,3,4)
#define MY_ID 5

// 系统参数
#define PWM_MAX 63999
#define FRAME_START 0xAA
#define BROADCAST_ADDR 0xFF
#define CMD_SET_STATE 0x01
#define DEBOUNCE_MS 20
#define FRAME_SIZE 4

// 调试引脚定义
#define DEBUG_TX_PIN GPIO_PIN_2  // PA2 - 发送指示
#define DEBUG_RX_PIN GPIO_PIN_3  // PA3 - 接收指示

// 按键宏定义
#ifndef KEY_Pin
  #define KEY_Pin GPIO_PIN_0
  #define KEY_GPIO_Port GPIOA
#endif

#ifndef SET_Pin
  #define SET_Pin GPIO_PIN_1
  #define SET_GPIO_Port GPIOA
#endif
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// 全局变量
volatile LightState_t state = STATE_RED;

// RS485接收相关 - 简化设计
static uint8_t rx_byte;
static uint8_t rx_buf[FRAME_SIZE];
static uint8_t rx_index = 0;
static uint8_t expecting_frame = 0;  // 0=等待帧头, 1=接收数据

// 事件标志
volatile uint8_t frame_received = 0;
volatile uint8_t need_blink = 0;

// 调试计数
volatile uint32_t tx_count = 0;
volatile uint32_t rx_count = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void UpdateLEDs(void);
void SendState(LightState_t s);
void ProcessReceivedFrame(void);
void UART_StartReceive(void);
/* USER CODE END PFP */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  // 初始化调试引脚
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = DEBUG_TX_PIN | DEBUG_RX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOA, DEBUG_TX_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, DEBUG_RX_PIN, GPIO_PIN_RESET);
  
  // 启动PWM
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
  
  // 设置初始状态
  UpdateLEDs();
  
  // 启动串口接收
  UART_StartReceive();
  
  // RS485初始为接收模式
  HAL_GPIO_WritePin(SET_GPIO_Port, SET_Pin, GPIO_PIN_RESET);
  
  // 按键状态跟踪变量
  uint8_t last_key_state = 1;  // 1=释放, 0=按下
  uint32_t key_press_time = 0;
  uint8_t key_processed = 0;   // 按键已处理标志
  
  // 程序运行指示
  for(int i=0; i<3; i++)
  {
    HAL_GPIO_TogglePin(GPIOA, DEBUG_TX_PIN);
    HAL_Delay(100);
  }
  HAL_GPIO_WritePin(GPIOA, DEBUG_TX_PIN, GPIO_PIN_RESET);
  /* USER CODE END 2 */

  /* Infinite loop */
  while(1)
  {
    // 1. 处理接收到的数据帧
    if(frame_received)
    {
      frame_received = 0;
      ProcessReceivedFrame();
    }
    
    // 2. 处理LED闪烁指示
    if(need_blink)
    {
      need_blink = 0;
      
      // 保存当前亮度
      uint16_t saved_red = __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1);
      uint16_t saved_blue = __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_4);
      
      // 闪烁
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
      HAL_Delay(20);
      
      // 恢复
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, saved_red);
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, saved_blue);
    }
    
    // 3. 轮询检测按键
    uint8_t current_key_state = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);
    
    // 检测下降沿（按键按下）
    if(last_key_state == 1 && current_key_state == 0)
    {
      key_press_time = HAL_GetTick();
      key_processed = 0;  // 重置处理标志
    }
    
    // 检测上升沿（按键释放）且按下时间>20ms
    if(last_key_state == 0 && current_key_state == 1)
    {
      if(!key_processed && (HAL_GetTick() - key_press_time) > DEBOUNCE_MS)
      {
        // 按键有效，切换状态
        state = (state + 1) % 3;
        UpdateLEDs();
        
        // 发送状态到总线
        SendState(state);
        
        // 标记已处理
        key_processed = 1;
        
        // 防连按延时
        HAL_Delay(200);
      }
    }
    
    // 更新按键状态
    last_key_state = current_key_state;
    
    // 4. 程序运行指示 - DEBUG_TX_PIN每3秒闪烁一次
    static uint32_t last_blink_time = 0;
    if(HAL_GetTick() - last_blink_time > 3000)
    {
      last_blink_time = HAL_GetTick();
      HAL_GPIO_TogglePin(GPIOA, DEBUG_TX_PIN);
    }
    
    // 短暂延时
    HAL_Delay(1);
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
  * @brief 串口接收回调 - 简化版，避免复杂状态机
  * @param huart: UART句柄
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart->Instance == USART1)
  {
    // 接收指示
    HAL_GPIO_WritePin(GPIOA, DEBUG_RX_PIN, GPIO_PIN_SET);
    
    if(rx_byte == FRAME_START)
    {
      // 收到帧头，开始接收
      expecting_frame = 1;
      rx_index = 0;
      rx_buf[rx_index++] = rx_byte;
    }
    else if(expecting_frame && rx_index < FRAME_SIZE)
    {
      // 接收数据字节
      rx_buf[rx_index++] = rx_byte;
      
      if(rx_index >= FRAME_SIZE)
      {
        // 收到完整帧
        expecting_frame = 0;
        frame_received = 1;  // 通知主循环处理
      }
    }
    else
    {
      // 未同步到有效帧，重置
      expecting_frame = 0;
      rx_index = 0;
    }
    
    // 接收指示关闭
    HAL_GPIO_WritePin(GPIOA, DEBUG_RX_PIN, GPIO_PIN_RESET);
    
    // 重新启动接收 - 必须确保执行
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

/**
  * @brief 启动串口接收
  * @retval None
  */
void UART_StartReceive(void)
{
  expecting_frame = 0;
  rx_index = 0;
  frame_received = 0;
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

/**
  * @brief 更新LED状态
  * @retval None
  */
void UpdateLEDs(void)
{
  switch(state)
  {
    case STATE_OFF:
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
      break;
    case STATE_RED:
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_MAX);
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
      break;
    case STATE_BLUE:
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, PWM_MAX);
      break;
  }
}

/**
  * @brief 发送状态到RS485总线
  * @param s: 要发送的状态
  * @retval None
  */
void SendState(LightState_t s)
{
  uint8_t tx[4] = {FRAME_START, BROADCAST_ADDR, CMD_SET_STATE, (uint8_t)s};
  
  // 发送指示
  HAL_GPIO_WritePin(GPIOA, DEBUG_TX_PIN, GPIO_PIN_SET);
  
  // 切换到发送模式
  HAL_GPIO_WritePin(SET_GPIO_Port, SET_Pin, GPIO_PIN_SET);
  HAL_Delay(1);
  
  // 发送数据
  HAL_UART_Transmit(&huart1, tx, 4, 100);
  tx_count++;
  
  // 等待发送完成
  while(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET);
  HAL_Delay(1);
  
  // 切换回接收模式
  HAL_GPIO_WritePin(SET_GPIO_Port, SET_Pin, GPIO_PIN_RESET);
  
  // 发送指示关闭
  HAL_Delay(10);
  HAL_GPIO_WritePin(GPIOA, DEBUG_TX_PIN, GPIO_PIN_RESET);
}

/**
  * @brief 处理接收到的数据帧（在主循环中调用）
  * @retval None
  */
void ProcessReceivedFrame(void)
{
  uint8_t* f = rx_buf;
  
  // 验证帧头
  if(f[0] != FRAME_START) 
  {
    return;
  }
  
  // 验证目标地址：广播或本机ID
  uint8_t target_addr = f[1];
  if(target_addr != BROADCAST_ADDR && target_addr != MY_ID) 
  {
    return;
  }
  
  // 验证命令
  if(f[2] != CMD_SET_STATE) 
  {
    return;
  }
  
  // 获取新状态
  uint8_t new_state = f[3];
  
  // 验证状态值是否有效
  if(new_state <= STATE_BLUE)
  {
    // 更新状态
    state = (LightState_t)new_state;
    UpdateLEDs();
    rx_count++;
    
    // 设置闪烁标志
    need_blink = 1;
  }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  // 不要关闭全局中断！
  while(1)
  {
    // 错误指示：快速闪烁DEBUG_TX_PIN
    HAL_GPIO_TogglePin(GPIOA, DEBUG_TX_PIN);
    HAL_Delay(50);
  }
}