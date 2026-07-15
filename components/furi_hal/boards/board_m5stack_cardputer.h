/**
 * @file board_m5stack_cardputer.h
 * Board definition: M5Stack Cardputer (Standard v1.0 / v1.1)
 *
 * MCU:      ESP32-S3FN8 (dual-core Xtensa LX7, 8 MB Flash, NO PSRAM)
 * Display:  ST7789V2 240×135 RGB565 via SPI2
 * Input:    8×7 matrix keyboard via 74HC138 decoder (56 physical keys)
 * SD Card:  SPI3 (separate bus from LCD — CS=12, MOSI=14, MISO=39, SCLK=40)
 * IR:       TX (GPIO44), no RX
 * Speaker:  NS4168 I2S Amp — BCLK=41, LRCK=43, SDATA=42
 * Mic:      PDM SPM1423 — DATA=46, CLK=43 (shares LRCK pin)
 * Grove:    SDA=2, SCL=1
 * USB:      CDC on GPIO19/GPIO20 (native USB-OTG)
 *
 * NOTE — No PSRAM:
 *   The standard Cardputer has NO PSRAM. SubGHz/NFC/NRF24 and wolf3d/doom
 *   are excluded via fam_config.py.  BOARD_HAS_SUBGHZ / BOARD_HAS_NFC = 0.
 *
 * NOTE — Display scaling:
 *   The Flipper 128×64 canvas is aspect-fit scaled to 240×120 and centred
 *   vertically on the 240×135 display (7 px margin top/bottom).
 *   This is handled automatically by furi_hal_display.c — no code change
 *   needed here.
 */

#pragma once

/* ---- Board metadata ---- */
#define BOARD_NAME        "M5Stack Cardputer"
#define BOARD_TARGET      "esp32s3"

/* ---- LCD (ST7789V2) via SPI2 ---- */
#define BOARD_PIN_LCD_MOSI      35
#define BOARD_PIN_LCD_SCLK      36
#define BOARD_PIN_LCD_CS        37
#define BOARD_PIN_LCD_DC        34
#define BOARD_PIN_LCD_RST       33
#define BOARD_PIN_LCD_BL        38

/* The LCD SPI bus is TX-only; MISO is not needed on SPI2.
 * Define it as -1 so furi_hal_display.c passes -1 to spi_bus_initialize. */
#define BOARD_PIN_LCD_SPI_MISO  (-1)

/* ---- LCD Display Configuration ---- */
#define BOARD_LCD_H_RES         240
#define BOARD_LCD_V_RES         135
#define BOARD_LCD_SPI_HOST      SPI2_HOST
#define BOARD_LCD_SPI_FREQ_HZ   (40 * 1000 * 1000)
#define BOARD_LCD_CMD_BITS      8
#define BOARD_LCD_PARAM_BITS    8
#define BOARD_LCD_SWAP_XY       true    /* Panel is landscape */
#define BOARD_LCD_MIRROR_X      true
#define BOARD_LCD_MIRROR_Y      false
#define BOARD_LCD_INVERT_COLOR  true    /* ST7789V2 uses inversion ON */
#define BOARD_LCD_GAP_X         40
#define BOARD_LCD_GAP_Y         53
#define BOARD_LCD_BL_ACTIVE_LOW false   /* Backlight active-high */
#define BOARD_LCD_COLOR_ORDER_BGR false

/* Flipper framebuffer color mapping (RGB565, ESP32-S3 SPI byte order) */
#define BOARD_LCD_FG_COLOR      0xA0FD  /* Flipper Orange 0xFDA0 byte-swapped */
#define BOARD_LCD_BG_COLOR      0x0000  /* Black */

/* ---- SD Card via SPI3 (separate from LCD SPI2) ---- */
#define BOARD_HAS_SD            1
#define BOARD_PIN_SD_CS         12
#define BOARD_PIN_SD_MOSI       14      
#define BOARD_PIN_SD_MISO       39      
#define BOARD_PIN_SD_SCLK       40      

/* Tell furi_hal_sd.c to use SPI3 instead of SPI2.
 * The SD driver will initialise its own bus on these pins. */
#define BOARD_SD_SPI_HOST       SPI3_HOST

/* ---- 74HC138 Keyboard Decoder (Original/v1.1) ---- */
#define BOARD_KB_TYPE_74HC138

/* 74HC138 Address Pins (Outputs) */
#define BOARD_KB_PIN_A0         8
#define BOARD_KB_PIN_A1         9
#define BOARD_KB_PIN_A2         11

/* Matrix Row Pins (Inputs) */
#define BOARD_KB_ROW_COUNT      7
#define BOARD_KB_COL_COUNT      8  /* From 74HC138 */

/* ---- Encoder / Rotary input — NOT PRESENT ---- */
#define BOARD_PIN_ENCODER_A     UINT16_MAX
#define BOARD_PIN_ENCODER_B     UINT16_MAX
#define BOARD_PIN_ENCODER_BTN   UINT16_MAX
#define BOARD_PIN_BUTTON_KEY    UINT16_MAX
#define BOARD_PIN_BUTTON_BOOT   0   /* BOOT/IO0 button (flash mode) */
#define BOARD_PIN_BATTERY_ADC   10  /* Battery voltage divider */

/* ---- Touch Controller — NOT PRESENT ---- */
#define BOARD_PIN_TOUCH_SCL     UINT16_MAX
#define BOARD_PIN_TOUCH_SDA     UINT16_MAX
#define BOARD_PIN_TOUCH_RST     UINT16_MAX
#define BOARD_PIN_TOUCH_INT     UINT16_MAX
#define BOARD_TOUCH_I2C_ADDR    0x00
#define BOARD_TOUCH_I2C_PORT    I2C_NUM_0
#define BOARD_TOUCH_I2C_FREQ_HZ 0
#define BOARD_TOUCH_I2C_TIMEOUT 0

/* ---- SubGHz / CC1101 — NOT PRESENT ---- */
#define BOARD_PIN_CC1101_SCK    UINT16_MAX
#define BOARD_PIN_CC1101_CSN    UINT16_MAX
#define BOARD_PIN_CC1101_MISO   UINT16_MAX
#define BOARD_PIN_CC1101_MOSI   UINT16_MAX
#define BOARD_PIN_CC1101_GDO0   UINT16_MAX
#define BOARD_PIN_CC1101_GDO2   UINT16_MAX
#define BOARD_PIN_CC1101_SW1    UINT16_MAX
#define BOARD_PIN_CC1101_SW0    UINT16_MAX
#define BOARD_CC1101_SPI_SHARED 0

/* ---- NRF24L01 — NOT PRESENT ---- */
#define BOARD_PIN_NRF24_SCK     UINT16_MAX
#define BOARD_PIN_NRF24_MISO    UINT16_MAX
#define BOARD_PIN_NRF24_MOSI    UINT16_MAX
#define BOARD_PIN_NRF24_CSN     UINT16_MAX
#define BOARD_PIN_NRF24_CE      UINT16_MAX
#define BOARD_HAS_NRF24         0

/* ---- Power Enable — NOT PRESENT ---- */
#define BOARD_PIN_PWR_EN        UINT16_MAX

/* ---- IR (TX only) ---- */
#define BOARD_PIN_IR_TX         44
#define BOARD_PIN_IR_RX         1

/* ---- NFC / PN532 ---- */
#define BOARD_PIN_NFC_SCL       BOARD_PIN_QWIIC_SCL
#define BOARD_PIN_NFC_SDA       BOARD_PIN_QWIIC_SDA
#define BOARD_PIN_NFC_IRQ       UINT16_MAX
#define BOARD_PIN_NFC_RST       UINT16_MAX
#define BOARD_NFC_I2C_PORT      I2C_NUM_1
#define BOARD_NFC_I2C_ADDR      0x28  /* WS1850S default */

/* ---- Speaker (NS4168 via I2S) ---- */
#define BOARD_PIN_SPEAKER_BCLK  41
#define BOARD_PIN_SPEAKER_WCLK  43
#define BOARD_PIN_SPEAKER_DOUT  42
#define FURI_HAL_SPEAKER_GPIO   BOARD_PIN_SPEAKER_DOUT

/* ---- Microphone (PDM) ---- */
#define BOARD_PIN_MIC_DATA      46
#define BOARD_PIN_MIC_CLK       43

/* ---- WS2812 RGB LED — NOT PRESENT ---- */
#define BOARD_PIN_WS2812_DATA   UINT16_MAX
#define BOARD_WS2812_LED_COUNT  0

/* ---- RFID / RDM6300 — NOT PRESENT ---- */
#define BOARD_PIN_RFID_RX       UINT16_MAX
#define BOARD_PIN_RFID_TX       UINT16_MAX
#define BOARD_RFID_UART_NUM     1

/* ---- Grove / Qwiic I2C ---- */
#define BOARD_PIN_QWIIC_SDA     2
#define BOARD_PIN_QWIIC_SCL     1
#define I2C_SDA_GPIO            BOARD_PIN_QWIIC_SDA
#define I2C_SCL_GPIO            BOARD_PIN_QWIIC_SCL

/* Aliases for furi_hal_i2c_bus.c (mapped to Grove port) */
#define KB_I2C_PIN_SDA          I2C_SDA_GPIO
#define KB_I2C_PIN_SCL          I2C_SCL_GPIO
#define KB_I2C_FREQ_HZ          400000

/* ---- Feature flags ---- */
#define BOARD_HAS_TOUCH         0
#define BOARD_HAS_ENCODER       0
#define BOARD_HAS_SD_CARD       1
#define BOARD_HAS_BLE           1
#define BOARD_HAS_RGB_LED       0
#define BOARD_HAS_VIBRO         0
#define BOARD_HAS_SPEAKER       1
#define BOARD_HAS_IR            1
#define BOARD_HAS_IR_RX         1
#define BOARD_HAS_IBUTTON       0
#define BOARD_HAS_RFID          0
#define BOARD_HAS_NFC           1
#define BOARD_HAS_SUBGHZ        0
#define BOARD_HAS_MIC           1

/* ---- Battery (no fuel gauge on standard Cardputer) ---- */
#define BQ27220_ADDR            0x00
#define BQ_I2C_PORT             I2C_NUM_0
#define BQ_I2C_SDA              UINT16_MAX
#define BQ_I2C_SCL              UINT16_MAX
#define HIGH_DRAIN_CURRENT_THRESHOLD (-200)
#define FURI_HAL_POWER_VIRTUAL_CAPACITY_MAH     (1520U)  /* 120 mAh internal + 1400 mAh base */
#define BQ25896_CHARGE_LIMIT    1280
#define FURI_HAL_POWER_ADC_DIVIDER_RATIO        (2.0f)
