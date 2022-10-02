//-----------------------------------------------------------------------------
//
//  Firmware for AC light dimmer - KRIDA Electronics Dimmer Board
//  Controlled via UART (RX TX pins on board)
//  Autodetect AC Line frequency
//  255 dimming levels. 0 - off, 255 fully on
//  Default UART speed 9600bps. Can be changed!
//  THis version is for Pro Micro (Like the Spark Fun or equivilent versions)
//  which uses ATMEGA32U4 microprocessor
//  
//  DIMMER VCC  -> ARDUINO UNO PIN 2 
//  DIMMER SYNC -> ARDUINO UNO PIN 3 
//  DIMMER GATE -> ARDUINO UNO PIN 4 
//  DIMMER GATE -> ARDUINO UNO PIN 5
//  ARDUINO RX -> TX MASTER DEVICE
//  ARDUINO TX -> RX MASTER DEVICE
// 
//  Commands from either the USB serial port or the async serial port:
//
//  snnn                      Set dim level manually
//  o                         Turn light fully on
//  f                         Turn light fully off
//  thh:mm:ss                 Set current time
//  ahh:mm:ss                 Set alarm time and turn alarm on
//  a                         Turn off alarm if it is on, or turn on if off
//  q                         Query current time and alarm time 
//  w                         Set wake up time in secs if default not desired.
//  d                         Force alarm going off. 
//  
// 
//-----------------------------------------------------------------------------


#define MAX_CMDLINE             40 // Maximum size of command line.
#define DEFAULT_WAKEUP_TIME   (30 * 60)  // 30 minutes - Default time to brighten a light in seconds.
#define DEFAULT_STARTING_WAKEUP_BRIGHTNESS 16

#if defined (__AVR_ATmega32U4__)   // Arduino micro is an atmega32u4 processor. and 
#include "jo_atmega32u4_regs.h"    // these regs are not defined. so define them! 
#endif


// These variables are used in the dimming functions
unsigned char count;
unsigned char GATE;
unsigned char frequency;
unsigned char STATUS;
unsigned char STATE;
unsigned char time_delay;
unsigned char F;

// Other global variables...
unsigned short wakeup_seconds = DEFAULT_WAKEUP_TIME;    // Nbr of secs to do the dimming wake-up.
unsigned short max_wakeup_brightness = 245;
char time_set = false;
char alarm_set = false;
unsigned long wakeup_count;
unsigned long wakeup_count_counter;

unsigned int TIMER_1_DELAY, TIMER_1_BUF_DELAY;
unsigned int TIMER_1_IMPULSE, TIMER_1_BUF_IMPULSE;
unsigned int VARIABLE;

typedef struct {
    index = 0;
    buffer[ MAX_CMDLINE+1];
} cmdBuffer;

cmdBuffer serialCmdLine;
cmdBuffer serial1CmdLine;

char process_time = 0;
char alarm_triggered = 0;
int current_dimmer_setting = 0;


typedef struct t_time {
    char  hours;
    char  mins;
    char  secs;
    long  days;
} TIME;

TIME curtime;
TIME alarm;


//-----------------------------------------------------------------------------
// Setup() -- Set things up, like timers and the zero crossing interrupt...
//-----------------------------------------------------------------------------
void setup() {

    pinMode(2, INPUT);     // On/off button
    pinMode(4, OUTPUT);    // Dimmer

    digitalWrite(4, LOW);  // Dimmer.  Start being off.

    TCCR2A = 0x00;
    TCCR2B = 0x07;         // Prescaller 1024
    TCNT2  = 0x15;         // Timer2 interrupt 15.04 ms
    OCR2A  = 0x00;
    OCR2B  = 0x00;  

    TCCR1A = 0x00;
    TCCR1B = 0x00;
    TCNT1H = 0x00;
    TCNT1L = 0x00;
    ICR1H  = 0x00;
    ICR1L  = 0x00;
    OCR1AH = 0x00;
    OCR1AL = 0x00;
    OCR1BH = 0x00;
    OCR1BL = 0x00;

    TIMSK1 = 0x01;
    TIMSK2 = 0x01;

    attachInterrupt(1, zero_cross_int, RISING);

    // Wait a few seconds for something plugged into the USB port...
    for (int i = 0; i < 5 && !Serial; i++) 
    {
        ; // wait for serial port to connect. Needed for native USB
    }
   
    // Serial through the USB port...
    Serial.begin(9600); // UART SPEED
    Serial.println(">");

    // Serial through Tx/Rx pins
    Serial1.begin(9600); // UART SPEED
    Serial1.println(">");

}


//-----------------------------------------------------------------------------
// zero_cross_int() -- Our interrupt routine for zero crossings...
//-----------------------------------------------------------------------------
void zero_cross_int()  {
 
    if (STATUS && STATE)
    {
        TIMER_1_DELAY = TIMER_1_BUF_DELAY; 
        
        TCNT1H = TIMER_1_DELAY >> 8;
        TCNT1L = TIMER_1_DELAY & 0xff;
        
        TCCR1B = 0x02;
    }
    
    frequency = TCNT2;
    
    TCNT2 = 0x00; 
    
    if (frequency > 147 && frequency < 163) 
    {
        F=100; 
        STATUS=1;
    }  

    if (frequency > 122 && frequency < 137) 
    {
        F=83;  
        STATUS=1;
    }
  
    process_time += 1;

    if( alarm_triggered )
    {
        if( current_dimmer_setting >= max_wakeup_brightness ) 
        {
            alarm_triggered = false;
        } 
        else 
        {
            if(--wakeup_count_counter == 0)
            {
                wakeup_count_counter = wakeup_count;  // Reset the countdown counter.
                setDimLevel( current_dimmer_setting + 1 );
            }
        }
    }
}



//-----------------------------------------------------------------------------
// Timer 1 interrupt routine...
//-----------------------------------------------------------------------------
ISR(TIMER1_OVF_vect) {

    TIMER_1_IMPULSE = TIMER_1_BUF_IMPULSE; 

    TCNT1H=TIMER_1_IMPULSE >> 8;
    TCNT1L=TIMER_1_IMPULSE & 0xff;

    digitalWrite(4, HIGH);

    time_delay++;

    if (time_delay == 2)
    {
        digitalWrite(4, LOW);
        time_delay = 0;
        TCCR1B = 0x00;    
    }

}



//-----------------------------------------------------------------------------
// Timer 2 interrupt routine...
//-----------------------------------------------------------------------------
ISR(TIMER2_OVF_vect) {

    TCNT0 = 0x15;  // every 15.04 ms
    
    digitalWrite(4, LOW);  
    
    STATUS = 0;
    
    TCCR1B = 0x00;
    
    count++;
    
    if(count == 133) 
    { 
        count=0;
        Serial.println("AC LINE IS NOT DETECTED. PLEASE CHECK WIRING. ");
    }      
}



//-----------------------------------------------------------------------------
// Read input from a serial port and build command line buffer. Return TRUE if
// we have a full command to process (received a CRLF - we'll ignore CRs)...
//-----------------------------------------------------------------------------
bool read_serial_input(serial serialport, cmdLine serialCmdLine)
{
    char c;

    c = serialport->read();

    //Serial.println((uint8_t)c, HEX);

    if (c == '\r')                // LF?
    {
        // null terminate command line. 
        serialCmdLine->buffer[serialCmdLine->index] = 0;  
        serialCmdLine->index = 0;   // reset command line index.
        return true;

    } 
    else if (c == '\n') 
    {
        // Ignore CRs...

    } 
    else               // Anything else, add to command line if not full...
    {
        if (serialCmdLine->index < MAX_CMDLINE - 1) 
        {
            serialCmdLine->buffer[serialCmdLine->index++] = c;  
        }
    }

    return false;
}



//-----------------------------------------------------------------------------
// setDimLevel() -- Given int between 1-255, set how dim you want the light...
//-----------------------------------------------------------------------------
void setDimLevel(int value)
{
    if (STATUS==1)
    {
        if (value!=0)
        {
            Serial.print("Setting level to ");
            Serial.println(value, DEC);
        }
    }
    
    if (STATUS==0)
    {
        Serial.println("Error AC line is not detected");
    }

    if (value > 245) 
    {
        value = 245;
    }
    if (value < 5) 
    {
        value = 5;
    }

    current_dimmer_setting = value;
    
    value = 256 - value;  // Inverse the level value.

    if (value == 251)      // If originally was <= 5
    { 
        STATE = 0;  
        TCCR1B = 0x00;
        digitalWrite(4, LOW);  
        Serial.println("Dimmer is now off");
    }
    else
    {
        STATE = 1;
             
        VARIABLE = ((((value * F)/256)*100)-1);  

        VARIABLE = VARIABLE * 2;
        
        TIMER_1_BUF_DELAY = 65535 - VARIABLE;  
        TIMER_1_BUF_IMPULSE = 65535 - (( F * 100)-( VARIABLE / 2));  
    }
}



//-----------------------------------------------------------------------------
// Convert an ascii decimal number to an integer value...
//-----------------------------------------------------------------------------
int next_int(char **p)
{
    int value = 0;

    while( **p >= '0' && **p <= '9')
    {
        value *= 10;
        value += **p - '0';
        (*p)++;
    }

    return value;
}



#define GOBBLE_SPACES(p)  { while( *p == ' ')  p++; }

//-----------------------------------------------------------------------------
// Parse time and put into a time structure (clear days for now)...
//-----------------------------------------------------------------------------
bool parse_time(char *cmd, TIME *ts)
{
    char hours;
    char mins;
    char secs;

    GOBBLE_SPACES(cmd);

    hours = next_int(&cmd);
    if(hours < 0 || hours > 23 || *cmd != ':')
        return false;
    cmd++;                          // Bump over the ':'
    
    mins = next_int(&cmd); 
    if(mins < 0 || mins >=60 || *cmd != ':')
        return false;
    cmd++;                          // Bump over the ':'
    
    secs = next_int(&cmd);   
    if( secs < 0 || secs >= 60)
        return false;
    cmd++;                          // Bump over the ':'
    
    ts->hours = hours;
    ts->mins  = mins;
    ts->secs  = secs;
    ts->days = 0;

    return true;
}



//-----------------------------------------------------------------------------
// print_status() -- Print out current time and alarm settings...
//-----------------------------------------------------------------------------
void print_status()
{
    char buf[25];

    
    Serial.println("");
    Serial.print("Current time: ");
    sprintf(buf, "%02d:%02d:%02d", curtime.hours, curtime.mins, curtime.secs);
    Serial.print(buf);
    Serial.print(" Alarm: ");
    sprintf(buf, "%02d:%02d:%02d", alarm.hours, alarm.mins, alarm.secs);
    Serial.print(buf);
    Serial.print(" Dimmer set at: ");
    Serial.print(current_dimmer_setting, DEC);
    Serial.println(" ");
}


//-----------------------------------------------------------------------------
// Process a command.  Pass ptr to a command buffer( probably the serial
// buffer)...
//-----------------------------------------------------------------------------
void process_command(char* cmd)
{
    int value;

        Serial.print("Processing: ");
        Serial.println(serialCmdLine);
        
    switch(*cmd++) 
    {
        case 's':                        // Set dim level manually...
            value = atoi(cmd);           // Parse decimal number s/b 1-255;
            setDimLevel(value);          // set. Will complain if not 1-255;
            break;

        case 'o':                        // Quick full on...
            setDimLevel(255);            // set. Will complain if not 1-255;
            alarm_triggered = false;     // Turn off alarm, if it was on.
            break;

        case 'f':                        // Quick full off...
            setDimLevel(0);              // set. Will complain if not 1-255;
            alarm_triggered = false;     // Turn off alarm, if it was on.
            break;

        case 't':                              // Set time...
            if( !parse_time(cmd, &curtime) ) { // Set current time into time structure;
                Serial.println("Invalid format");
                time_set = false;
            } else {
                time_set = true;    
            }    
            break;

        case 'a':                              // Set alarm...
            if( !parse_time(cmd, &alarm) )     // Set alarm time into time structure;
            {
                if( alarm_set )                // Time not given but alarm previously set?   
                {
                    alarm_set = false;         // So turn off alarm.  
                } 
                else                           // Else, time not given and alarm previous not set.
                {
                    alarm_set = false;         // Just turn alarm on asumming alarm time previously set.
                }
            }
            else
            {    
                alarm_set = true;              // Alarm is now set.
            }    
            break;

        case 'q':                        // Query current time and alarm time... 
            print_status();
            break;
            
        case 'w':                        // Set wake up time in secs if default not desired.
            wakeup_seconds = atoi(cmd);  // Parse decimal number;
            break;

        case 'd':                        // Force alarm going off. 
            trigger_alarm();
            break;
                
        default:
            Serial.println("Invalid command");
            break;
    }

}




//-----------------------------------------------------------------------------
// update_clock() -- Called once a second to update our clock variables...
//-----------------------------------------------------------------------------
void update_clock()
{
    if( ++curtime.secs == 60 ) 
    {
        curtime.secs = 0;
        if( ++curtime.mins == 60 )
        {
            curtime.mins = 0;
            if( ++curtime.hours == 24 )
            {
                curtime.hours = 0;
                ++curtime.days;
            }
        }
    }
}




//-----------------------------------------------------------------------------
// check_alarm_trigger() -- Called to check if it is time to trigger alarm...
//-----------------------------------------------------------------------------
bool check_alarm_trigger()
{
    // If current time is not set and alarm not set, then no reason to check alarm.
    if( !time_set || !alarm_set )
       return false;
      
    if( curtime.hours == alarm.hours &&
        curtime.mins  == alarm.mins  &&
        curtime.secs  == alarm.secs  )
    {
        return true;
    }

    return false;
}



//-----------------------------------------------------------------------------
// trigger_alarm() -- trigger the alarm, set up processing, which will be 
//                    driven out of zero cross interrupt routine...
//-----------------------------------------------------------------------------
void trigger_alarm() 
{
    Serial.println("Alarm triggered!");
    setDimLevel( DEFAULT_STARTING_WAKEUP_BRIGHTNESS );
    int temp = max_wakeup_brightness - DEFAULT_STARTING_WAKEUP_BRIGHTNESS;
    wakeup_count_counter = wakeup_count = ( long(wakeup_seconds) * 120 ) / (long) temp;
    Serial.print("wakeup_seconds=");
    Serial.print(wakeup_seconds);
    Serial.print(" spread=");
    Serial.print(temp);
    Serial.print(" wakeup_count=");
    Serial.println(wakeup_count);
    alarm_triggered = true;  
}


//-----------------------------------------------------------------------------
// Main processing loop...
//-----------------------------------------------------------------------------
void loop() 
{

    if( Serial.available() ) {             
        if( read_serial_input(&Serial, &serialCmdLine) )
            process_command(&Serial, &serialCmdLine);
    }

    if( Serial1.available() ) {             
        if( read_serial_input(&Serial1, &serial1CmdLine) )
            process_command(&Serial1, &serial1CmdLine);
    }

    if( process_time >= 120 ) {         // process_time incremented 120 times/second.
        update_clock();                 // Update our clock once a second.
        noInterrupts();
        process_time -= 120;
        interrupts();
    }

    if( !alarm_triggered )
    {
        if( check_alarm_trigger() ) 
        {
            trigger_alarm();
        }
    }

}
