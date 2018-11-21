#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

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
#define THOUSAND	1000L
#define NO_POINT	50
#define NUM_THREADS	 4
#define AMP 1
#define MEAN 1
#define FREQ 1

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Global Variable
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
int badr[5];			// PCI 2.2 assigns 6 IO base addresses
void *hdl;
uintptr_t dio_in;
uintptr_t iobase[6];
uint16_t adc_in[2];
int chan;

typedef struct {
  float amp,
  mean,
  freq;
} channel_para;

typedef int wave_pt[NO_POINT];

channel_para *ch;
wave_pt *wave_type;

int wave[2];

void f_Malloc(void){
  int i;
  if((ch = (channel_para*)malloc(2 * sizeof(channel_para))) == NULL) {
    printf("Not enough memory.\n");
  }

  for(i=0; i<2; i++){
    ch[i].amp = AMP;
    ch[i].mean = MEAN;
    ch[i].freq = FREQ;
  }

  if((wave_type = (wave_pt*)malloc(4 * sizeof(wave_pt))) == NULL) {
    printf("Not enough memory.\n");
    exit(1);
  }
}
<<<<<<< HEAD

=======


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread[NUM_THREADS];

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Functions
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



void f_PCIsetup(){
  struct pci_dev_info info;


  unsigned int i;

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

      //initialise port A, B
   out8(DIO_CTLREG,0x90);		// Port A : Input Port B: Output
}


void f_WaveGen(){
  int i;
  float dummy;

  //Sine wave array
  float delta = (2.0*3.142)/NO_POINT;
  for(i=0; i<NO_POINT; i++){
    dummy = ((sinf((float)(i*delta))))*0x7fff/5;
    wave_type[0][i] = dummy;
   //printf("%d\n", wave_type[0][i]);
  }
    //Square wave array
  //the value of 2 in dummy when i<NO_POINT/2 indicates the value of 'ON'
  //the value of 0 in dummy when i<NO_POINT indicates the value of 'OFF'
  for(i=0;i<NO_POINT;i++){
    if(i<=NO_POINT/2) dummy = 1*0x7fff;
    if(i>NO_POINT/2) dummy = -1*0x7fff;
    wave_type[1][i] =  dummy /5;
  }
  //Saw-tooth wave array
  //the delta used is similar to the one used for sine wave
  //the dummy is increased by multiple of delta when i <NO_POINT/2
  //then the dummy is decrease by multiple of delta when i<NO_POINT
  //the value of '1' in dummy when i<NO_POINT indicates the max value of wave form when i=NO_POINT/2
  delta = 2.0/NO_POINT;
  for(i=0;i<NO_POINT;i++){
    if(i<=NO_POINT/2) dummy = i*delta*0x7fff;
    if(i>NO_POINT/2 && i<NO_POINT) dummy = (-2+i*delta)*0x7fff;
    wave_type[3][i] =  dummy /5;
  }
  //Triangular wave array
  //the value of 2 in delta indicates the max vertical value of the wave form when i = NO_POINT-1, the value is closed to 2
  //the min vertical value of wave form is 0 when i = 0
  delta = 4.0/NO_POINT;
  for(i=0;i<NO_POINT;i++){
    if(i<=NO_POINT/4) dummy = i*delta*0x7fff;
    if(i>NO_POINT/4 && i<=NO_POINT*3/4) dummy = (2-i*delta)*0x7fff;
    if(i>NO_POINT*3/4 && i<NO_POINT) dummy = (-4+i*delta)*0x7fff;
    wave_type[2][i] =  dummy /5;
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

// Start Keyboard
int getInt(int lowLim, int highLim)
{
    int outnum;
    while (true)
    {
        //get int input
        scanf("%d", &outnum);
        //flush input buffer
        char c;
        while ((c = getchar()) != '\n' && c != EOF) { }
        
        //check if outnum is within low and high limit
        //otherwise continue loop
        if (outnum >= lowLim && outnum <= highLim)
            return outnum;
        else
            printf("Please input a valid number!\nYour number should be within %d and %d\n\n", lowLim, highLim);
    }
}

void changeWavePrompt(){
    printf("You have indicated to change waveform.\n Please select the channel:\ 
                            \n1. Channel 1\
                            \n2. Channel 2\n");
    int chn = getInt(1, 2);
                        
    printf("Channel %d selected, please choose your desired waveform:\ 
                        \n1. Sine Wave\
                        \n2. Square Wave\
                        \n3. Sawtooth Wave\
                        \n4. Triangular Wave\n", chn);
    //set wave for channel
    if (chn == 1)
    	wave_select1 = getInt(1, 4);
    else
    	wave_select2 = getInt(1, 4);
	   
    // print selected wave
    char *wave_str[] = {"Sine","Square","Sawtooth","Triangular"};
    printf("%s Wave Selected\n", wave_str[wave-1]);
}
	
void save_file(char *filename, FILE *fp, char *data){
    	printf("File saving in progress, please wait...");
	if ((fp = fopen("file.data", "w")) == NULL){
		perror("cannot open");
	}
	if (fputs(data, fp) == EOF){
		printf("Cannot write");
	}
	fclose(fp);
    	printf("File saved!");
}
	   
void saveFilePrompt(){
    char filename;
    printf("You have indicated to save the output to a file.\nPlease name your file:\n");
    scanf("%s", &filename);
                        
    FILE *fp;
    char *str = "test";
    save_file(filename, fp, str);
}
	   
void f_KeyboardInput(){
    //to stop screen output
    pthread_cancel(3);
	
    printf("Keyboard interrupt detected, please choose your next action:\ 
                        \n1. Change Waveform\
                        \n2. Save and Output File\
                        \n3. Cancel\
                        \n4. End the Program\n");
    int input = getInt(1, 4);
    
    if (input == 1){
        changeWavePrompt();
    }else if (input == 2){
        saveFilePrompt();
    }else if (input == 3){
	//clear console screen
        system("clear");
	if(pthread_create(&thread[3], NULL, &t_ScreenOutput, NULL)){
           printf("ERROR; thread \"t_ScreenOutput\" not created.");
        }
    }else if (input == 4){
        printf("Bye bye\nHope to see you again soon :p");
	f_termination();
    }
    pthread_exit(NULL);
}

// End Keyboard
	   
>>>>>>> 7047c568baccf87c56d45a0ed4dbbfa65a3e5908
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Threads
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void *t_Wave1(void* arg){

  unsigned int i;
  unsigned int current1[NO_POINT],current2[NO_POINT];
  float dummy1,dummy2;
  struct timespec start1, stop1;
  double accum1=0, accum2=0;

  printf("Wave1 thread created.\n");

  while(1){

    //Channel 1
    if (clock_gettime(CLOCK_REALTIME,&start1)==-1){
      perror("clock gettime");
      exit(EXIT_FAILURE);
    }

    switch(wave[0]){
      case 1 :
      for (i=0; i<NO_POINT; i++){
        current1[i]= (wave_type[0][i] * ch[0].amp)*0.6 + (ch[0].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;			// scale + offset
      }
      for (i=0; i<NO_POINT; i++){
        out16(DA_CTLREG,0x0a23);		  // DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				  // Clear DA FIFO  buffer
        out16(DA_Data,(short) current1[i]);
        delay (((1/ch[0].freq*1000)-(accum1))/NO_POINT);
      }
      break;
      case 2 :
      for (i=0; i<NO_POINT; i++){
        current1[i]= (wave_type[1][i] * ch[0].amp)*0.6 + (ch[0].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;			// scale + offset
      }
      for (i=0; i<NO_POINT; i++){
        out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				  // Clear DA FIFO  buffer
        out16(DA_Data,(short) current1[i]);
        delay (((1/ch[0].freq*1000)-(accum1))/NO_POINT);
      }
      break;
      case 3 :
      for (i=0; i<NO_POINT; i++){
        current1[i]= (wave_type[2][i] * ch[0].amp)*0.6 + (ch[0].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;			// scale + offset
      }
      for (i=0; i<NO_POINT; i++){
        out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
        out16(DA_Data,(short) current1[i]);
        delay (((1/ch[0].freq*1000)-(accum1))/NO_POINT);
      }
      break;
      case 4 :
      for (i=0; i<NO_POINT; i++){
        current1[i]= (wave_type[3][i] * ch[0].amp)*0.6 + (ch[0].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;			// scale + offset
      }
      for (i=0; i<NO_POINT; i++){
        out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
        out16(DA_Data,(short) current1[i]);
        delay (((1/ch[0].freq*1000)-(accum1))/NO_POINT);
      }
      break;
    }

    if (clock_gettime(CLOCK_REALTIME,&stop1)==-1){
      perror("clock gettime");
      exit(EXIT_FAILURE);
    }

    accum1=(double)(stop1.tv_sec-start1.tv_sec)+(double)(stop1.tv_nsec-start1.tv_nsec)/BILLION;
  }
}

void *t_Wave2(void* arg){

  unsigned int i;
  unsigned int current2[NO_POINT];
  struct timespec start2, stop2;
  double accum2=0;

  printf("Wave2 thread created.\n");

  while(1){

    //Channel 2
    if (clock_gettime(CLOCK_REALTIME,&start2)==-1){
      perror("clock gettime");
      exit(EXIT_FAILURE);
    }
    switch(wave[1]){
      case 1 :
      for (i=0; i<NO_POINT; i++){
        current2[i]= (wave_type[0][i] * ch[1].amp)*0.6 + (ch[1].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;			// scale + offset
      }
      for (i=0; i<NO_POINT; i++){
        out16(DA_CTLREG,0x0a43);			// DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
        out16(DA_Data,(short) current2[i]);
        delay (((1/ch[1].freq*1000)-(accum2))/NO_POINT);
      }
      break;
      case 2 :
      for (i=0; i<NO_POINT; i++){
        current2[i]= (wave_type[1][i] * ch[1].amp)*0.6 + (ch[1].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;			// scale + offset
      }
      for (i=0; i<NO_POINT; i++){
        out16(DA_CTLREG,0x0a43);			// DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
        out16(DA_Data,(short) current2[i]);
        delay (((1/ch[1].freq*1000)-(accum2))/NO_POINT);
      }
      break;
      case 3 :
      for (i=0; i<NO_POINT; i++){
        current2[i]= (wave_type[2][i] * ch[1].amp)*0.6 + (ch[1].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;			// scale + offset
      }
      for (i=0; i<NO_POINT; i++){
        out16(DA_CTLREG,0x0a43);			// DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
        out16(DA_Data,(short) current2[i]);
        delay (((1/ch[1].freq*1000)-(accum2))/NO_POINT);
      }
      break;
      case 4 :
      for (i=0; i<NO_POINT; i++){
        current2[i]= (wave_type[3][i] * ch[1].amp)*0.6 + (ch[1].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;			// scale + offset
      }
      for (i=0; i<NO_POINT; i++){
        out16(DA_CTLREG,0x0a43);			// DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
        out16(DA_Data,(short) current2[i]);
        delay (((1/ch[1].freq*1000)-(accum2))/NO_POINT);
      }
      break;
    }

    if (clock_gettime(CLOCK_REALTIME,&stop2)==-1){
      perror("clock gettime");
      exit(EXIT_FAILURE);
    }

    accum2=(double)(stop2.tv_sec-start2.tv_sec)+(double)(stop2.tv_nsec-start2.tv_nsec)/BILLION;
  }
}

void *t_HardwareInput(void* arg){

  int mode;
  unsigned int count;

  printf("HardwaveInput thread created.\n");

  while(1)  {
    out8(DIO_CTLREG,0x90);					// Port A : Input,  Port B : Output,  Port C (upper | lower) : Output | Output
    dio_in=in8(DIO_PORTA); 					// Read Port A


    if((dio_in & 0x08) == 0x08) {
      out8(DIO_PORTB, dio_in);					// output Port A value -> write to Port B

      if((dio_in & 0x04) == 0x04) {
        raise(SIGINT);
      }
      else if ((mode = dio_in & 0x03) != 0) {
        count=0x00;

        while(count <0x02) {
          chan= ((count & 0x0f)<<4) | (0x0f & count);
          out16(MUXCHAN,0x0D00|chan);		// Set channel	 - burst mode off.
          delay(1);					// allow mux to settle
          out16(AD_DATA,0); 				// start ADC
          while(!(in16(MUXCHAN) & 0x4000));
          adc_in[(int)count]=in16(AD_DATA);
          count++;
          delay(5);		// Write to MUX register - SW trigger, UP, DE, 5v, ch 0-7
        }
      }

      switch ((int)mode) {
        case 1:
        ch[0].amp = (float)adc_in[0] * 5.00 / 0xffff; //scale from 16 bits to 5
        ch[1].amp =(float)adc_in[1] * 5.00 / 0xffff; //scale from 16 bits to 5
        break;
        case 2:
        ch[0].freq = ( float)adc_in[0] * 4.50 / 0xffff+0.5; //scale from 16 bits to 0.5 ~ 5
        ch[1].freq = ( float)adc_in[1] * 4.50 / 0xffff+0.5; //scale from 16 bits to 0.5 ~ 5
        break;
        case 3:
        ch[0].mean = ( float)adc_in[0] * 2.00 / 0xffff; //scale from 16 bits to 2.00
        ch[1].mean = ( float)adc_in[1] * 2.00 / 0xffff; //scale from 16 bits to 2.00
        break;
      }
    }	//if take input from keyboard
    delay(100);
  } //end of while
} //end of thread

void *t_ScreenOutput(void* arg){
  sleep(1);
  printf("Real Time Inputs are as follow:-\n\n");
  printf("\t\tAmp.\tMean\tFreq.\n");
  while(1){
    printf("\nChannel 1: \t%2.2f\t%2.2f\t%2.2f", ch[0].amp , ch[0].mean, ch[0].freq);
    printf("\nChannel 2: \t%2.2f\t%2.2f\t%2.2f\n\n", ch[1].amp , ch[1].mean, ch[1].freq);
    fflush(stdout);
    delay(500);
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Functions
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void f_PCIsetup(){
  struct pci_dev_info info;
  unsigned int i;

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
  for(i=0;i<6;i++) {
    if(info.BaseAddressSize[i]>0) {
      printf("Aperture %d  Base 0x%x Length %d Type %s\n", i,
      PCI_IS_MEM(info.CpuBaseAddress[i]) ?  (int)PCI_MEM_ADDR(info.CpuBaseAddress[i]) :
      (int)PCI_IO_ADDR(info.CpuBaseAddress[i]),info.BaseAddressSize[i],
      PCI_IS_MEM(info.CpuBaseAddress[i]) ? "MEM" : "IO");
    }
  }

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

  //initialise port A, B
  out8(DIO_CTLREG,0x90);		// Port A : Input Port B: Output
}

void f_WaveGen(){
  int i;
  float dummy;

  //Sine wave array
  float delta = (2.0*3.142)/NO_POINT;
  for(i=0; i<NO_POINT; i++){
    dummy = ((i*delta))*0x7fff/5;
    wave_type[0][i] = dummy;
  }
  //Square wave array
  //the value of 2 in dummy when i<NO_POINT/2 indicates the value of 'ON'
  //the value of 0 in dummy when i<NO_POINT indicates the value of 'OFF'
  for(i=0;i<NO_POINT;i++){
    if(i<=NO_POINT/2) dummy = 1*0x7fff;
    if(i>NO_POINT/2) dummy = -1*0x7fff;
    wave_type[1][i] =  dummy /5;
  }
  //Saw-tooth wave array
  //the delta used is similar to the one used for sine wave
  //the dummy is increased by multiple of delta when i <NO_POINT/2
  //then the dummy is decrease by multiple of delta when i<NO_POINT
  //the value of '1' in dummy when i<NO_POINT indicates the max value of wave form when i=NO_POINT/2
  delta = 2.0/NO_POINT;
  for(i=0;i<NO_POINT;i++){
    if(i<=NO_POINT/2) dummy = i*delta*0x7fff;
    if(i>NO_POINT/2 && i<NO_POINT) dummy = (-2+i*delta)*0x7fff;
    wave_type[3][i] =  dummy /5;
  }
  //Triangular wave array
  //the value of 2 in delta indicates the max vertical value of the wave form when i = NO_POINT-1, the value is closed to 2
  //the min vertical value of wave form is 0 when i = 0
  delta = 4.0/NO_POINT;
  for(i=0;i<NO_POINT;i++){
    if(i<=NO_POINT/4) dummy = i*delta*0x7fff;
    if(i>NO_POINT/4 && i<=NO_POINT*3/4) dummy = (2-i*delta)*0x7fff;
    if(i>NO_POINT*3/4 && i<NO_POINT) dummy = (-4+i*delta)*0x7fff;
    wave_type[2][i] =  dummy /5;
  }
}

void f_termination(){
  out16(DA_CTLREG,(short)0x0a23);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_Data, 0x8fff);					// Mid range - Unipolar

  out16(DA_CTLREG,(short)0x0a43);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_Data, 0x8fff);
  pci_detach_device(hdl);

  free((void *) ch);
  free((void *) wave_type);
}

// Start Keyboard
void f_StrCat(char *s, const char *t){
  while(*s != '\0')
  s++;
  while(*s++ = *t++);
}

int f_GetInt(int lowLim, int highLim)
{
  int outnum;
  while (true)
  {
    char c;

    //get int input
    scanf("%d", &outnum);
    //flush input buffer
    while ((c = getchar()) != '\n' && c != EOF) { }

    //check if outnum is within low and high limit
    //otherwise continue loop
    if (outnum >= lowLim && outnum <= highLim)
    return outnum;
    else
    printf("Please input a valid number!\nYour number should be within %d and %d\n\n", lowLim, highLim);
  }
}

void f_ChangeWavePrompt(){
  int chn;
  const char *wave_str[] = {"Sine","Square","Sawtooth","Triangular"};

  printf("\nYou have indicated to change waveform.\nPlease select the channel:
  \n1. Channel 1
  \n2. Channel 2\n\n");

  chn =  f_GetInt(1, 2);

  printf("\nChannel %d selected, please choose your desired waveform:
  \n1. Sine Wave
  \n2. Square Wave
  \n3. Sawtooth Wave
  \n4. Triangular Wave\n\n", chn);
  //set wave for channel
  wave[chn-1] = f_GetInt(1, 4);

  // print selected wave
  printf("\n%s Wave Selected\n\n", wave_str[wave[chn-1]-1]);
}

void f_SaveFile(char *filename, FILE *fp, char *data){
  char *filetype = ".txt";
  f_f_StrCat(filename, filetype);
  printf("\nFile saving in progress, please wait...\n");

  if ((fp = fopen(filename, "w")) == NULL){
    perror("cannot open\n\n");
  }
  if (fputs(data, fp) == EOF){
    printf("Cannot write\n\n");
  }
  fclose(fp);
  printf("File saved!\n\n");
}

void f_SaveFilePrompt(){
  char filename[10];
  FILE *fp;
  char data[100];
  printf("\n\nYou have indicated to save the output to a file.\nPlease name your file:\n\n");
  scanf("%s", &filename);

  //set data to store into file
  sprintf(data,
    "\t\t\t\t\t\tAmp. Mean Freq.\n
    Channel 1: \t%2.2f\t%2.2f\t%2.2f\n
    Channel 2: \t%2.2f\t%2.2f\t%2.2f\n\n"
    , ch[0].amp , ch[0].mean, ch[0].freq,  ch[1].amp , ch[1].mean, ch[1].freq);

    f_SaveFile(filename, fp, data);
  }
}

void f_ReadFile(char *filename, FILE *fp){
  const char *filetype = ".txt";
  int count;
  float temp_amp[2], temp_mean[2], temp_freq[2];
  f_StrCat(filename, filetype);
  printf("\nFile reading in progress, please wait...\n");

  if ((fp = fopen(filename, "r")) == NULL){
    perror("cannot open\n\n");
  }
  //get variables from file
  count = fscanf(fp, "%*[^C]Channel 1: %f %f %f Channel 2: %f %f %f",
  &temp_amp[0], &temp_mean[0], &temp_freq[0],
  &temp_amp[1], &temp_mean[1], &temp_freq[1]);
  //if received all 6 values successfully, set values to variables
  if (count == 6){
    int i;
    for(i = 0; i < 2; i++){
      ch[i].amp = temp_amp[i];
      ch[i].mean = temp_mean[i];
      ch[i].freq = temp_freq[i];
      //printf("%f, %f, %f", temp_amp[i], temp_mean[i], temp_freq[i]);
    }
    printf("Read File Successfully\n\n");
  }else{
    printf("Read File Fail\n\n");
  }
  fclose(fp);
}

void f_ReadFilePrompt(){
  char filename[10];
  FILE *fp;
  char data[100];
  printf("\n\nYou have indicated to read the file.\nPlease name your file:\n\n");
  scanf("%s", &filename);

  f_ReadFile(filename, fp);
}

void f_KeyboardInput(){
  int input;

  //to stop screen output
  if(pthread_cancel(thread[3]) == 0)
  printf("Killed screen thread\n");

  while(true){
    printf("\nMain Menu\nPlease choose your next action:\n\n");
    printf("1. Change Waveform
    \n2. Save and Output File
    \n3. Read File
    \n4. Return to Display
    \n5. End the Program\n\n");
    input = f_GetInt(1, 5);

    if (input == 1){
      f_f_ChangeWavePrompt();
    }else if (input == 2){
      f_SaveFilePrompt();
    }else if (input == 3){
      f_ReadFilePrompt();
    }else if (input == 4){
      //clear console screen
      system("clear");
      if(pthread_create(&thread[3], NULL, &t_ScreenOutput, NULL)){
        printf("ERROR; thread \"t_ScreenOutput\" not created.");
      } printf("Screen Output main %d\n", thread[3]);
      break;
    }else if (input == 5){
      printf("\nBye bye\nHope to see you again soon :p\n");
      raise(SIGINT);
    }
  }
}
// End Keyboard

void signal_handler(){
  int t;
  pthread_t temp;
  temp = pthread_self();
  printf("\nHardware Termination Raised\n");
  for(t = 3; t >= 0; t--){
    if(thread[t] != temp) {
      pthread_cancel(thread[t]);
      printf("Thread %d is killed.\n",thread[t]);
    }
  }// for loop
  printf("Thread %d is killed.\n", temp);
  pthread_exit(NULL);
  delay(500);
} //handler


//++++++++++++++++++++++++++++++++++
//MAIN
//++++++++++++++++++++++++++++++++++

int main(int argc, char* argv[]) {

  int i;
  int j=0; //thread count

  f_PCIsetup();
  f_Malloc();
  f_WaveGen();

  signal(SIGINT, signal_handler);
  signal(SIGQUIT, f_KeyboardInput);

  if(argc>=2){
    for(i=1;i<argc;i++){
      if(strcmp(argv[i],"-sine")==0){
        printf("%d. Sine wave chosen as wave\n",i);
        wave[i-1]=1;
      }
      if(strcmp(argv[i],"-square")==0){
        printf("%d. Square wave chosen as wave\n",i);
        wave[i-1]=2;
      }
      if(strcmp(argv[i],"-saw")==0){
        printf("%d. Saw wave chosen as wave\n",i);
        wave[i-1]=3;
      }
      if(strcmp(argv[i],"-tri")==0){
        printf("%d. Triangular wave chosen as wave\n",i);
        wave[i-1]=4;
      }
    }//end of for loop, checking argv[]
  }
  if (argc==1) {
    wave[0]=1; wave[1]=2;
  }
  if (argc==2) {
    wave[1]=2;
  }

  printf("Initialisation Complete\n");

  if(pthread_create(&thread[j], NULL, &t_HardwareInput, NULL)){
    printf("ERROR; thread \"t_HardwareInput\" not created.");
  }j++;

  if(pthread_create(&thread[j], NULL, &t_Wave1, NULL)){
    printf("ERROR; thread \"t_Wave1\" not created.");
  } j++;

  if(pthread_create(&thread[j], NULL, &t_Wave2, NULL)){
    printf("ERROR; thread \"t_Wave2\" not created.");
  } j++;

  if(pthread_create(&thread[j], NULL, &t_ScreenOutput, NULL)){
    printf("ERROR; thread \"t_ScreenOutput\" not created.");
  } j++;

  pthread_exit(NULL);
}
