#include "reg52.h"
#include "intrins.h"
#include <string.h>
#include <stdio.h>

#define SIZE 12
sfr AUXR = 0x8E;
sbit D5  = P3 ^ 7;
sbit D6  = P3 ^ 6;
sbit R_A = P3 ^ 2;
sbit R_B = P3 ^ 3;
sbit L_A = P3 ^ 4;
sbit L_B = P3 ^ 5;

char buffer[SIZE];
char str[5];
code char LJWL[]         = "AT+CWJAP=\"Call Me\",\"11946078\"\r\n";          // 入网指令
code char LJFWQ[]        = "AT+CIPSTART=\"TCP\",\"120.77.79.24\",38082\r\n"; // 连接服务器指令
char TCMS[]              = "AT+CIPMODE=1\r\n";                               // 透传指令
char SJCS[]              = "AT+CIPSEND\r\n";                                 // 数据传输开始指令
char RESET[]             = "AT+RST\r\n";                                     // 重启模块指令
char AT_OK_Flag          = 0;                                                // OK返回值的标志位
char AT_Connect_Net_Flag = 0;                                                // WIFI GOT IP返回值的标志位
char LED_MODE            = 0;
int count                = 0;

void UartInit(void) // 9600bps@11.0592MHz
{
    AUXR = 0x01;
    SCON = 0x50; // 配置串口工作方式1，REN使能接收
    TMOD &= 0xF0;
    TMOD |= 0x20; // 定时器1工作方式位8位自动重装

    TH1 = 0xFD;
    TL1 = 0xFD; // 9600波特率的初值
    TR1 = 1;    // 启动定时器

    EA = 1; // 开启总中断
    ES = 1; // 开启串口中断
    // IP |= 0x10;
}

void Timer0Init(void) // 定时器0初始化
{
    TMOD &= 0xF0; // 清除定时器0工作方式位
    TMOD |= 0x01; // 设置定时器0为模式1，16位计数器

    TH0 = 0xFC;
    TL0 = 0x00;

    ET0 = 1; // 允许定时器0中断
    EA  = 1; // 开启总中断
    TR0 = 1; // 启动定时器0
}

void go_down()
{
    R_A = 1;
    R_B = 0;
    L_A = 1;
    L_B = 0;
}

void stop()
{
    R_A = 0;
    R_B = 0;
    L_A = 0;
    L_B = 0;
}

void go_right()
{
    R_A = 1;
    R_B = 0;
    L_A = 0;
    L_B = 0;
}

void go_left()
{
    R_A = 0;
    R_B = 0;
    L_A = 1;
    L_B = 0;
}

void go_up()
{
    R_A = 0;
    R_B = 1;
    L_A = 0;
    L_B = 1;
}

void Delay1000ms() //@11.0592MHz
{
    unsigned char i, j, k;

    _nop_();
    i = 8;
    j = 1;
    k = 243;
    do {
        do {
            while (--k)
                ;
        } while (--j);
    } while (--i);
}

void sendByte(char data_msg)
{
    SBUF = data_msg;
    while (!TI)
        ;
    TI = 0;
}

void sendString(char *str)
{
    while (*str != '\0') {
        sendByte(*str);
        str++;
    }
}

void main()
{
    int mark = 0;
    D5 = D6 = 1; // 灭状态灯
    // 配置C51串口的通信方式
    UartInit();
    Timer0Init();
    Delay1000ms(); // 给espwifi模块上电时间

    // 发送联网AT指令并等待成功
    sendString(LJWL);
    // while(!AT_Connect_Net_Flag);
    while (!AT_OK_Flag)
        ;
    AT_OK_Flag = 0;
    // 发送连服务器指令并等待成功
    sendString(LJFWQ);
    while (!AT_OK_Flag)
        ;
    AT_OK_Flag = 0;
    // 发送透传模式指令并等待成功
    sendString(TCMS);
    while (!AT_OK_Flag)
        ;
    AT_OK_Flag = 0;
    // 发送数据传输指令并等待成功
    sendString(SJCS);
    while (!AT_OK_Flag)
        ;

    if (AT_Connect_Net_Flag) {
        D5 = 0; // 点亮D5,代表入网成功
    }
    if (AT_OK_Flag) {
        D6 = 0; // 点亮D6,代表连接服务器并打开透传模式成功
    }

    while (1) {

        // if (LED_MODE == 3) {
        //     D5 ^= 1;
        //     D6 ^= 1;
        // }
    }
}

void Uart_Handler() interrupt 4
{
    static int i = 0; // 静态变量，被初始化一次
    char tmp;

    if (RI) // 中断处理函数中，对于接收中断的响应
    {
        RI  = 0; // 清除接收中断标志位
        tmp = SBUF;
        if (tmp == 'W' || tmp == 'O' || tmp == 'L' || tmp == 'F' || tmp == 'a') {
            i = 0;
        }
        buffer[i++] = tmp;
        buffer[i]   = '\0';
        // 入网成功的判断依据WIFI GOT IP
        if (buffer[0] == 'W' && buffer[5] == 'G') {
            AT_Connect_Net_Flag = 1;
            memset(buffer, '\0', SIZE);
        }
        // 连接服务器等OK返回值指令的判断
        if (buffer[0] == 'O' && buffer[1] == 'K') {
            AT_OK_Flag = 1;
            memset(buffer, '\0', SIZE);
        }
        // 联网失败出现FAIL字样捕获
        if (buffer[0] == 'F' && buffer[1] == 'A') {
            for (i = 0; i < 5; i++) {
                D5 = 0;
                Delay1000ms();
                D5 = 1;
                Delay1000ms();
            }
            sendString(RESET);
            memset(buffer, '\0', SIZE);
        }


        // 判断各种指令并执行相应操作
        if ( buffer[0]=='a' && buffer[3] == 'u' ) {
            D6 = 1;
            go_up();
            memset(buffer, '\0', SIZE);
        }
        else if (buffer[0]=='a' && buffer[3] == 'd') {
            D6 = 0;
            go_down();
            memset(buffer, '\0', SIZE);
        }
        else if (buffer[0]=='a' && buffer[3] == 's') {
            stop();
            memset(buffer, '\0', SIZE);
        } 
        else if (buffer[0]=='a' && buffer[3] == 'l') {
            go_left();
            D5 = 0;
            memset(buffer, '\0', SIZE);
        } 
        else if (buffer[0]=='a' && buffer[3] == 'r') {
            go_right();
            D5 = 1;
            memset(buffer, '\0', SIZE);
        }
        else {
            // Handle unrecognized commands or perform default action
            // sendString("Unrecognized command");
            // Do something else if needed
        }

        if (i >= 12) i = 0;
    }
}


void Timer0_ISR(void) interrupt 1 // 定时器0中断服务程序
{

    count++; // 计数器自增

    if (count == 10) // 计数到1000时执行操作
    {
        // D6 = !D6;
        // 执行需要的操作
        count = 0; // 计数器清零
        sendString("...");
    }
}