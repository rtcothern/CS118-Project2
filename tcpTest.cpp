#include "tcp.h"
#include <iostream>

using namespace std;

int main(int argc, char **argv){
  TCPHeader synHeader(0, 0, 1024, false, true, false);
  // char* testPay = new char[1024];
  // for (int i = 0; i < 1024; i++){
  //   testPay[i] = i;
  // }
  // synHeader.setPayload(testPay);
  cout << "A: " + synHeader.A << ", S: "
    << synHeader.S << ", F: " << synHeader.F << endl;
  char* encoded = synHeader.encode();
  TCPHeader decodedSynHeader = TCPHeader::decode(encoded);
  cout << "\nASD here" << endl;
  cout << "A: " + decodedSynHeader.A << ", S: "
    << decodedSynHeader.S << ", F: " << decodedSynHeader.F << endl;
}
