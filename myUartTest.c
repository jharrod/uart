#include <sys/signal.h>
#include <sys/types.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1         //POSIX compliant source
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

void signal_handler_IO (int status);    //definition of signal handler
int wait_flag=TRUE;                     //TRUE while no signal received
char devicename[80];
long Baud_Rate = 38400;         // default Baud Rate (110 through 38400)
long BAUD;                      // derived baud rate from command line
long DATABITS;
long STOPBITS;
long PARITYON;
long PARITY;
int Data_Bits = 8;              // Number of data bits
int Stop_Bits = 1;              // Number of stop bits
int Parity = 0;                 // Parity as follows:
                  // 00 = NONE, 01 = Odd, 02 = Even, 03 = Mark, 04 = Space
int Format = 4;
FILE *input;
FILE *output;
int status;


/******************************************************************************************/

long convertBaud(long baud) {
    switch (Baud_Rate) {    
    
        case 38400:    
            return B38400;
            break;
        case 115200:
            return B115200;
            break;
        case 19200:
            return B19200;
            break;
        case 9600:
            return B9600;
            break;
        case 4800:
            return B4800;
            break;
        case 2400:
            return B2400;
            break;
        case 1800:
            return B1800;
            break;
        case 1200:
            return B1200;
            break;
        case 600:
            return B600;
            break;
        case 300:
            return B300;
            break;
        case 200:
            return B200;
            break;
        case 150:
            return B150;
            break;
        case 134:
            return B134;
            break;
        case 110:
            return B110;
            break;
        case 75:
            return B75;
            break;
        case 50:
            return B50;
            break;
        default:
            printf("\nSpecified Baud Rate Not Supported. Setting Baud Rate to 19200\n");
            return B19200;
            break;       
    } 
}

/******************************************************************************************/

long convertDataBits(int dBits) {
    switch (dBits) {
        case 8:
        default:
           return CS8;
           break;
        case 7:
           return CS7;
           break;
        case 6:
           return CS6;
           break;
        case 5:
           return CS5;
           break;
        }  //end of switch data_bits
}

/******************************************************************************************/

long convertStopBits(int sBits) {
    switch (sBits) {
        case 1:
        default:
           return 0;
           break;
        case 2:
           return CSTOPB;
           break;
    }  
}      


/******************************************************************************************/

int main(int argc, char *argv[]) {

    int fd, tty, c, res, i;
    int error = 0;
    char In1, Key;
    char buf[255];                       //buffer for where data is put

    struct termios oldtio, newtio;       //place for old and new port settings for serial port
    struct termios oldkey, newkey;       //place tor old and new port settings for keyboard teletype
    
    tcgetattr(fd,&oldtio); // save current port settings 
    
    if (argc==7) {
        int k;
        k=sscanf(argv[1],"%s",devicename);
        if (k != 1) error=1;
        k=sscanf(argv[2],"%li",&Baud_Rate);
        if (k != 1) error=1;
        k=sscanf(argv[3],"%i",&Data_Bits);
        if (k != 1) error=1;
        k=sscanf(argv[4],"%i",&Stop_Bits);
        if (k != 1) error=1;
        k=sscanf(argv[5],"%i",&Parity);
        if (k != 1) error=1;
        k=sscanf(argv[6],"%i",&Format);
        if (k != 1) error=1;
        
       //Print    
        printf("Device Name: %s\n", devicename);
        printf("Baud Rate: %li\n", Baud_Rate);
        printf("Data Bits: %i\n", Data_Bits);
        printf("Stop Bits: %i\n", Stop_Bits);
        printf("Parity: %i\n", Parity);
        printf("Format: %i\n\n", Format);      
        
        if (error == 1){
            printf("\nError Reading Arguments\n");
            exit(1);
        }  
    }
    else {
        printf("Incorrect Number of Arguments\n\n");
        //print options:
        printf("Arguments:\n");
        printf("1: Device Name(path)\n2: Baud Rate\n3: Number of Data Bits\n");
        printf("4: Number of Stop Bits\n5: Parity (0=none, 1=odd, 2=even)\n");
        printf("6: Format of Data Recieved: 1=hex, 2=dec, 3=hex/asc, 4=dec/asc, 5=asc\n\n");
        exit(1);
    }
       

     
    struct sigaction saio;               //definition of signal action
    
   
    input = fopen("/dev/tty", "r");      //open the terminal keyboard
    output = fopen("/dev/tty", "w");     //open the terminal screen

    if (!input || !output) {
        fprintf(stderr, "Unable to open /dev/tty\n");
        exit(1);
    }
            

            
    // set new port settings for non-canonical input processing  //must be NOCTTY
    newkey.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
    newkey.c_iflag = IGNPAR;
    newkey.c_oflag = 0;
    newkey.c_lflag = 0;       //ICANON;
    newkey.c_cc[VMIN]=1;
    newkey.c_cc[VTIME]=0;
    tcflush(tty, TCIFLUSH);
    tcsetattr(tty,TCSANOW,&newkey);        
    
    BAUD = convertBaud(Baud_Rate);
    DATABITS = convertDataBits(Data_Bits);
    STOPBITS = convertStopBits(Stop_Bits);
    
    switch (Parity) {
        
        case 0:
        default:                       //none
           PARITYON = 0;
           PARITY = 0;
           break;
        case 1:                        //odd
           PARITYON = PARENB;
           PARITY = PARODD;
           break;
        case 2:                        //even
           PARITYON = PARENB;
           PARITY = 0;
           break;
    }
    
    //open the device(com port) to be non-blocking (read will return immediately)
    fd = open(devicename, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
         perror(devicename);
         exit(-1);
      }
    
    //install the serial handler before making the device asynchronous
    saio.sa_handler = signal_handler_IO;
    sigemptyset(&saio.sa_mask);   //saio.sa_mask = 0;
    saio.sa_flags = 0;
    saio.sa_restorer = NULL;
    sigaction(SIGIO,&saio,NULL);

    // allow the process to receive SIGIO
   fcntl(fd, F_SETOWN, getpid());
    // Make the file descriptor asynchronous (the manual page says only
    // O_APPEND and O_NONBLOCK, will work with F_SETFL...)
    fcntl(fd, F_SETFL, FASYNC);
    
    



    // set new port settings for canonical input processing 
    newtio.c_cflag = BAUD | CRTSCTS | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;       //ICANON;
    newtio.c_cc[VMIN]=1;
    newtio.c_cc[VTIME]=0;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd,TCSANOW,&newtio);
    // loop while waiting for input. normally we would do something useful here
    while (STOP==FALSE)
    {
       status = fread(&Key,1,1,input);
       if (status==1)  //if a key was hit
       {
          switch (Key)
          { /* branch to appropiate key handler */
             case 0x1b: /* Esc */
                STOP=TRUE;
                break;
             default:
                write(fd,&Key,1);          //write 1 byte to the port
                break;
          }  //end of switch key
       }  //end if a key was hit
       // after receiving SIGIO, wait_flag = FALSE, input is available and can be read
       if (wait_flag==FALSE)  //if input is available
       {
          res = read(fd,buf,255);
          if (res>0) {
             for (i=0; i<res; i++)  //for all chars in string
             {
                In1 = buf[i];
                
                printf("%c", buf[i]);
                
             }  //end of for all chars in string
          }  //end if res>0
//          buf[res]=0;
//          printf(":%s:%d\n", buf, res);
//          if (res==1) STOP=TRUE; /* stop loop if only a CR was input */
          wait_flag = TRUE;      /* wait for new input */
       }  //end if wait flag == FALSE

    } 
    
    // restore old port settings
    tcsetattr(fd,TCSANOW,&oldtio);
    tcsetattr(tty,TCSANOW,&oldkey);
    close(tty);
    close(fd);        //close the com port    
      
    fclose(input);
    fclose(output);       
             
}

/***************************************************************************
* signal handler. sets wait_flag to FALSE, to indicate above loop that     *
* characters have been received.                                           *
***************************************************************************/

void signal_handler_IO (int status)
{
//    printf("received SIGIO signal.\n");
   wait_flag = FALSE;
}

