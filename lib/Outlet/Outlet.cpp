#include "Outlet.h"


Outlet::Outlet(int relaisPin)
{
  /* set pin for relais output */
  this->relaisPin = relaisPin;

  /* configure pin */
  pinMode(relaisPin, OUTPUT);
  this->setOff();

}

/*----------------------------------------------------------------------------*/

void Outlet::setState(String state)
{
  if(state == "OFF")
  {
    this->setOff();
  }
  else if(state == "ON")
  {
    this->setOn();
  }
}

/*----------------------------------------------------------------------------*/

void Outlet::setOn()
{
  digitalWrite(relaisPin, RELAIS_CLOSE);
  this->state = outletState_on;
}

/*----------------------------------------------------------------------------*/

void Outlet::setOff()
{
  digitalWrite(relaisPin, RELAIS_OPEN);
  this->state = outletState_off;
}

/*----------------------------------------------------------------------------*/

int Outlet::getState()
{
  return (int) this->state;
}

/*----------------------------------------------------------------------------*/

String Outlet::getStateStr()
{
  if(this->state == outletState_off)
  {
    return "OFF";
  }
  else if (this->state == outletState_on)
  {
    return "ON";
  }

  return "ERROR";
}
