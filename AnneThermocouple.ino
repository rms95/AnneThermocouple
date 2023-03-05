#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif

#include <Arduino.h>
#include <Adafruit_MAX31855.h>
#include "epd_driver.h"
#include "firasans.h"
#include "opensans8.h"
#include "Button2.h"
#include "lilygo.h"
#include "pins.h"

Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);
Button2 btn3(BUTTON_3);
Adafruit_MAX31855 thermocouple(14 /* CLK */, 12 /* CS */, 13 /* SO */);

uint8_t *framebuffer;
char printBuffer[20];

void buttonPressed(Button2 &b)
{

}

#define UPDATE_RATE 20 /// seconds

#define CHART_MARGIN 20
#define CHART_X_START 60
#define CHART_X_STOP (EPD_WIDTH - CHART_MARGIN * 2)
#define CHART_X_RANGE (60 * 60 * 8 + 1) // 8 Hours
#define CHART_X_GRID_MAJOR (60 * 60) // 1 hour
#define CHART_WIDTH (CHART_X_STOP - CHART_X_START)
#define CHART_Y_START CHART_MARGIN
#define CHART_Y_STOP (EPD_HEIGHT - CHART_MARGIN * 2)
#define CHART_Y_RANGE 1301 // degree C
#define CHART_Y_GRID_MAJOR 100 // degree C
#define CHART_HEIGHT (CHART_Y_STOP - CHART_Y_START)
#define CHART_X_CONVERT(seconds) (CHART_X_START + seconds * CHART_WIDTH / CHART_X_RANGE)
#define CHART_Y_CONVERT(degC) (CHART_Y_STOP - degC * CHART_HEIGHT / CHART_Y_RANGE)

void display_redraw() {
  epd_poweron();
  epd_clear();
  
  memset(framebuffer, 0xFF, sizeof(framebuffer));

  // Draw chart grid
  int cursor_x = 0;
  int cursor_y = 0;
  for (int y = 0; CHART_Y_CONVERT(y) > CHART_Y_START; y += CHART_Y_GRID_MAJOR) // Temperature grid (horizontal lines)
  {
    epd_draw_line(CHART_X_START, CHART_Y_CONVERT(y), CHART_X_STOP, CHART_Y_CONVERT(y), y ? 0x80 : 0, framebuffer);
    int cursor_x = 12;
    int cursor_y = CHART_Y_CONVERT(y) + 6;
    sprintf(printBuffer, "%d", y);
    write_string(&OpenSans8, printBuffer, &cursor_x, &cursor_y, framebuffer);
  }
  for (int x = 0; CHART_X_CONVERT(x) < CHART_X_STOP; x += CHART_X_GRID_MAJOR) // Time grid (vertical lines)
  {
    epd_draw_line(CHART_X_CONVERT(x), CHART_Y_START, CHART_X_CONVERT(x), CHART_Y_STOP, x ? 0x80 : 0, framebuffer);
    int cursor_x = CHART_X_CONVERT(x) - 8;
    int cursor_y = CHART_Y_STOP + 18;
    sprintf(printBuffer, "%dh", x / 3600);
    write_string(&OpenSans8, printBuffer, &cursor_x, &cursor_y, framebuffer);
  }

  // Draw text
  cursor_x = 80;
  cursor_y = 60;
  write_string(&FiraSans, "Temperature:", &cursor_x, &cursor_y, framebuffer);

  // Draw buffer
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
}

void add_chart_point(int time, int c) {
  int newX = CHART_X_CONVERT(time) - 9;
  int newY = CHART_Y_CONVERT(c) + 23;
  epd_poweron();
  write_string(&FiraSans, "°", &newX, &newY, NULL);
  epd_poweroff();
  newX = CHART_X_CONVERT(time) - 9;
  newY = CHART_Y_CONVERT(c) + 23;
  write_string(&FiraSans, "°", &newX, &newY, framebuffer);
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) delay(1);
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) {
      Serial.println("ERROR,alloc memory failed !!!");
      while (1);
  }
  display_redraw();

  btn1.setPressedHandler(buttonPressed);
  btn2.setPressedHandler(buttonPressed);
  btn3.setPressedHandler(buttonPressed);

  Serial.println("log,Thermocouple logger");
  // wait for MAX chip to stabilize
  delay(500);

  Serial.println("log,Initializing sensor...");
  if (!thermocouple.begin()) {
    Serial.println("ERROR,Thermocouple begin");
    while (1) delay(10);
  }
  Serial.println("log,READY");

  // Example data
  return;
  int temp = 25;
  for (int i = 0; i < 3550 * 5; i += 60 * 2)
  {
    temp += random(10) - 3;
    add_chart_point(i, temp);
  }
}

int lastUpdate = -UPDATE_RATE;
int everyNframes = 0;

void loop()
{
  btn1.loop();
  btn2.loop();
  btn3.loop();
  int now = millis() / 1000;
  if (now > lastUpdate + UPDATE_RATE)
  {
    lastUpdate = now;

    // Read data
    Serial.print("Temp,");
    Serial.print(thermocouple.readInternal());

    double c = thermocouple.readCelsius();
    if (isnan(c)) {
      Serial.println();
      uint8_t e = thermocouple.readError();
      if (e & MAX31855_FAULT_OPEN) Serial.println("ERROR,Thermocouple is open - no connections.");
      if (e & MAX31855_FAULT_SHORT_GND) Serial.println("ERROR,Thermocouple is short-circuited to GND.");
      if (e & MAX31855_FAULT_SHORT_VCC) Serial.println("ERROR,Thermocouple is short-circuited to VCC.");
      return;
    }
    Serial.print(",");
    Serial.println(c);

    // Add point to chart
    add_chart_point(now, c);

    // Re-draw frame buffer every once in a while
    if (everyNframes++ > 20)
    {
      epd_draw_grayscale_image(epd_full_screen(), framebuffer);
      everyNframes = 0;
    }

    // Update temperature
    int cursor_x = 350;
    int cursor_y = 60;
    epd_clear_area({ .x = 345, .y = 20, .width = 200, .height = 45 });
    sprintf(printBuffer, "%d °C", (int)c);
    write_string(&FiraSans, printBuffer, &cursor_x, &cursor_y, NULL);
  }
}
