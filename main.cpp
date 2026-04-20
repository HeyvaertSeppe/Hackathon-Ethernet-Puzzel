Hier is de opgeschoonde versie van je code. Ik heb de schreeuwerige blok-comments weggehaald, de u achtervoegsels bij de getallen verwijderd (die maken het vaak wat rommelig om te lezen), en de uitleg aangepast naar beknopte en professionele comments.
```cpp
#include "project.h"
#include <stdio.h>
#include <string.h>

extern "C" {
    // Redirect printf() output naar de UART component
    int _write(int file, char *ptr, int len) {
        for (int i = 0; i < len; i++) {
            UART_UartPutChar(ptr[i]);
        }
        return len;
    }
}

volatile uint32_t millis_teller = 0;

extern "C" CY_ISR(SysTick_Handler) {
    millis_teller++;
}

// LCD adressering en instellingen
#define LCD_I2C_ADDR    0x27
#define LCD_BACKLIGHT   0x08
#define LCD_ENABLE      0x04
#define LCD_RS          0x01

static void lcd_i2c_schrijf_byte(uint8_t data) {
    static uint8_t i2c_buffer; 
    i2c_buffer = data | LCD_BACKLIGHT;
    I2C_1_I2CMasterWriteBuf(LCD_I2C_ADDR, &i2c_buffer, 1, I2C_1_I2C_MODE_COMPLETE_XFER);
    while (I2C_1_I2CMasterStatus() & I2C_1_I2C_MSTAT_XFER_INP) {}
    I2C_1_I2CMasterClearStatus();
}

static void lcd_pulseer_enable(uint8_t data) {
    lcd_i2c_schrijf_byte(data | LCD_ENABLE);
    CyDelayUs(2);
    lcd_i2c_schrijf_byte(data & ~LCD_ENABLE);
    CyDelayUs(100);
}

static void lcd_verstuur_nibble(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    lcd_i2c_schrijf_byte(data);
    lcd_pulseer_enable(data);
}

static void lcd_verstuur_byte(uint8_t value, uint8_t mode) {
    lcd_verstuur_nibble(value & 0xF0, mode);
    lcd_verstuur_nibble((value << 4) & 0xF0, mode);
}

void lcd_commando(uint8_t cmd) { 
    lcd_verstuur_byte(cmd, 0x00); 
}

void lcd_schrijf_karakter(char c) { 
    lcd_verstuur_byte((uint8_t)c, LCD_RS); 
}

void lcd_start(void) {
    CyDelay(50);
    lcd_verstuur_nibble(0x30, 0x00); CyDelay(5);
    lcd_verstuur_nibble(0x30, 0x00); CyDelayUs(150);
    lcd_verstuur_nibble(0x30, 0x00);
    lcd_verstuur_nibble(0x20, 0x00);
    
    lcd_commando(0x28);
    lcd_commando(0x0C);
    lcd_commando(0x06);
    lcd_commando(0x01);
    CyDelay(2);
}

void lcd_wissen(void) {
    lcd_commando(0x01);
    CyDelay(2);
}

void lcd_zet_cursor(uint8_t kolom, uint8_t rij) {
    const uint8_t offsets[] = {0x00, 0x40, 0x14, 0x54};
    if (rij > 3) rij = 3; 
    lcd_commando(0x80 | (kolom + offsets[rij]));
}

void lcd_print_gecentreerd(uint8_t rij, const char *tekst) {
    uint8_t lengte = strlen(tekst);
    if (lengte > 20) lengte = 20; 
    
    uint8_t spatiesLinks = (20 - lengte) / 2;
    uint8_t spatiesRechts = 20 - lengte - spatiesLinks;

    lcd_zet_cursor(0, rij);
    for (uint8_t i = 0; i < spatiesLinks; i++) lcd_schrijf_karakter(' ');
    for (uint8_t i = 0; i < lengte; i++) lcd_schrijf_karakter(tekst[i]);
    for (uint8_t i = 0; i < spatiesRechts; i++) lcd_schrijf_karakter(' ');
}

// Leest de fysieke status van de modulepinnen in
static int leesModulePin(int moduleNummer) {
    uint8_t waarde = 0xFF; 
    switch (moduleNummer) {
        case 0: waarde = Module_0_Read(); break;
        case 1: waarde = Module_1_Read(); break;
        case 2: waarde = Module_2_Read(); break;
        case 3: waarde = Module_3_Read(); break;
        case 4: waarde = Module_4_Read(); break;
        default: break;
    }
    
    // Bepaal welke specifieke plug in deze module zit
    for (int j = 0; j < 5; j++) {
        if ((waarde & (1 << j)) == 0) return j; 
    }
    return -1;
}

// State machine definities
enum SpelStatus {
    WACHTEN,
    SPELEN,
    OPGELOST
};

static SpelStatus huidigeStatus = WACHTEN;

static int      doelPinnen[5];
static int      laatstePlugStatus[5];  
static uint32_t startTijd        = 0;
static uint32_t opgelostTijd     = 0;
static uint32_t laatsteLcdUpdate = 0; 
static uint32_t laatsteLedBlink  = 0;
static bool     lcdKnipperStatus = false;

static uint32_t willekeurig_getal_seed = 0xACE1;

static uint32_t genereer_willekeurig(void) {
    willekeurig_getal_seed ^= willekeurig_getal_seed << 13;
    willekeurig_getal_seed ^= willekeurig_getal_seed >> 17;
    willekeurig_getal_seed ^= willekeurig_getal_seed << 5;
    return willekeurig_getal_seed;
}

static void berekenNieuweCode(void) {
    int huidigePlugs[5];
    for(int i = 0; i < 5; i++) {
        huidigePlugs[i] = leesModulePin(i);
        laatstePlugStatus[i] = huidigePlugs[i];
    }

    int posities[5] = {0, 1, 2, 3, 4};
    bool codeIsGoed = false;

    // Genereer een code totdat we er een hebben die niet per ongeluk al direct klopt 
    // met de huidige positie van de pinnen (anti-cheat).
    while (!codeIsGoed) {
        for (int i = 4; i > 0; i--) {
            int j = (int)(genereer_willekeurig() % (uint32_t)(i + 1));
            int tijdelijk = posities[i];
            posities[i] = posities[j];
            posities[j] = tijdelijk;
        }

        codeIsGoed = true; 
        for (int i = 0; i < 5; i++) {
            if (huidigePlugs[i] != -1 && posities[i] == huidigePlugs[i]) {
                codeIsGoed = false; 
                break; 
            }
        }
    }

    for (int i = 0; i < 5; i++) {
        doelPinnen[i] = posities[i];
    }
    
    printf("\r\n================================\r\n");
    printf("Nieuwe puzzelcode gegenereerd.\r\n");
    for(int i = 0; i < 5; i++) {
        printf("Module %d -> Plug %d\r\n", i, doelPinnen[i]);
    }
    printf("================================\r\n\n");
}

static void logica_Wachten(void) {
    uint32_t nu = millis_teller;
    RedLED_Write(0);
    GreenLED_Write(0);
    Done_Pin_Write(0);

    // Update het LCD periodiek op 1Hz
    if ((nu - laatsteLcdUpdate) > 1000) {
        laatsteLcdUpdate = nu;
        lcd_print_gecentreerd(0, ""); 
        lcd_print_gecentreerd(1, "Los andere puzzels");
        lcd_print_gecentreerd(2, "op om te activeren");
        lcd_print_gecentreerd(3, "");
    }

    // Trigger vanuit de ESP32 via pin P0[5]
    if (Activate_Pin_Read() == 1) {
        CyDelay(50); // Debounce
        if (Activate_Pin_Read() == 1) {
            printf("\r\nActivatiesignaal ontvangen. Puzzel start.\r\n");
            
            willekeurig_getal_seed ^= millis_teller; 
            berekenNieuweCode();
            
            huidigeStatus = SPELEN;
            startTijd = millis_teller; 
            laatsteLedBlink = millis_teller;
        }
    }
}

static void logica_Spelen(void) {
    uint32_t nu = millis_teller;
    uint32_t verstrekenTijd = nu - startTijd;

    if ((nu - laatsteLedBlink) > 500) {
        RedLED_Write(RedLED_Read() ^ 1);
        laatsteLedBlink = nu;
    }

    int aantalIngeplugd = 0;
    int aantalCorrect   = 0;

    for (int i = 0; i < 5; i++) {
        int gelezenPin = leesModulePin(i);
        
        // Log wijzigingen in pin statussen naar de terminal
        if (gelezenPin != laatstePlugStatus[i]) {
            if (gelezenPin != -1) {
                printf("Module %d: Plug %d", i, gelezenPin);
                if (gelezenPin == doelPinnen[i]) {
                    printf(" -> CORRECT\r\n");
                } else {
                    printf(" -> FOUT (verwacht %d)\r\n", doelPinnen[i]);
                }
            } else {
                printf("Module %d: Losgekoppeld\r\n", i);
            }
            laatstePlugStatus[i] = gelezenPin; 
        }

        if (gelezenPin != -1) {
            aantalIngeplugd++;
            if (gelezenPin == doelPinnen[i]) {
                aantalCorrect++;
            }
        }
    }

    // Controleer de win-conditie
    if (aantalIngeplugd == 5 && aantalCorrect == 5) {
        printf("\r\n*** Puzzel succesvol opgelost ***\r\n");
        huidigeStatus = OPGELOST;
        opgelostTijd  = nu;
        
        Done_Pin_Write(1); 
        RedLED_Write(0);
        GreenLED_Write(1);
        return; 
    }

    // LCD refresh logica op basis van de verstreken tijd
    if ((nu - laatsteLcdUpdate) > 200) {
        laatsteLcdUpdate = nu;

        if (verstrekenTijd < 10000) {
            if ((verstrekenTijd % 1000) < 500) {
                lcd_print_gecentreerd(0, "");
                lcd_print_gecentreerd(1, "Welkom bij de");
                lcd_print_gecentreerd(2, "ethernet puzzel");
                lcd_print_gecentreerd(3, "");
            } else {
                lcd_print_gecentreerd(0, "");
                lcd_print_gecentreerd(1, "");
                lcd_print_gecentreerd(2, "");
                lcd_print_gecentreerd(3, "");
            }
        } 
        else if (verstrekenTijd < 20000) {
            lcd_print_gecentreerd(0, "");
            lcd_print_gecentreerd(1, "Welkom bij de");
            lcd_print_gecentreerd(2, "ethernet puzzel");
            lcd_print_gecentreerd(3, "");
        } 
        else if (verstrekenTijd < 25000) {
            lcd_print_gecentreerd(0, "");
            lcd_print_gecentreerd(1, "Steek alle plugs in");
            lcd_print_gecentreerd(2, "de juiste volgorde");
            lcd_print_gecentreerd(3, "");
        } 
        else {
            if (aantalIngeplugd < 5) {
                lcd_print_gecentreerd(0, "");
                lcd_print_gecentreerd(1, "Steek alle plugs in");
                lcd_print_gecentreerd(2, "de juiste volgorde");
                lcd_print_gecentreerd(3, "");
            } else {
                char tekstBuffer[21]; 
                if (aantalCorrect == 1) {
                    snprintf(tekstBuffer, sizeof(tekstBuffer), "1 is correct");
                } else {
                    snprintf(tekstBuffer, sizeof(tekstBuffer), "%d zijn correct", aantalCorrect);
                }
                lcd_print_gecentreerd(0, "");
                lcd_print_gecentreerd(1, tekstBuffer);
                lcd_print_gecentreerd(2, "probeer nog eens");
                lcd_print_gecentreerd(3, "");
            }
        }
    }
}

static void logica_Opgelost(void) {
    uint32_t nu = millis_teller;
    
    // Visuele feedback op het LCD na afronding
    if ((nu - laatsteLcdUpdate) > 400) {
        lcdKnipperStatus = !lcdKnipperStatus;
        if (lcdKnipperStatus) {
            lcd_print_gecentreerd(0, "");
            lcd_print_gecentreerd(1, "ALLES JUIST!!!");
            lcd_print_gecentreerd(2, "!!!!!!!!!!!!!!!!");
            lcd_print_gecentreerd(3, "");
        } else {
            lcd_print_gecentreerd(0, "");
            lcd_print_gecentreerd(1, "");
            lcd_print_gecentreerd(2, "");
            lcd_print_gecentreerd(3, "");
        }
        laatsteLcdUpdate = nu;
    }

    // Auto-reset na 30 seconden
    if ((nu - opgelostTijd) > 30000) {
        printf("\r\n--- Puzzel is gereset ---\r\n");
        huidigeStatus = WACHTEN;
    }
}

int main(void) {
    CyGlobalIntEnable;
    
    CySysTickStart();
    CySysTickSetCallback(0, SysTick_Handler);
    CySysTickSetReload(48000 - 1);
    
    UART_Start();
    printf("\r\n\r\n--- SYSTEEM OPGESTART ---\r\n");

    I2C_1_Start();
    lcd_start();

    willekeurig_getal_seed = millis_teller ^ 0xDEADBEEF;
    huidigeStatus = WACHTEN;
    laatsteLcdUpdate = 0;

    for (;;) {
        switch(huidigeStatus) {
            case WACHTEN: 
                logica_Wachten(); 
                break;
            case SPELEN:  
                logica_Spelen();  
                break;
            case OPGELOST:  
                logica_Opgelost();  
                break;
        }
    }
}

```
