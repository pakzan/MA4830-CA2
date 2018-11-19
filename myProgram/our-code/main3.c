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

int wave[2];

typedef struct {
  float amp,
        mean,
        freq;
} chan_para;

typedef int wave_pt[NO_POINT];

wave_pt *wave_type;
chan_para *ch;

void f_malloc(void){
	int i;
  if((ch = (chan_para*)malloc(2 * sizeof(chan_para))) == NULL) {
    printf("Not enough memory.\n");
    exit(1);
  }
  for(i=0; i<2; i++){ //initialize default parameter
    ch[i].amp  = AMP;
    ch[i].mean = MEAN;
    ch[i].freq = FREQ;
  }

  if((wave_type = (wave_pt*)malloc(4 * sizeof(wave_pt))) == NULL) {
    printf("Not enough memory.\n");
    exit(1);
  }

}


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
    //pthread_mutex_lock(&mutex);

	//printf("wave1\n");

        //Channel 1
    if (clock_gettime(CLOCK_REALTIME,&start1)==-1){
      perror("clock gettime");
      exit(EXIT_FAILURE);
    }

        switch(wave[0]){
        case 1 :
        for (i=0; i<NO_POINT; i++){
          current1[i]= (wave_type[0][i] * ch[0].amp)*0.6 + (ch[0].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
			// add offset +  scale
       }
       for (i=0; i<NO_POINT; i++){
          out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar
          out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
          out16(DA_Data,(short) current1[i]);
          delay (((1/ch[0].freq*1000)-(accum1))/NO_POINT);
       }
         break;
        case 2 :
        for (i=0; i<NO_POINT; i++){
          current1[i]= (wave_type[1][i] * ch[0].amp)*0.6 + (ch[0].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
			// add offset +  scale
        }
        for (i=0; i<NO_POINT; i++){
          out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar
          out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
          out16(DA_Data,(short) current1[i]);
          delay (((1/ch[0].freq*1000)-(accum1))/NO_POINT);
       }
         break;
        case 3 :
        for (i=0; i<NO_POINT; i++){
          current1[i]= (wave_type[3][i] * ch[0].amp)*0.6 + (ch[0].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
			// add offset +  scale
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
          current1[i]= (wave_type[2][i] * ch[0].amp)*0.6 + (ch[0].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
			// add offset +  scale
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
    //pthread_mutex_unlock(&mutex);
 }
}

void *t_Wave2(void* arg){

  unsigned int i;
  unsigned int current2[NO_POINT];
  struct timespec start2, stop2;
  double accum2=0;


printf("Wave2 thread created.\n");

  while(1){

    //pthread_mutex_lock(&mutex);
   //printf("wave2\n");
      //Channel 2
    if (clock_gettime(CLOCK_REALTIME,&start2)==-1){
     perror("clock gettime");
     exit(EXIT_FAILURE);
    }

        switch(wave[1]){
        case 1 :
        for (i=0; i<NO_POINT; i++){
          current2[i]= (wave_type[0][i] * ch[1].amp)*0.6 + (ch[1].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
			// add offset +  scale
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
          current2[i]= (wave_type[1][i] * ch[1].amp)*0.6 + (ch[1].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
			// add offset +  scale
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
          current2[i]= (wave_type[3][i] * ch[1].amp)*0.6 + (ch[1].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
			// add offset +  scale
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
          current2[i]= (wave_type[2][i] * ch[1].amp)*0.6 + (ch[1].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
			// add offset +  scale
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

   // pthread_mutex_unlock(&mutex);
  }
}

void *t_HardwareInput(void* arg){

  int mode;
  unsigned int count;

  printf("HardwaveInput thread created.\n");

  while(1)  {
	//printf("\nDIO Functions\n");
	out8(DIO_CTLREG,0x90);					// Port A : Input,  Port B : Output,  Port C (upper | lower) : Output | Output

	dio_in=in8(DIO_PORTA); 					// Read Port A
	//printf("Port A : %02x\n", dio_in);

	out8(DIO_PORTB, dio_in);					// output Port A value -> write to Port B


    if((dio_in & 0x08) == 0x08) {

    	//printf("In");
      if((dio_in & 0x04) == 0x04) {

        while(1) raise(SIGINT);

        //Termination of program
      }
      else if ((mode = dio_in & 0x03) != 0) {
        //printf("\n\nRead multiple ADC\n");
        count=0x00;

        while(count <0x02) {
          chan= ((count & 0x0f)<<4) | (0x0f & count);
          out16(MUXCHAN,0x0D00|chan);		// Set channel	 - burst mode off.
          delay(1);					// allow mux to settle
          out16(AD_DATA,0); 				// start ADC
          while(!(in16(MUXCHAN) & 0x4000));
          adc_in[(int)count]=in16(AD_DATA);
                    // print ADC
          //printf("ADC Chan: %02x Data [%3d]: %4x \n",chan,(int)count, adc_in[count]);
          fflush( stdout );
          count++;
          delay(5);		// Write to MUX register - SW trigger, UP, DE, 5v, ch 0-7
        }
      }
      //pthread_mutex_lock(&mutex);
      //printf("lock input\n");

      switch ((int)mode) {
        case 1:
        ch[0].amp = (float)adc_in[0] * 5.00 / 0xffff; //scale from 16 bits to 5
        ch[1].amp =(float)adc_in[1] * 5.00 / 0xffff; //scale from 16 bits to 5
        //printf("AMP Chan#0 Data [%3.2f] \t Chan#1 Data [%3.2f]\n", ch[0].amp, ch[1].amp);
        break;
        case 2:
        ch[0].freq = ( float)adc_in[0] * 4.50 / 0xffff+0.5; //scale from 16 bits to .5-5
        ch[1].freq = ( float)adc_in[1] * 4.50 / 0xffff+0.5; //scale from 16 bits to .5-5
        //printf("Freq Chan#0 Data [%3.2f] \t Chan#1 Data [%3.2f]\n", ch[0].freq, ch[1].freq);
        break;
        case 3:
        ch[0].mean = ( float)adc_in[0] * 2.00 / 0xffff; //scale from 16 bits to 10.00
        ch[1].mean = ( float)adc_in[1] * 2.00 / 0xffff; //scale from 16 bits to 10.00
        //printf("MEAN Chan#0 Data [%3.2f] \t Chan#1 Data [%3.2f]\n", ch[0].mean, ch[1].mean);
        break;
      }
     //pthread_mutex_unlock(&mutex);
     //printf("unlock input\n");

    }	//if take input from keyboard

  } //end of while
} //end of thread

void *t_ScreenOutput(void* arg){
  printf("Real Time Inputs are as follow:-\n\n");
  printf("\t\tAmp.\tMean\tFreq.\n");
  while(1){
    printf("\nChannel 1: \t%2.2f\t%2.2f\t%2.2f", ch[0].amp , ch[0].mean, ch[0].freq);
    printf("\nChannel 2: \t%2.2f\t%2.2f\t%2.2f\n\n", ch[1].amp , ch[1].mean, ch[1].freq);
    fflush(stdout);
    delay(500);
  }
}




void signal_handler(){
  int t;
  printf("\nHardware Termination Raised\n");
  for(t=0;t<NUM_THREADS-1;t++){
    if(pthread_cancel(thread[t]) == 0)
    printf("Killed thread %d\n",t);
    else printf("Nothing to kill\n")	;
	delay(100);
  }
  if (t ==  NUM_THREADS-1)
  {
    printf("Killed thread %d\n",t);
    printf("BYE\n");
    raise(SIGUSR1);
    pthread_cancel(thread[t]);
    //exit(0);
    }


  fflush(stdout);
  delay(1000);

}


//++++++++++++++++++++++++++++++++++
//MAIN
//++++++++++++++++++++++++++++++++++

int main(int argc, char* argv[]) {

  int i,j=0; //thread count

  //printf("Prompt SU");

  f_PCIsetup();
  f_WaveGen();
  f_malloc();
  signal(SIGINT, signal_handler);

  printf("%d\n",argc);

    if(argc>=2){
    //pthread_mutex_lock(&mutex);
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
    //pthread_mutex_unlock(&mutex);
  }
    if (argc==1) {wave[0]=2; wave[1]=1;} printf("Sine Sine\n");
    if (argc==2) wave[2]=1;


  printf("Initialisation Complete\n");

  if(pthread_create(&thread[j], NULL, &t_Wave1, NULL)){
   printf("ERROR; thread \"t_Wave1\" not created.");
  } j++;

   if(pthread_create(&thread[j], NULL, &t_Wave2, NULL)){
   printf("ERROR; thread \"t_Wave2\" not created.");
   } j++;

  if(pthread_create(&thread[j], NULL, &t_ScreenOutput, NULL)){
    printf("ERROR; thread \"t_ScreenOutput\" not created.");
  } j++;

      if(pthread_create(&thread[j], NULL, &t_HardwareInput, NULL)){
    printf("ERROR; thread \"t_HardwareInput\" not created.");
  }j++;



  pause();   //unreachable code
  //pthread_exit(NULL);
  printf("BYE");
  fflush(stdout);
  delay(1000);
  exit (0);
 //while(1) printf("IM BACK!!!");
  //f_termination();
}
