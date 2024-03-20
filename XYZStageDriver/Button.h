#ifndef _BUTTON_H_
#define _BUTTON_H_


class Button {
  
 const byte buttonPin;
  
  public:
  bool is_pressed(){
    return digitalRead(buttonPin) != LOW;
  };

}

#endif