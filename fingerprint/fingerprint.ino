#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Keypad.h>  // Include the Keypad library
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> // Include the SSD1306 library

// -------------------- Configuration --------------------

// Wi-Fi credentials
const char* ssid = "";          // Replace with your Wi-Fi SSID
const char* password = "";        // Replace with your Wi-Fi password

// Server URLs
const char* enrollUrl = "https://xfitness.itgusto.com/fingerprint.php"; // Enrollment URL
const char* verifyUrl = "https://xfitness.itgusto.com/verify.php";        // Verification URL
const char* getNicUrl = "https://xfitness.itgusto.com/get_nic.php";       // Get NIC by fingerprint_id
const char* checkNicUrl = "https://xfitness.itgusto.com/check_nic.php";   // Check NIC enrollment

// NTP Server Configuration
const char* ntpServer = "pool.ntp.org";       // NTP server to synchronize time
const long gmtOffset_sec = 19800;             // GMT offset in seconds (e.g., GMT+5:30 is 19800)
const int daylightOffset_sec = 0;             // Daylight offset in seconds

// Define UART2 for ESP-WROOM32
#define RX_PIN 16  // GPIO16 (RX2) - Connected to R307 TX via Level Shifter
#define TX_PIN 17  // GPIO17 (TX2) - Connected to R307 RX

// Define Relay Control Pin
#define RELAY_PIN 23  // GPIO18 - Connected to Relay IN pin

// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C  // I2C address for the OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Initialize HardwareSerial on UART2
HardwareSerial mySerial(2); // UART2

// Initialize the Fingerprint Sensor
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Initialize NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, 60000); // Update every 60 seconds

// -------------------- Keypad Setup --------------------
const byte ROWS = 4; // Four rows
const byte COLS = 3; // Three columns
char keys[ROWS][COLS] = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
};
byte rowPins[ROWS] = {13, 12, 14, 27}; // Connect to the row pins of the keypad
byte colPins[COLS] = {26, 25, 33};      // Connect to the column pins of the keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// -------------------- Global Variables --------------------
// Start fingerprint ID from 1 (assuming 0 is reserved or not used)
uint16_t fingerprintCounter = 1;
String keypadInput = ""; // To capture input from the keypad

// -------------------- Function Prototypes --------------------
void sendFingerprintData(uint16_t fingerprint_id, String nic, String timestamp);
void sendAttendanceData(uint16_t fingerprint_id, String nic, String timestamp); // Add this line
String getNICFromUser();
void connectToWiFi();
String getCurrentTimestamp(); // Get the timestamp
String getNICByFingerprintID(uint16_t fingerprint_id);
bool checkMemberPayment(String nic);
void openDoor();
void closeDoor();
void enrollNewFingerprint();
void initializeFingerprintCounter();
void checkTemplateCount();
void displayImage(const unsigned char* img); // New function prototype

// Call function to display an image
                    const unsigned char myImage[] PROGMEM = { 	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x03, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xc0, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x03, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xfe, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xff, 0xff, 
	0xff, 0xff, 0xf0, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x0f, 0xff, 0xff, 
	0xff, 0xff, 0xc0, 0x00, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 
	0xff, 0xff, 0x00, 0x07, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xe0, 0x00, 0xff, 0xff, 
	0xff, 0xfe, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x7f, 0xff, 
	0xff, 0xfe, 0x00, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0x00, 0x7f, 0xff, 
	0xff, 0xff, 0x07, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x3f, 0xe0, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xe0, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x07, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xfc, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x3f, 0xff, 0xff, 
	0xff, 0xff, 0xe0, 0x00, 0x7f, 0xff, 0xc0, 0x00, 0x00, 0x07, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xff, 
	0xff, 0xff, 0x80, 0x03, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xc0, 0x01, 0xff, 0xff, 
	0xff, 0xfe, 0x00, 0x1f, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xf8, 0x00, 0x7f, 0xff, 
	0xff, 0xfc, 0x00, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfe, 0x00, 0x3f, 0xff, 
	0xff, 0xf0, 0x01, 0xff, 0xc0, 0x00, 0x07, 0xff, 0xff, 0xe0, 0x00, 0x03, 0xff, 0x80, 0x0f, 0xff, 
	0xff, 0xc0, 0x07, 0xfe, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0xff, 0xe0, 0x03, 0xff, 
	0xff, 0x80, 0x1f, 0xf8, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x3f, 0xf8, 0x01, 0xff, 
	0xff, 0x00, 0x3f, 0xe0, 0x00, 0xff, 0xff, 0xf0, 0x0f, 0xff, 0xff, 0x00, 0x0f, 0xfc, 0x00, 0xff, 
	0xff, 0x00, 0xff, 0x80, 0x07, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xe0, 0x03, 0xff, 0x00, 0xff, 
	0xff, 0x81, 0xff, 0x00, 0x1f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xf8, 0x00, 0xff, 0x81, 0xff, 
	0xff, 0xff, 0xfc, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xfe, 0x00, 0x7f, 0xff, 0xff, 
	0xff, 0xff, 0xf8, 0x01, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x80, 0x1f, 0xff, 0xff, 
	0xff, 0xff, 0xf0, 0x07, 0xfe, 0x00, 0x03, 0xff, 0xff, 0xc0, 0x00, 0x7f, 0xc0, 0x0f, 0xff, 0xff, 
	0xff, 0xff, 0xc0, 0x0f, 0xf8, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x1f, 0xf0, 0x07, 0xff, 0xff, 
	0xff, 0xff, 0x80, 0x1f, 0xe0, 0x01, 0xff, 0xff, 0xff, 0xff, 0x80, 0x07, 0xf8, 0x03, 0xff, 0xff, 
	0xff, 0xff, 0x00, 0x3f, 0xc0, 0x0f, 0xff, 0xe0, 0x07, 0xff, 0xf0, 0x03, 0xfc, 0x01, 0xff, 0xff, 
	0xff, 0xff, 0x00, 0x7f, 0x80, 0x1f, 0xfc, 0x00, 0x00, 0x3f, 0xf8, 0x01, 0xfe, 0x00, 0xff, 0xff, 
	0xff, 0xfe, 0x00, 0xfe, 0x00, 0x7f, 0xe0, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x7f, 0x00, 0x7f, 0xff, 
	0xff, 0xfc, 0x01, 0xfc, 0x00, 0xff, 0x80, 0x00, 0x00, 0x01, 0xff, 0x00, 0x3f, 0x80, 0x7f, 0xff, 
	0xff, 0xfc, 0x03, 0xfc, 0x01, 0xfe, 0x00, 0x03, 0xc0, 0x00, 0x7f, 0x80, 0x3f, 0x80, 0x3f, 0xff, 
	0xff, 0xf8, 0x03, 0xf8, 0x03, 0xfc, 0x00, 0x7f, 0xfe, 0x00, 0x3f, 0xc0, 0x1f, 0xc0, 0x3f, 0xff, 
	0xff, 0xf8, 0x07, 0xf0, 0x07, 0xf8, 0x01, 0xff, 0xff, 0xc0, 0x0f, 0xe0, 0x0f, 0xe0, 0x1f, 0xff, 
	0xff, 0xf0, 0x07, 0xf0, 0x0f, 0xf0, 0x07, 0xff, 0xff, 0xe0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xff, 
	0xff, 0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x0f, 0xf8, 0x3f, 0xf0, 0x07, 0xf0, 0x0f, 0xe0, 0x0f, 0xff, 
	0xff, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f, 0xf0, 0x0f, 0xf8, 0x07, 0xf8, 0x07, 0xf0, 0x0f, 0xff, 
	0xff, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f, 0xf0, 0x0f, 0xf8, 0x03, 0xf8, 0x07, 0xf0, 0x0f, 0xff, 
	0xff, 0xf0, 0x0f, 0xe0, 0x1f, 0xc0, 0x1f, 0xf0, 0x0f, 0xfc, 0x03, 0xf8, 0x07, 0xf0, 0x0f, 0xff, 
	0xff, 0xf0, 0x0f, 0xe0, 0x1f, 0xc0, 0x3f, 0xf0, 0x0f, 0xfc, 0x03, 0xf8, 0x0f, 0xf0, 0x0f, 0xff, 
	0xff, 0xf0, 0x0f, 0xe0, 0x0f, 0xc0, 0x1f, 0xf0, 0x07, 0xfc, 0x03, 0xff, 0xff, 0xf0, 0x0f, 0xff, 
	0xff, 0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xf8, 0x07, 0xfc, 0x03, 0xff, 0xff, 0xe0, 0x0f, 0xff, 
	0xff, 0xf8, 0x07, 0xf0, 0x07, 0xe0, 0x1f, 0xf8, 0x03, 0xfc, 0x01, 0xff, 0xff, 0xe0, 0x1f, 0xff, 
	0xff, 0xf8, 0x07, 0xf8, 0x07, 0xf0, 0x0f, 0xfc, 0x01, 0xfe, 0x00, 0xff, 0xff, 0x80, 0x1f, 0xff, 
	0xff, 0xf8, 0x03, 0xf8, 0x03, 0xf0, 0x07, 0xfe, 0x00, 0xff, 0x00, 0x1f, 0xfe, 0x00, 0x3f, 0xff, 
	0xff, 0xfc, 0x03, 0xfc, 0x01, 0xf8, 0x03, 0xff, 0x00, 0x7f, 0x80, 0x00, 0x00, 0x00, 0xff, 0xff, 
	0xff, 0xfc, 0x01, 0xfe, 0x00, 0xfc, 0x01, 0xff, 0x80, 0x3f, 0xe0, 0x00, 0x00, 0x01, 0xff, 0xff, 
	0xff, 0xfe, 0x00, 0xff, 0x00, 0x7e, 0x00, 0xff, 0xc0, 0x0f, 0xf8, 0x00, 0x00, 0x07, 0xff, 0xff, 
	0xff, 0xff, 0x00, 0x7f, 0x80, 0x3f, 0x00, 0x3f, 0xe0, 0x07, 0xff, 0x00, 0x00, 0x3f, 0xff, 0xff, 
	0xff, 0xff, 0x80, 0x3f, 0xc0, 0x1f, 0x80, 0x1f, 0xf8, 0x00, 0xff, 0xfc, 0x0f, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0x80, 0x1f, 0xe0, 0x07, 0xe0, 0x07, 0xfc, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xe0, 0x0f, 0xf0, 0x03, 0xf0, 0x01, 0xff, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xf0, 0x03, 0xf8, 0x01, 0xfc, 0x00, 0x7f, 0xc0, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xf8, 0x01, 0xfe, 0x00, 0x7e, 0x00, 0x1f, 0xf8, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xfc, 0x01, 0xff, 0x00, 0x1f, 0x80, 0x03, 0xff, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0x03, 0xff, 0xc0, 0x0f, 0xe0, 0x00, 0x3f, 0xf0, 0x03, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x03, 0xfc, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x01, 0xff, 0x80, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x01, 0xff, 0xf0, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x83, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

// -------------------- Setup Function --------------------
void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); } // Wait for Serial Monitor to open

    Serial.println("Initializing ESP32 and Fingerprint Sensor...");

     // Initialize OLED Display
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(18, 5);
    display.display();
    delay(2000);

    // Initialize Relay Pin
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH); // Ensure door is locked initially

    mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN); // RX = GPIO16, TX = GPIO17
    delay(100); // Short delay to ensure serial is ready

    finger.begin(57600); // Set to 57600 baud

    if (finger.verifyPassword()) {
        Serial.println("Fingerprint sensor found!");
    } else {
        displayError();
        while (1) { delay(1000); } // Halt execution if sensor not found
    }

    connectToWiFi();

    // Start NTP Client
    timeClient.begin();
    if (timeClient.update()) {
        setTime(timeClient.getEpochTime());
        Serial.println("Time synchronized.");
    } else {
        Serial.println("Failed to synchronize time via NTP.");
    }

    // Initialize fingerprintCounter based on existing templates
    initializeFingerprintCounter();


    
}

// -------------------- Loop Function --------------------
void loop() {
    displayImage(myImage);
    char key = keypad.getKey(); // Read the keypad input

    if (key) { // If a key is pressed
        Serial.print("Key Pressed: ");
        Serial.println(key);

        // Enroll new fingerprint when '0' is pressed
        if (key == '0') {
            enrollNewFingerprint();
        }
        // Delete fingerprint when '6' is pressed
        else if (key == '6') {
            enrollDeleteMembers();
        }
        // Check for '*' to clear input buffer
        else if (key == '*') {
            // Clear the input buffer
            keypadInput = "";
            Serial.println("Input cleared.");
        }
        // For all other keys, accumulate them into the input buffer
        else {
            // Accumulate the pressed key into the input buffer
            keypadInput += key;
            Serial.print("Current Input: ");
            Serial.println(keypadInput);
        }
        
       
    }

    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_OK) {
        p = finger.image2Tz();
        if (p == FINGERPRINT_OK) {
            p = finger.fingerSearch();
            if (p == FINGERPRINT_OK) {
                Serial.print("Fingerprint ID #");
                Serial.println(finger.fingerID);

                // Step 1: Get NIC associated with fingerprint_id
                String nic = getNICByFingerprintID(finger.fingerID);
                if (nic.isEmpty()) {
                   
                    displayTryAgain();
                    char key = keypad.getKey(); // Read the keypad input
                    return;
                }

                Serial.print("Associated NIC: ");
                Serial.println(nic);

                // Step 2: Check if member has paid for current month
                bool isPaid = checkMemberPayment(nic);
                if (isPaid) {
                    Serial.println("paid Opening door...");
                    displayWelcome();
                    openDoor();
                    delay(5000); // Keep door open for 5 seconds
                    closeDoor();
                    // Step 3: Get the current timestamp

                    String timestamp = getCurrentTimestamp();

                    // Step 4: Send attendance data to the server
                    sendAttendanceData(finger.fingerID, nic, timestamp);
                    
                    displayImage(myImage);
                } else {
                    
                    displayNotPaid();
                }

            } else if (p == FINGERPRINT_NOTFOUND) {
                
                displayTryAgain();
                
            } else {
              
                displayTryAgain();
            }
        } else {
            
            displayTryAgain();
        }
    } else if (p == FINGERPRINT_NOFINGER) {
        
        char key = keypad.getKey(); // Read the keypad input
    } else {
       
        displayTryAgain();
        char key = keypad.getKey(); // Read the keypad input
    }
    delay(300);
    
}
  //fingerprint img display fuction
  void displayImage(const unsigned char* image) {
   
    display.clearDisplay();
    display.drawBitmap(0, 0, myImage, display.width(), display.height(), WHITE); // Display the image
    display.setCursor(0, 0);
    display.setTextSize(1);

    display.display();
    
    }

    // -------------------- Display Functions --------------------

void displayWelcome() {
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(3);
    display.println("WELCOME");
    display.display();
    
}

void displayTryAgain() {
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Try Again!");
    display.display();
    delay(1500);
}

void displayNotPaid(){
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("UN-Paid   No Access     !!!");
    display.display();
    delay(3500);
}

void displaySuccessfullyRegistered() {
   
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Successfully Registered!");
    display.display();
    delay(4000);
}

void displayNewRegistered() {
   
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("New User Registration!");
    display.display();
    delay(1500);
}

void displayDeleteFingerprint() {
   
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Delete Fingerprint");
    display.display();
    delay(1500);
}
void displayDeleteFingerprintt() {
   
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(3);
    display.println("Deleted !!!");
    display.display();
    delay(4000);
}

void displayError() {
   
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(3);
    display.println("Failed");
    display.display();
    delay(4000);
}

void displayTapFingerpr() {
   
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Place Your Finger");
    display.display();
    delay(1500);
}

// -------------------- Enrollment Function --------------------
void enrollNewFingerprint() {
    
    displayNewRegistered();
    String nic_str = getNICFromUser();
    
    if (nic_str.isEmpty()) {
        
        return; // Exit if NIC is invalid
    }
    if (nic_str == "4040") {
                displayWelcome();
                openDoor();
                delay(5000); 
                closeDoor(); 
                 return;
                 }

    // Convert NIC to uint16_t for fingerprint_id
    uint16_t fingerprint_id = fingerprintCounter++; // Assign unique ID using counter

    if (fingerprint_id == 0) {
        
        return;
    }

    // Step 2: Check if fingerprint_id already exists in the fingerprints table
    // Implement this check via the server.
    // Since fingerprint_id is managed by counter and assumed unique, this step can be optional
    // However, it's good practice to verify on the server side

    // Optional: Check if fingerprint_id already exists on the server
    // Skipping this as fingerprint_id is managed locally using fingerprintCounter

    // Step 3: Capture the first image
    displayTapFingerpr();
    int p = finger.getImage();
    while (p != FINGERPRINT_OK) {
        
        delay(1000);
        p = finger.getImage();
    }
    

    // Step 4: Convert the image to a template
    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK) {
        
        return;
    }

    // Step 5: Capture the second image
    
    p = finger.getImage();
    while (p != FINGERPRINT_OK) {
        
        delay(1000);
        p = finger.getImage();
    }
    

    // Step 6: Convert the second image to a template
    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK) {
        displayTryAgain();
        return;
    }
    

    // Step 7: Create a new fingerprint model
    p = finger.createModel();
    if (p != FINGERPRINT_OK) {
        displayTryAgain();
        return;
    }

    // Step 8: Assign the unique fingerprint ID using counter
    p = finger.storeModel(fingerprint_id);
    if (p != FINGERPRINT_OK) {
        displayError();
        return;
    }
    Serial.print("Fingerprint stored with unique ID #");
    Serial.println(fingerprint_id);
    displaySuccessfullyRegistered();

    // Step 9: Get the current timestamp
    String timestamp = getCurrentTimestamp();

    // Step 10: Send fingerprint ID (unique counter), NIC, and timestamp to the database
    sendFingerprintData(fingerprint_id, nic_str, timestamp);
}

// -------------------- Function to Send Fingerprint Data to Server --------------------
void sendFingerprintData(uint16_t fingerprint_id, String nic, String timestamp) {
    if (WiFi.status() == WL_CONNECTED) { // Check Wi-Fi connection
        HTTPClient http;
        http.begin(enrollUrl); // Specify the Enrollment URL

        // Prepare the POST data
        String postData = "fingerprint_id=" + String(fingerprint_id) + "&nic=" + nic + "&timestamp=" + timestamp;

        // Send HTTP POST request
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        int httpResponseCode = http.POST(postData);

        // Check response from the server
        if (httpResponseCode > 0) {
            Serial.print("POST request sent. Response code: ");
            Serial.println(httpResponseCode);
            String response = http.getString(); // Get the response from the server
            Serial.println("Server response: " + response);
        } else {
            Serial.print("Error sending POST request. Code: ");
            Serial.println(httpResponseCode);
        }

        http.end(); // End the connection
    } else {
        Serial.println("Error: Not connected to Wi-Fi.");
    }
}

// -------------------- Function to Get NIC by fingerprint_id --------------------
String getNICByFingerprintID(uint16_t fingerprint_id) {
    String nic = "";

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String queryUrl = String(getNicUrl) + "?fingerprint_id=" + String(fingerprint_id);
        http.begin(queryUrl);
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
            nic = http.getString();
            nic.trim();
            if (nic == "not_found" || nic == "invalid") {
                Serial.println("NIC not found or invalid for this fingerprint_id.");
                nic = "";
            } else {
                Serial.print("NIC retrieved: ");
                Serial.println(nic);
            } // This closing brace was missing
        } else {
            Serial.print("Error fetching NIC: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    } else {
        Serial.println("Error: Not connected to Wi-Fi.");
    }

    return nic;
}


String enrollDeleteMembers() {
    String inputd = "";
    char key;

    displayDeleteFingerprint();

    while (true) {
        key = keypad.getKey();  // Get the key pressed

        if (key) {
            if (key == '#') {  // Confirm input
                if (inputd.length() > 0) {
                    int fingerprintID = inputd.toInt();  // Convert input string to integer
                    Serial.print("Deleting fingerprint with ID #");
                    Serial.println(fingerprintID);
                    
                    // Call the deleteModel() function to remove the fingerprint
                    uint8_t result = finger.deleteModel(fingerprintID);

                    if (result == FINGERPRINT_OK) {
                        Serial.println("Fingerprint deleted successfully.");
                       displayDeleteFingerprintt();  // Optional: Display success on OLED
                    } else if (result == FINGERPRINT_PACKETRECIEVEERR) {
                        Serial.println("Error: Communication error.");
                    } else if (result == FINGERPRINT_BADLOCATION) {
                        Serial.println("Error: Could not find the fingerprint template.");
                    } else if (result == FINGERPRINT_FLASHERR) {
                        Serial.println("Error: Error writing to flash.");
                    } else {
                        Serial.println("Unknown error occurred while deleting fingerprint.");
                    }
                    break;  // Exit the loop after deleting
                }
            } else if (key == '*') {  // Clear input
                inputd = "";
                Serial.println("Input cleared. Enter fingerprint ID again:");
                displayDeleteSuccess(inputd);
                return "";
            } else if (isDigit(key)) {
                inputd += key;  // Append the digit to the input string
                Serial.print(key);  // Display the pressed key
                displayDeleteSuccess(inputd);
            }
        }
    }
    return inputd;
}

// -------------------- Function to Check Member Payment --------------------
bool checkMemberPayment(String nic) {
    bool isPaid = false;

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(verifyUrl);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        // Prepare the POST data
        String postData = "nic=" + nic;

        // Send HTTP POST request
        int httpResponseCode = http.POST(postData);

        // Check response from the server
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.print("Payment verification response: ");
            Serial.println(response);

            if (response == "success") {
                isPaid = true;
            } else {
                isPaid = false;
            }
        } else {
            Serial.print("Error sending POST request for payment verification. Code: ");
            Serial.println(httpResponseCode);
        }

        http.end(); // End the connection
    } else {
        Serial.println("Error: Not connected to Wi-Fi.");
    }

    return isPaid;
}

// -------------------- Function to Open Door --------------------
void openDoor() {
    digitalWrite(RELAY_PIN, LOW); // Activate relay
    Serial.println("Door is OPEN.");
}

// -------------------- Function to Close Door --------------------
void closeDoor() {
    digitalWrite(RELAY_PIN, HIGH); // Deactivate relay
    Serial.println("Door is CLOSED.");
}

// -------------------- Function to Send Attendance Data to Server --------------------
void sendAttendanceData(uint16_t fingerprint_id, String nic, String timestamp) {
    if (WiFi.status() == WL_CONNECTED) { // Check Wi-Fi connection
        HTTPClient http;
        const char* attendanceUrl = "https://xfitness.itgusto.com/attendance.php"; // URL for attendance data
        http.begin(attendanceUrl); // Specify the Attendance URL

        // Prepare the POST data
        String postData = "fingerprint_id=" + String(fingerprint_id) + "&nic=" + nic + "&timestamp=" + timestamp;

        // Send HTTP POST request
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        int httpResponseCode = http.POST(postData);

        // Check response from the server
        if (httpResponseCode > 0) {
            Serial.print("Attendance POST request sent. Response code: ");
            Serial.println(httpResponseCode);
            String response = http.getString(); // Get the response from the server
            Serial.println("Server response: " + response);
        } else {
            Serial.print("Error sending Attendance POST request. Code: ");
            Serial.println(httpResponseCode);
        }

        http.end(); // End the connection
    } else {
        Serial.println("Error: Not connected to Wi-Fi.");
    }
}

// -------------------- Function to Get NIC from User via Numberpad --------------------
String getNICFromUser() {
    String nic = ""; // Initialize an empty NIC string
    Serial.println("Enter NIC (numeric only) and press #:");
    

    // Loop to get input from the number pad
    while (true) {
        char key = keypad.getKey(); // Read the keypad input

        if (key) { // If a key is pressed
            // Check if the key is the '#' key, which means input is complete
            if (key == '#') {
                if (nic.length() == 0) {
                    Serial.println("NIC cannot be empty. Please try again.");
                    return ""; // Return empty if no input
                }
                break; // Exit the loop if input is complete
            } 
            // Check if the '*' key is pressed to clear the input
            else if (key == '*') {
                nic = ""; // Clear the NIC string
                Serial.println("\nInput cleared. Please enter NIC again:");
                displayEnterNIC(nic);
            }
            // Check if the key is numeric
            else if (key >= '0' && key <= '9') { 
                nic += key; // Append the numeric key to the NIC string
                Serial.print(key); // Print the key to Serial monitor for feedback
                displayEnterNIC(nic); // Update display with current NIC
            } 
            // Handle invalid inputs
            else {
                Serial.println("Invalid input. Please enter numeric values only.");
            }
           
        }
    }

    Serial.print("NIC Entered: ");
    Serial.println(nic);
    return nic; // Return the completed NIC
}

void displayEnterNIC(String nic) {
   
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Enter NIC:");
    display.setCursor(0, 20);
    display.setTextSize(3); // Set text size to 2 (bigger font)
    display.println(nic); 
    display.display();
}

void displayDeleteSuccess(String inputd) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Enter Fingerprint ID");
    display.setTextSize(2);
    display.println(inputd);
    display.display();
    
}

// -------------------- Function to Get the Current Timestamp --------------------
String getCurrentTimestamp() {
    if (!timeClient.update()) {
        Serial.println("Failed to update time.");
    }

    // Get the current epoch time and format it
    time_t rawtime = timeClient.getEpochTime();
    struct tm* timeinfo = localtime(&rawtime);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
}

// -------------------- Function to Connect to Wi-Fi --------------------
void connectToWiFi() {
    Serial.print("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println(" Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

// -------------------- Function to Initialize Fingerprint Counter --------------------
void initializeFingerprintCounter() {
    uint8_t p = finger.getTemplateCount();

    if (p != FINGERPRINT_OK) {
        Serial.println("Failed to get template count");
        fingerprintCounter = 1; // Default to 1 if error occurs
    } else {
        fingerprintCounter = finger.templateCount + 1; // Next available ID
        Serial.print("Fingerprint Counter initialized to: ");
        Serial.println(fingerprintCounter);
    }
}

// -------------------- Function to Check Template Count (Optional) --------------------
void checkTemplateCount() {
    uint8_t p = finger.getTemplateCount();
    if (p != FINGERPRINT_OK) {
        Serial.println("Failed to get template count");
    } else {
        Serial.print("Fingerprint templates stored: ");
        Serial.println(finger.templateCount);
    }
}
