/*
 * main.c
 *
 * Created: 07/12/2020 
 * Author: Damian Larkin (18230253) & James Cusack (18250416)
 *
 * Program to satisfy ET4047 Project 2
 */ 


#include <stdio.h>  /* Required for sprintf */
#include <avr/io.h>
#include <avr/interrupt.h>

#define FOUR_VOLTS 818 // define 4V on ADC
#define HUNDRED_MICROSECONDS 100

unsigned char qcntr = 0, sndcntr = 0;   /*indexes into the queue*/
unsigned char queue[50];       /*character queue*/ 

// global variables
unsigned long Time_Period, Time_Period_High, Time_Period_Low;
unsigned int start_edge, end_edge;		// globals for times
volatile unsigned char timecount1; // number of overflows Timer/Counter1
volatile unsigned int timecount0; // number of overflows Timer/Counter0
volatile unsigned int time_overflow; // number of overflows needed Timer/Counter0
volatile int tcnt0_start; // Timer/Counter0 start value
volatile int adc_flag; // new adc result flag
volatile uint16_t adc_reading; // variable to hold adc reading
volatile uint16_t adc_reading_mv; // adc reading converted to mV
volatile unsigned long clocks;
volatile unsigned int timer_cont_flag; // timer continuous mode flag
volatile unsigned int adc_cont_flag; // adc continuous mode flag
volatile unsigned int capture_flag; // new input capture flag
volatile unsigned int b4_toggle_flag; // PORTD4 toggle flag

/***********************************************
* Timer/Counter0 initialization function
************************************************/
void timer_init(void)
{
	// timecount0 = 0; // initialize to 0
	tcnt0_start = 61; // begin timer count at 125
	time_overflow = 1; // initialize to 0
	b4_toggle_flag = 0;
	
	TCCR0B = (5<<CS00);	// Set T0 Source = Clock (16MHz)/1024 and put Timer in Normal mode
	
	TCCR0A = 0;			// Not strictly necessary as these are the reset states
	
	TCNT0 = tcnt0_start;	// assign timer count start
	TIMSK0 = (1<<TOIE0);	// Enable Timer 0 interrupt
}

/***********************************************
* Timer/Counter1 initialization function
************************************************/
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

/***********************************************
* Timer/Counter2 initialization function
************************************************/
void timer2_init(void)
{
	TCCR2B = ((1<<CS22) | (0<<CS21) | (1<<CS20) | (0<<WGM22));
	TCCR2A = ((1<<COM2B1) | (0<<COM2B0) | (0<<WGM21) | (1<<WGM20)); 
	TIMSK2 = 0; // disable interrupts
}

/***********************************************
* ADC initialization function
************************************************/
void adc_init(void)
{
	adc_flag = 0; // initialize variable to 0
	
	// ADC initialization
	ADMUX = ((1<<REFS0) | (0 << ADLAR) | (0<<MUX0));  // AVCC selected for VREF, ADC0 as ADC input
	ADCSRA = ((1<<ADEN) | (1<<ADSC) | (1<<ADATE) | (1<<ADIE) | (7<<ADPS0)); /* Enable ADC, Start Conversion, Auto Trigger enabled, 
																		Interrupt enabled, Prescale = 128  */
	ADCSRB = (4<<ADTS0); // Select AutoTrigger Source to Timer/Counter0 Overflow
}

/***********************************************
* USART initialization function
************************************************/
void usart_init(void)
{
	UCSR0A	= 0x00;
	
	UCSR0B	= (1<<RXEN0)|(1<<TXEN0)|(1<<TXCIE0);	  /*enable receiver, transmitter and transmit interrupt, 0x58;*/
	UBRR0	= 103;  /*baud rate = 9600, USART 2X = 0 so UBRR0 = ((16*10^6)/(16*9600))-1 = 103.167, rounded to 103 */
}

/***********************************************
* function to load queue and start sending process
************************************************/
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

/***********************************************
* main function
************************************************/
int main(void)
{  
   char ch;  /* character variable for received character*/ 
   char buffer[60];  /* similar size to queue */
   
   DDRD = 0b11011000; // set PORTD bits 7,6,4,3 to outputs
   //PORTD = 0;
   
   // call initialisation functions
   usart_init();
   timer_init();
   timer1_init();
   timer2_init();
   adc_init();
   unsigned int val = 0;
   sei(); /*global interrupt enable */
   
   while (1)         
   {
      if (UCSR0A & (1<<RXC0)) /*check for character received*/
      {
		  if (capture_flag == 1)
		  {
			 //capture_flag = 0;
			 ch = UDR0;    /*get character sent from PC*/
			 switch (ch)
			 { 
				// case 0-9 controls LED bit 3 PWM
				// PWM START
				case '0':
					sprintf(buffer, "LED bit 3 set to OFF");
					sendmsg(buffer);
					OCR2B = 0; // OFF
					break;
				case '1':
					sprintf(buffer, "LED bit 3 set to 10%% brightness");
					sendmsg(buffer);
					OCR2B = 26; // 10% of 256 approx.
					break;
				case '2':
					sprintf(buffer, "LED bit 3 set to 20%% brightness");
					sendmsg(buffer);
					OCR2B = 50; // 20% of 256 approx.
					break;
				case '3':
					sprintf(buffer, "LED bit 3 set to 30%% brightness");
					sendmsg(buffer);
					OCR2B = 77; // 30% of 256 approx.
					break;
				case '4':
					sprintf(buffer, "LED bit 3 set to 40%% brightness");
					sendmsg(buffer);
					OCR2B = 102; // 40% of 256 approx.
					break;
				case '5':
					sprintf(buffer, "LED bit 3 set to 50%% brightness");
					sendmsg(buffer);
					OCR2B = 128; // 50% of 256 approx.
					break;
				case '6':
					sprintf(buffer, "LED bit 3 set to 60%% brightness");
					sendmsg(buffer);
					OCR2B = 154; // 60% of 256 approx.
					break;
				case '7':
					sprintf(buffer, "LED bit 3 set to 70%% brightness");
					sendmsg(buffer);
					OCR2B = 179; // 70% of 256 approx.
					break;
				case '8':
					sprintf(buffer, "LED bit 3 set to 80%% brightness");
					sendmsg(buffer);
					OCR2B = 205; // 80% of 256 approx.
					break;
				case '9':
					sprintf(buffer, "LED bit 3 set to 90%% brightness");
					sendmsg(buffer);
					OCR2B = 230; // 90% of 256 approx.
					break;
				// PWM END
				case 'T': // Period 555 Timer
				case 't':
					sprintf(buffer, "Period of 555 timer in microseconds: %lu", clocks);
					sendmsg(buffer);
					break;
				case 'L': // Low Pulse 555 Timer - CHECK
				case 'l':
					sprintf(buffer, "Low pulse of 555 timer in microseconds: %lu", Time_Period_Low);
					sendmsg(buffer);
					break;
				case 'H': // High Pulse 555 Timer
				case 'h':
					sprintf(buffer, "High pulse of 555 timer in microseconds: %lu", Time_Period_High);
					sendmsg(buffer);
					break;
				case 'C': // Continuous Timer Reporting
				case 'c':
					sprintf(buffer, "Continuously reporting timer input period in microseconds");
					sendmsg(buffer);
					timer_cont_flag = 1; // set timer continuous reporting
					//sprintf(buffer, "val = %i", timer_cont_flag);
					break;
				case 'E': // Stop Continuous Timer Reporting
				case 'e':
					sprintf(buffer, "Continuous timer input reporting stopped.");
					sendmsg(buffer);
					timer_cont_flag = 0; // stop timer continuous reporting
					//sprintf(buffer, "val = %i", timer_cont_flag);
					break;
				case 'A': // ADC0
				case 'a':
					sprintf(buffer, "ADC Value: %u", adc_reading);
					sendmsg(buffer);
					break;
				case 'V': // ADC0 in mV
				case 'v':
					adc_reading_mv = ((adc_reading*5000)/1024); // mV calculation
					sprintf(buffer, "ADC Value: %u mV", adc_reading_mv);
					sendmsg(buffer);
					break;
				case 'M': // ADC0 Continuous Reporting
				case 'm':
					sprintf(buffer, "Continuously reporting ADC0 conversion result in mV");
					sendmsg(buffer);
					adc_cont_flag = 1; // set ADC0 continuous reporting
					break;
				case 'N': // Stop ADC0 Continuous Reporting
				case 'n':
					sprintf(buffer, "Stop continuous reporting of ADC0 input");
					sendmsg(buffer);
					adc_cont_flag = 0; // stop ADC0 continuous reporting
					break;
				case 'W': // Toggle PORTD4
				case 'w':
					sprintf(buffer, "Toggle the LED bit 4 at 125ms");
					b4_toggle_flag = 1; // enable PORTD4 toggling
					sendmsg(buffer);
					break;
				case 'U': //  Stop Toggle PORTD4
				case 'u':
					sprintf(buffer, "Stop toggling LED bit 4");
					b4_toggle_flag = 0; // disable PORTD4 toggling
					sendmsg(buffer);
					break;
				case 'P': // PORTD Status
				case 'p':
					sprintf(buffer, "PORTD Status: %X", PIND);
					sendmsg(buffer);
					break;
				case 'S': // OCR2B Status
				case 's':
					sprintf(buffer, "OCR2B Status: %d", OCR2B);
					sendmsg(buffer);
					break;
				default:
					sprintf(buffer, "Input not recognized.");
					sendmsg(buffer);
					break;
			} 
		 } 
	}
	  if (timer_cont_flag == 1) // check if continuous reporting enabled
	  {
		  if (qcntr == sndcntr) // check data can be sent
		  {
		  val = Time_Period_High + Time_Period_Low;
		  sprintf(buffer, "Value: %u", val);
		  sendmsg(buffer);
		  capture_flag = 0; // reset flag
		  }
	  }
	  else if (adc_flag == 1) // check new adc data available
	  {
		  if (adc_cont_flag == 1) // check if continuous reporting enabled
		  {
			  if (qcntr == sndcntr) // check data can be sent
			  {
			  adc_reading_mv = ((adc_reading*5000)/1024); // mV calculation
			  sprintf(buffer, "ADC Value: %u mV", adc_reading_mv);
			  sendmsg(buffer);
			  adc_flag = 0; // reset flag
			  }
		  }
	  }
   }
   return 1; 
} 

/***********************************************
* USART character send complete ISR
************************************************/
ISR(USART_TX_vect)
{
   /*send next character and increment index*/
   if (qcntr != sndcntr)  
      UDR0 = queue[sndcntr++]; 
}

/***********************************************
* Timer/Counter0 overflow ISR
************************************************/
ISR(TIMER0_OVF_vect)
{
	TCNT0 = tcnt0_start;
	++timecount0;
	
	if (b4_toggle_flag == 1)
	{
		if (timecount0 >= time_overflow)
		{
			PORTD ^= (1<<PORTD4);
			timecount0 = 0;
		}
	} else {
		PORTD &= ~(1<<PORTD4);
	}
}

/***********************************************
* Timer/Counter1 overflow ISR
************************************************/
ISR(TIMER1_OVF_vect)
{
	++timecount1;
}

/***********************************************
* Timer/Counter1 capture ISR
************************************************/
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
	TCCR1B = TCCR1B ^ (1<<ICES1);
	
	start_edge = end_edge;
	timecount1 = 0;
	
	if((Time_Period_High+Time_Period_Low)>HUNDRED_MICROSECONDS)
	{
		PORTD |= (1<<PORTD6);
	} else {
		PORTD &= ~(0<<PORTD6);
	}
}

/***********************************************
* ADC ISR
************************************************/
ISR(ADC_vect)
{
	adc_reading = ADC;
	adc_flag = 1;
	
	if((adc_reading) > FOUR_VOLTS)
	{
		PORTD |= (1<<PORTD7);
	} else {
		PORTD &= ~(1<<PORTD7);
	}
}

