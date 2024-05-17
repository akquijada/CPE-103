#include <WebServer.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include "esp_camera.h"

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

const char *ssid = "RJDizon";
const char *password = "bawalconnect123";

WebServer server(80);
Servo sX, sY, sSpin;

const int trigPin = 12; // Define pin connected to ultrasonic sensor trig
const int echoPin = 13; // Define pin connected to ultrasonic sensor echo

const char index_html[] PROGMEM = R"rawliteral(
<body>
  <img id="img" style="width: 320px; height: auto;">
  <br>
  <button onclick="camUp()">Capture</button>
  <br>
  <label id="lv">90</label>
  <br>
  <input type="range" onchange="servo()" id="v" min="0" max="140" step="1" value="70">
  <br>
  <label id="lh">90</label>
  <br>
  <input type="range" onchange="servo()" id="h" min="0" max="140" step="1" value="70">
  <br>
  <button onclick="startSpin()">Dispense</button>
  <button onclick="spinBackwards()">Backwards</button> <!-- New button -->
  <p id="foodStatus"> </p> 
  <script>
    function camUp() {
      img = document.getElementById("img")
      img.src = "/capture?" + new Date().getTime()
    }

    function servo() {
      let xhr = new XMLHttpRequest()
      xhr.open("GET", "/servo?v="+v.value+"&h="+h.value, false)
      xhr.send()
    }

    function startSpin() {
      let xhr = new XMLHttpRequest();
      xhr.open("GET", "/servo?v=" + v.value + "&h=" + h.value + "&spin=true", false);
      xhr.send();
    }

    function spinBackwards() {
      let xhr = new XMLHttpRequest();
      xhr.open("GET", "/spinbackwards", false);
      xhr.send();
    }

    window.onload = () => {
      camUp()
      h = document.getElementById("h")
      v = document.getElementById("v")

      lv = document.getElementById("lv")
      lh = document.getElementById("lh")

      v.addEventListener("input", (event) => {
        lv.innerHTML = event.target.value
      })
      h.addEventListener("input", (event) => {
        lh.innerHTML = event.target.value
      })

      // Check food status initially
      checkFoodStatus();

      // Schedule checkFoodStatus every 10 seconds
      setInterval(checkFoodStatus, 10000);
    }

    function checkFoodStatus() {
      var foodStatus = document.getElementById("foodStatus");
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
          if (xhr.status == 200) {
            // Request successful, update food status
            if (xhr.responseText == "true") {
              foodStatus.innerHTML = "Still have food";
            } else {
              foodStatus.innerHTML = "Running out of food";
            }
          } else {
            // Request failed
            foodStatus.innerHTML = "Failed to get food status";
          }
        }
      };
      xhr.open("GET", "/foodstatus", true);
      xhr.send();
    }
  </script>
</body>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sX.attach(14);
  sY.attach(15);
  sSpin.attach(4);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  Serial.println(WiFi.localIP());

  server.on("/", root);
  server.on("/capture", capture);
  server.on("/servo", servo);
  server.on("/foodstatus", handleFoodStatus);
  server.on("/spinbackwards", spinBackwards); // New route for spinning the servo backwards
  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();
}

void root() {
  Serial.println("Root");
  server.send(200, "text/html", index_html);
}

void capture() {
  Serial.println("Capture");
  
  camera_fb_t * fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);
  fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  server.sendHeader("Content-Type", "image/jpeg");  
  server.sendContent((const char *)fb->buf, fb->len);
    
  esp_camera_fb_return(fb);
}

void servo() {
  if (server.args() > 0) {
    // Control the servos based on the values received from the web page
    sX.write(server.arg("v").toInt());
    sY.write(server.arg("h").toInt());

    // Check if the button is pressed for continuous spinning
    if (server.arg("spin") == "true") {
      sSpin.write(180);  // Assuming 180 is maximum rotation for continuous spinning
      delay(5000);
      digitalWrite(90, LOW);
    }
  } 
}

void handleFoodStatus() {
  long duration, distance;
  // Measure distance using ultrasonic sensor
  digitalWrite(trigPin, LOW); 
  delayMicroseconds(2); 
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10); 
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = (duration * 0.034 / 2); // Convert to distance in cm
  Serial.print(distance)
  // Determine food status based on distance
  if (distance >= 7 && distance <= 12) {
    server.send(200, "text/plain", "true"); // Running out of food
  } else {
    server.send(200, "text/plain", "false"); // Still have food
  }
}

void spinBackwards() {
  // Write the necessary logic to spin the servo backwards
  sSpin.write(0); // Adjust the angle according to your servo's specifications for spinning backwards
  delay(5000);    // Adjust the delay time as needed
  sSpin.write(90); // Return the servo to its neutral position
  server.send(200, "text/plain", "Backwards");
}