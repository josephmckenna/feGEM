
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "ModbusTcp.h"

int main(int argc, char * argv[])
{
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  signal(SIGPIPE, SIG_IGN);

  ModbusTcp drv;

  drv.fTrace = true;
  drv.Connect("algas:502");

  int slave = 1;

  if (0)
    {
      drv.fTrace = true;
      drv.Function4(slave, 5040-1, 1);

      char buf[10000];
      int rd = drv.Read(buf, 10000);
      printf("Read %d: payload: 0x", rd);

      if (1)
        {
          for (int i=8; i<rd; i++)
            printf(" %02x", buf[i]&0xFF);
          printf("\n");
        }

      exit(1);
    }

  int firstReg = strtoul(argv[1], NULL, 0);
  int numReg = strtoul(argv[2], NULL, 0);

  if (firstReg<0)
    {
      firstReg = -firstReg;

      bool x = drv.fTrace;
      drv.fTrace = true;
      
      drv.Function6(slave, firstReg-1, numReg);

      char buf[10000];
      int rd = drv.Read(buf, 10000);
      printf("Read %d: payload: 0x", rd);

      if (1)
        {
          for (int i=8; i<rd; i++)
            printf(" %02x", buf[i]&0xFF);
          printf("\n");
        }

      drv.fTrace = x;

      exit(1);
    }

  while (1)
    {
      drv.Function3(slave, firstReg-1, numReg);

      char buf[10000];
      int rd = drv.Read(buf, 10000);
      printf("Read %d: payload: 0x", rd);

      if (1)
        {
          for (int i=8; i<rd; i++)
            printf(" %02x", buf[i]&0xFF);
          printf("\n");
        }

      sleep(1);
    }

  return 0;
}

// end
