#ifndef KEY_H
#define KEY_H

#include <stdbool.h>

typedef enum
{
    KEY_EVENT_NONE = 0,
    KEY_EVENT_SHORT_PRESS,
    KEY_EVENT_LONG_START,
    KEY_EVENT_LONG_RELEASE
} Key_Event_t;

void Key_Init(void);

/*
 * 每次主循环调用一次。
 * 函数本身完全非阻塞。
 */
Key_Event_t Key_Task(void);

/*
 * 返回经过消抖后的稳定按键状态。
 */
bool Key_IsPressed(void);

#endif /* KEY_H */
