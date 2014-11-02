/*
	Copyright (c) 2011 by Ernst Buchmann 
	
	Code based on the work of Stefan Engelke and Brennan Ball
	
    Permission is hereby granted, free of charge, to any person 
    obtaining a copy of this software and associated documentation 
    files (the "Software"), to deal in the Software without 
    restriction, including without limitation the rights to use, copy, 
    modify, merge, publish, distribute, sublicense, and/or sell copies 
    of the Software, and to permit persons to whom the Software is 
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be 
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
    DEALINGS IN THE SOFTWARE.

    
*/

/*
IO pins
PB0: o, Output for battery voltage measurement, connected to top of voltage divider
PB1: o, LED output
PB2: i, Switch input
PB4: i, ADC input
*/

#define F_CPU 125000 //Watchdog oscillator as main clock source

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define BATTLOW (uint16_t)698UL //cutoff @ 3000mv

#define STATE_OFF 0
#define STATE_ON 1
volatile uint8_t state = STATE_OFF;

#define STATE_ADC_OFF 0
#define STATE_ADC_CONV 1
#define STATE_ADC_FINISHED 2
volatile uint8_t adcstate = STATE_ADC_OFF;

#define CNT10S 76
volatile uint8_t messCnt = 0;

#define ANTIBOUNCE 3
volatile uint8_t antiBounceCnt = 0;

#define FLAGS_WDT 1
volatile uint8_t flags = 0;

unsigned long seed __attribute__ ((section (".noinit"))); //"Random" seed for rand()

void setState(uint8_t st)
{
	if(st == STATE_ON)
	{
		TCCR0A = (1<<WGM00) | (1<<WGM01) | (1<<COM0B1);
		TCCR0B = (1<<CS00);
		TIMSK0 = (1<<OCIE0B);
		state = STATE_ON;
	}
	else
	{
		state = STATE_OFF; 
		TCCR0A = 0;
		TCCR0B = 0;
		TIMSK0 = 0;
		PORTB &= ~((1<<PINB0) | (1<<PINB1));
		ADCSRA &= ~((1<<ADEN) | (1<<ADSC));
		messCnt = 0;
		adcstate = STATE_ADC_OFF;
	}
}

void off()
{
	setState(STATE_OFF);
}

void on()
{
	setState(STATE_ON);
}

void toggle()
{
	if(state == STATE_ON)
	{
		off();
	}
	else
	{
		on();
	}
}

int main(void)
{
	srand(seed);
	DDRB = (1<<PINB0) | (1<<PINB1); //IO setup
	PORTB |= (1<<PINB2); //enable pullup on PB2
	//Enable watchdog: Interrupt-mode
	WDTCR |= (1<<WDCE) | (1<<WDTIE) | (1<<WDP0) | (1<<WDP1); //One interrupt all 0.15 to 0.125 seconds
	//Pinchange
	GIMSK = (1<<PCIE);
	PCMSK = (1<<PCINT2);
	//ADC init: Vref=1.1V
	ADMUX = (1<<REFS0) | (1<<MUX1); //PB4 as ADC-Pin
	ADCSRA = (1<<ADIE); //Divide clk by 2 ~125kHz/2 = ~75kHz
	sei();
    while(1)
    {
		if(state == STATE_OFF)
			set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		else
			set_sleep_mode(SLEEP_MODE_IDLE);	
		sleep_enable();
		sleep_cpu();
		if(state == STATE_ON)
		{
			if((messCnt > CNT10S) && (adcstate == STATE_ADC_OFF))
			{
				messCnt = 0;
				PORTB |= (1<<PINB0);
				ADCSRA |= (1<<ADEN) | (1<<ADSC);
				adcstate = STATE_ADC_CONV;
			}
			if(adcstate == STATE_ADC_FINISHED)
			{
				if(ADC < BATTLOW)
				{
					off();
				}
				ADCSRA &= ~((1<<ADEN) | (1<<ADSC));
				adcstate = STATE_ADC_OFF;
			}
			if((flags & FLAGS_WDT) > 0)
				OCR0B = 128 + rand() % 128;
		}
		flags = 0;
    }
}

ISR(PCINT0_vect)
{
	if((PINB & (1<<PINB2)) == 0)
	{
		if(antiBounceCnt >= ANTIBOUNCE)
			toggle();
		antiBounceCnt = 0;
	}
}

ISR(WDT_vect)
{
	flags |= FLAGS_WDT;
	if(state == STATE_ON)
		messCnt++;
	if(antiBounceCnt < ANTIBOUNCE)
		antiBounceCnt++;
}

ISR(ADC_vect)
{
	adcstate = STATE_ADC_FINISHED;
}

ISR(TIM0_COMPB_vect){}