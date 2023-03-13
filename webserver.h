enum MethodType
{
  MethodUnknown,
  MethodGet,
  MethodPost,
  MethodHead
};

struct TimeStamp
{
    unsigned int yy;
    byte mm;
    byte dd;
    byte hh;
    byte min;
    byte ss;
};

const byte DAYS_IN_MONTH[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define STRING_BUFFER_SIZE 256

typedef char BUFFER[STRING_BUFFER_SIZE];
