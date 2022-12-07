// Control a servo with two buttons
// author: Xiangyu Wen
// Date: 2022.11.01

// hardware connection:
// servo (lower) orange wire -> PD0
// servo (lower) red wire -> V Bus
// servo (lower) brown wire -> GND

// servo (upper) orange wire -> PD1 (recommended)
// servo (upper) red wire -> V Bus
// servo (upper) brown wire -> GND

// What you need to do:
// 1. ignore the button-driven rotation function
// 2. try to control the servos with your computer
//    (you are recommended to use UART to communicate with PC and get values to control the servo.)
// 3. think about how to control two servos with the PWM (try to implement it).

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/uartstdio.c"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_ints.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/debug.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"
#include <math.h>

#define PWM_FREQUENCY 55
char angle[50];
volatile uint32_t ui8AdjustLow;
volatile uint32_t ui8AdjustUp;
volatile uint32_t ui32Load;

void str_to_value();

int main()
{
    //ui8Adjust is used to contol the servo angle.
    //We initialize ui8Adjust to 83 to make sure the servo is at the center position.

    volatile uint32_t ui32PWMClock;
    ui8AdjustLow = 83; //26-141
    ui8AdjustUp = 83; //35-150

    // init tht sys ctl
    SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
    SysCtlPWMClockSet(SYSCTL_PWMDIV_64);

    // enable UART0 and GPIOA.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    // Enable PWM1 to generate PWM signals
    // Enable GPIOD to output signals to servo
    // Enable GPIOF to use buttons
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    // Configure PA0 for RX
    // Configure PA1 for TX
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    // Set PORTA pin0 and pin1 as UART type
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // set UART base addr., clock get and baud rate.
    // used to communicate with computer
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // enable UART5 and GPIOE
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART5);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    // Configure board PE4 for RX
    // configure board PE5 for TX
    GPIOPinConfigure(GPIO_PE4_U5RX);
    GPIOPinConfigure(GPIO_PE5_U5TX);
    // set PORTE pin4 and pin5 as UART type
    GPIOPinTypeUART(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5);
    // set UART base addr., system clock, baud rate
    // used to communicate with HC-05
    UARTConfigSetExpClk(UART5_BASE, SysCtlClockGet(), 38400,
        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_2|GPIO_PIN_1|GPIO_PIN_3, 2) ;

    // set interrupt for receiving and showing values
    IntMasterEnable();
    IntEnable(INT_UART5);
    UARTIntEnable(UART5_BASE, UART_INT_RX | UART_INT_RT);

    //PD0
    GPIOPinTypePWM(GPIO_PORTD_BASE, GPIO_PIN_0);
    GPIOPinConfigure(GPIO_PD0_M1PWM0);
    //PD1
    GPIOPinTypePWM(GPIO_PORTD_BASE, GPIO_PIN_1);
    GPIOPinConfigure(GPIO_PD1_M1PWM1);

    //init UART
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    //init UART interrupts
    IntMasterEnable();
    IntEnable(INT_UART0);
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);

    //init PWM
    ui32PWMClock = SysCtlClockGet()/64;
    ui32Load = (ui32PWMClock / PWM_FREQUENCY) - 1;
    PWMGenConfigure(PWM1_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN);
    PWMGenPeriodSet(PWM1_BASE, PWM_GEN_0, ui32Load);

    // set the servo's initial position of PD0
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_0, ui8AdjustLow * ui32Load/1000);
    PWMOutputState(PWM1_BASE, PWM_OUT_0_BIT, true);
    PWMGenEnable(PWM1_BASE, PWM_GEN_0);
    // set the servo's initial position of PD0
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_1, ui8AdjustUp * ui32Load/1000);
    PWMOutputState(PWM1_BASE, PWM_OUT_1_BIT, true);
    PWMGenEnable(PWM1_BASE, PWM_GEN_0);


    while(1)
    {
        //if (UARTCharsAvail(UART0_BASE)) UARTCharPut(UART5_BASE, UARTCharGet(UART0_BASE));
        // since the main controlling function is implemented in while loop
        // we need to use delay function to control the rotating speed of the servo.

            //SysCtlDelay(SysCtlClockGet() / (1000 * 3));
    }
}

void UART5IntHandler(void)
{
    uint8_t flag = 0;
    uint32_t ui32Status;

    angle[0] = '\0';

    ui32Status = UARTIntStatus(UART5_BASE, true); //get interrupt status

    UARTIntClear(UART5_BASE, ui32Status); //clear the asserted interrupts

    while(UARTCharsAvail(UART5_BASE)) //loop while there are chars
    {
        angle[flag] = UARTCharGetNonBlocking(UART5_BASE);// get the string from master
        UARTCharPut(UART0_BASE, angle[flag]); // show in serial terminal for debug
        flag++;
        //UARTCharPut(UART0_BASE, angle[flag] = UARTCharGet(UART5_BASE));
        //SysCtlDelay(SysCtlClockGet() / (1000 * 3)); //delay some time
    }
    str_to_value(); // convert the string get from above the angle value
    if(ui8AdjustLow < 26) // set the working zone from -90 to 90
    {
        ui8AdjustLow = 26;
    }
    else if(ui8AdjustLow > 141) // set the working zone
    {
        ui8AdjustLow = 141;
    }
    //PD1
    if(ui8AdjustUp < 35) // set the working zone from -90 to 90
    {
        ui8AdjustUp = 35;
    }
    else if(ui8AdjustUp > 150) // set the working zone
    {
        ui8AdjustUp = 150;
    }
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_0, ui8AdjustLow * ui32Load/1000); // control servo1 to specific angle using pwm

    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_1, ui8AdjustUp * ui32Load/1000); // control servo 2 to specific angle using pwm
}

//using to function can convert string in specific protocol to angle value
void str_to_value(){
    int i = 0;
    ui8AdjustLow = 0;
    ui8AdjustUp = 0;
    char up[4];
    char low[4];
    while(angle[i] == 'S'){
        i++;
    }
    while((angle[i] >= '0' && angle[i] <= '9')){
        up[i] = angle[i];
        i++;
    }
    up[i] = '\0';
    i++;
    int j = 0;
    while((angle[i] >= '0' && angle[i] <= '9')){
        low[j++] = angle[i++];
    }
    low[j] = '\0';

    int len1 = strlen(up);
    int len2 = strlen(low);

    for(i = 0; i<len1; i++){
        //ui8AdjustUp = ui8AdjustUp + ((up[len1 - (i + 1)] - '0') * pow(10, i));
        ui8AdjustUp = ui8AdjustUp * 10 + (up[i]-48);
    }
    for(i = 0; i<len2; i++){
        //ui8AdjustLow = ui8AdjustLow + ((low[len2 - (i + 1)] - '0') * pow(10, i));
        ui8AdjustLow = ui8AdjustLow * 10 + (low[i] -48);
    }
}
