#ifndef OUTLET_H
#define OUTLET_H

#include <Arduino.h>

#include "Outlet_Types.h"

class Outlet
{
private:
  int relaisPin;
  outletStateType state;


public:
  Outlet(int relaisPin);

  void setState(String state);

  void setOn();
  void setOff();

  int getState();
  String getStateStr();


};

#endif /* OUTLET_H */
