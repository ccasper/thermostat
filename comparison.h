#ifndef COMPARISON_H_
#define COMPARISON_H_

// The max/min for Arduino do not work correctly and therefore we define these as
// replacements.
//
// It's a really weird bug, it seems that if "max" or "min" are defined, simply including
// <stdint.h> will make them disappear See
// https://forum.arduino.cc/index.php?topic=330924.0
#define cmax(a, b)          \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })
#define cmin(a, b)          \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
  })

#endif  // COMPARISON_H_
