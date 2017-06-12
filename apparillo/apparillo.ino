#include <string.h>

#include <FastLED.h>
#include <OneWire.h>


// LED config

#define LED_DATA_PIN 5
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

// iButton config and content

#define MEM_SIZE 512
#define SIGNIFICANT_BYTES 64
// pin 3 is data of iButton
OneWire ds(3);

// key levels
#define LEVEL_UNKNOWN  -1
#define LEVEL_STEWARD   0
#define LEVEL_OPERATOR  1
#define LEVEL_SERVICE   2

#define IS_STEWARD_KEY(buffer) memcmp(buffer,significantBytes[LEVEL_STEWARD],SIGNIFICANT_BYTES)==0
#define IS_OPERATOR_KEY(buffer) memcmp(buffer,significantBytes[LEVEL_OPERATOR],SIGNIFICANT_BYTES)==0
#define IS_SERVICE_KEY(buffer) memcmp(buffer,significantBytes[LEVEL_SERVICE],SIGNIFICANT_BYTES)==0

#define IS_STEWARD_BUTTON(val) (990 < val && val < 1010)
#define IS_OPERATOR_BUTTON(val) (500 < val && val < 520)
#define IS_SERVICE_BUTTON(val) (330 < val && val < 350)

const byte significantBytes[][SIGNIFICANT_BYTES] = {
    {
        // first 64 byte of steward key
        0x0F, 0xAA, 0x00, 0x80, 0x03, 0x00, 0x00, 0x00,
        0x42, 0x41, 0x52, 0x20, 0x01, 0x01, 0x01, 0x00, 
        0x62, 0x85, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
        0x1D, 0x00, 0x00, 0x4B, 0xB1, 0x5C, 0xB5, 0x70, 
        0xA7, 0x52, 0xB2, 0x4C, 0xB4, 0x4F, 0x94, 0x4C, 
        0xC1, 0x5C, 0xB4, 0xCF, 0xA3, 0x4B, 0xB1, 0x5C, 
        0xB4, 0x4E, 0x2E, 0xD9, 0x00, 0x00, 0x25, 0x05
    },
    {
        // first 64 byte of operator key
        0x0F, 0xAA, 0x00, 0x80, 0x03, 0x00, 0x00, 0x00,
        0x42, 0x41, 0x52, 0x20, 0x02, 0x01, 0x01, 0x00,
        0x62, 0xC1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x1D, 0x00, 0x00, 0x85, 0x77, 0x96, 0x7B, 0xAA,
        0x6D, 0x8C, 0x78, 0x86, 0x7A, 0x89, 0x5A, 0x86,
        0x88, 0x96, 0x7A, 0x09, 0x69, 0x85, 0x77, 0x96,
        0x7A, 0x88, 0x5F, 0xC3, 0x00, 0x00, 0x6D, 0xAC
    },
    {
        // first 64 byte of service key
        0x0F, 0xAA, 0x00, 0x80, 0x03, 0x00, 0x00, 0x00,
        0x42, 0x41, 0x52, 0x20, 0x03, 0x01, 0x01, 0x00,
        0x63, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x1D, 0x00, 0x00, 0x0E, 0xEE, 0x1F, 0xF2, 0x33,
        0xE4, 0x15, 0xEF, 0x0F, 0xF2, 0x12, 0xDD, 0x12, 
        0x03, 0x1F, 0xF1, 0x92, 0xE0, 0x0E, 0xEE, 0x1F, 
        0xF1, 0x11, 0x38, 0x8D, 0x00, 0x00, 0x83, 0xF0
    }
};

int flashRead(void) {
    int level = LEVEL_UNKNOWN;
    uint8_t buffer[SIGNIFICANT_BYTES];
    memset(buffer, 0, sizeof(buffer));  
    if (ds.reset()) {
        ds.write(0xCC);
        ds.write(0xF0);
        ds.write(0x00);
        ds.write(0x00);
        // Read each byte of memory
        for( int i = 0; i < SIGNIFICANT_BYTES; i++) {
            buffer[i] = ds.read();
        }
    }
    if (IS_STEWARD_KEY(buffer)) {
        Serial.println("Steward key");
        level = LEVEL_STEWARD;
    }
    else if (IS_OPERATOR_KEY(buffer)) {
        Serial.println("Operator key");
        level = LEVEL_OPERATOR;
    }
    else if (IS_SERVICE_KEY(buffer)) {
        Serial.println("Service key");
        level = LEVEL_SERVICE;
    }
    else {
        Serial.println("Unknown key");
    }
    return level;
}

void flashWrite(int level) {
    if (level < LEVEL_STEWARD || level > LEVEL_SERVICE) {
        Serial.println("Cannot write to flash: invalid level");
    }

    // Each loop is a 4 byte chunk
    uint8_t bytesPerWrite = 8;
    for(uint16_t offset = 0; offset < (SIGNIFICANT_BYTES / bytesPerWrite); offset++) {
        uint16_t memAddress = offset * bytesPerWrite;
        // Fail if presense fails
        if (!ds.reset()) { 
            Serial.println("Cannot write to flash: device removed"); 
            return; 
        }
        ds.write(0xCC); // Skip ROM
        ds.write(0x0F); // Write Scratch Pad

        // Compute the two byte address
        uint8_t a1 = (uint8_t) memAddress;
        uint8_t a2 = (uint8_t) (memAddress >> 8);

        // Write the offset bytes . . . why in this order?!?!?
        ds.write(a1);
        ds.write(a2);

        // Fill the 4 byte scratch pad
        for (uint8_t i = 0; i < bytesPerWrite; i++) {
            ds.write(significantBytes[level][memAddress + i]);
        }

        // Fail if presense fails
        if (!ds.reset()) { 
            Serial.write("Write to flash failed: device removed during write (1)"); 
            return; 
        }
        ds.write(0xCC); // Skip ROM
        ds.write(0xAA); // Read scratch pad

        // Confirm offset gets mirrored back, otherwise fail
        if(ds.read() != a1) {
            Serial.write("Write to flash failed: offset confirmation failed (1)");
            return;
        }
        if(ds.read() != a2) {
            Serial.write("Write to flash failed: offset confirmation failed (2)");
            return;
        }

        // Read the confirmation byte, store it
        uint8_t eaDS = ds.read();
        // Check the scratchpad contents
        for (int i = 0; i < bytesPerWrite; i++) {  
            uint8_t verifyByte = ds.read();
            if (verifyByte != significantBytes[level][(offset*bytesPerWrite + i)]) { 
            Serial.write("Write to flash failed: verification failed");
            return; 
            }
        }
        // Fail if presense fails
        if (!ds.reset()) { 
            Serial.write("Write to flash failed: device removed during write (2)");
            return; 
        }
        // Commit the 8 bytes above
        ds.write(0xCC); // Skip ROM
        ds.write(0x55); // Copy scratchpad

        // Confirmation bytes
        ds.write(a1);
        ds.write(a2);
        ds.write(eaDS, 1); // Pullup!
        delay(5); // Wait 5 ms per spec.
    }
    Serial.println("Flash written.");
}

CRGB colorForLevel(int level) {
    switch(level) {
    default:
    case LEVEL_UNKNOWN:
        return CRGB::Blue;
    case LEVEL_STEWARD:
        return CRGB::Green;
    case LEVEL_OPERATOR:
        return CRGB::Yellow;
    case LEVEL_SERVICE:
        return CRGB::Red;
    }
}

void setup(void) {
  // initialize inputs/outputs
  // start serial port
  Serial.begin(9600);
  
  // pin 2 is used as voltage for iButton reader
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  
  // pin 4 is used as voltage for LED
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  
  // pin 12 is ground for buttons
  pinMode(12,OUTPUT);
  digitalWrite(12,LOW);
  
  // pin 12 is voltage for buttons
  pinMode(12,OUTPUT);
  digitalWrite(12,HIGH);
  
  // setup led
  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(leds, NUM_LEDS);
}

void loop(void) {

    int level = LEVEL_UNKNOWN;

    if (ds.reset()) {
        Serial.println("device found");
        level = flashRead();
        
        int button = analogRead(14);
        
        if (IS_STEWARD_BUTTON(button)) {
            Serial.println("Steward button pressed");
            flashWrite(LEVEL_STEWARD);
            delay(1000);
        }
        if (IS_OPERATOR_BUTTON(button)) {
            Serial.println("Operator button pressed");
            flashWrite(LEVEL_OPERATOR);
            delay(1000);
        }
        if (IS_SERVICE_BUTTON(button)) {
            Serial.println("Service button pressed");
            flashWrite(LEVEL_SERVICE);
            delay(1000);
        }
    }
    leds[0] = colorForLevel(level);
    FastLED.show();
}






