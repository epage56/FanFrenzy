#include <FastLED.h>

#define LED_PIN     6
#define NUM_LEDS    100 
#define BRIGHTNESS  155
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// Ultrasonic sensor pins
const int TRIG_PIN_1 = 10; //10
const int ECHO_PIN_1 = 9; //9 
const int TRIG_PIN_2 = 12;
const int ECHO_PIN_2 = 11;

// Button pins
const int BUTTON_PIN_1 = 2;
const int BUTTON_PIN_2 = 3;

// Game parameters
const float TUBE_HEIGHT = 89.9;   // in cm
const float BALL_RADIUS = 3.99 / 2; // in cm
const int BOTTOM_OFFSET = 19;     // 19 LED offset from the bottom
const int MIN_GAP_SIZE = 30; 
const int MAX_GAP_SIZE = 40;

const int TRANSITION_STEPS = 60;  // Number of steps for transition
const int OBSTACLE_DURATION = 3000; // 3 seconds between obstacle changes

// Game state variables
struct Obstacle {
  int gapStart;
  int gapSize;
};

Obstacle currentObstacle = {0, 0};
Obstacle nextObstacle = {0, 0};
int transitionProgress = 0;
bool gameStarted = false;
unsigned long gameStartTime = 0;
bool isWarmupPeriod = true;
bool isGameOver = false;
int gameOverPosition = 0;
int gameMode = 1; // 1 for single player, 2 for two players
int losingPlayer = 0; // 0 for no loser, 1 for player 1, 2 for player 2
int zonesSurvived = 0; // to tell 1 player version how long they lasted.

// Serial print control
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 500; // Print every 500ms

// Warm-up period parameters
const unsigned long WARMUP_DURATION = 10000; // 10 seconds
const unsigned long FLASH_INTERVAL = 250; // 0.25 seconds

unsigned long lastObstacleChangeTime = 0;

// Distance reading filter variables
float lastDistance1 = 0;
float lastDistance2 = 0;
const float MAX_DISTANCE_CHANGE = 10.0; // Maximum allowed change in cm

void setup() {
  delay(3000); // Power-up safety delay
  
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  Serial.begin(115200);
  
  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);
  pinMode(BUTTON_PIN_1, INPUT_PULLUP);
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);
  
  generateNewObstacle(currentObstacle);
  generateNewObstacle(nextObstacle);
  
  Serial.println("Press button 2 to start single-player mode");
}

void loop() {
  unsigned long currentTime = millis();
  
  if (!gameStarted) {
    checkButtonsAndStartGame(currentTime);
  } else if (isWarmupPeriod) {
    warmupPeriod(currentTime);
  } else if (!isGameOver) {
    playGame(currentTime);
  } else {
    displayGameOver();
  }
  delay(16); // Approximately 60 fps
}

void checkButtonsAndStartGame(unsigned long currentTime) {
  if (digitalRead(BUTTON_PIN_2) == LOW) {
    gameMode = 1;
    redRedRedWhiteSequence();
    gameStarted = true;
    gameStartTime = currentTime;
    lastObstacleChangeTime = currentTime;
    isWarmupPeriod = true;
    Serial.println("Single-player mode activated. Warm-up period begins.");
  } else if (gameStarted && digitalRead(BUTTON_PIN_1) == LOW && gameMode == 1) {
    gameMode = 2;
    orangeGreenAnimation();
    gameStartTime = currentTime; // Reset warm-up timer
    lastObstacleChangeTime = currentTime;
    Serial.println("Two-player mode activated. Warm-up period restarted.");
  }
}

void warmupPeriod(unsigned long currentTime) {
  updateObstacles(currentTime);
  displayWarmupObstacles();
  
  if (gameMode == 1 && digitalRead(BUTTON_PIN_1) == LOW) {
    gameMode = 2;
    orangeGreenAnimation();
    gameStartTime = currentTime; // Reset warm-up timer
    lastObstacleChangeTime = currentTime;
    Serial.println("Two-player mode activated. Warm-up period restarted.");
  }
  
  if (currentTime - gameStartTime >= WARMUP_DURATION) {
    isWarmupPeriod = false;
    Serial.println("Warm-up period ended. Game fully starts now!");
  }
}

void playGame(unsigned long currentTime) {
  float distance1 = readDistance(TRIG_PIN_1, ECHO_PIN_1, lastDistance1);
  float distance2 = readDistance(TRIG_PIN_2, ECHO_PIN_2, lastDistance2);
  
  updateObstacles(currentTime);
  displayObstacles();
  checkGameOver(distance1, distance2);
  
  // Reduce frequency of serial prints
  if (currentTime - lastPrintTime >= PRINT_INTERVAL) {
    printGameStatus(distance1, distance2);
    lastPrintTime = currentTime;
  }
}

float readDistance(int trigPin, int echoPin, float& lastReading) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH);
  float currentReading = TUBE_HEIGHT + BALL_RADIUS - (1.04 * (duration * 0.034 / 2));
  currentReading = constrain(currentReading, 0, TUBE_HEIGHT);
  
  if (lastReading == 0) {
    // First reading, no filtering
    lastReading = currentReading;
  } else if (abs(currentReading - lastReading) > MAX_DISTANCE_CHANGE) {
    // Change is too large, use the last reading
    currentReading = lastReading;
  } else {
    // Update the last reading
    lastReading = currentReading;
  }
  
  return currentReading;
}

void updateObstacles(unsigned long currentTime) {
  if (currentTime - lastObstacleChangeTime >= OBSTACLE_DURATION) {
    currentObstacle = nextObstacle;
    generateNewObstacle(nextObstacle);
    lastObstacleChangeTime = currentTime;
    transitionProgress = 0;
    zonesSurvived++; // Increment the counter
    
    Serial.print("New obstacle: Start=");
    Serial.print(nextObstacle.gapStart);
    Serial.print(", Size=");
    Serial.println(nextObstacle.gapSize);
    Serial.print("Zones survived: ");
    Serial.println(zonesSurvived);
  } else {
    unsigned long elapsedTime = currentTime - lastObstacleChangeTime;
    transitionProgress = map(elapsedTime, 0, OBSTACLE_DURATION, 0, TRANSITION_STEPS);
  }
}

void displayObstacles() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int i = BOTTOM_OFFSET; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Blue;
  }
  
  float t = (float)transitionProgress / TRANSITION_STEPS;
  t = easeInOutCubic(t);
  int gapStart = lerp(currentObstacle.gapStart, nextObstacle.gapStart, t);
  int gapSize = lerp(currentObstacle.gapSize, nextObstacle.gapSize, t);
  
  for (int i = 0; i < gapSize && i + gapStart + BOTTOM_OFFSET < NUM_LEDS; i++) {
    leds[i + gapStart + BOTTOM_OFFSET] = CRGB::Black;
  }
  
  FastLED.show();
}

void displayWarmupObstacles() {
  static bool isBlue = true;
  static unsigned long lastToggle = 0;
  
  if (millis() - lastToggle >= FLASH_INTERVAL) {
    isBlue = !isBlue;
    lastToggle = millis();
  }
  
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  if (isBlue) {
    for (int i = BOTTOM_OFFSET; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Blue;
    }
  }
  
  float t = (float)transitionProgress / TRANSITION_STEPS;
  t = easeInOutCubic(t);
  int gapStart = lerp(currentObstacle.gapStart, nextObstacle.gapStart, t);
  int gapSize = lerp(currentObstacle.gapSize, nextObstacle.gapSize, t);
  
  for (int i = 0; i < gapSize && i + gapStart + BOTTOM_OFFSET < NUM_LEDS; i++) {
    leds[i + gapStart + BOTTOM_OFFSET] = CRGB::Black;
  }
  
  FastLED.show();
}

void generateNewObstacle(Obstacle &obstacle) {
  obstacle.gapSize = random(MIN_GAP_SIZE, MAX_GAP_SIZE + 1);
  obstacle.gapStart = random(0, NUM_LEDS - BOTTOM_OFFSET - obstacle.gapSize);
}

void checkGameOver(float ballDistance1, float ballDistance2) {
  float t = (float)transitionProgress / TRANSITION_STEPS;
  t = easeInOutCubic(t);
  int zoneStartLED = lerp(currentObstacle.gapStart, nextObstacle.gapStart, t);
  int zoneEndLED = zoneStartLED + lerp(currentObstacle.gapSize, nextObstacle.gapSize, t);
  
  float zoneStartDistance = mapLEDToDistance(zoneStartLED + BOTTOM_OFFSET);
  float zoneEndDistance = mapLEDToDistance(zoneEndLED + BOTTOM_OFFSET);
  
  if (ballDistance1 < zoneStartDistance || ballDistance1 >= zoneEndDistance) {
    gameOver(ballDistance1, zoneStartDistance, zoneEndDistance, 1);
  } else if (gameMode == 2 && (ballDistance2 < zoneStartDistance || ballDistance2 >= zoneEndDistance)) {
    gameOver(ballDistance2, zoneStartDistance, zoneEndDistance, 2);
  }
}

void gameOver(float ballDistance, float zoneStartDistance, float zoneEndDistance, int player) {
  isGameOver = true;
  losingPlayer = player;
  Serial.print("Game Over! Player ");
  Serial.print(player);
  Serial.println(" lost!");
  
  Serial.print("Ball position: ");
  Serial.print(ballDistance, 2);
  Serial.println(" cm");
  
  Serial.print("Zone: ");
  Serial.print(zoneStartDistance, 2);
  Serial.print(" cm to ");
  Serial.print(zoneEndDistance, 2);
  Serial.println(" cm");
  
  // Determine which boundary was crossed
  if (ballDistance < zoneStartDistance) {
    gameOverPosition = map(zoneStartDistance, 0, TUBE_HEIGHT, BOTTOM_OFFSET, NUM_LEDS - 1);
  } else {
    gameOverPosition = map(zoneEndDistance, 0, TUBE_HEIGHT, BOTTOM_OFFSET, NUM_LEDS - 1);
  }
  
  Serial.print("Game over at LED position: ");
  Serial.println(gameOverPosition);
}

void displayGameOver() {
  // Flash blue LEDs 3 times
  for (int i = 0; i < 3; i++) {
    displayObstacles();
    FastLED.show();
    delay(250);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(250);
  }

  // Display the final state with red boundary
  displayObstacles();
  displayRedBoundary();
  FastLED.show();
  delay(1000); // Display for 1 second

  // Victory animation
  if (gameMode == 1) {
    // Single player mode
    pinkVictorySweep();
  } else {
    // Two player mode
    // Pulsing orange or green sweep based on losing player
    for (int j = 0; j < 3; j++) {
      if (losingPlayer == 1) {
        orangeSweep();
      } else {
        greenSweep();
      }
    }
  }

  // Turn off all LEDs
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // Wait for button press to restart the game
  while (digitalRead(BUTTON_PIN_2) == HIGH) {
    delay(50);
  }
  gameStarted = false;
  isGameOver = false;
  losingPlayer = 0;
  gameMode = 1;
}

void redRedRedWhiteSequence() {
  for (int i = 0; i < 3; i++) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int j = BOTTOM_OFFSET; j < NUM_LEDS; j++) {
      leds[j] = CRGB::Red;
    }
    FastLED.show();
    delay(500);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(500);
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int j = BOTTOM_OFFSET; j < NUM_LEDS; j++) {
    leds[j] = CRGB::Green;
  }
  FastLED.show();
  delay(1000);
}

void orangeGreenAnimation() {
  // Orange from top, Green from bottom
  for (int i = 0; i <= (NUM_LEDS - BOTTOM_OFFSET) / 2; i++) {
    leds[NUM_LEDS - 1 - i] = CRGB::Orange;
    leds[BOTTOM_OFFSET + i] = CRGB::Green;
    FastLED.show();
    delay(20);
  }
  
  // Bounce back
  for (int i = (NUM_LEDS - BOTTOM_OFFSET) / 2; i >= 0; i--) {
    leds[NUM_LEDS - 1 - i] = CRGB::Black;
    leds[BOTTOM_OFFSET + i] = CRGB::Black;
    FastLED.show();
    delay(20);
  }
  
  // Pulsing effect
  for (int j = 0; j < 3; j++) {
    for (int b = 0; b < 255; b += 5) {
      fill_solid(leds + BOTTOM_OFFSET, NUM_LEDS - BOTTOM_OFFSET, CRGB(b, b, 0)); // Yellow pulsing
      FastLED.show();
      delay(5);
    }
    for (int b = 255; b > 0; b -= 5) {
      fill_solid(leds + BOTTOM_OFFSET, NUM_LEDS - BOTTOM_OFFSET, CRGB(b, b, 0)); // Yellow pulsing
      FastLED.show();
      delay(5);
    }
  }
  
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(500);
}

void orangeSweep() {
  for (int i = BOTTOM_OFFSET; i < NUM_LEDS; i++) {
    int brightness = sin8(map(i, BOTTOM_OFFSET, NUM_LEDS - 1, 0, 255));
    leds[i] = CRGB(255, 69, 0); // Orange color
    leds[i].nscale8(brightness);
    FastLED.show();
    delay(10);
  }
  
  for (int i = NUM_LEDS - 1; i >= BOTTOM_OFFSET; i--) {
    leds[i] = CRGB::Black;
    FastLED.show();
    delay(5);
  }
}

void greenSweep() {
  for (int i = BOTTOM_OFFSET; i < NUM_LEDS; i++) {
    int brightness = sin8(map(i, BOTTOM_OFFSET, NUM_LEDS - 1, 0, 255));
    leds[i] = CRGB::Green;
    leds[i].nscale8(brightness);
    FastLED.show();
    delay(10);
  }
  
  for (int i = NUM_LEDS - 1; i >= BOTTOM_OFFSET; i--) {
    leds[i] = CRGB::Black;
    FastLED.show();
    delay(5);
  }
}

void pinkVictorySweep() {
  // First, display the number of zones survived with a sweep-up effect
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(1000); // Pause for a moment

  // Sweep up effect for zones survived
  for (int i = BOTTOM_OFFSET; i < min(BOTTOM_OFFSET + zonesSurvived, NUM_LEDS); i++) {
    // Create a fading tail effect
    for (int j = BOTTOM_OFFSET; j <= i; j++) {
      int brightness = 255 - (i - j) * 25; // Fade out over 10 LEDs
      if (brightness < 0) brightness = 0;
      leds[j] = CRGB::White;
      leds[j].fadeToBlackBy(255 - brightness);
    }
    FastLED.show();
    delay(30); // Adjust for speed of animation
  }
  delay(1000); // Pause to show the final count

  // Fade out the white LEDs
  for (int i = 255; i >= 0; i -= 5) {
    for (int j = BOTTOM_OFFSET; j < NUM_LEDS; j++) {
      leds[j].fadeToBlackBy(5);
    }
    FastLED.show();
    delay(10);
  }

  // Now continue with the original pink sweep animation
  for (int j = 0; j < 3; j++) { // Repeat the sweep 3 times
    // Sweep up
    for (int i = BOTTOM_OFFSET; i < NUM_LEDS; i++) {
      int brightness = sin8(map(i, BOTTOM_OFFSET, NUM_LEDS - 1, 0, 255));
      leds[i] = CRGB(255, 20, 147); // Deep Pink color
      leds[i].nscale8(brightness);
      
      // Add sparkles
      if (random8() < 20) { // 20/256 chance for each LED
        leds[random8(BOTTOM_OFFSET, NUM_LEDS)] = CRGB::White;
      }
      
      FastLED.show();
      delay(10);
    }
    
    // Sweep down
    for (int i = NUM_LEDS - 1; i >= BOTTOM_OFFSET; i--) {
      leds[i] = CRGB::Black;
      
      // Add sparkles
      if (random8() < 20) { // 20/256 chance for each LED
        leds[random8(BOTTOM_OFFSET, NUM_LEDS)] = CRGB::White;
      }
      
      FastLED.show();
      delay(5);
    }
  }
  
  // Final sparkle effect
  for (int i = 0; i < 100; i++) {
    leds[random8(BOTTOM_OFFSET, NUM_LEDS)] = CRGB::White;
    FastLED.show();
    delay(10);
    fadeToBlackBy(leds + BOTTOM_OFFSET, NUM_LEDS - BOTTOM_OFFSET, 10);
  }

  // Reset the zones survived counter for the next game
  zonesSurvived = 0;
}

void displayRedBoundary() {
  int redStart = max(BOTTOM_OFFSET, gameOverPosition - 2);
  int redEnd = min(NUM_LEDS - 1, gameOverPosition + 2);
  
  Serial.print("Red boundary: ");
  Serial.print(redStart);
  Serial.print(" to ");
  Serial.println(redEnd);
  
  for (int i = redStart; i <= redEnd; i++) {
    leds[i] = CRGB::Red;
  }
}

void printGameStatus(float distance1, float distance2) {
  float t = (float)transitionProgress / TRANSITION_STEPS;
  t = easeInOutCubic(t);
  int zoneStartLED = lerp(currentObstacle.gapStart, nextObstacle.gapStart, t);
  int zoneEndLED = zoneStartLED + lerp(currentObstacle.gapSize, nextObstacle.gapSize, t);
  
  float zoneStartDistance = mapLEDToDistance(zoneStartLED + BOTTOM_OFFSET);
  float zoneEndDistance = mapLEDToDistance(zoneEndLED + BOTTOM_OFFSET);
  
  Serial.print("Game Mode: ");
  Serial.print(gameMode == 1 ? "Single Player" : "Two Players");
  Serial.print(" | P1 Ball Height: ");
  Serial.print(distance1, 2);
  Serial.print(" cm");
  
  if (gameMode == 2) {
    Serial.print(" | P2 Ball Height: ");
    Serial.print(distance2, 2);
    Serial.print(" cm");
  }
  
  Serial.print(" | Zone: ");
  Serial.print(zoneStartDistance, 2);
  Serial.print(" cm to ");
  Serial.print(zoneEndDistance, 2);
  Serial.println(" cm");
  
  Serial.print("Zone LED: ");
  Serial.print(zoneStartLED + BOTTOM_OFFSET);
  Serial.print(" to ");
  Serial.println(zoneEndLED + BOTTOM_OFFSET);
}

int lerp(int a, int b, float t) {
  return a + t * (b - a);
}

float easeInOutCubic(float t) {
  return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
}

float mapLEDToDistance(int ledPosition) {
  return map(ledPosition, BOTTOM_OFFSET, NUM_LEDS - 1, 0, TUBE_HEIGHT);
}
