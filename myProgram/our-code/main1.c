#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#define	INTERRUPT	  iobase[1] + 0		// Badr1 + 0 : also ADC register
#define	MUXCHAN		  iobase[1] + 2		// Badr1 + 2
#define	TRIGGER		  iobase[1] + 4		// Badr1 + 4
#define	AUTOCAL		  iobase[1] + 6		// Badr1 + 6
#define DA_CTLREG	  iobase[1] + 8		// Badr1 + 8

#define	AD_DATA	    iobase[2] + 0		// Badr2 + 0
#define AD_FIFOCLR	iobase[2] + 2		// Badr2 + 2

#define	TIMER0		  iobase[3] + 0		// Badr3 + 0
#define	TIMER1		  iobase[3] + 1		// Badr3 + 1
#define	TIMER2		  iobase[3] + 2		// Badr3 + 2
#define	COUNTCTL	  iobase[3] + 3		// Badr3 + 3
#define	DIO_PORTA	  iobase[3] + 4		// Badr3 + 4
#define	DIO_PORTB	  iobase[3] + 5		// Badr3 + 5
#define	DIO_PORTC	  iobase[3] + 6		// Badr3 + 6
#define	DIO_CTLREG	iobase[3] + 7		// Badr3 + 7
#define	PACER1		  iobase[3] + 8		// Badr3 + 8
#define	PACER2		  iobase[3] + 9		// Badr3 + 9
#define	PACER3		  iobase[3] + a		// Badr3 + a
#define	PACERCTL	  iobase[3] + b		// Badr3 + b

#define DA_Data		  iobase[4] + 0		// Badr4 + 0
#define	DA_FIFOCLR	iobase[4] + 2		// Badr4 + 2

#define BILLION 1000000000L
#define MILLION 1000000L
#define THOUSAND 1000L
#define NO_POINT    100

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Global Variable
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
int badr[5];			// PCI 2.2 assigns 6 IO base addresses
void *hdl;
uintptr_t dio_in;
uint16_t adc_in[2];

static unsigned int sine_wave[NO_POINT];
static unsigned int sq_wave[NO_POINT];
static unsigned int tri_wave[NO_POINT];
static unsigned int saw_wave[NO_POINT];

typedef struct {
  float amp,
        mean,
        freq;
} channel; channel ch1, ch2;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Functions
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void f_PCIsetup(){
  struct pci_dev_info info;
  uintptr_t iobase[6];

  printf("\fDemonstration Routine for PCI-DAS 1602\n\n");

  memset(&info,0,sizeof(info));
  if(pci_attach(0)<0) {
    perror("pci_attach");
    exit(EXIT_FAILURE);
  }

  // Vendor and Device ID
  info.VendorId=0x1307;
  info.DeviceId=0x01;

  if ((hdl=pci_attach_device(0, PCI_SHARE|PCI_INIT_ALL, 0, &info))==0) {
    perror("pci_attach_device");
    exit(EXIT_FAILURE);
  }

    //for hardware input
  for(i=0;i<6;i++) {			// Another printf BUG ? - Break printf to two statements
    if(info.BaseAddressSize[i]>0) {
      printf("Aperture %d  Base 0x%x Length %d Type %s\n", i,
        PCI_IS_MEM(info.CpuBaseAddress[i]) ?  (int)PCI_MEM_ADDR(info.CpuBaseAddress[i]) :
        (int)PCI_IO_ADDR(info.CpuBaseAddress[i]),info.BaseAddressSize[i],
        PCI_IS_MEM(info.CpuBaseAddress[i]) ? "MEM" : "IO");
    }
  }

  // Determine assigned BADRn IO addresses for PCI-DAS1602
  printf("\nDAS 1602 Base addresses:\n\n");
  for(i=0;i<5;i++) {
    badr[i]=PCI_IO_ADDR(info.CpuBaseAddress[i]);
    printf("Badr[%d] : %x\n", i, badr[i]);
  }

  printf("\nReconfirm Iobase:\n");  	// map I/O base address to user space
  for(i=0;i<5;i++) {			// expect CpuBaseAddress to be the same as iobase for PC
    iobase[i]=mmap_device_io(0x0f,badr[i]);
    printf("Index %d : Address : %x ", i,badr[i]);
    printf("IOBASE  : %x \n",iobase[i]);
  }
  					// Modify thread control privity
  if(ThreadCtl(_NTO_TCTL_IO,0)==-1) {
    perror("Thread Control");
    exit(1);
  }

      // Initialise Board
  out16(INTERRUPT,0x60c0);		// sets interrupts	 - Clears
  out16(TRIGGER,0x2081);			// sets trigger control: 10MHz,clear,
      // Burst off,SW trig.default:20a0
  out16(AUTOCAL,0x007f);			// sets automatic calibration : default

  out16(AD_FIFOCLR,0); 			// clear ADC buffer
  out16(MUXCHAN,0x0900);		// Write to MUX register-SW trigger,UP,DE,5v,ch 0-0
    // x x 0 0 | 1  0  0 1  | 0x 7   0 | Diff - 8 channels
    // SW trig |Diff-Uni 5v| scan 0-7| Single - 16 channels
}
void f_WaveGen(){
  int i;
  float dummy;

  //Sine wave array
  float delta = (2.0*3.142)/NO_POINT;
  for(i=0; i<NO_POINT; i++){
    dummy = (sinf((float)(i*delta))+1)*0xf000;
    sine_wave[i] = (unsigned) dummy;
  }

  //Square wave array
  //Triangular wave array
  //Saw-tooth wave array
}
void f_termination(){
  out16(DA_CTLREG,(short)0x0a23);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_Data, 0x8fff);					// Mid range - Unipolar

  out16(DA_CTLREG,(short)0x0a43);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_Data, 0x8fff);

  printf("\n\nExit Demo Program\n");
  pthread_exit(NULL);
  pci_detach_device(hdl);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Threads
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void *t_Wave(void* arg){
  unsigned int i;
  unsigned int current1[NO_POINT],current2[NO_POINT];
  float dummy1,dummy2;
  struct timespec start1, stop1, start2, stop2;
  double accum1=0, accum2=0;

  while(1){
    pthread_mutex_lock(mutex);

    printf("\n\nWrite Sine Demo to multiple DAC\n");

    //Channel 1
    if (clock_gettime(CLOCK_REALTIME,&start1)==-1){
      perror("clock gettime");
      exit(EXIT_FAILURE);
    }

    for(i=0;i<NO_POINT;i++) {
      dummy1= (sine_wave[i] + ch1.mean*0xf000) * ch1.amp;
      current1[i]= (unsigned) dummy1;			// add offset +  scale
      out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar
      out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
      out16(DA_Data,(short) current1[i]);
      delay (10+(ch1.freq-accum1)/NO_POINT);
    }

    if (clock_gettime(CLOCK_REALTIME,&stop1)==-1){
     perror("clock gettime");
     exit(EXIT_FAILURE);
    }

    accum1=(double)(stop1.tv_sec-start1.tv_sec)+(double)(stop1.tv_nsec-start1.tv_nsec)/BILLION;

    //Channel 2
    if (clock_gettime(CLOCK_REALTIME,&start2)==-1){
     perror("clock gettime");
     exit(EXIT_FAILURE);
    }

    for(i=0;i<NO_POINT;i++) {
     dummy2= (sine_wave[i] + ch2.mean*0xf000) * ch2.amp;
     current2[i]= (unsigned) dummy2;			// add offset +  scale
     out16(DA_CTLREG,0x0a43);			// DA Enable, #1, #1, SW 5V unipolar
     out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
     out16(DA_Data,(short) current2[i]);
     delay (10+(ch2.freq-accum2)/NO_POINT);
    }

    if (clock_gettime(CLOCK_REALTIME,&stop2)==-1){
      perror("clock gettime");
      exit(EXIT_FAILURE);
    }

    accum2=(double)(stop2.tv_sec-start2.tv_sec)+(double)(stop2.tv_nsec-start2.tv_nsec)/BILLION;

    pthread_mutex_unlock(mutex);
  }
}
void *t_HardwareInput(void* arg){
  int mode;
  unsigned int count;
  while(1)  {
    printf("\nDIO Functions\n");
    out8(DIO_CTLREG,0x90);		// Port A : Input Port B: Output
    dio_in=in8(DIO_PORTA);
    printf("Port A : %02x\n",dio_in);	// Read back
    out8(DIO_PORTB,dio_in);						// Write to Port B

    if((dio_in & 0x08) == 0x08) {
      if((dio_in & 0x04) == 0x04) {
        //Termination of program
      }
      else if ((mode = dio_in & 0x02) != 0) {
        printf("\n\nRead multiple ADC\n");
        count=0x00;

        while(count <0x02) {
          chan= ((count & 0x0f)<<4) | (0x0f & count);
          out16(MUXCHAN,0x0900|chan);		// Set channel	 - burst mode off.
          delay(1);					// allow mux to settle
          out16(AD_DATA,0); 				// start ADC
          while(!(in16(MUXCHAN) & 0x4000));
          adc_in[(int)count]=in16(AD_DATA);
                    // print ADC
          printf("ADC Chan: %02x Data [%3d]: %4x \n",chan,(int)count,(unsigned int)adc_in);
          fflush( stdout );
          count++;
          delay(5);		// Write to MUX register - SW trigger, UP, DE, 5v, ch 0-7
        }
      }
      // pthread_mutex_lock(mutex);
      switch (mode) {
        case 0x01/* value */:
        chan[0].amp = (unsigned float)adc_in[0] * 10.00 / 0x80; //scale from 16 bits to 10.00
        chan[1].amp = (unsigned float)adc_in[1] * 10.00 / 0x80; //scale from 16 bits to 10.00
        printf("ADC Chan#0 Data [%3.2f] \t Chan#1 Data [%3.2f]\n", chan[0].amp, chan[1].amp);
        break;
        case 0x10:
        chan[0].freq = (unsigned float)adc_in[0] * 10.00 / 0x80; //scale from 16 bits to 10.00
        chan[1].freq = (unsigned float)adc_in[1] * 10.00 / 0x80; //scale from 16 bits to 10.00
        break;
        case 0x11:
        chan[0].mean = (unsigned float)adc_in[0] * 10.00 / 0x80; //scale from 16 bits to 10.00
        chan[1].mean = (unsigned float)adc_in[1] * 10.00 / 0x80; //scale from 16 bits to 10.00
        break;
      }
      // pthread_mutex_unlock(mutex);

    }	//if take input from keyboard
  } //end of while
} //end of thread
void *t_ScreenOutput(void* arg){
  printf("Real Time Inputs are as follow:-\n\n");
  printf("\t\tAmp.\tMean\tFreq.\n");
  while(1){
    printf("\rChannel 1: \t%2.2f\t%2.2f\t%2.2f", ch1.amp,ch1.mean,ch1.freq);
  }
}


int main() {

  f_PCIsetup();
  f_WaveGen();

  printf("Initialisation Complete");

  if(pthread_create(NULL, NULL, &t_Wave, NULL)){
    printf("ERROR; thread \"t_Wave\" not created.");
  };
  if(pthread_create(NULL, NULL, &t_HardwareInput, NULL)){
    printf("ERROR; thread \"t_HardwareInput\" not created.");
  };
  if(pthread_create(NULL, NULL, &t_ScreenOutput, NULL)){
    printf("ERROR; thread \"t_ScreenOutput\" not created.");
  };

  while(1);   //unreachable code
  f_termination();
}
