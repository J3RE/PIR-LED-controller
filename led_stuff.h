#ifndef led_stuff_h
#define led_stuff_h

  #include <avr/io.h>

  #define HUE_MAX         191
  #define VALUE_MAX       255

  #define APPLY_DIMMING(X) (X)

  #define SATURATION_MAX  255
  #define HSV_SECTION_6 (0x20)
  #define HSV_SECTION_3 (0x40)

  /*
    HSV-color space
    hue 0..255
    saturation 0..255
    value 0..255
  */
  typedef struct
  {
    uint8_t hue;
    uint8_t saturation;
    uint8_t value;
  } HSV;

  /*
    RGB-color space
    red 0..255
    green 0..255
    blue 0..255
  */
  typedef struct
  {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  } RGB;

  uint8_t fadeHSV(HSV* hsv, int8_t fade_hue, int8_t fade_value);
  void hsv2rgb(HSV* hsv, RGB* rgb);
  void rgb2pwm(RGB* rgb);

#endif