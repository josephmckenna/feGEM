#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <signal.h>
#include "lmk04800.h"

#define SPI_DEVICE "/dev/spidev1.0"

static void LMK04800_Write(unsigned short addr, unsigned char cmd);
static unsigned char LMK04800_Read(unsigned short addr);
static int GetSPIDevice(const char* device);
static void spi_transfer(int fd, uint8_t const *tx, size_t tx_len, uint8_t const *rx, size_t rx_len);

#define NUM_MSS_GPIO 32

static int fd_gpio[NUM_MSS_GPIO];
//static unsigned char off_value = 0;
//static unsigned char on_value = 1;

#define FP_LED1 fd_gpio[24]
#define FP_LED2 fd_gpio[25]
#define FP_LED3 fd_gpio[26]
#define FP_LED4 fd_gpio[27]
#define FP_LED5 fd_gpio[28]
#define FP_LED6 fd_gpio[29]
#define FP_LED7 fd_gpio[30]
#define FP_LED8 fd_gpio[31]

#define SEL_NIM fd_gpio[3]
#define SOURCE_SEL fd_gpio[4]
#define SEL_EXT fd_gpio[5]
#define CLK0_3_EN fd_gpio[6]
#define CLK4_7_EN fd_gpio[7]
#define CLK8_11_EN fd_gpio[8]
#define CLK12_15_EN fd_gpio[9]
#define CLK16_19_EN fd_gpio[10]
#define CLK20_23_EN fd_gpio[11]
#define LMK_SYNC fd_gpio[12]

void InitGPIO(void) {
    fd_gpio[1]  = open("/sys/class/gpio/gpio1/value",  O_WRONLY);
    fd_gpio[2]  = open("/sys/class/gpio/gpio2/value",  O_WRONLY);
    fd_gpio[3]  = open("/sys/class/gpio/gpio3/value",  O_WRONLY);
    fd_gpio[4]  = open("/sys/class/gpio/gpio4/value",  O_WRONLY);
    fd_gpio[5]  = open("/sys/class/gpio/gpio5/value",  O_WRONLY);
    fd_gpio[6]  = open("/sys/class/gpio/gpio6/value",  O_WRONLY);
    fd_gpio[7]  = open("/sys/class/gpio/gpio7/value",  O_WRONLY);
    fd_gpio[8]  = open("/sys/class/gpio/gpio8/value",  O_WRONLY);
    fd_gpio[9]  = open("/sys/class/gpio/gpio9/value",  O_WRONLY);
    fd_gpio[10] = open("/sys/class/gpio/gpio10/value", O_WRONLY);
    fd_gpio[11] = open("/sys/class/gpio/gpio11/value", O_WRONLY);
    fd_gpio[12] = open("/sys/class/gpio/gpio12/value", O_WRONLY);
    fd_gpio[13] = open("/sys/class/gpio/gpio13/value", O_RDONLY);
    fd_gpio[14] = open("/sys/class/gpio/gpio14/value", O_RDONLY);
    fd_gpio[15] = open("/sys/class/gpio/gpio15/value", O_RDONLY);
    fd_gpio[16] = open("/sys/class/gpio/gpio16/value", O_RDONLY);
    fd_gpio[17] = open("/sys/class/gpio/gpio17/value", O_RDONLY);
    fd_gpio[18] = open("/sys/class/gpio/gpio18/value", O_RDONLY);
    fd_gpio[19] = open("/sys/class/gpio/gpio19/value", O_RDONLY);
    fd_gpio[20] = open("/sys/class/gpio/gpio20/value", O_RDONLY);
    fd_gpio[21] = open("/sys/class/gpio/gpio21/value", O_RDONLY);
    fd_gpio[22] = open("/sys/class/gpio/gpio22/value", O_RDONLY);
    fd_gpio[23] = open("/sys/class/gpio/gpio23/value", O_RDONLY);
    fd_gpio[24] = open("/sys/class/gpio/gpio24/value", O_WRONLY);
    fd_gpio[25] = open("/sys/class/gpio/gpio25/value", O_WRONLY);
    fd_gpio[26] = open("/sys/class/gpio/gpio26/value", O_WRONLY);
    fd_gpio[27] = open("/sys/class/gpio/gpio27/value", O_WRONLY);
    fd_gpio[28] = open("/sys/class/gpio/gpio28/value", O_WRONLY);
    fd_gpio[29] = open("/sys/class/gpio/gpio29/value", O_WRONLY);
    fd_gpio[30] = open("/sys/class/gpio/gpio30/value", O_WRONLY);
    fd_gpio[31] = open("/sys/class/gpio/gpio31/value", O_WRONLY);
}

static void LMK04800_Write(unsigned short addr, unsigned char cmd) {
  unsigned char tx[3];

  int fd = GetSPIDevice(SPI_DEVICE);

  tx[0] = (addr >> 8) & 0xff;
  tx[1] = (addr >> 0) & 0xff;
  tx[2] = cmd;
  spi_transfer(fd, tx, 3, 0, 0); 
  // printf("LMK04800 Write: 0x%X,\t0x%X\n", addr, cmd);
}

static unsigned char LMK04800_Read(unsigned short addr) {
  unsigned char tx[2];
  unsigned char rx[1];

  int fd = GetSPIDevice(SPI_DEVICE);

  tx[0] = 0x80 | ((addr >> 8) & 0xff);
  tx[1] = (addr >> 0) & 0xff;
 
  spi_transfer(fd, tx, 2, rx, 1); 

  return rx[0];
}

static void spi_transfer(int fd, uint8_t const *tx, size_t tx_len, uint8_t const *rx, size_t rx_len) {
	struct spi_ioc_transfer	xfer[2];
	//unsigned char		*bp;
	int			status;

	memset(xfer, 0, sizeof xfer);

	xfer[0].tx_buf = (__u64) tx;
	xfer[0].len = tx_len;

	if(rx) {
	  xfer[1].rx_buf = (__u64) rx;
	  xfer[1].len = rx_len;
	  status = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
	} else {
	  status = ioctl(fd, SPI_IOC_MESSAGE(1), xfer);
	}
	if (status < 0) {
		perror("SPI_IOC_MESSAGE");
		return;
	}

#if 0
	if(rx) {
	  printf("response(%2d, %2d): ", rx_len, status);
	  for (const unsigned char* bp = rx; rx_len; rx_len--)
	    printf(" %02x", *bp++);
	  printf("\n");
	}
#endif
}

static int GetSPIDevice(const char* device) {
  static int fd;
  static int init;

  if(!init) {
    init = 1;
    fd = open(device, O_RDWR);
  }

  return fd;
}

#ifdef HAVE_MIDAS
#include "tmfe.h"
#include "tmvodb.h"
#endif

void usage()
{
  printf("Usage:\n");
  printf("\n");
  printf("  --clock0 --- select internal clock\n");
  printf("  --clock1 --- select eSATA clock\n");
  printf("  --clock2 --- select LEMO clock\n");
  printf("\n");
  printf("  --lemo-nim --- select LEMO level NIM\n");
  printf("  --lemo-ttl --- select LEMO level TTL\n");
  printf("\n");
  printf("  -h Hostname --- connect to MIDAS on given hostname\n");
}

int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);
  signal(SIGPIPE, SIG_IGN);

  printf("main!\n");

  int iclock = -1;
  int lemo_level = -1;
  int isync = -1;
  const char* midas_host = NULL;

  for (int i=0; i<argc; i++) {
    printf("argv[%d] = [%s]\n", i, argv[i]);
    if (strcmp(argv[i], "--clock0") == 0) {
      iclock = 0;
    } else if (strcmp(argv[i], "--clock1") == 0) {
      iclock = 1;
    } else if (strcmp(argv[i], "--clock2") == 0) {
      iclock = 2;
    } else if (strcmp(argv[i], "--lemo-ttl") == 0) {
      lemo_level = 0;
    } else if (strcmp(argv[i], "--lemo-nim") == 0) {
      lemo_level = 1;
    } else if (strcmp(argv[i], "-h") == 0) {
      midas_host = argv[i+1];
      i++;
    }
  }

#ifdef HAVE_MIDAS
  TMFE* mfe = NULL;
  TMFeEquipment* eq = NULL;
  TMVOdb* s = NULL; // Settings
  TMVOdb* v = NULL; // Variables

  if (midas_host) {
    printf("midas host: %s\n", midas_host);
    
    mfe = TMFE::Instance();

    TMFeError err = mfe->Connect("fecdm", midas_host);
    if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
    }

    //mfe->SetWatchdogSec(0);
    
    TMFeCommon *eqc = new TMFeCommon();
    eqc->EventID = 0;
    eqc->FrontendName = "fecdm";
    eqc->Buffer = "";
    eqc->LogHistory = 1;
    
    eq = new TMFeEquipment("CDM");
    eq->Init(eqc);
    eq->SetStatus("Starting...", "white");
    
    mfe->RegisterEquipment(eq);

    TMVOdb* odb = MakeOdb(mfe->fDB);
    s = odb->Chdir(("Equipment/" + eq->fName + "/Settings").c_str(), true);
    v = odb->Chdir(("Equipment/" + eq->fName + "/Variables").c_str(), true);
    
    //setup_watch(mfe, eq, ps);

    //mfe->RegisterRpcHandler(ps);
    //mfe->SetTransitionSequence(-1, -1, -1, -1);

    bool internal_clock = true;
    bool esata_clock = false;
    bool lemo_clock = false;

    s->RB("internal_clock", 0, &internal_clock, true);
    s->RB("esata_clock", 0, &esata_clock, true);
    s->RB("lemo_clock", 0, &lemo_clock, true);

    if (internal_clock) {
      iclock = 0;
    } else if (esata_clock) {
      iclock = 1;
    } else if (lemo_clock) {
      iclock = 2;
    } else {
      iclock = -1;
      mfe->Msg(MERROR, "main", "Invalid CDM clock selection");
    }

    bool esata_sync = true;
    bool lemo_sync = false;

    s->RB("esata_sync", 0, &esata_sync, true);
    s->RB("lemo_sync", 0, &lemo_sync, true);

    if (esata_sync) {
      isync = 1;
    } else if (lemo_sync) {
      isync = 2;
    } else {
      isync = -1;
      mfe->Msg(MERROR, "main", "Invalid CDM sync selection");
    }

    bool lemo_level_nim = false;
    bool lemo_level_ttl = false;

    s->RB("lemo_level_ttl", 0, &lemo_level_ttl, true);
    s->RB("lemo_level_nim", 0, &lemo_level_nim, true);

    if (lemo_level_ttl) {
      lemo_level = 0;
    } else if (lemo_level_nim) {
      lemo_level = 1;
    } else {
      lemo_level = -1;
      mfe->Msg(MERROR, "main", "Invalid CDM lemo level selection");
    }
  }

#if 0
  if (mfe) {
    if (iclock == 0) {
      mfe->Msg(MINFO, "main", "CDM internal clock selected");
    } else if (iclock == 1) {
      mfe->Msg(MINFO, "main", "CDM eSATA clock selected");
    } else if (iclock == 2) {
      mfe->Msg(MINFO, "main", "CDM LEMO clock selected");
    } else {
      mfe->Msg(MERROR, "main", "CDM no clock selected");
    }
  }
#endif
#endif

  printf("clock: %d, sync: %d, lemo_level %d\n", iclock, isync, lemo_level);

  if (iclock < 0 || lemo_level < 0 || isync < 0) {
    usage();
#ifdef HAVE_MIDAS
    if (mfe) {
      mfe->Disconnect();
    }
#endif
    exit(1);
  }

  //int ret = -1;
  //int fd;

  int n;
  tLMK04800 settings;

  InitGPIO();

  if (lemo_level == 0) { // select TTL
    write(SEL_NIM, "0", 1); // 0 - TTL, 1 - NIM
    //mfe->Msg(MINFO, "main", "CDM lemo TTL level selected");
  } if (lemo_level == 1) { // select NIM
    write(SEL_NIM, "1", 1); // 0 - TTL, 1 - NIM
    //mfe->Msg(MINFO, "main", "CDM lemo NIM level selected");
  }

  write(SOURCE_SEL, "0", 1); // 0 - Internal OSC, 1 - Atomic Clock

  if (isync == 1) {
    write(SEL_EXT, "0", 1); // 0 - eSATA, 1 - External SYNC (NIM/TTL)
    //mfe->Msg(MINFO, "main", "CDM sync from eSATA selected");
  } else if (isync == 2) {
    write(SEL_EXT, "1", 1); // 0 - eSATA, 1 - External SYNC (NIM/TTL)
    //mfe->Msg(MINFO, "main", "CDM sync from LEMO selected");
  }

  write(LMK_SYNC, "1", 1); // LMK SYNC OFF
  
  write(CLK0_3_EN, "1", 1);
  write(CLK4_7_EN, "1", 1);
  write(CLK8_11_EN, "1", 1);
  write(CLK12_15_EN, "1", 1);
  write(CLK16_19_EN, "1", 1);
  write(CLK20_23_EN, "1", 1);

  LMK04800_SetDefaults(&settings);

  settings.SPI_3WIRE_DIS = 1;
  settings.RESET_MUX 	= 6;	// SPI Readback
  settings.RESET_TYPE = 3;	// Output Push/Pull
  settings.OSCin_FREQ = 1; 	// 63-127 MHz. actual freq is 100 MHz
  settings.SYNC_POL   = 1;
  settings.VCO_MUX    = 1;
  settings.OSCout_FMT = 0; 
  settings.SYSREF_GBL_PD = 1;
  settings.CLKin_SEL_MODE = 0; // 0 - Atomic, 1 - eSATA, 2 - NIM
  settings.HOLDOVER_EN = 0;
  settings.PLL1_CP_POL = 1;


  if (iclock == 0) {
    //mfe->Msg(MINFO, "main", "CDM internal clock selected");

    settings.CLKin_SEL_MODE = 0; // 0 - Atomic, 1 - eSATA, 2 - NIM

    settings.CLKin0_R = 1; //16; 	// 10 MHz Input clock expected from Atomic Clock
    settings.CLKin1_R = 10; 	// 62.5 Mhz Input clock expected from eSATA
    settings.CLKin2_R = 10; 	// 62.5 Mhz Input clock expected from external clock
    settings.PLL1_N = 10; //160; 	// This makes the above "R" settings work!

    // PLL1 freq 10 MHz
  } else if (iclock == 1) {
    //mfe->Msg(MINFO, "main", "CDM eSATA clock selected");

    settings.CLKin_SEL_MODE = 1; // 0 - Atomic, 1 - eSATA, 2 - NIM

    settings.CLKin0_R = 1; //16; 	// 10 MHz Input clock expected from Atomic Clock
    settings.CLKin1_R = 10; 	// 62.5 Mhz Input clock expected from eSATA
    settings.CLKin2_R = 10; 	// 62.5 Mhz Input clock expected from external clock
    settings.PLL1_N = 16; 	// This makes the above "R" settings work!

    // PLL1 freq 6.25 MHz
    // PLL1 R side is: 62.5 MHz/R = 62.5/10 = 6.25 MHz
    // PLL1 N side is: 100 MHz/N = 100/16 = 6.25 MHz
  } else if (iclock == 2) {
    //mfe->Msg(MINFO, "main", "CDM LEMO clock selected");

    settings.CLKin_SEL_MODE = 2; // 0 - Atomic, 1 - eSATA, 2 - NIM

    settings.CLKin0_R = 1; //16; 	// 10 MHz Input clock expected from Atomic Clock
    settings.CLKin1_R = 10; 	// 62.5 Mhz Input clock expected from eSATA
    settings.CLKin2_R = 10; 	// 62.5 Mhz Input clock expected from external clock
    settings.PLL1_N = 16; 	// This makes the above "R" settings work!

    // PLL1 freq 6.25 MHz
    // PLL1 R side is: 62.5 MHz/R = 62.5/10 = 6.25 MHz
    // PLL1 N side is: 100 MHz/N = 100/16 = 6.25 MHz
  } else {
    mfe->Msg(MERROR, "main", "CDM invalid clock selection");
  }

  // Note: VCO0 runs at around 2000 MHz
  // Note: VCO1 runs at around 3000 MHz/VCO1_DIV => 1500 MHz if VCO1_DIV is set to zero

  // PLL2 R side: 100 MHz / R2 divider = 100/64 = 1.5625 MHz
  // PLL2 N side: 1500 MHz / VCO1_DIV = 1500/2 / N2 prescaler = 1500/(2*8) / N2 divider = 1500/(2*8*60) = 1.5625
  // PLL2 freq = 1500 MHz

  // PLL2 freq = 2000 MHz
  // PLL2 N side: 2000 MHz / N2 prescaler / N2 divider = 2000/(8*160) = 1.5625

  // Note: N2 prescaler is "pll2_p"
  // Note: N2 divider is "pll2_n"

  settings.PLL2_R = 64; // "R2 divider"
  settings.PLL2_P = 8; // PLL2_N_Prescaler or "N2 prescaler"
  settings.PLL2_N = 60; // 120; // "N2 divider"
  settings.PLL2_N_CAL = 60; // 120;
  settings.VCO1_DIV = 0; // value 0 = divide by 2
  settings.OSCout_FMT = 0;
  settings.CLKin_OVERRIDE = 1;

  for(n=0; n<4; n++) {
    settings.ch[n].DCLKoutX_DIV = 24;		// 30 - 50MHz, 24 - Should come out as 62.5 MHz on LMK04821
    settings.ch[n].DCLKoutX_DDLY_PD = 1;	// Disable delay
    settings.ch[n].CLKoutX_Y_PD = 0;	// Enable
    settings.ch[n].SDCLKoutY_PD = 0;	// Enable
    settings.ch[n].DCLKoutX_FMT = 1;	// LVPECL
    settings.ch[n].SDCLKoutY_FMT = 1;	// LVPECL
    settings.ch[n].DCLKoutX_ADLYg_PD = 0;
    settings.ch[n].DCLKoutX_ADLY_PD = 0;
    settings.ch[n].DCLKoutX_MUX = 3;
  }

  for(n=4; n<7; n++) {
    settings.ch[n].DCLKoutX_DIV = 24;		// 30 - 50MHz, 24 - Should come out as 62.5 MHz on LMK04821
    settings.ch[n].DCLKoutX_DDLY_PD = 1;	// Disable delay
    settings.ch[n].CLKoutX_Y_PD = 0;	// Enable
    settings.ch[n].SDCLKoutY_PD = 0;	// Enable
    settings.ch[n].DCLKoutX_FMT = 1;	// LVPECL
    settings.ch[n].SDCLKoutY_FMT = 1;	// LVPECL
    settings.ch[n].DCLKoutX_ADLYg_PD = 0;
    settings.ch[n].DCLKoutX_ADLY_PD = 0;
    settings.ch[n].DCLKoutX_MUX = 3;
  }

  LMK04800_Program(&settings, LMK04800_Write);
  
  write(LMK_SYNC, "1", 1);
  write(LMK_SYNC, "0", 1);
  write(LMK_SYNC, "1", 1);

  unsigned char x3 = LMK04800_Read(3);
  unsigned char x4 = LMK04800_Read(4);
  unsigned char x5 = LMK04800_Read(5);
  unsigned char x6 = LMK04800_Read(6);
  unsigned char xC = LMK04800_Read(0xC);
  unsigned char xD = LMK04800_Read(0xD);
  unsigned char x184 = LMK04800_Read(0x184);

  printf("LMK regs: %d %d %d %d %d %d, reg184: 0x%02x\n", x3, x4, x5, x6, xC, xD, x184);

  for(n = 24; n<NUM_MSS_GPIO; n++) {
    write(fd_gpio[n], "1", 1);
  }
  usleep(200000);
  
  for(n = 24; n<NUM_MSS_GPIO; n++) {
    write(fd_gpio[n], "0", 1);
  }
  usleep(200000);
 
#ifdef HAVE_MIDAS
  if (eq) {
    eq->SetStatus("Ok", "#00FF00");
  }
#endif

  int pll1_lock_count = 0;
  int pll2_lock_count = 0;

  do {
#ifdef HAVE_MIDAS
    if (mfe && mfe->fShutdown) {
      break;
    }

    if (mfe) {
      mfe->PollMidas(1000);
    }
#endif

    for(n = 24; n<NUM_MSS_GPIO; n++) {
      write(fd_gpio[n], "0", 1);
    }
    usleep(200000);

    unsigned char x182 = LMK04800_Read(0x182);
    unsigned char x183 = LMK04800_Read(0x183);
    //unsigned char x184 = LMK04800_Read(0x184);
    //unsigned char x185 = LMK04800_Read(0x185);
    //unsigned char x188 = LMK04800_Read(0x188);

    bool pll1_locked = x182 & (1<<1);
    bool pll1_lock_lost = x182 & (1<<2);

    bool pll2_locked = x183 & (1<<1);
    bool pll2_lock_lost = x183 & (1<<2);

    if (pll1_lock_lost) {
      pll1_lock_count++;
      LMK04800_Write(0x182, 1);
      LMK04800_Write(0x182, 0);
    }

    if (pll2_lock_lost) {
      pll2_lock_count++;
      LMK04800_Write(0x183, 1);
      LMK04800_Write(0x183, 0);
    }

    bool ok = true;

    if (!pll1_locked || !pll2_locked) {
      ok = false;
    }

#ifdef HAVE_MIDAS
      if (eq) {
	if (ok) {
	  eq->SetStatus("Ok", "#00FF00");
	} else {
	  eq->SetStatus("PLL not locked", "red");
	}
      }
#endif

    printf("PLL1: locked %d, lost %d, count %d, PLL2: locked %d, lost %d, count %d, ok %d\n", pll1_locked, pll1_lock_lost, pll1_lock_count, pll2_locked, pll2_lock_lost, pll2_lock_count, ok);

    //printf("reg182: 0x%02x, reg183: 0x%02x, DAC: 0x%02x 0x%02x, HOLDOVER: 0x%02x %d\n", x182, x183, x184, x185, x188, x188&(1<<4));

    for(n = 24; n<NUM_MSS_GPIO; n++) {
      bool x = false;
      if (ok) {
	x = (n%2 == 1);
      } else {
	x = (n%2 == 0);
      }

      if (x) {
	write(fd_gpio[n], "1", 1);
      }
    }
    usleep(200000);

  } while(1);
 
  close(GetSPIDevice(SPI_DEVICE));

#ifdef HAVE_MIDAS
  if (mfe) {
    mfe->Disconnect();
  }
#endif

  return 0;
}
