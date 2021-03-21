#include "Sleep.h"

#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

ISR(WDT_vect) {}

void SleepSetup(uint8_t wdp) {
    /* Clear the reset flag. */
    MCUSR &= ~(1 << WDRF);

    /* In order to change WDE or the prescaler, we need to
     * set WDCE (This will allow updates for 4 clock cycles).
     */
    WDTCSR |= (1 << WDCE) | (1 << WDE);

    /* set new watchdog timeout prescaler value */
    WDTCSR = wdp & 0x07;

    /* Enable the WD interrupt (note no reset). */
    WDTCSR |= _BV(WDIE);
}

void SleepStart() {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    cli();
    sleep_bod_disable();
    sei();
    wdt_reset();

    /* Now enter sleep mode. */
    sleep_mode();

    /* The program will continue from here after the WDT timeout*/
    sleep_disable(); /* First thing to do is disable sleep. */
}