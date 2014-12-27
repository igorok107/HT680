#include "iostm8s003f3.h"

unsigned short int BitTicks;
unsigned char PreAmb = 1;
unsigned long Data = 0;
unsigned char BitCount;
unsigned short int TIM1_PSCR = 8;
unsigned char Flag;

/*Выбор генератора (0xE1 - HSI; 0xD2 - LSI; 0xB4 - HSE)*/
//#define HSE 0xB4
#define HSI 0xE1
#define LSI 0xD2

#define CLK_DEF HSI

void delay_ms(unsigned int t)
{
  CLK_SWR = LSI; //Выбор LSI генератора
  while (CLK_SWCR_SWBSY);
  
  while (t > 0) 
  {
    if (TIM4_SR_UIF == 1) { t--; TIM4_SR_UIF = 0;};
  }
  
  CLK_SWR = CLK_DEF;
  while (CLK_SWCR_SWBSY);
}

void init(void){
#ifdef HSI
  CLK_ICKR_LSIEN = 1;
#else
  CLK_ICKR_LSIEN = 0;
#endif
#ifdef HSI
  CLK_ICKR_HSIEN = 1;
#else
  CLK_ICKR_HSIEN = 0;
#endif
#ifdef HSE  
  CLK_ECKR_HSEEN = 1;
  while (!CLK_ECKR_HSERDY);    
#else
  CLK_ECKR_HSEEN = 0;
#endif
  CLK_CKDIVR = 0; //Делитель  
  CLK_SWCR = 0; //Reset the clock switch control register.
  CLK_SWCR_SWEN = 1; //Переключение на выбранный генератор  
  CLK_SWR = CLK_DEF; 
  
  while (CLK_SWCR_SWBSY);
  
  //Прерывания внешение
  EXTI_CR1_PCIS = 2; //Прерывание на порт C (0: падающий и низкий уровень, 1: возрастающий, 2: падающий, 3: оба)  
  PC_CR1_C17=1; //Подтяжка вверх
  PC_CR2_C27=1; //Разрешаем прерывания
  CPU_CFG_GCR_AL = 1; //Прерывания во сне
  
  //Таймер для задержки ms
  TIM4_CNTR = 0;
  TIM4_PSCR_PSC = 0; //Предделитель
  TIM4_ARR = 127;
  TIM4_CR1_URS = 1;
  TIM4_EGR_UG = 1;  //Вызываем Update Event
  TIM4_CR1_CEN = 1;
  
  //Таймер 1
  TIM1_CR1_URS = 1; //Прерывание только по переполнению счетчика  
  TIM1_IER_UIE = 1; // Разрешаем прерывания  
  
  //GPIO
  PA_DDR_DDR3 = 1; //Настраиваем 4й пин порта A на выход
  PA_CR1_C13 = 1; //Переключаем его в режим push-pull
};

void RESET (void)
{
  TIM1_CR1_CEN = 0; //Остановка таймера
  TIM1_CNTRH = 0; // Сброс счётчика
  TIM1_CNTRL = 0;  
  TIM1_PSCRH = TIM1_PSCR >> 8; // Установка пределителя тактовой частоты
  TIM1_PSCRL = TIM1_PSCR & 0xFF;
  TIM1_ARRH = 21845 >> 8; // Уровень переполнения
  TIM1_ARRL = 21845 & 0xFF;
  TIM1_EGR_UG = 1;  //Вызываем Update Event  (Обновляем пределитель)
  
  PreAmb = 1;
  Data = 0;
  BitCount = 0;
  Flag = 0;
  EXTI_CR1_PCIS = 2;
  PC_CR2_C27 = 1;
}

void Button1(void)
{
  PA_ODR_ODR3 = 0;
}

void Button2(void)
{
  PA_ODR_ODR3 = 1;
}

#pragma vector = TIM1_OVR_UIF_vector 
__interrupt void TIM1_Interrupt(void) 
{
  switch (Flag){
  case 1: RESET(); break;
  case 2:
    TIM1_CR1_CEN = 0;
    if (PreAmb == 0x15)
    {
      BitCount++;
      Data = (Data << 1) ^ !PC_IDR_IDR7;
      if (BitCount >= 36)
      {
        switch (Data & 0xFFFF)
        {
        case 0x8EBA: // Кнопка 1
          Button1();
          break;
        case 0x8EEA: // Кнопка 2
          Button2();
          break;
        }
        RESET();
        delay_ms(150);        
      }
    } else if ((PreAmb & 0x1) == PC_IDR_IDR7)
    {
      PreAmb = (PreAmb << 1) ^ !PC_IDR_IDR7;
    } else RESET();
    PC_CR2_C27 = 1;
    break;
  default:
    Flag = 1;
    TIM1_CR1_CEN = 0;
    TIM1_EGR_UG = 1;
    break;
  }
 TIM1_SR1_UIF = 0;
};

#pragma vector=0x07
__interrupt void PinC7_interrupt(void) 
{
  PC_CR2_C27 = 0;
  switch (Flag){
  case 2:
    TIM1_CR1_CEN = 1;
    break;
  case 1:
    if (TIM1_CR1_CEN == 1)
    {
      TIM1_CR1_CEN = 0;
      BitTicks = (TIM1_CNTRH << 8 | TIM1_CNTRL) * 3;
      if ( BitTicks != 0 )
      {
        TIM1_ARRH = BitTicks >> 8;
        TIM1_ARRL = BitTicks & 0xFF;
        TIM1_PSCRH = TIM1_PSCR/2 >> 8;
        TIM1_PSCRL = TIM1_PSCR/2 & 0xFF;
        TIM1_EGR_UG = 1;
        TIM1_CR1_CEN = 1;
        Flag = 2;
      } else RESET();
    } else
    {
      TIM1_CR1_CEN = 1;
      EXTI_CR1_PCIS = 2;
      PC_CR2_C27 = 1;
    }
    break;  
  default:
    if (TIM1_CR1_CEN == 1) RESET();
    else 
    {
      TIM1_CR1_CEN = 1;
      EXTI_CR1_PCIS = 1;
      PC_CR2_C27 = 1;
    }
    break;
  }
};

void main( void )
{
  init();
  RESET();
  asm("WFI");
}
