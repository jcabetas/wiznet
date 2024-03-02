#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include "string.h"
#include "bloques.h"
#include "nextion.h"

class person {
 public:
  uint16_t valor;
  person(uint16_t valIni)
  {
      valor = valIni;
  }
};

void reportHeap(BaseSequentialStream *tty)
{
    size_t numFrag, totFragFreeSpace,largFreeBlock;
    numFrag = chHeapStatus(NULL, &totFragFreeSpace,&largFreeBlock);
    chprintf(tty,"chCoreGetStatus:%X  numFrag:%d totFragFree:%d largFre:%d\r\n",chCoreGetStatusX(),numFrag, totFragFreeSpace,largFreeBlock);
}


void testNew(BaseSequentialStream *tty)
{
    person *person1, *person2,*person3;

    reportHeap(tty);
    person1 = new person(55);
    person2 = new person(67);
    person3 = new person(67);
    reportHeap(tty);
    delete(person1);
    delete(person2);
    delete(person3);
    reportHeap(tty);
}

