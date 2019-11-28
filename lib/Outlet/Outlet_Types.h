#ifndef OUTLET_TYPES_H
#define OUTLET_TYPES_H

#define RELAIS_CLOSE    HIGH
#define RELAIS_OPEN     LOW

typedef enum outletStateTag {
  outletState_off   = 0,
  outletState_on    = 1,
  outletState_err   = 255,
} outletStateType;


#endif /* OUTLET_TYPES_H */
