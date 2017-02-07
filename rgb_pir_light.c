#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "led_stuff.h"

#define RED     PA5 // OCR1B
#define GREEN   PA7 // OCR0B
#define BLUE    PA6 // OCR1A

#define LDR     PA0
#define PIR     PA1
#define BUTTON  PA2
#define LED     PA3

#define PWR_LDR PB0

// divide times by 2, because we count in 2ms steps

#define FADE_DELAY        70/2

// #define TIMEOUT_PIR       30000/2
// #define TIMEOUT_BUTTON    600000/2

// for testing shorter times
#define TIMEOUT_PIR       6000/2
#define TIMEOUT_BUTTON    8000/2

#define BUTTON_POLL_TIME  20/2
#define BUTTON_LONG_PRESS 1500/2
#define LED_BLINK_TIME    200/2

#define LDR_THRESHOLD     750
#define FADE_UP_STEPS     10
#define FADE_DOWN_STEPS   -20

volatile unsigned long millis_counter = 0;
volatile uint8_t action = 0;
uint8_t pir_old = 0;
uint8_t button_history = 0xFF;

// function prototypes
unsigned long millis();
void checkButton();
uint8_t checkLDR();
void trimLDR();

int main()
{

  // configure outputs
  DDRA = (1 << LED);
  // enable internal pullup for button
  PORTA = (1 << BUTTON);

  // timer0 config
  TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
  // prescaler 64
  TCCR0B = (1 << CS01) | (1 << CS00);
  // enable timer0 overflow intterupt
  TIMSK0 = (1 << TOIE0);

  // timer1 config
  TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
  // prescaler 64
  TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);

  // enable pin change intterupt on PIR-pin
  GIMSK = (1 << PCIE0);
  PCMSK0 = (1 << PCINT1);

  HSV *color_hsv = &(HSV) {.hue = 0, .saturation = SATURATION_MAX, .value = 0};
  RGB *color_rgb = &(RGB) {.red = 0, .green = 0, .blue = 0};

  unsigned long millis_fade = 0;
  unsigned long millis_timeout = 0;
  unsigned long millis_button = 0;
  unsigned long millis_led = 0;

  int8_t adj_value = 0;
  uint8_t mode = 0;
  uint8_t led = 0;
  uint8_t blink_led = 3;

  // blink 3 times after reset
  while(blink_led)
  {
    if(led)
    {
      PORTA &= ~(1 << LED);
      led = 0;
      blink_led--;
    }
    else
    {
      PORTA |= (1 << LED);
      led = 1;
    }
    _delay_ms(LED_BLINK_TIME);
  }
  _delay_ms(LED_BLINK_TIME);

  // enable interrupts
  sei();

  for(;;)
  {

    // check button
    if((millis() - millis_button) >= BUTTON_POLL_TIME)
    {
      checkButton();
      millis_button = millis();
    }

    // blink led
    if(blink_led > 0)
    {
      if((millis() - millis_led) >= LED_BLINK_TIME)
      {
        if(led)
        {
          PORTA &= ~(1 << LED);
          led = 0;
          blink_led--;
        }
        else
        {
          PORTA |= (1 << LED);
          led = 1;
        }
        millis_led = millis();
      }
    }

    // something happend
    if(action)
    {
      //  default state
      uint8_t state = 3;

      /*
        mode:
          0...off
          1...on-PIR
          2...on-button
      */
      if(!mode)
      {
        if (action == 1)
        {
          if(checkLDR())
          {
            // mode 1
            state = 1;
          }

        }
        else
        {
          // mode 2
          state = 1;
        }
      }
      else
      {
        // retriggered by PIR sensor only if initially triggered by PIR
        if ((action == 1) && (mode == 1))
        {
          state = 2;
        }
        else if (action == 2)
        {
          // turn off
          // set mode to 2, to prevent retriggering by the PIR sensor while fading off
          state = 0;
          mode = 2;
        }
      }

      switch(state)
      {
        // turn off
        case 0:
          adj_value = FADE_DOWN_STEPS;
          blink_led++;
          break;

        // turn on by PIR or button
        case 1:
          mode = action;
          adj_value = FADE_UP_STEPS;
          
          /*
            +2 blinks activated by PIR
            +3 blinks activated by button
          */
          blink_led += action + 1;

          millis_timeout = millis();
          millis_fade = millis_timeout;

          // configrue RGB-Channels as outputs
          DDRA |= (1 << RED) | (1 << GREEN) | (1 << BLUE);
          break;

        // retrigger by PIR sensor
        case 2:
          millis_timeout = millis();
          adj_value = FADE_UP_STEPS;
          blink_led++;
          break;

        default:
          break;
      }
    
      // action handled, no need to do this again
      action = 0;
    }

    if(mode)
    {
      if((millis() - millis_fade) > FADE_DELAY)
      {
        if(!fadeHSV(color_hsv, 2, adj_value))
        {
          mode = 0;
        }
        hsv2rgb(color_hsv, color_rgb);
        rgb2pwm(color_rgb);
        millis_fade = millis();
      }

      if((millis() - millis_timeout) > ((mode < 2) ? TIMEOUT_PIR : TIMEOUT_BUTTON))
      {
        adj_value = FADE_DOWN_STEPS;
      }
    }
    // only sleep if button is released and blinking is done
    else if ((button_history == 0xff) && !blink_led)
    {

      // blink before sleeping, for debugging
      PORTA |= (1 << LED);
      _delay_ms(150);
      PORTA &= ~(1 << LED);


      // configure all pins as input while sleeping
      DDRA = 0x00;
      DDRB = 0x00;
      // all low except, the pullup resistor for the button
      PORTA = (1 << BUTTON);
      PORTB = 0x00;

      // also enable pin change interrupt on button pin
      PCMSK0 |= (1 << PCINT2);
      set_sleep_mode (SLEEP_MODE_PWR_DOWN);
      power_all_disable ();  // power off ADC, Timer 0 and 1, serial interface

      cli();
      sleep_enable ();       // ready to sleep
      sei();
      sleep_cpu ();          // sleep
      sleep_disable ();      // precaution
      power_timer0_enable();
      power_timer1_enable();
      power_adc_enable();

      // disable pin change interrupt for the button, since we are polling the butten now
      PCMSK0 &= ~(1 << PCINT2);

      // set LED as output
      DDRA = (1 << LED);
      PORTA = (1 << BUTTON);
    }
  
  }
  return 0;
}


// triggers every 2,048ms
ISR(TIM0_OVF_vect)
{
  millis_counter++;
}

// the PIR-sensor triggered an interrupt, do something
ISR(PCINT0_vect)
{
  if ((PINA & (1 << PIR)) && !pir_old)
  { 
    // if positive edge on PIR-pin
    action = 1;
  }
  pir_old = PINA & (1 << PIR);
}

/*
  get the value of the millis counter 
  make sure the value doesn't change while reading
*/
unsigned long millis()
{
  cli();
  unsigned long temp = millis_counter;
  sei();
  return temp;
}


/*
  clever debouncing function
  check out the hackaday article from Elliot Williams
  http://hackaday.com/2015/12/10/embed-with-elliot-debounce-your-noisy-buttons-part-ii/
*/
void checkButton()
{
  static unsigned long millis_button_pressed = 0;
  static uint8_t long_press_detected;

  // read button status
  button_history = button_history << 1;
  button_history |= (PINA  >> BUTTON) & 0x01;

  // button pressed
  if ((button_history & 0b11000111) == 0b11000000)
  { 
    millis_button_pressed = millis();
    button_history = 0b00000000;
  }
  else if(button_history == 0b00000000)
  {
    // button pressed for longer than BUTTON_LONG_PRESS
    if((millis() - millis_button_pressed) >= BUTTON_LONG_PRESS)
    {
      // long button press detected
      long_press_detected = 1; 
      trimLDR();
    }
  }
  // button released
  else if ((button_history & 0b11000111) == 0b00000111)
  { 
    if(!long_press_detected)
    {
      // short press
      action = 2;
    }  
    button_history = 0b11111111;
    long_press_detected = 0;
  }
}

// check the ambient brightness level and return 1 if dark enough to turn on LEDs
uint8_t checkLDR()
{
  uint16_t average_ldr = 0;

  // configure ADC
  // Vcc as analog reference, single ended input PA0
  ADMUX = 0x00;

  // enable ADC, slow conversion
  ADCSRA = (1 << ADEN) | (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2);
  
  // dummy readings
  for (int i = 0; i < 4; ++i)
  {
    // start conversion
    ADCSRA |= (1<<ADSC);
    // wait until conversion is finished
    while(ADCSRA & (1<<ADSC));
    average_ldr += ADC;
  }

  // check LDR
  // set as ldr supply as output
  DDRB |= (1 << PWR_LDR);
  // power up LDR
  PORTB |= (1 << PWR_LDR);
  // wait a bit, to let the voltage stabilize
  _delay_ms(200);
  
  average_ldr = 0;
  for (int i = 0; i < 8; ++i)
  {
    // start conversion
    ADCSRA |= (1<<ADSC);
    // wait until conversion is finished
    while(ADCSRA & (1<<ADSC));
    average_ldr += ADC;
    _delay_ms(50);
  }
  average_ldr >>= 3;

  // disable ADC
  ADCSRA = 0x00;
  
  // power down LDR
  // set LDR supply as input to save energy
  DDRB &= ~(1 << PWR_LDR);
  PORTB &= ~(1 << PWR_LDR);

  if(average_ldr < LDR_THRESHOLD)
  {
    return 1;
  }

  return 0;
}

/*
  this mode is for adjusting the threshold
  turns RGB-LEDs on if the ambient brightness is lower than the Threshold
  to escape this mode reset the circuit
*/
void trimLDR()
{
  // set outputs and turn on LED
  DDRA |= (1 << RED) | (1 << GREEN) | (1 << BLUE) | (1 << LED);
  PORTA |= (1 << LED);

  while(1)
  {
    if(checkLDR())
    {
      OCR1B = 127;
      OCR0B = 127;
      OCR1A = 127; 
    }
    else
    {
      OCR1B = 0;
      OCR0B = 0;
      OCR1A = 0;
    }
  }
}