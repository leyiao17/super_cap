#ifndef POWER_H
#define POWER_H

#include <stdbool.h>

void Power_Init(void);

void Power_BoostOn(void);
void Power_BoostOff(void);

bool Power_IsBoostOn(void);

#endif /* POWER_H */
