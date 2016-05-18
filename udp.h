#include <string>
#include <thread>
#include <iostream>
#include <vector>
#include <map>
#include <sstream>

using namespace std;

typedef uint16_t SourcePort;
typedef uint16_t DestPort;
typedef uint16_t HeaderLength;
typedef uint16_t Checksum;

class UdpHeader{
  private:
    SourcePort source;
    DestPort dest;
    HeaderLength len;
    Checksum check;

  public:
    void setSourcePort(SourcePort s);
    void setDestPort(DestPort d);
    void setHeaderLength(HeaderLength h);
    void setChecksum(Checksum c);
    SourcePort getSourcePort();
    DestPort getDestPort();
    HeaderLength getHeaderLength();
    Checksum getChecksum();

};
