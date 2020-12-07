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
volatile uint16_t adc_reading_mv;
volatile unsigned long clocks;
volatile unsigned int timer_cont_flag;
volatile unsigned int adc_cont_flag;
volatile unsigned int capture_flag;


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
		capture_flag = 0;
		
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
   timer_init();
   timer1_init();
   adc_init();
   unsigned int val = 0;
   sei(); /*global interrupt enable */
   while (1)         
   {
      if (UCSR0A & (1<<RXC0)) /*check for character received*/
      {
		  if (capture_flag == 1)
		  {
			  capture_flag = 0;
         ch = UDR0;    /*get character sent from PC*/
         switch (ch)
         {
            case 'T':
			case 't':
				sprintf(buffer, "Period of 555 timer in microseconds: %lu", clocks);
				sendmsg(buffer);
				break;
            case 'L':
			case 'l':
				sprintf(buffer, "Low pulse of 555 timer in microseconds: %lu", Time_Period_Low);
				sendmsg(buffer);
				break;
			case 'H':
			case 'h':
				sprintf(buffer, "High pulse of 555 timer in microseconds: %lu", Time_Period_High);
				sendmsg(buffer);
				break;
			case 'C':
			case 'c':
				//sprintf(buffer, "Continuously report timer input period in microseconds");
				timer_cont_flag = 1;
				sprintf(buffer, "val = %i", timer_cont_flag);
				sendmsg(buffer);
				break;
			case 'E':
			case 'e':
				sprintf(buffer, "Continuous timer input reporting stopped.");
				timer_cont_flag = 0;
				//sprintf(buffer, "val = %i", timer_cont_flag);
				sendmsg(buffer);
				break;
			case 'A':
			case 'a':
				sprintf(buffer, "ADC Value: %u", adc_reading);
				sendmsg(buffer);
				break;
			case 'V':
			case 'v':
				adc_reading_mv = ((adc_reading*5000)/1024); // mV calculation
				sprintf(buffer, "ADC Value: %u mV", adc_reading_mv);
				sendmsg(buffer);
				break;
			case 'M':
			case 'm':
				sprintf(buffer, "Continuously report ADC0 conversion result in mV");
				sendmsg(buffer);
				adc_cont_flag = 1;
				break;
			case 'N':
			case 'n':
				sprintf(buffer, "Stop continuous reporting of ADC0 input");
				sendmsg(buffer);
				adc_cont_flag = 0;
				break;
			case 'W':
			case 'w':
				sprintf(buffer, "Toggle the LED bit 4 at 125ms");
				sendmsg(buffer);
				break;
			case 'U':
			case 'u':
				sprintf(buffer, "Stop toggling LED bit 4");
				sendmsg(buffer);
				break;
			case 'P':
			case 'p':
				sprintf(buffer, "Report state of PORTD outputs");
				sendmsg(buffer);
				break;
			case 'S':
			case 's':
				sprintf(buffer, "Report current value of OCR2B register");
				sendmsg(buffer);
				break;
            default:
				sprintf(buffer, "Input not recognized.");
				sendmsg(buffer); /*send second message*/
				break;
         } 
      } 
		 }
	  if (timer_cont_flag == 1)
	  {
		  val = Time_Period_High + Time_Period_Low;
		  sprintf(buffer, "Value: %u", val);
		  sendmsg(buffer);
		  capture_flag = 0;
	  }
	  if (adc_cont_flag == 1)
	  {
		  adc_reading_mv = ((adc_reading*5000)/1024); // mV calculation
		  sprintf(buffer, "ADC Value: %u mV", adc_reading_mv);
		  sendmsg(buffer);
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
	
	
	end_edge = ICR1;
	capture_flag = 1;
	clocks = ((unsigned long)timecount1 * 65536) + (unsigned long)end_edge - (unsigned long)start_edge;
	Time_Period = (clocks/2);
	
	if (TCCR1B &(1<<ICES1) == (1<<ICES1))
	{
		Time_Period_Low = (clocks/2);
	} else {
		Time_Period_High = (clocks/2);
	}
	// TCCR1B = TCCR1B ^ (0<<ICES1);
	
	start_edge = end_edge;
	timecount1 = 0;
	
	if((Time_Period_High+Time_Period_Low)>HUNDRED_MICROSECONDS)
	{
		PORTD |= (1<<PORTD6);
	} else {
		PORTD = (0<<PORTD6);
	}
}

ISR(ADC_vect)
{
	adc_reading = ADC;
	adc_flag = 1;
	
	if((adc_reading) > FOUR_VOLTS)
	{
		PORTD |= (1<<PORTD7);
	} else {
		PORTD = (0<<PORTD7);
	}
}

