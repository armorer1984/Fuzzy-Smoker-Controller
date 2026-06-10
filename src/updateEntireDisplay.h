#include <LiquidCrystal_I2C.h>

// I2C SDA:GPIO21, SCL:GPIO22
#define COLUMS           20   //LCD columns
#define ROWS             4    //LCD rows
LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);

void updateEntireDisplay(int setTemp, int currentReadTemp, int controlMode, int currentFanPercentMinMax){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Temp: ");
  lcd.print(setTemp,0);
  lcd.print(" ");
  lcd.setCursor(0,1);
  lcd.print("Actual Temp: ");
  lcd.print(currentReadTemp);
  lcd.print(" ");
  lcd.setCursor(0,2);
  lcd.print("Control: ");
  if (controlMode==0){
    lcd.print("AUTO   ");
  } else{
    lcd.print("MANUAL");
  }
  lcd.setCursor(0,3);
  lcd.print("Fan: ");
  lcd.print(currentFanPercentMinMax);
  lcd.print("%   ");
}