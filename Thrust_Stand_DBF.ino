/* DBF THRUST STAND CONTROLLER FOR ARDUINO NANO
 * 
 * OPERATION OF THRUST STAND:
 *
 * Insert SD card, then reset Arduino Nano.
 * Status light should be red until the SD card is opened, then turn green.
 * If status light never turns green, data will not write to card.
 * Inserting a card without reset will not allow writing until the next reset.
 * Green light does not imply that data is correct, 
 * but does imply that load cell was recognized.
 * 
 * Data will always write to Serial Monitor.
 * Lack of SD card or USB cable will not interfere with each other.
 * Loss of power will not interfere with data saving to SD card
 * Serial to .txt: https://freeware.the-meiers.org/
 * Serial to excel: Not implemented :(
 * 
 * Data on SD card will be named "THRUST_XX.TXT".
 * XX is the number of the test performed.
 * The Arduino Nano counts tests 1-by-1, increasing,  
 * and resets the counter to 0 after 99.
 * 
 * Data format: (Milliseconds since reset), (Thrust in grams)
 */

// HX722 Libraries
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

// Display library
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// MicroSD Libraries
#include <SPI.h>
#include <SD.h>

// SD pins:
#define HX711_dout 2 //mcu > HX711 dout pin
#define HX711_sck  3 //mcu > HX711 sck pin

// HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

// LCD constructior:
LiquidCrystal_I2C lcd(0x27,20,4);

const int calVal_eepromAdress = 0;
unsigned long t = 0;

// SD File
File myFile;
long index;
String filename;

// Status LEDs
#define RED A1
#define GREEN A2

void setup() {
    pinMode(RED, OUTPUT);
    pinMode(GREEN, OUTPUT);
    greenLight(false);
    Serial.begin(57600); delay(10);
    Serial.println();
    Wire.begin();

    init_SD();
    init_Display();
    init_LoadCell();
}

void loop() {
    static boolean newDataReady = 0;
    const int serialPrintInterval = 0; //increase value to slow down serial print activity
    
    // check for new data/start next conversion:
    if (LoadCell.update()) newDataReady = true;
    
    // get smoothed value from the dataset:
    if (newDataReady) {
        if (millis() > t + serialPrintInterval) {
            float i = LoadCell.getData();
            t = millis();
            Serial.println("" + String(t) + "," + String(i, 4));
            write_Data_To_LCD(i);
            newDataReady = 0;
            
            // Write data to file
            myFile = SD.open(filename, FILE_WRITE);
            if (myFile) {
                myFile.println("" + String(t) + "," + String(i, 4));
                myFile.close();
            }
        }
    }
}

// Multiplexed 2x1 LED matrix controller
// Green light turns on if SD setup is completed
void greenLight(bool do_green) {
    digitalWrite(RED, !do_green);
    digitalWrite(GREEN, do_green);
}

// Initialize the SD card
void init_SD () {
    Serial.println("SD card starting...");
    // Initialize SD card
    if (!SD.begin(10)) {
        Serial.println("SD initialization failed!");
    }
    Serial.println("SD initialization done.");

    create_Filename();
    myFile = SD.open(filename, FILE_WRITE);
    delay(500);
    Serial.println("Attempting to open file: " + filename + " --> " + myFile);
    if (myFile) {
        Serial.println("Open SD file passed!");
        myFile.println("Start of test " + String(index));
        myFile.close();
        greenLight(true);
    } else {
        Serial.println("Open SD file failed!");
        greenLight(false);
    }
}

// Create the filename for the SD output.  Loops back from 99 to 0.
// Format: TEST_XX.txt.  "XX" = "0X" for XX < 10.
// Filename limit: filename<9 characters + ".txt"
// I.E. THRUST_XX = 9 characters = Illegal
void create_Filename() {
    // Get file index, set to mod(100) if necessary
    EEPROM.get(calVal_eepromAdress + 4, index);
    index++;
    if (index > 99) {
        index = 0;
    }
    EEPROM.put(calVal_eepromAdress + 4, index);    
    
    // Open file, turn green LED on if successful
    String extra_0 = "";
    if (index < 10) {
        extra_0 = "0";
    }
    filename = "TEST_" + extra_0 + String(index) + ".txt";
}

// Initialize load cell
void init_LoadCell () {
    Serial.println("HX711 starting...");
    LoadCell.begin();
    //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
    float calibrationValue; // calibration value (see example file "Calibration.ino")
    //calibrationValue = 696.0; // uncomment this if you want to set the calibration value in the sketch
#if defined(ESP8266)|| defined(ESP32)
    //EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
#endif
    EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom

    // Start the load cell
    unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
    boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
    LoadCell.start(stabilizingtime, _tare);
    if (LoadCell.getTareTimeoutFlag()) {
        Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
        greenLight(false);
        declare_Load_Cell_Failure();
        while (1);
    }
    else {
        LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
        Serial.println("HX711 startup is complete");
    }
}

// Initialize 20x4 LCD with I2C backpack
void init_Display () {
    Serial.println("LCD starting...");
    
    // Check if file loaded properly
    myFile = SD.open(filename);
    boolean file_Success = myFile;
    myFile.close();

    // Start LCD
    lcd.init();
    lcd.backlight();

    // Write filename
    lcd.setCursor(0,0);
    if (file_Success) {
        lcd.print("Writing: ");
    } else {
        lcd.print("Failed: ");
    }
    lcd.print(filename);
}

// Write current time and data
void write_Data_To_LCD (float dataIn) {
    // Write time in ms
    lcd.setCursor(0, 2);
    lcd.print("Time (ms): " + String(t));
    // Write data
    lcd.setCursor(0, 3);
    // Clear old data if necessary
    lcd.print("Thrust (g): " + String(dataIn, 2));
    if ((dataIn < 10) && (dataIn > 0)) {
        lcd.print("    ");
    } else if ((dataIn < 100) && (dataIn > -10))  {
        lcd.print("   ");
    } else if ((dataIn < 1000) && (dataIn > -100))  {
        lcd.print("  ");
    } else if ((dataIn < 10000) && (dataIn > -1000))  {
        lcd.print(" ");
    }
}

void declare_Load_Cell_Failure () {
    lcd.setCursor(0, 0);
    lcd.print("LOAD CELL INIT FAIL!");
}
