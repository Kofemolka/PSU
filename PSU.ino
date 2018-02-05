#include <LiquidCrystal_I2C.h>

#define ctrlPin 2
#define ctrlBtnPin 3
#define pwrOkPin 13
#define ledPin 11
#define VT_PIN A6
#define AT_PIN A7
#define v3pin A0
#define v5pin A1
#define v12pin A2

const float vstb = 5.05;
const float R2 = 2200;
const float v5R1 = 212;
const float v12R1 = 4690;
const float vUR1 = 4750;

enum ECtrlButtonState{
	None,
	Short,
	Long
};

struct sens_values_t {
  unsigned int count;
  unsigned int icount;
  
  float v3;
  float v5;
  float v12;
  float vU;
  float iU;

  float v3_raw;
  float v5_raw;
  float v12_raw;
  float vU_raw;
  float iU_raw;
};

sens_values_t sens_values;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

void setup() { 
	pinMode(ctrlPin, OUTPUT);
	pinMode(ctrlBtnPin, INPUT);
	pinMode(pwrOkPin, INPUT);

	pinMode(VT_PIN, INPUT);
	pinMode(AT_PIN, INPUT);
	pinMode(v3pin, INPUT);
	pinMode(v5pin, INPUT);
	pinMode(v12pin, INPUT);

	pinMode(ledPin, OUTPUT);	

	Serial.begin(9600);
	digitalWrite(ctrlPin, 0);

	lcd.init();                      
	lcd.backlight();	
	lcd.clear();

  // initialize timer1 
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 625;            // compare match register 16MHz/256/100Hz
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();             // enable all interrupts

  sens_values.count = 0;
  sens_values.icount = 0;
  
	standBy();
}


ISR(TIMER1_COMPA_vect)         
{
  sens_values.v3_raw += analogRead(v3pin);
  sens_values.v5_raw += analogRead(v5pin);
  sens_values.v12_raw += analogRead(v12pin);  
  sens_values.iU_raw += analogRead(AT_PIN);
  sens_values.vU_raw += analogRead(VT_PIN);

  static const int maxReadings = 9; // 10/sec
  static const int maxIReadings = 49; // 2/sec
  
  if(++sens_values.count >= maxReadings)
  {
     sens_values.count = 0;     

     sens_values.v3 = (sens_values.v3_raw * vstb / 1024) / maxReadings;
     
     sens_values.v5 = (sens_values.v5_raw * vstb / 1024) / maxReadings;
     sens_values.v5 = sens_values.v5 / (R2 / (R2 + v5R1)); 

     sens_values.v12 = (sens_values.v12_raw * vstb / 1024) / maxReadings;
     sens_values.v12 = sens_values.v12 / (R2 / (R2 + v12R1)); 

     sens_values.vU = (sens_values.vU_raw * vstb / 1024) / maxReadings;
     sens_values.vU = sens_values.vU / (R2 / (R2 + vUR1));     
      
     sens_values.v3_raw = 0.0;
     sens_values.v5_raw = 0.0;
     sens_values.v12_raw = 0.0;
     sens_values.vU_raw = 0.0;     
  }

  if(++sens_values.icount >= maxIReadings)
  {
     sens_values.icount = 0;     

     sens_values.iU = (0.0264 * sens_values.iU_raw) / maxIReadings;
     sens_values.iU -= 13.51;

     sens_values.iU_raw = 0.0;
  }
}

int checkPwrState()
{
	  return digitalRead(pwrOkPin);	
}

int testCtrlButton()
{
    static bool longPressDetected = false;
  	static int ctrlBtnState = 0;
  	static long press = millis();     
  
  	int v = digitalRead(ctrlBtnPin);
  	if (v != ctrlBtnState)
  	{
    		ctrlBtnState = v;
    
    		if (v == HIGH)
    		{
    			  press = millis();            
    		}
    		else
    		{
            if(longPressDetected)
            {
               longPressDetected = false;
               return None;
            }
            
            return Short;      			
    		}
  	}
    else if(v == HIGH)
    {
        if((millis() - press) > 1000)
        {
           longPressDetected = true;
           return Long;
        }
    }

	  return None;	
}

void printValue(int col, int row, int width, float value)
{
   lcd.setCursor(col, row);
   if(value < 0.02)
   {
      for(int i=0;i<width;i++)
      {
          lcd.print("-");
      }
   }
   else
   {
      lcd.print(value, 2);
      
      if(value<10)
      {
          lcd.print(" ");
      }
   }
}

void userScene()
{	
  	float watts = sens_values.iU * sens_values.vU;
  	const float maxWatts = 25.0;
  
  	lcd.setCursor(0, 0);
  	lcd.print("V:");
  	printValue(2, 0, 5, sens_values.vU);
  
  	lcd.setCursor(8, 0);
  	lcd.print("I:");
  	printValue(10, 0, 5, sens_values.iU);
  
  	lcd.setCursor(0, 1);
  	lcd.print("P:");
  	printValue(2, 1, 5, watts);
  
  	lcd.setCursor(8, 1);
  	lcd.print("M:");
  	printValue(10, 1, 5, maxWatts);
}

void allVoltsScene()
{  
  	lcd.setCursor(0, 0);
  	lcd.print("3:");
    printValue(2, 0, 5, sens_values.v3);
  	
  	lcd.setCursor(8, 0);
  	lcd.print("12:");
  	printValue(11, 0, 5, sens_values.v12);
  	
  	lcd.setCursor(0, 1);
  	lcd.print("5:");
  	printValue(2, 1, 5, sens_values.v5);
  
  	lcd.setCursor(8, 1);
  	lcd.print("Uv:");
  	printValue(11, 1, 5, sens_values.vU);
}

enum EPSUState{
  	Standby,
  	Powering,
  	Volts,
  	User
};

static EPSUState state = Standby;

void standBy()
{
  	state = Standby;
  	digitalWrite(ctrlPin, LOW);
  	lcd.clear();
  	lcd.setCursor(0, 0);
  	lcd.print("Standby");

   delay(1000);
}

void loop()
{		
  static const int refreshRate = 10;
	static long powerOnstart = 0;
  static int refreshCounter = 1;
		
	int btnState = testCtrlButton();	

  refreshCounter++;
  
	switch (state)
	{
	case Standby:    
		if (btnState == Short || checkPwrState())
		{
			state = Powering;
			powerOnstart = millis();
			digitalWrite(ctrlPin, HIGH);
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Power On");
			delay(500);
		}
		break;

	case Powering:
		if (checkPwrState())
		{
			state = Volts;
			lcd.clear();
		}
		else
		{
			if (millis() - powerOnstart > 5000)
			{
				standBy();
			}
		}
		break;

	case Volts:
		if (!checkPwrState() || btnState == Long)
		{
			standBy();
			break;
		}

		if (btnState == Short)
		{
			state = User;
			lcd.clear();
			break;
		}

    if(refreshCounter >= refreshRate)   
		{
		    allVoltsScene();		        
		}
		
		break;

	case User:
		if (!checkPwrState() || btnState == Long)
		{
			standBy();
			break;
		}

		if (btnState == Short)
		{
			state = Volts;
			lcd.clear();
			break;
		}

    if(refreshCounter >= refreshRate)   
    {
		    userScene();		
    }
		break;
	}

  if(refreshCounter >= refreshRate)   
  {
     refreshCounter = 1;
  }
  
	delay(10);
}

