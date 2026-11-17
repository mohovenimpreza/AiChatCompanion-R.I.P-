/*
  ESP32 desk companion: an animated face on a Nokia 5110 LCD (PCD8544).
  The face state is chosen by the PC program (pc/bridge.py), which sends one word
  per line over USB serial (115200 baud): IDLE, LISTEN, THINK, TALK, SLEEP, SHUTDOWN.

  LCD wiring (Adafruit_PCD8544 constructor order: SCLK, DIN, DC, CS, RST):
    SCLK=GPIO14, DIN=GPIO23, DC=GPIO2, CS=GPIO15, RST=GPIO4
  Libraries: Adafruit GFX Library, Adafruit PCD8544 Nokia 5110 LCD library.
*/
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

Adafruit_PCD8544 display = Adafruit_PCD8544(14, 23, 2, 15, 4);

#define SLEEPING  0
#define LISTENING 1
#define THINKING  2
#define TALKING   3
#define IDLE      4

int state = IDLE;
unsigned long stateStart = 0;
int animFrame = 0;
unsigned long lastFrame = 0;

void drawMascot(int x, int y) {
  display.fillRect(x+2, y, 18, 2, BLACK);
  display.fillRect(x, y+2, 2, 14, BLACK);
  display.fillRect(x+20, y+2, 2, 14, BLACK);
  display.fillRect(x+2, y+2, 14, 14, BLACK);
  display.fillRect(x+4, y+4, 4, 4, WHITE);
  display.fillRect(x+14, y+4, 4, 4, WHITE);
  display.fillRect(x+6, y+11, 10, 3, WHITE);
  display.fillRect(x,    y+16, 4, 6, BLACK);
  display.fillRect(x+8,  y+16, 4, 6, BLACK);
  display.fillRect(x+16, y+16, 4, 6, BLACK);
  display.fillRect(x+4,  y+20, 4, 2, BLACK);
  display.fillRect(x+12, y+20, 4, 2, BLACK);
}

void typeWrite(const char* text, int x, int y, int delayMs) {
  display.setTextSize(1);
  display.setTextColor(BLACK);
  for (int i = 0; i < strlen(text); i++) {
    display.setCursor(x, y);
    for (int j = 0; j <= i; j++) {
      display.print(text[j]);
    }
    display.display();
    delay(delayMs);
  }
}

void bootSequence() {
  display.clearDisplay();
  drawMascot(31, 8);
  display.display();
  delay(1500);

  for (int i = 0; i < 4; i++) {
    display.clearDisplay();
    drawMascot(31 + (i % 2 == 0 ? 1 : -1), 8);
    display.display();
    delay(100);
  }

  display.clearDisplay();
  drawMascot(31, 2);
  display.display();
  delay(400);

  display.clearDisplay();
  drawMascot(31, 2);
  display.display();
  delay(200);
  typeWrite("Welcome sir!", 10, 36, 80);
  delay(1000);

  for (int i = 0; i < 3; i++) {
    display.clearDisplay();
    display.display();
    delay(150);
    display.clearDisplay();
    drawMascot(31, 2);
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(10, 36);
    display.print("Welcome sir!");
    display.display();
    delay(150);
  }
  delay(500);
}

void shutdownSequence() {
  display.clearDisplay();
  drawMascot(31, 2);
  display.display();
  delay(500);

  for (int i = 0; i < 6; i++) {
    display.clearDisplay();
    drawMascot(31 + (i % 2 == 0 ? 2 : -2), 2);
    display.display();
    delay(100);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.println("Goodbye!");
  display.println("See you soon");
  display.println("");
  display.println("(sleep mode)");
  display.display();
  delay(3000);

  for (int i = 0; i < 5; i++) {
    display.clearDisplay();
    display.display();
    delay(200);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(0, 0);
    display.println("Goodbye!");
    display.println("See you soon");
    display.println("");
    display.println("(sleep mode)");
    display.display();
    delay(200);
  }

  display.clearDisplay();
  display.display();
}

void drawStar(int x, int y) {
  display.drawPixel(x,   y,   BLACK);
  display.drawPixel(x+1, y-1, BLACK);
  display.drawPixel(x-1, y-1, BLACK);
  display.drawPixel(x+1, y+1, BLACK);
  display.drawPixel(x-1, y+1, BLACK);
}

void drawStars(int frame) {
  int stars[][2] = {{5,15},{15,20},{70,18},{75,13},{10,42},{72,40}};
  for (int i = 0; i < 6; i++) {
    if ((i + frame) % 2 == 0) drawStar(stars[i][0], stars[i][1]);
  }
}

void drawEye(int x, int y, bool closed, bool lookLeft, bool lookRight) {
  if (closed) {
    display.drawFastHLine(x-3, y, 6, BLACK);
  } else {
    display.drawCircle(x, y, 4, BLACK);
    int px = lookLeft ? x-1 : lookRight ? x+1 : x;
    display.fillCircle(px, y, 1, BLACK);
  }
}

void drawEyebrow(int x, int y, int mood) {
  if      (mood == 0) display.drawFastHLine(x-3, y, 6, BLACK);
  else if (mood == 1) display.drawFastHLine(x-3, y-2, 6, BLACK);
  else if (mood == 2) display.drawLine(x-3, y, x+3, y-2, BLACK);
}

void drawMouth(int state, int frame) {
  int cx = 42, cy = 38;

  if (state == IDLE || state == SLEEPING) {
    display.drawFastHLine(cx-6, cy, 12, BLACK);

  } else if (state == LISTENING) {
    display.drawFastHLine(cx-5, cy, 10, BLACK);

  } else if (state == THINKING) {
    for (int i = 0; i <= frame % 3; i++) {
      display.fillCircle(cx-4+(i*4), cy, 1, BLACK);
    }

  } else if (state == TALKING) {
    // static bla bla bla at top
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(8, 11);
    display.print("bla bla bla...");

    // mouth opens and closes
    if (frame % 2 == 0) {
      display.drawCircle(cx, cy + 4, 3, BLACK);
    } else {
      display.drawFastHLine(cx-3, cy + 4, 6, BLACK);
    }
  }
}

void drawZzz(int frame) {
  int offset = frame % 12;
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(68, 18-offset/2);
  display.print("z");
  if (offset > 3) { display.setCursor(73, 12-offset/2); display.print("Z"); }
  if (offset > 7) { display.setCursor(78,  6-offset/2); display.print("Z"); }
}

void drawQuestion(int frame) {
  int offset = frame % 12;
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(68, 18-offset/2);
  display.print("?");
  if (offset > 3) { display.setCursor(73, 12-offset/2); display.print("?"); }
  if (offset > 7) { display.setCursor(78,  6-offset/2); display.print("?"); }
}

void drawStatus(int state) {
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  switch(state) {
    case SLEEPING:  display.print("a lil nap..."); break;
    case LISTENING: display.print("im all ears!"); break;
    case THINKING:  display.print("let me think.."); break;
    case TALKING:   display.print("bla bla bla..."); break;
    case IDLE:      display.print("ready when u r"); break;
  }
  display.drawFastHLine(0, 9, 84, BLACK);
}

void drawFace(int state, int frame) {
  display.clearDisplay();
  drawStatus(state);
  drawStars(frame);

  bool leftClosed=false, rightClosed=false;
  bool lookLeft=false,   lookRight=false;
  int  browMood=0;

  if (state == SLEEPING) {
    leftClosed=true; rightClosed=true;
    drawZzz(frame);
  } else if (state == LISTENING) {
    leftClosed=(frame%8==0);
    browMood=1;
    drawQuestion(frame);
  } else if (state == THINKING) {
    lookLeft=(frame%6<3);
    lookRight=(frame%6>=3);
    browMood=2;
  } else if (state == TALKING) {
    rightClosed=true;
  } else if (state == IDLE) {
    bool blink=(frame%24<2);
    leftClosed=blink;
    rightClosed=blink;
  }

  drawEyebrow(22, 18, browMood);
  drawEyebrow(62, 18, browMood);
  drawEye(22, 26, leftClosed,  lookLeft, lookRight);
  drawEye(62, 26, rightClosed, lookLeft, lookRight);

  display.drawPixel(42, 31, BLACK);
  display.drawPixel(41, 32, BLACK);
  display.drawPixel(43, 32, BLACK);

  drawMouth(state, frame);
  display.display();
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);   // do not block the animation while waiting for a command
  display.begin();
  display.setContrast(55);
  display.clearDisplay();
  bootSequence();
  stateStart = millis();
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if      (cmd == "IDLE")     state = IDLE;
    else if (cmd == "LISTEN")   state = LISTENING;
    else if (cmd == "THINK")    state = THINKING;
    else if (cmd == "TALK")     state = TALKING;
    else if (cmd == "SLEEP")    state = SLEEPING;
    else if (cmd == "SHUTDOWN") { shutdownSequence(); return; }
  }

  unsigned long now = millis();
  if (now - lastFrame > 200) {
    animFrame++;
    lastFrame = now;
    drawFace(state, animFrame);
  }
}