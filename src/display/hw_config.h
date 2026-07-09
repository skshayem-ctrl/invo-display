#pragma once

#define LVGL_DRAW_BUF_LINES  120
#define LVGL_TICK_PERIOD_MS  2
#define MIPI_DSI_DPI_CLK_MHZ 80
#define LCD_H_RES  720
#define LCD_V_RES  720
#define LCD_HSYNC  20
#define LCD_HBP    20
#define LCD_HFP    40
#define LCD_RST_GPIO 27
#define LCD_VSYNC  4
#define LCD_VBP    12
#define LCD_VFP    24
#define LCD_BK_LIGHT_GPIO 26
#define MIPI_DSI_LANE_NUM  2
#define MIPI_DSI_LANE_MBPS 1500
#define DSI_PHY_LDO_CHAN   3
#define DSI_PHY_LDO_MV     2500
#define TOUCH_I2C_SDA  7
#define TOUCH_I2C_SCL  8
#define TOUCH_RST_GPIO 23
#define TOUCH_INT_GPIO (-1)
#define STARTUP_PHASES 10

/* RS485 / Modbus RTU — UART1 on J8 dedicated TXD/RXD pins */
#define MB_UART_NUM    UART_NUM_1
#define MB_UART_TX     37  /* J8 TXD pin (UART0 native, routed here via GPIO matrix) */
#define MB_UART_RX     38  /* J8 RXD pin */
#define MB_UART_DE     22  /* direction control → module EN (HIGH=TX, LOW=RX) */
#define MB_BAUD        9600

/* DALY BMS RS485 — UART2 */
#define BMS_UART_NUM   UART_NUM_2
#define BMS_UART_TX    4
#define BMS_UART_RX    5
#define BMS_UART_DE    3
#define BMS_BAUD       9600

