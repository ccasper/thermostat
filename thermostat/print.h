#ifndef PRINT_H_
#define PRINT_H_

namespace thermostat {

class Print {
  public:
    virtual void write(uint8_t) = 0;

    // 1 byte values.
    void print(const char ch) {
      write(ch);
    };

    void print(const unsigned char ch) {
      write(ch);
    };

    // 2 byte values.
    void print(const int value) {
      print(static_cast<long>(value));
    }

    void print(unsigned int value) {
      print(static_cast<unsigned long>(value));
    };

    // 4 byte values.
    void print(const long value);

    void print(unsigned long);

    void print(double);

    // Variable size requires '\0' terminator.
    void print(const char*);
};

void Print::print(long value) {
  if (value < 0) {
    write('-');
    print(static_cast<unsigned long>(value * -1));
    return;
  }
  print(static_cast<unsigned long>(value));
}

void Print::print(const char* chars) {
  while (*chars) {
    write(*chars++);
  }
}

void Print::print(double value) {
  // Negative values.
  if (value < 0.0) {
    print('-');
    value = value * -1;
  }

  // Round nearest with 2 digits of precision.
  value += 0.005;

  unsigned long whole = (unsigned long)value;

  // Print two digits of the remainder.
  unsigned long remainder = (value - (double)whole) * 100.0;

  print(whole);
  write('.');
  print(remainder);
}

void Print::print(unsigned long value) {
  // A 4 byte unsigned value can be a max length of 10 base 10 digits.
  unsigned char buf[10];

  if (value == 0) {
    write('0');
    return;
  }

  // Divide and conquer with bytes added in reverse.
  int i = 0;
  while (value > 0) {
    buf[i++] = '0' + (value % 10);
    value /= 10;
  }

  // Print the characters in reverse.
  for (int j = i - 1; j >= 0; j--) {
    write(buf[j]);
  }
}
} // namespace thermostat

#endif // PRINT_H_
