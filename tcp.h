#include <stdint.h>
#include <cstring>
#include <iostream>

using namespace std;

#define MAX_PAY_SIZE 1024
#define HEAD_SIZE 8

class TCPHeader{
private:
  char* payload;
  int paySize;
public:
  uint16_t SeqNum;
  uint16_t AckNum;
  uint16_t Window;
  bool A;
  bool S;
  bool F;

  char* getPayload() { return payload;}
  void setPayload(char* payload, int size){
    //delete(this->payload);
    this->payload = payload;
    paySize = size;
  }

  TCPHeader(){
    SeqNum = 0;
    AckNum = 0;
    Window = 0;
    A = 0;
    S = 0;
    F = 0;
    paySize = 0;
    // payload = new char[MAX_PAY_SIZE];
    // memset(payload,'0', MAX_PAY_SIZE);
  };
  TCPHeader(uint16_t SeqNum, uint16_t AckNum, uint16_t Window,
              bool A, bool S, bool F){
    this->SeqNum = SeqNum;
    this->AckNum = AckNum;
    this->Window = Window;
    this->A = A;
    this->S = S;
    this->F = F;
    paySize = 0;
    // payload = new char[MAX_PAY_SIZE];
    // memset(payload,'0', MAX_PAY_SIZE);
  }

  int getPacketSize(){
    return 8 + paySize;
  }

  // Encode into a byte array
  char* encode(){
    char* encoded = new char[HEAD_SIZE+paySize];
    //memset(encoded,0,HEAD_SIZE+MAX_PAY_SIZE);
    memcpy(encoded, &SeqNum, 2);
    memcpy(encoded+2, &AckNum, 2);
    memcpy(encoded+4, &Window, 2);
    uint16_t flagRep = F + 2*S + 4*A;
    memcpy(encoded+6, &flagRep, 2);
    memcpy(encoded+8, payload, paySize);

    // for(int x=0;x<1032;x++){
    //   cout << encoded[x];
    // }

    cout << endl << "-------------------------------" << endl;

    return encoded;
  }
  static TCPHeader decode(char* encoded, int paySize){
    TCPHeader toReturn;
    memcpy(&toReturn.SeqNum, encoded, 2);
    // cout << "SeqNum: " << toReturn.SeqNum << endl;
    memcpy(&toReturn.AckNum, encoded+2, 2);
    // cout << "AckNum: " << toReturn.AckNum << endl;
    memcpy(&toReturn.Window, encoded+4, 2);
    // cout << "Window: " << toReturn.Window << endl;
    uint16_t flagRep;
    memcpy(&flagRep, encoded+6, 2);
    // cout << "WinflagRep: " << flagRep << endl;

    //There's probably a more clever way of doing this, but I don't
    //want to deal with figuring it out right now
    switch (flagRep) {
      case 7:
        toReturn.A = 1;
        toReturn.S = 1;
        toReturn.F = 1;
        break;
      case 6:
        toReturn.A = 1;
        toReturn.S = 1;
        toReturn.F = 0;
        break;
      case 5:
        toReturn.A = 1;
        toReturn.S = 0;
        toReturn.F = 1;
        break;
      case 4:
        toReturn.A = 1;
        toReturn.S = 0;
        toReturn.F = 0;
        break;
      case 3:
        toReturn.A = 0;
        toReturn.S = 1;
        toReturn.F = 1;
        break;
      case 2:
        toReturn.A = 0;
        toReturn.S = 1;
        toReturn.F = 0;
        break;
      case 1:
        toReturn.A = 0;
        toReturn.S = 0;
        toReturn.F = 1;
        break;
      case 0:
      default:
        toReturn.A = 0;
        toReturn.S = 0;
        toReturn.F = 0;
        break;
    }
    // cout << "toReturn.A: " << toReturn.A << endl;
    // cout << "toReturn.S: " << toReturn.S << endl;
    // cout << "toReturn.F: " << toReturn.F << endl;
    char* newPay = new char[paySize];
    memcpy(newPay, encoded+8, paySize);

    toReturn.setPayload(newPay, paySize);

    return toReturn;
  }
};
