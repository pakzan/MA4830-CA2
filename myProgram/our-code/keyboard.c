#include <stdio.h>
#include <signal.h>
#include <stdbool.h>

void signal_reset(){
	printf("Reset.");
}

void signal_handler(){
	printf("\nSignal raised.");
}
	
bool save_file(char *filename, FILE *fp, char *data){
	if ((fp = fopen("file.data", "w")) == NULL){
		perror("cannot open");
		return false;
	}
	if (fputs(data, fp) == EOF){
		printf("Cannot write");
		return false;
	}
	fclose(fp);
	return true;
}
	
int main(){
	//start of signal handler
	//signal(SIGINT, signal_handler);
	//while (1){printf("test\n");raise(SIGINT);}
	//end of signal handler
	
	//start of buffered file IO
	//char *str = "test";
	//FILE *fp;
	//save_file("file.data", fp, str);
	//end of buffered file IO
	
	//start of reset
	//signal(SIGQUIT, signal_reset);
	//while (1){printf("test\n");}
	//end of reset
}
#/** PhEDIT attribute block
#-11:16777215
#0:81:default:-3:-3:0
#81:100:default:7929857:-3:0
#100:126:default:-3:-3:0
#126:155:default:7929857:-3:0
#155:213:default:-3:-3:0
#213:405:TextFont09:7929857:-1:0
#405:407:TextFont09:0:-1:0
#407:423:default:-3:-3:0
#423:448:default:37119:-3:0
#448:531:default:-3:-3:0
#531:556:default:37119:-3:0
#556:586:TextFont09:37119:-1:0
#586:657:TextFont09:0:-1:0
#657:708:TextFont09:37119:-1:0
#708:770:TextFont09:0:-1:0
#770:786:TextFont09:37119:-1:0
#786:788:default:-3:-3:0
#**  PhEDIT attribute block ends (-0000558)**/
