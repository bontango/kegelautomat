#ifndef GPIODEFS_H
#define GPIODEFS_H

/* =====================================================================
 *  GPIO assignments Kegelautomat  --  ESP32-S3-WROOM-1-N16R8
 *  Date: 2026-07-14   (details: Steuerplatine_Doku.md, section 5)
 *
 *  The ESP32-S3 GPIO matrix lets almost every signal be re-mapped
 *  freely. Route the PCB the easy way first, then enter the real pin
 *  numbers here. Only the DO-NOT-USE list below is off limits.
 *
 *  ---- DO-NOT-USE LIST ---------------------------------------------
 *   GPIO 26-32 : SPI flash                       -> n/a
 *   GPIO 33-37 : Octal PSRAM (module "R8")       -> NEVER use
 *   GPIO 19,20 : USB D-/D+ (USB-Serial/JTAG)     -> flash/console
 *   GPIO 43,44 : UART0 TX/RX (boot log)          -> spare possible *
 *   GPIO 0     : boot strap + BOOT button        -> keep as input/boot
 *   GPIO 45,46 : strapping (VDD_SPI / boot-ROM)  -> avoid
 *   GPIO 3     : strapping (JTAG select)         -> usable (= I2S LRC)
 *   GPIO 22-25 : do not exist on the S3
 *   * 43/44 free only if the console runs over USB (brief boot log)
 *
 *  NOT 5 V tolerant: never pull any GPIO above ~3.6 V.
 *  Free GPIO pool (usable): 0-18, 21, 38-42, 47, 48 (mind strapping).
 * ===================================================================== */

// 3 Digital Pins for I2S / MAX98357A (audio.c)
#define I2S_BLK_PIN        9    // Bit Clock (BCLK)
#define I2S_WS_PIN         3    // Word Select (LRCLK)  [strapping pin, ok]
#define I2S_DATA_OUT_PIN   10   // Serial Data Out (DIN to MAX98357A)

// 3 Digital Pins for buttons/dips (buttons.c)
// DIP1-3 share the I2S lines; enabled via READ_DIP_GPIO.
// Only read the DIPs while I2S is idle (buffer must be Hi-Z otherwise).
#define ADJUST_GPIO        4
#define SET_GPIO           5
#define READ_DIP_GPIO      12   // enables the DIP buffer (tri-state)
#define DIP1_GPIO          I2S_WS_PIN
#define DIP2_GPIO          I2S_BLK_PIN
#define DIP3_GPIO          I2S_DATA_OUT_PIN

// 4 Digital Pins for SD card, SPI2 host (sdcard.c)
#define PIN_NUM_MISO       48
#define PIN_NUM_MOSI       2
#define PIN_NUM_CLK        38
#define PIN_NUM_CS         1

// 6 Digital Pins on SPI3 host: MAX7219 display + MCP23S17 contacts
// SCLK/MOSI drive the 74HCT541 (-> MAX7219, 5 V) AND the MCP directly (3.3 V)
#define SPI3_SCLK_PIN      11
#define SPI3_MOSI_PIN      13
#define SPI3_MISO_PIN      14   // only the MCP23S17 drives MISO
#define MAX7219_CS_PIN     15   // = LOAD (displays.c)
#define MCP23S17_CS_PIN    16   // (contacts.c)
#define MCP23S17_INT_PIN   17   // INTA/INTB mirrored -> single INT line

// 4 Digital Pins for 74HC595 lamp cascade, dedicated IOs (lamps.c)
// SER/SRCLK/RCLK via 74HCT541 (5 V); /OE via 2N7002 (open-drain)
#define HC595_SER_PIN      6
#define HC595_SRCLK_PIN    7
#define HC595_RCLK_PIN     8
#define HC595_OE_PIN       18   // INVERTED: HIGH = lamps ON, LOW/boot = OFF

// 2 Digital Pins for coils / coin gate, direct -> 74HCT541 -> IRL540 (coils.c)
#define COIL1_PIN          21
#define COIL2_PIN          47

#endif // GPIODEFS_H
