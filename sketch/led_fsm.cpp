/*
 LED fsm logic
 this allows for 'background' LED blinking patterns.
 */

#include <WiFi.h>

// external function prototypes
void led_on(bool turn_it_on);

namespace LED_FSM
{
  unsigned long pervious_toggle_millis;
  unsigned long next_toggle_millis;
  uint16_t t_on_millis;
  uint16_t t_off_millis;
  uint8_t blink_times;
  uint8_t blink_count;
  bool    led_gpio_state;
  uint8_t fsm_state;
  uint8_t next_state;

  const uint8_t FSM_STATE_IDLE       = 0;
  const uint8_t FSM_STATE_CONFIGURED = 1;
  const uint8_t FSM_STATE_IN_BURST   = 2;

  bool setup_blink_pattern(uint8_t times, uint16_t on_millis, uint16_t off_millis)
  {
    if(fsm_state != FSM_STATE_IDLE)
    {
      Serial.println("[NOTOK] cannot configure FSM state while not IDLE");
      return false;
    }
    if((times < 0) || (times > 10))
    {
      Serial.println("'times' (" + String(times) + ") out of limits");
      return false;
    }
    blink_times = times;
    t_on_millis = on_millis;
    t_off_millis = off_millis;
    blink_count = 0;    
    next_state = FSM_STATE_CONFIGURED;
    return true;
  } // bool setup_blink_pattern(uint8_t times, uint16_t on_millis, uint16_t off_millis)

} // namespace LED_FSM
