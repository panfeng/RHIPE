#include "ream.h"
#include <netinet/in.h>


const int i___ = 1;
#define is_bigendian() ( (*(char*)&i___) == 0 )


/***************************************************
 **
 ** Stream Setup
 **
 ***************************************************/
int setup_stream(Streams *s){
  if (!(s->BSTDOUT = freopen(NULL, "wb", stdout))){
    fprintf(stderr,"ERROR: Could not reopen standard output in binary mode");
    return(-1);
  }
  if (!(s->BSTDIN = freopen(NULL, "rb", stdin))){
    fprintf(stderr,"ERROR: Could not reopen standard input in binary mode");
    return(-1);
  }
  if (!(s->BSTDERR = freopen(NULL, "wr", stderr))){
    fprintf(stderr,"ERROR: Could not reopen standard error in binary mode");
    return(-1);
  }
  char *buffsizekb;
  int buffs=1024*10;
  if ((buffsizekb=getenv("rhipe_stream_buffer")))
    buffs = (int)strtol(buffsizekb,NULL,10);


  setvbuf(s->BSTDOUT, 0, _IOFBF , buffs);
  setvbuf(s->BSTDIN,  0, _IOFBF,  buffs);
  setvbuf(s->BSTDERR, 0, _IONBF , 0);
  s->NBSTDOUT = fileno(s->BSTDOUT);
  s->NBSTDIN =  fileno(s->BSTDIN);
  s->NBSTDERR = fileno(s->BSTDERR);
  return(0);
}



/*************************************
 ** Variable Length Encoding
 ************************************/

uint32_t nlz( int64_t x) {
  if (x == 0) return(64); 
  uint32_t n = 0; 
  if (x <=  0x00000000FFFFFFFFLL) {n = n +32; x = x <<32;} 
  if (x <=  0x0000FFFFFFFFFFFFLL) {n = n +16; x = x <<16;}
  if (x <=  0x00FFFFFFFFFFFFFFLL) {n = n + 8; x = x << 8;}
  if (x <=  0x0FFFFFFFFFFFFFFFLL) {n = n + 4; x = x << 4;}
  if (x <=  0x3FFFFFFFFFFFFFFFLL) {n = n + 2; x = x << 2;}
  if (x <=  0x7FFFFFFFFFFFFFFFLL) {n = n + 1;} 
  return n; 
}


uint32_t getVIntSize( int64_t i) {
  if (i >= -112 && i <= 127) {
    return 1;
  }
  if (i < 0) {
    i = ~i;
  }
  int dataBits = sizeof(int64_t)*8 - nlz(i);
  return (dataBits + 7) / 8 + 1;
}
uint32_t isNegativeVInt(const int8_t value) {
  return (uint32_t)(value < -120 || (value >= -112 && value < 0));
}
uint32_t decodeVIntSize(const int8_t value) {
  if (value >= -112) {
    return 1;
  } else if (value < -120) {
    return -119 - value;
  }
  return -111 - value;
}

// void writeVInt64ToFileDescriptor( int64_t  i , int fd) {
//   int8_t x ;
//   if (i >= -112 && i <= 127) {
//     x=(int8_t)i;
//     writen(fd,&x,sizeof(x));
//     return;
//   }
//   int32_t len = -112;
//   if (i < 0) {
//     i ^= -1L; // take one's complement'
//     len = -120;
//   }
//   int64_t tmp = i;
//   while (tmp != 0) {
//     tmp = tmp >> 8;
//     len--;
//   }
//   x=(int8_t)len;
//   writen(fd,&x,sizeof(x));
//   len = (len < -120) ? -(len + 120) : -(len + 112);
//   int idx;
//   for (idx = len; idx != 0; idx--) {
//     int32_t shiftbits = (idx - 1) * 8;
//     int64_t mask = 0xFFL << shiftbits;
//     x = (int8_t)((i & mask) >> shiftbits);
//     writen(fd,&x,sizeof(x));
//   }
// }

void writeVInt64ToFileDescriptor( int64_t  i , FILE* fd) {
  char x ;
  if (i >= -112 && i <= 127) {
    x=(char)i;
    fwrite(&x,sizeof(x),1,fd);
    return;
  }
  int32_t len = -112;
  if (i < 0) {
    i ^= -1L; // take one's complement'
    len = -120;
  }
  int64_t tmp = i;
  while (tmp != 0) {
    tmp = tmp >> 8;
    len--;
  }
  x=(char)len;
  // mmessage("B=%x %d",x,x);
  fwrite(&x,sizeof(x),1,fd);
  len = (len < -120) ? -(len + 120) : -(len + 112);
  int32_t idx;
  for (idx = len; idx != 0; idx--) {
    int32_t shiftbits = (idx - 1) * 8;
    int64_t mask = 0xFFLL << shiftbits;
    x = (char)((i & mask) >> shiftbits);
    // mmessage("B=%x",x);
    fwrite(&x,sizeof(x),1,fd);
  }
  

  // int32_t x = (int32_t) i, tonetwork;
  // tonetwork = reverseUInt(x);
  // fwrite(&tonetwork,sizeof(int32_t),1,fd);
  
}

// int64_t readVInt64FromFileDescriptor(int fd){
//   uint8_t  firstByte = 0 ;
//   Readn(fd,&firstByte,sizeof(uint8_t));
//   int len = decodeVIntSize((int8_t)firstByte);
//   if (len == 1) {
//     return (int8_t)firstByte;
//   }
//   int64_t  i = 0;
//   int32_t idx;
//   for (idx = 0; idx < len-1; idx++) {
//     int8_t b;
//     int32_t x;
//     Readn(fd,&x,sizeof(b));
//     b=(int8_t)x;
//     i = i << 8;
//     i = i | (b & 0xFF);
//   }
//   return  (isNegativeVInt(firstByte) ? (i ^ -1L) : i);
// }


int64_t readVInt64FromFileDescriptor(FILE* fd){
  uint8_t  firstByte = 0 ;
  if(fread(&firstByte,sizeof(uint8_t),1,fd)<=0) 
    return(0);

  int len = decodeVIntSize((int8_t)firstByte);
  if (len == 1) {
    return (int8_t)firstByte;
  }
  int64_t  i = 0;
  int32_t idx;
  for (idx = 0; idx < len-1; idx++) {
    int8_t b;
    int32_t x;
    // Readn(fd,&x,sizeof(b));
    fread(&x,sizeof(b),1,fd);
    b=(int8_t)x;
    i = i << 8;
    i = i | (b & 0xFF);
  }
  return  (isNegativeVInt(firstByte) ? (i ^ -1L) : i);
  // int32_t r,fromnetwork;
  // fread(&r,sizeof(uint32_t),1,fd);
  // fromnetwork = reverseUInt(r);
  // return(fromnetwork);
}

/*****************************
 *
 * Signal Handlers
 * blindly taken from Stevens
 *
 ***************************/
void sig_chld(int signo)
{
        pid_t   pid;
        int             stat;
        while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0);
        return;
}

Sigfunc *signal(int signo, Sigfunc *func)
{
  struct sigaction      act, oact;
  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (signo == SIGALRM) {
#ifdef  SA_INTERRUPT
    act.sa_flags |= SA_INTERRUPT;       /* SunOS 4.x */
#endif
  } else {
#ifdef  SA_RESTART
    act.sa_flags |= SA_RESTART;         /* SVR4, 44BSD */
#endif
  }
  if (sigaction(signo, &act, &oact) < 0)
    return(SIG_ERR);
  return(oact.sa_handler);
}
Sigfunc * Signal(int signo, Sigfunc *func)      /* for our signal() function */
{
  Sigfunc *sigfunc;
  if ( (sigfunc = signal(signo, func)) == SIG_ERR)
    fprintf(stderr,"signal error");
  return(sigfunc);
}

void sigHandler(int i) {
  pid_t pid;
  int  stat;
  // if (i==SIGTERM || i==SIGHUP  || i==SIGINT || i==SIGQUIT)
  //   __active__=0;
  while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0);
  return;
}


/**********************************
 ** Stevens writen, Readn functions
 *********************************/

ssize_t readn(int fd, void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nread;
  char  *ptr;
  
  ptr =(char*) vptr;
  nleft = n;
  while (nleft > 0) {
    if ( (nread = read(fd, ptr, nleft)) < 0) {
      if (errno == EINTR)
        nread = 0;              /* and call read() again */
      else
        return(-1);
    } else if (nread == 0)
      break;                            /* EOF */
    
    nleft -= nread;
    ptr   += nread;
  }
  return(n - nleft);            /* return >= 0 */
}


ssize_t Readn(int fd, void *ptr, size_t nbytes)
{
  ssize_t n;
  if ( (n = readn(fd, ptr, nbytes)) < 0)
    return(-1);
  else return(n);
}

ssize_t writen(int fd, const void *vptr, int n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;
  ptr = (const char*)vptr;
  nleft = n;
  while (nleft > 0) {
    if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
      if (errno == EINTR)
        nwritten = 0;           /* and call write() again */
      else
        return(-1);                     /* error */
    }
    nleft -= nwritten;
    ptr   += nwritten;
  }
  return(n);
}

uint32_t reverseUInt (uint32_t i) {
    uint8_t c1, c2, c3, c4;

    if (is_bigendian()) {
        return i;
    } else {
        c1 = i & 255;
        c2 = (i >> 8) & 255;
        c3 = (i >> 16) & 255;
        c4 = (i >> 24) & 255;

        return ((uint32_t)c1 << 24) + ((uint32_t)c2 << 16) + ((uint32_t)c3 << 8) + c4;
    }
}



