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
typedef uint8_t Payload[121];

class UdpHeader{
  private:
    SourcePort source;
    DestPort dest;
    HeaderLength len;
    Checksum checksum;
    Payload payload;

  public:
    UdpHeader();
    void setSourcePort(SourcePort s);
    void setDestPort(DestPort d);
    void setHeaderLength(HeaderLength h);
    void setChecksum(Checksum c);
    void setPayload(Payload p);
    SourcePort getSourcePort();
    DestPort getDestPort();
    HeaderLength getHeaderLength();
    Checksum getChecksum();
    Payload* getPayload();

};


UdpHeader::UdpHeader(){
  for(int i=0;i<121;i++){
    payload[i] = 0;
  }
}

////////////////////////////////////////////////////
// UdpHeader Setters
////////////////////////////////////////////////////

void UdpHeader::setSourcePort(SourcePort s){
  source = s;
}

void UdpHeader::setDestPort(DestPort d){
  dest = d;
}

void UdpHeader::setHeaderLength(HeaderLength h){
  len = h;
}

void UdpHeader::setChecksum(Checksum c){
  checksum = c;
}

void UdpHeader::setPayload(Payload p){
  for(int i=0;i<121;i++){
    payload[i] = p[i];
  }
}

////////////////////////////////////////////////////
// UdpHeader Getters
////////////////////////////////////////////////////

SourcePort UdpHeader::getSourcePort(){
  return source;
}

DestPort UdpHeader::getDestPort(){
  return dest;
}

HeaderLength UdpHeader::getHeaderLength(){
  return len;
}

Checksum UdpHeader::getChecksum(){
  return checksum;
}

Payload* UdpHeader::getPayload(){
  return &payload;
}
