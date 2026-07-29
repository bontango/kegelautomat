#ifndef GPIODEFS_H
#define GPIODEFS_H

/* =====================================================================
 *  GPIO assignments Kegelautomat  --  ESP32-S3-WROOM-1-N16R8
 *  Board revision v1.0        (details: Steuerplatine_Doku.md, section 5)
 *  Date: 2026-07-29           (pin numbers unchanged since 2026-07-14)
 *
 *  The ESP32-S3 GPIO matrix lets almost every signal be re-mapped
 *  freely. Route the PCB the easy way first, then enter the real pin
 *  numbers here. Only the DO-NOT-USE list below is off limits.
 *
 *  ---- DO-NOT-USE LIST ---------------------------------------------
 *   GPIO 26-32 : SPI flash                       -> n/a
 *   GPIO 33-37 : Octal PSRAM (module "R8")       -> NEVER use
 *   GPIO 19,20 : USB D-/D+ (native USB-Serial/JTAG) -> NOT used (nc), free
 *   GPIO 43,44 : UART0 TX/RX                      -> CH340C bridge
 *                                                    (programming/console via USB-C)
 *   GPIO 0     : boot strap + BOOT button         -> keep as input/boot
 *   GPIO 45,46 : strapping (VDD_SPI / boot-ROM)   -> avoid
 *   GPIO 3     : strapping (JTAG select)          -> usable (= HC595 SER),
 *                                                    10k pulldown to GND on the board
 *   GPIO 22-25 : do not exist on the S3
 *
 *  NOT 5 V tolerant: never pull any GPIO above ~3.6 V.
 *  Free GPIO pool (usable): 0-18, 21, 38-42, 47, 48 (mind strapping).
 *  Unused by this mapping: GPIO 39-42 (JTAG) + 19/20 (native USB) -> spare IOs.
 * ===================================================================== */

// 3 Digital Pins for I2S / MAX98357A (audio.c)
#define I2S_BLK_PIN        21    // Bit Clock (BCLK)
#define I2S_WS_PIN         47    // Word Select (LRCLK)
#define I2S_DATA_OUT_PIN   14   // Serial Data Out (DIN to MAX98357A)

// 3 Digital Pins for buttons/dips (buttons.c)
// DIP1-3 share the I2S lines, decoupled by 3 diodes (cathode towards the ESP pin).
// Read: drive READ_DIP_GPIO HIGH, I2S pins as inputs with internal pulldown.
// Only read the DIPs while I2S is idle; keep READ_DIP_GPIO LOW during playback.
#define ADJUST_GPIO        4
#define SET_GPIO           5
#define READ_DIP_GPIO      13   // common pole of the DIP switch (HIGH = read)
#define DIP1_GPIO          I2S_WS_PIN
#define DIP2_GPIO          I2S_BLK_PIN
#define DIP3_GPIO          I2S_DATA_OUT_PIN

// 4 Digital Pins for SD card, SPI2 host (sdcard.c)
#define PIN_NUM_MISO       48
#define PIN_NUM_MOSI       2
#define PIN_NUM_CLK        38
#define PIN_NUM_CS         1

// 6 Digital Pins on SPI3 host: MAX7221 display + MCP23S17 contacts
// SCLK/MOSI drive the 74HCT541 (-> MAX7221, 5 V) AND the MCP directly (3.3 V)
// Display = multiplexed 8x8 matrix (8 SEG + 8 DIG) over a ~1 m 34-pin ribbon.
// MAX7221 (not 7219): real CS + slew-limited segment drivers -> long-cable friendly.
// Add 68-100 ohm series R at the 541 outputs on CLK/DIN/CS; run this device at ~1 MHz.
// Both CS lines carry a 10k pull-up to 3V3 on the board (R58 / R57, v1.0) so they are
// HIGH from power-on -- neither chip clocks in a stray frame before spibus_init().
#define SPI3_SCLK_PIN      17
#define SPI3_MOSI_PIN      8
#define SPI3_MISO_PIN      7   // only the MCP23S17 drives MISO
#define MAX7221_CS_PIN     18   // = CS/LOAD pin, active-LOW latch (displays.c)
#define MCP23S17_CS_PIN    15   // (contacts.c)
#define MCP23S17_INT_PIN   6  // INTA/INTB mirrored -> single INT line

// 4 Digital Pins for 74HC595 lamp cascade, dedicated IOs (lamps.c)
// SER/SRCLK/RCLK via 74HCT541 (5 V); /OE via 2N7002 (T33, open-drain,
// 10k gate pulldown + 10k drain pull-up to 5 V -> lamps blanked during boot)
#define HC595_SER_PIN      3    // strapping pin (JTAG select), output only -> ok
#define HC595_SRCLK_PIN    10
#define HC595_RCLK_PIN     9
#define HC595_OE_PIN       16  // INVERTED: HIGH = lamps ON, LOW/boot = OFF

// 2 Digital Pins for coils / coin gate, direct -> 74HCT541 -> IRL540 (coils.c)
#define COIL1_PIN          12
#define COIL2_PIN          11

#endif // GPIODEFS_H
