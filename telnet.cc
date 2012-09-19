#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>  
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>
#include <string>
#include <map>

#include "3270.hh"

using namespace std;

void hexdump(const char * buffer, size_t n);
void write_some_3270_stuff(int fd);

// FIXME: data is the wrong approach. collect the string until EOR, then handle it
void data(int fd, char *ptr, size_t len);

// FIXME: only work in EOR & BINARY & 3270 mode if they are really negotiated

// FIXME: for negotiation an ordered list would make sense instead of the state
//        machine I wrote here, which also includes options this site is willing
//        to accept if the peer wants it

static const int yes = 1;
unsigned verbose = 0;

sockaddr_in local;

void telnet(int fd);

int
main()
{
  local.sin_family       = AF_INET;
  local.sin_addr.s_addr  = htonl(INADDR_ANY);
  local.sin_port         = htons(2323);

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd==-1) {
    perror("socket tcp");
    exit(EXIT_FAILURE);
  }
  
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) );
  
  if (bind(fd, (sockaddr*) &local, sizeof(sockaddr_in)) < 0) {
    perror("bind tcp");
    fprintf(stderr, "port = %d\n", ntohs(local.sin_port));
    exit(EXIT_FAILURE);
  }
  
  if (listen(fd, 20)<0) {
    perror("listen tcp");
    exit(EXIT_FAILURE);
  }
  
  printf("ready\n");
  
  while(true) {  
    sockaddr_in client;
    socklen_t len = sizeof(client);
    int fd0 = accept(fd, (struct sockaddr *)&client, &len);
    
    struct hostent *hp = gethostbyaddr(&client.sin_addr, len, client.sin_family);

    printf("tcp: got connection from %s:%d on", 
      hp ? hp->h_name : inet_ntoa(client.sin_addr),
      ntohs(client.sin_port));
    printf(" %s:%d\n", inet_ntoa(local.sin_addr), ntohs(local.sin_port));
    
    pid_t pid = fork();
    if (pid==-1) {
      perror("fork tcp");
      exit(EXIT_FAILURE);
    }
    
    if (pid==0) {
      close(fd);
      telnet(fd0);
      exit(0);
    }
    close(fd0);
  }
}

// RFC 854: TELNET COMMAND STRUCTURE
// IAC <command>
// IAC <option code> <option>
// IAC IAC := 255

// Commands:

#define TN_EOR	0xef
// Subnegotiation End
#define TN_SE	0xf0
// Subnegotiation Begin
#define TN_SB	0xfa

// Option Codes:

// Indicates the desire to begin performing, or confirmation that you are
// now performing, the indicated option.
#define TN_WILL 0xfb

// Indicates the refusal to perform, or continue performing, the indicated
// option.
#define TN_WONT 0xfc

// Indicates the request that the other party perform, or confirmation that
// you are expecting the other party to perform, the indicated option.
#define TN_DO	0xfd

// Indicates the demand that the other party stop performing, or
// confirmation that you are no longer expecting the other party to perform,
// the indicated option.
#define TN_DONT 0xfe

// Interpret As Command
#define TN_IAC	0xff

#define TNO_BINARY		0
#define TNO_ECHO		1
#define TNO_STATUS		5
#define TNO_TERMINAL_TYPE	24
#define TNO_EOR			25
#define TNO_3270REGIME		39
// RFC 1416, RFC 2941, RFC 2942, RFC 2943, RFC 2951
// negotiate about window size, RFC 1073
#define TNO_NAWS		31
// RFC 1079
#define TNO_TERMINAL_SPEED	32
// RFC 1372
#define TNO_REMOTE_FLOW_CONTROL	33
// RFC 1184
#define TNO_LINEMODE		34
#define TNO_AUTHENTICATION	37
// RFC 2946
#define TNO_ENCRYPTION		38
// RFC 1572
#define TNO_NEW_ENVIRONMENT	39
#define TNO_TN3270E		40
#define TNO_START_TLS		46


#define TNS_IS	0
#define TNS_SEND 1

#define TN3270E_OP_SEND 8
#define TN3270E_OP_DEVICE_TYPE          2

enum tn_state_code {
  TN_STATE_INIT,
  TN_STATE_DO_TN3270E,
  TN_STATE_DO_3270REGIME,
  TN_STATE_DO_TERMINAL_TYPE,
  TN_STATE_DO_EOR,
  TN_STATE_WILL_EOR,
  TN_STATE_DO_BINARY,
  TN_STATE_WILL_BINARY,
  TN_STATE_TERMINAL_TYPE_SEND
};

// make that an in state and an out state!!!

struct tn_state {
  tn_state_code state;
  unsigned cmd;
  unsigned option;
  string data;
  string terminal_type;
};

void
tn_handle(int fd, tn_state &state)
{
  switch(state.state) {
    case TN_STATE_INIT: {
//      static const char msg[] = { TN_IAC, TN_DO, TNO_TN3270E };
//      printf("send: IAC DO TN3270E\n");
static const char msg[] = { TN_IAC, TN_DO, TNO_TERMINAL_TYPE };
printf("send: IAC DO TERMINAL-TYPE\n");
state.state = TN_STATE_DO_TERMINAL_TYPE;
      write(fd, msg, sizeof(msg));
//      state.state = TN_STATE_DO_TN3270E;
    } break;
    case TN_STATE_DO_TN3270E: {
      if (state.option != TNO_TN3270E) {
        printf("error: expected TN3270E\n");
        exit(1);
      }
      switch(state.cmd) {
        case TN_WILL:
          break;
        case TN_WONT: {
           static const char msg[] = { TN_IAC, TN_DO, TNO_3270REGIME };
           printf("send: IAC DO 3270-REGIME\n");
           write(fd, msg, sizeof(msg));
           state.state = TN_STATE_DO_3270REGIME;
        } break;
      }
    } break;
    case TN_STATE_DO_3270REGIME: {
      if (state.option != TNO_3270REGIME) {
        printf("error: expected 3270-REGIME\n");
        exit(1);
      }
      switch(state.cmd) {
        case TN_WILL:
          break;
        case TN_WONT: {
           static const char msg[] = { TN_IAC, TN_DO, TNO_TERMINAL_TYPE };
           printf("send: IAC DO TERMINAL-TYPE\n");
           write(fd, msg, sizeof(msg));
           state.state = TN_STATE_DO_TERMINAL_TYPE;
        } break;
      }
    } break;
    case TN_STATE_DO_TERMINAL_TYPE: {
      if (state.option != TNO_TERMINAL_TYPE) {
        printf("error: expected TERMINAL-TYPE\n");
        exit(1);
      }
      switch(state.cmd) {
        case TN_WILL:
          static const char msg[] = { TN_IAC, TN_SB, TNO_TERMINAL_TYPE, TNS_SEND, TN_IAC, TN_SE };
          write(fd, msg, sizeof(msg));
          printf("send: IAC SB TERMINAL-TYPE SEND IAC SE\n");
          state.state = TN_STATE_TERMINAL_TYPE_SEND;
          break;
        case TN_WONT: {
          printf("error: no way to negotiate 3270 terminal type\n");
          exit(1);
        } break;
      }
    } break;
    case TN_STATE_TERMINAL_TYPE_SEND: {
      if (state.cmd != TN_SB || state.option != TNO_TERMINAL_TYPE) {
        printf("error: expected SB TERMINAL-TYPE\n");
      }
      switch(state.data[0]) {
        case TNS_IS:
          printf(" IS ");
          break;
        case TNS_SEND:
          printf(" SEND ");
          break;
      }
      state.data.erase(0,1);
      printf("%s IAC SE\n", state.data.c_str());
      if (state.data.find("IBM-327")==string::npos) {
        if (state.terminal_type == state.data) {
          const char msg[] = "ABORT: IBM 3270 COMPATIBLE TERMINAL REQUIRED.\n";
          write(fd, msg, sizeof(msg));
          fwrite(msg, sizeof(msg), 1, stdout);
          exit(0);
        }
        state.terminal_type = state.data;
        static const char msg[] = { TN_IAC, TN_SB, TNO_TERMINAL_TYPE, TNS_SEND, TN_IAC, TN_SE };
        write(fd, msg, sizeof(msg));
        printf("send: IAC SB TERMINAL-TYPE SEND IAC SE\n");
      } else {
        state.terminal_type = state.data;
        static const char msg[] = { TN_IAC, TN_DO, TNO_EOR };
        printf("send: IAC DO EOR\n");
        write(fd, msg, sizeof(msg));
        state.state = TN_STATE_DO_EOR;
      }
    } break;
    
    case TN_STATE_DO_EOR: {
      if (state.option != TNO_EOR) {
        printf("error: expected EOR\n");
        exit(1);
      }
      static const char msg[] = { TN_IAC, TN_WILL, TNO_EOR };
      write(fd, msg, sizeof(msg));
      printf("send: IAC WILL EOR\n");
      state.state = TN_STATE_WILL_EOR;
    } break;
    case TN_STATE_WILL_EOR: {
      if (state.option != TNO_EOR) {
        printf("error: expected EOR\n");
        exit(1);
      }
      static const char msg[] = { TN_IAC, TN_DO, TNO_BINARY };
      write(fd, msg, sizeof(msg));
      printf("send: IAC DO BINARY\n");
      state.state = TN_STATE_DO_BINARY;
    } break;
    
    case TN_STATE_DO_BINARY: {
      if (state.option != TNO_BINARY) {
        printf("error: expected BINARY\n");
        exit(1);
      }
      static const char msg[] = { TN_IAC, TN_WILL, TNO_BINARY };
      write(fd, msg, sizeof(msg));
      printf("send: IAC WILL BINARY\n");
      state.state = TN_STATE_WILL_BINARY;
    } break;
    case TN_STATE_WILL_BINARY: {
#if 0
      static const char msg[] = { TN_IAC, TN_SB, TNO_TERMINAL_TYPE, TNS_SEND, TN_IAC, TN_SE };
      write(fd, msg, sizeof(msg));
      printf("send: IAC SB TERMINAL-TYPE SEND IAC SE\n");
      state.state = TN_STATE_TERMINAL_TYPE_SEND;
#else
      write_some_3270_stuff(fd);
#endif
    } break;
  }
}

void
telnet(int fd)
{
  char buffer[8192];
  
  string terminal_type;
  tn_state tn_state;
  tn_state.state = TN_STATE_INIT;
  tn_handle(fd, tn_state);
  
  unsigned state = 0;
  while(true) {
    ssize_t n = read(fd, buffer, sizeof(buffer));
//    printf("got %zi octets: \n", n);
    if (n<0) {
      perror("read");
      continue;
    }
    if (n==0)
      break;
    ssize_t left, right;
    left=right=0;
    for(ssize_t i=0; i<n; ++i) {
      unsigned char c = buffer[i];
//      printf("(%u:%02x) \n", state, c);
      switch(state) {
        case 0:
          switch(c) {
            case TN_IAC:
              state = 2;
              break;
            default:
              left=i;
              state = 1;
          }
          break;
        case 1: // !IAC ...
          switch(c) {
            case TN_IAC:
              data(fd, buffer+left, i);
              state = 2;
              break;
            default:
              ;
          }
          break;
        case 2: // IAC ...
          switch(c) {
            case 0xff:
              data(fd, buffer+i, 1);
              state = 0;
              break;
            case TN_SE:
              printf("IAC SE ");
              state = 10;
              break;
            case TN_SB:
              printf("IAC SB ");
              tn_state.cmd = c;
              state = 4;
              break;
            case TN_WILL:
              printf("IAC WILL ");
              tn_state.cmd = c;
              state = 3;
              break;
            case TN_WONT:
              printf("IAC WONT ");
              tn_state.cmd = c;
              state = 3;
              break;
            case TN_DO:
              printf("IAC DO ");
              tn_state.cmd = c;
              state = 3;
              break;
            case TN_DONT:
              printf("IAC DONT ");
              tn_state.cmd = c;
              state = 3;
              break;
            case TN_EOR:
              printf("IAC EOR\n");
              state = 0;
              break;
            default:
              state = 2;
          }
          break;
        case 3: // IAC (WILL|WONT|DO|DONT) ...
          switch(c) {
            case TNO_TN3270E:
              printf("TN3270E ");
              break;
            case TNO_3270REGIME:
              printf("3270REGIME ");
              break;
            case TNO_TERMINAL_TYPE:
              printf("TERMINAL-TYPE ");
              break;
            case TNO_EOR:
              printf("EOR ");
              break;
            case TNO_BINARY:
              printf("BINARY ");
              break;
            default: {
              printf("unhandled option %u: send IAC WONT %u\n", (unsigned)c, (unsigned)c);
              char msg[] = { TN_IAC, TN_WONT, c };
              write(fd, msg, sizeof(msg));
              state = 0;
            }
          }
          if (state!=0) {
            state = 0;
            printf("\n");
            tn_state.option = c;
            tn_handle(fd, tn_state);
          }
          break;
        case 4: // IAC SB ...
          switch(c) {
            case TNO_TN3270E:
              printf("TN3270E");
              break;
            case TNO_3270REGIME:
              printf("3270REGIME");
              break;
            case TNO_TERMINAL_TYPE:
              printf("TERMINAL-TYPE");
              break;
            case TNO_EOR:
              printf("EOR");
              break;
            case TNO_BINARY:
              printf("BINARY");
              break;
            default:
              printf("unhandled option %u\n", (unsigned) c);
              exit(0);
          }
          tn_state.option = c;
          state = 5;
          tn_state.data.clear();
          break;
        case 5: // IAC SB option ...
          switch(c) {
            case 0xFF:
              state = 6;
              break;
            default:
              if (tn_state.data.size()>=2048) {
                printf("more than 2048 octets suboption\n");
                exit(0);
              }
              tn_state.data += c;
              break;
          }
          break;
        case 6: // IAC SB option ... IAC
          switch(c) {
            case TN_SE:
              tn_handle(fd, tn_state);
              state = 0;
              break;
            case TN_IAC:
              tn_state.data += (char)0xFF;
              state = 5;
              break;
            default:
              printf("Whu? Unexpected way to end suboption");
              exit(1);
          }
          break;
      }
    }
  }
  printf("close connection\n");
}

void
write_some_3270_stuff(int fd)
{
#if 0
      char buffer[1922];
      int fdx = open("wupp.raw", O_RDONLY);
      if (fdx==-1)
        perror("open");
      if (read(fdx, buffer, 1922)!=1922)
        perror("read");
      close(fdx);
      write(fd, buffer, 1922); 
#endif

#if 0
{
      Out3270 out;
      out.erase();
      
      out.startField();
      out.setOutline(XAO_UNDERLINE);
      out.endField();

      const char *title = "E B C D I C   C H A R A C T E R   T A B L E";
      out.gotoxy((80-strlen(title))/2, 0);
      out.startField();
      out.setOutline(XAO_UNDERLINE);
      out.endField();
      out.text(title);
      string s;
      for(int i=0; i<16; ++i) {
        out.gotoxy(i*4+5, 2);
        s[0] = i < 10 ? i+'0' : i-10+'A';
        out.text(s);
      }
      
      unsigned c = 0x40;
      for(int y=0; y<12; ++y) {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%02x", c);
        out.gotoxy(0, y+4);
        out.text(buffer);
        for(int x=0; x<16; ++x) {
          if (c==0xff)
            break;
          out.gotoxy(x*4+5, y+4);
          out.data.append(1, c++);
        }
      }
       
      out.write(fd);
}
#endif

#if 1

{
      Out3270 out;
      out.erase();

      out.startField();
      out.attr = FA_PROTECT;
      out.setForeground(COLOR_BLUE);
      out.endField();
      out.gotoxy(30,1);
      out.text("W E L C O M E   T O");
      out.startField();
      out.setForeground(COLOR_ORANGE);
      out.endField();
      out.gotoxy(17,3);
      out.text("M   M    A    RRRR   K   K         1    33333");
      out.gotoxy(17,4);
      out.text("MM MM   A A   R   R  K   K        11        3");
      out.gotoxy(17,5);
      out.text("M M M  A   A  R   R  K  K        1 1       3 ");
      out.gotoxy(17,6);
      out.text("M   M  AAAAA  RRRR   KKK    ---    1      33 ");
      out.gotoxy(17,7);
      out.text("M   M  A   A  R R    K  K          1        3");
      out.gotoxy(17,8);
      out.text("M   M  A   A  R  R   K   K         1    3   3");
      out.gotoxy(17,9);
      out.text("M   M  A   A  R   R  K   K       11111   333 ");
      
      out.startField();
      out.setForeground(COLOR_DEFAULT);
      out.endField();
      out.gotoxy(13,11);
      out.startField();
      out.text("Welcome to MARK-13's Unprofessional Time Wasting System");
      out.gotoxy(25,12);
      out.text("For Assistance Contact SDES at 721-821-4291");
      out.gotoxy(42,13);
      out.text("(ask for help with TN3270)");

      out.startField();
      out.setForeground(COLOR_RED);
      out.endField();
      out.gotoxy(10,15);
      out.text("MARK-13's internal systems must only be used for conducting");
      out.gotoxy(10,16);
      out.text("MARK-13's business or for purposes authorized by MARK-13");
      out.gotoxy(10,17);
      out.text("management.");
      out.gotoxy(10,18);
      out.text("Use is subject to audit at any time by MARK-13 management.");
      
      out.startField();
      out.setForeground(COLOR_BLUE);
      out.endField();
      out.gotoxy(1, 21);
      out.text("LOGON  ===> [");
      
      out.startField();
      out.attr = FA_INT_NORM_NSEL | FA_MODIFY;
      out.endField();
      
      out.insertCursor();

      out.gotoxy(40, 21);
      out.startField();
      out.attr = FA_PROTECT;
      out.endField();
      out.text("]");

      out.gotoxy(1, 22);
      out.text("PASSWD ===> [");

      out.startField();
      out.attr = FA_INT_ZERO_NSEL | FA_INTENSITY | FA_MODIFY;
      out.endField();

      out.gotoxy(40,22);
      out.startField();
      out.attr = FA_PROTECT;
      out.endField();
      out.text("]");

      out.write(fd);
      return;
}

#endif

sleep(60);

exit(0);

#if 0
      {
      Out3270 out;
      out.erase();
      out.createPartition(1,
                      5,5,5,5,
                      10,10,10,10);
      out.write(fd);
      }

      {
      Out3270 out;
      out.activatePartition(1);
      out.write(fd);
      }
      
      {
      Out3270 out;                
      out.text("hallodu   was  ist  los?");

      out.write(fd);
      }
#endif
}

void
data(int fd, char *ptr, size_t len)
{
  printf("got data from client\n");
  hexdump(ptr, len);

  string password, login;
  In3270 in(ptr, len);
  while(in.parse()) {
    switch(in.aid) {
      case AID_ENTER:
        if (in.x==15 && in.y==21)
          login = in.data;
        else if (in.x==15 && in.y==22)
          password = in.data;
        printf("%u,%u: %s\n", in.x, in.y, in.data.c_str());
        break;
    }
  }
  
  Out3270 out;
  out.gotoxy(13, 23);
  out.startField();
  out.attr = FA_PROTECT;

//  out.attr = FA_INT_HIGH_SEL;
  out.setForeground(COLOR_RED);
  out.endField();
  if (login.empty())
    out.text("LOGON ERROR: NO USER ID        ");
  else if (password.empty())
    out.text("LOGIN ERROR: NO PASSWD         ");
  else
    out.text("LOGIN ERROR: WRONG ID OR PASSWD");

  out.write(fd);
}
