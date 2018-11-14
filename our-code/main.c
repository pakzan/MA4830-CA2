#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <math.h>
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

#define NO_POINT    100

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Global Variable
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
int badr[5];			// PCI 2.2 assigns 6 IO base addresses
void *hdl;

static unsigned int sine_wave[NO_POINT];
static unsigned int sq_wave[NO_POINT];
static unsigned int tri_wave[NO_POINT];
static unsigned int saw_wave[NO_POINT];

typedef struct {
  float amp,
        mean,
        freq;
} channel; channel ch1, ch2;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void f_PCIsetup(){
  struct pci_dev_info info;

  uintptr_t iobase[6];
  uintptr_t dio_in;
  uint16_t adc_in;

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
}
void f_WaveGen(){

  int i;
  float dummy;

  //Sine wave array
  float delta = (2.0*3.142)/NO_POINT;
  for(i=0; i<NO_POINT-1; i++){
    dummy = (sinf(float)(i*delta)+1)*0xf000;
    sine_wave[i] = (unsigned) dummy;
  }

  //Square wave array
  //Triangular wave array
  //Saw-tooth wave array
}
void *t_Wave(){
  unsigned int current_sine_wave[] = sine_wave[];

  while(1){
    pthread_mutex_lock(mutex);
    for(i=0;i<NO_POINT-1;i++){
      //modulate point
      current_sine_wave[i]=(current_sine_wave[i] + ch1.mean*0xf000)*ch1.amp;
      //point printing to oscilloscope

    }
    pthread_mutex_unlock(mutex);
  }
}
//void *t_HardwareInput()
void *t_ScreenOutput(){
  printf("Real Time Inputs are as follow:-\n\n");
  printf("\t\tAmp.\tMean\tFreq.\n");
  while(1){
    printf("\rChannel 1: \t%2.2f\t%2.2f\t%2.2f", ch1.amp,ch1.mean,ch1.freq);
  }
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

int main() {

  //unsigned int count;
  //unsigned short chan;

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
