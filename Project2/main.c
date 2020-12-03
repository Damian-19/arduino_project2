/*
 * USART_EX1_2020.c
 *
 * Updated: 25/11/2020 11:40:54
 *
 * This program assumes a clock frequency of 16 MHz
 * Baud rate of 9600 used
 * Based on Barnett Example Fig 2-40
 * This version uses sprintf
 * 
 * Created: 17/10/2011 01:09:43
 *  Author: Ciaran MacNamee
 */ 

#include <stdio.h>  /* Required for sprintf */
#include <avr/io.h>
#include <avr/interrupt.h>

#define FOUR_VOLTS 818
#define HUNDRED_MICROSECONDS 100

unsigned char qcntr = 0,sndcntr = 0;   /*indexes into the queue*/
unsigned char queue[50];       /*character queue*/ 

// global variables
unsigned long Time_Period, Time_Period_High, Time_Period_Low;
unsigned int start_edge, end_edge;		// globals for times
volatile unsigned char timecount1;
volatile unsigned int timecount0; // number of overflows reached
volatile unsigned int time_overflow; // number of overflows needed
volatile int tcnt0_start; // counter start
volatile int adc_flag; // new adc result flag
volatile uint16_t adc_reading; // variable to hold adc reading


/********************************
* timer initialization function
*********************************/
void timer_init(void)
{
	// timecount0 = 0; // initialize to 0
	tcnt0_start = 61; // begin timer count at 125
	time_overflow = 1; // initialize to 0
	
	TCCR0B = (5<<CS00);	// Set T0 Source = Clock (16MHz)/1024 and put Timer in Normal mode
	
	TCCR0A = 0;			// Not strictly necessary as these are the reset states
	
	TCNT0 = tcnt0_start;	// assign timer count start
	TIMSK0 = (1<<TOIE0);	// Enable Timer 0 interrupt
}

void timer1_init(void)
{
		TCCR1A = 0;											// Disable all o/p waveforms
		TCCR1B = ((1<<ICNC1) | (0<<ICES1) | (2<<CS10));		// Noise Canceller on, falling edge, CLK/8 (2MHz) T1 source
		TIMSK1 = ((1<<ICIE1) | (1 << TOIE1));				// Enable T1 OVF, T1 Input Cap Interrupt
		
		start_edge = 0;
		Time_Period = 0;
		Time_Period_High = 0;						/* Initialise Time_Period_High - not measured yet  */
		Time_Period_Low = 0;						/* Initialise Time_Period_Low - not measured yet  */
		
}
/********************************
* adc initialization function
*********************************/
void adc_init(void)
{
	// initialize global variables

	adc_flag = 0; // set if new adc result available
	
	// ADC initialization
	ADMUX = ((1<<REFS0) | (0 << ADLAR) | (0<<MUX0));  // AVCC selected for VREF, ADC0 as ADC input
	ADCSRA = ((1<<ADEN) | (1<<ADSC) | (1<<ADATE) | (1<<ADIE) | (7<<ADPS0)); /* Enable ADC, Start Conversion, Auto Trigger enabled, 
																		Interrupt enabled, Prescale = 128  */
	ADCSRB = (4<<ADTS0); // Select AutoTrigger Source to Timer/Counter0 Overflow
}

void Init_USART(void)
{
	UCSR0A	= 0x00;				/* Not necessary  */
	
	UCSR0B	= (1<<RXEN0)|(1<<TXEN0)|(1<<TXCIE0);	  /*enable receiver, transmitter and transmit interrupt, 0x58;*/
	UBRR0	= 103;  /*baud rate = 9600, USART 2X = 0 so UBRR0 = ((16*10^6)/(16*9600))-1 = 103.167, rounded to 103 */
	
	/* NB: the default state of UCSR0C is 0b00000110; which selects 8 bit, no parity, 1 stop bit */
	/* Don't be tempted to set it to all zeros - you will select 5 bit data word */
}
   
/*this function loads the queue and */
/*starts the sending process*/
void sendmsg (char *s)
{
   qcntr = 0;    /*preset indices*/
   sndcntr = 1;  /*set to one because first character already sent*/
   queue[qcntr++] = 0x0d;   /*put CRLF into the queue first*/
   queue[qcntr++] = 0x0a;
   while (*s) 
      queue[qcntr++] = *s++;   /*put characters into queue*/
   UDR0 = queue[0];  /*send first character to start process*/
}

int main(void)
{  
   char ch;  /* character variable for received character*/ 
   char buffer[60];  /* similar size to queue */
   
   DDRD = 0b11011000; // set PORTD bits 7,6,4,3 to outputs
   PORTD = 0;
   
   Init_USART();
   sei(); /*global interrupt enable */
   while (1)         
   {
      if (UCSR0A & (1<<RXC0)) /*check for character received*/
      {
         ch = UDR0;    /*get character sent from PC*/
         switch (ch)
         {
            case 'a':
				sprintf(buffer, "That was an a");
				sendmsg(buffer); /*send first message*/
				break;
            case 'b':
			case 'B':
				sprintf(buffer, "That was a b or a B");
				sendmsg(buffer); /*send second message*/
				break;
            default:
				sprintf(buffer, "That was not an a or a b");
				sendmsg(buffer); /*send second message*/
				break;
         } 
      } 
   }
   return 1; 
} 

/*this interrupt occurs whenever the */
/*USART has completed sending a character*/
ISR(USART_TX_vect)
{
   /*send next character and increment index*/
   if (qcntr != sndcntr)  
      UDR0 = queue[sndcntr++]; 
} 

ISR(TIMER0_OVF_vect)
{
	TCNT0 = tcnt0_start;
	++timecount0;
}

ISR(TIMER1_OVF_vect)
{
	++timecount1;
}

ISR(TIMER1_CAPT_vect)
{
	unsigned long clocks;
	
	end_edge = ICR1;
	clocks = ((unsigned long)timecount1 * 65536) + (unsigned long)end_edge - (unsigned long)start_edge;
	Time_Period = (clocks/2);
	
	if (TCCR1B &(0<<ICES1) == (0<<ICES1))
	{
		Time_Period_Low = (clocks/2);
	} else {
		Time_Period_High = (clocks/2);
	}
	TCCR1B = TCCR1B ^ (0<<ICES1);
	
	start_edge = end_edge;
	timecount1 = 0;
	
	if((Time_Period_High+Time_Period_Low)>HUNDRED_MICROSECONDS)
	{
		PORTD = PORTD | (1<<PORTD6);
	} else {
		PORTD = PORTD & ~(1<<PORTD6);
	}
}

ISR(ADC_vect)
{
	adc_reading = ADC;
	adc_flag = 1;
	
	if((adc_reading) > FOUR_VOLTS)
	{
		PORTD |= 0b10000000;
	} else {
		PORTD &= ~0b100000000;
	}
}

