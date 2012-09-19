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
  local.sin_port         = htons(23);

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

//---------------------------------------------------------------------------
//
// EBCDIC stuff
//
//---------------------------------------------------------------------------

// character set
// 00 default
// f1 APL
// 40-ef: symbol sets loaded by host program
// DBCS 0xf8

// US 037
// Bracket, CP 037 Modified
const char* ebcdic2utf8_table_bracket_cp037_modified[] = {
// 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F 
   " ", 0,   "â", "ä", "à", "á", "ã", "å", "ç", "ñ", "¢", ".", "<", "(", "+", "|", // 4
   "&", "é", "ê", "ë", "è", "í", "î", "ï", "ì", "ß", "!", "$", "*", ")", ";", "¬", // 5
   "-", "/", "Â", "Ä", "À", "Á", "Ã", "Å", "Ç", "Ñ", "¦", ",", "%", "_", ">", "?", // 6
   "ø", "É", "Ê", "Ë", "È", "Í", "Î", "Ï", "Ì", "`", ":", "#", "@", "'","=", "\"", // 7
   "Ø", "a", "b", "c", "d", "e", "f", "g", "h", "i", "«", "»", "ð", "ý", "þ", "±", // 8
   "°", "j", "k", "l", "m", "n", "o", "p", "q", "r", "ª", "º", "æ", "¸", "Æ", "¤", // 9
   "µ", "~", "s", "t", "u", "v", "w", "x", "y", "z", "¡", "¿", "Ð", "[", "Þ", "®", // A
   "^", "£", "¥", "·", "©", "§", "¶", "¼", "½", "¾", "Ý", "¨", "¯", "]", "´", "×", // B
   "{", "A", "B", "C", "D", "E", "F", "G", "H", "I", "­", "ô", "ö", "ò", "ó", "õ", // C
   "}", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "¹", "û", "ü", "ù", "ú", "ÿ", // D
   "\\","÷", "S", "T", "U", "V", "W", "X", "Y", "Z", "²", "Ô", "Ö", "Ò", "Ó", "Õ", // E
   "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "³", "Û", "Ü", "Ù", "Ú", 0    // F
};

// U.S. English CP 037
const char* ebcdic2utf8_table[] = {
// 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F 
   " ", 0,   "â", "ä", "à", "á", "ã", "å", "ç", "ñ", "¢", ".", "<", "(", "+", "|", // 4
   "&", "é", "ê", "ë", "è", "í", "î", "ï", "ì", "ß", "!", "$", "*", ")", ";", "¬", // 5
   "-", "/", "Â", "Ä", "À", "Á", "Ã", "Å", "Ç", "Ñ", "¦", ",", "%", "_", ">", "?", // 6
   "ø", "É", "Ê", "Ë", "È", "Í", "Î", "Ï", "Ì", "`", ":", "#", "@", "'","=", "\"", // 7
   "Ø", "a", "b", "c", "d", "e", "f", "g", "h", "i", "«", "»", "ð", "ý", "þ", "±", // 8
   "°", "j", "k", "l", "m", "n", "o", "p", "q", "r", "ª", "º", "æ", "¸", "Æ", "¤", // 9
   "µ", "~", "s", "t", "u", "v", "w", "x", "y", "z", "¡", "¿", "Ð", "Ý", "Þ", "®", // A
   "^", "£", "¥", "·", "©", "§", "¶", "¼", "½", "¾", "[", "]", "¯", "¨", "´", "×", // B
   "{", "A", "B", "C", "D", "E", "F", "G", "H", "I", "­", "ô", "ö", "ò", "ó", "õ", // C
   "}", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "¹", "û", "ü", "ù", "ú", "ÿ", // D
   "\\","÷", "S", "T", "U", "V", "W", "X", "Y", "Z", "²", "Ô", "Ö", "Ò", "Ó", "Õ", // E
   "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "³", "Û", "Ü", "Ù", "Ú", 0    // F
};

const char*
ebcdic2utf8(unsigned char i) {
  if (i<0x40)
    return 0;
  return ebcdic2utf8_table[i-0x40];
}

map<string, unsigned char> utf82ebcdic_table;
unsigned char utf82ebcdic(const string &s)
{
  if (utf82ebcdic_table.empty()) {
    for(int i=0x40; i<0xFF; ++i) {
      if (ebcdic2utf8_table[i-0x40])
        utf82ebcdic_table[ ebcdic2utf8_table[i-0x40] ] = i;
    }
  }
  map<string, unsigned char>::const_iterator p = utf82ebcdic_table.find(s);
  if (p==utf82ebcdic_table.end())
    return 0x4B; // '.'
  return p->second;
}

void
hexdump(const char * buffer, size_t n)
{
  size_t i, j, n16;
  
  n16 = n + (17 - (n%16));
  
  for(i=0; i<n16; ++i) {
    if (i%16 == 0) {
      if (i!=0) {
        printf(" ");
        for(; j<i; ++j) {
          if (j>=n)
            break;
          const char *c = ebcdic2utf8(buffer[j]);
          printf("%s", c ? c : ".");
        }
        printf("\n");
      }
      j = i;
      printf("%04zx", i);
    }
    if (i<n)
      printf(" %02x", (unsigned char)buffer[i]);
    else
      printf("   ");
  }
  printf("\n");
}

//---------------------------------------------------------------------------
//
// Out3270 stuff
//
//---------------------------------------------------------------------------

// x3270 -trace -tracefile stdout 127.0.0.1 2323
// x3270 does not support multiple partitions

static const unsigned WCC_RESET = 64;		// bit 1: reset partitions
// bit 2: for printers
// bit 3: for printers
static const unsigned WCC_START_PRINTER = 8;	// bit 4: print screen at end of write operation
static const unsigned WCC_SOUND_ALARM = 4;	// bit 5: bell
static const unsigned WCC_KEYBOARD_RESTORE = 2; // bit 6: unlock keyboard, reset AID byte
static const unsigned WCC_RESET_MDT = 1;        // bit 7: reset MDT bits in field attribute

/* 3270 commands */
#define CMD_W           0x01    /* write characters to the terminal */
#define CMD_RB          0x02    /* read buffer */
#define CMD_NOP         0x03    /* no-op */
#define CMD_EW          0x05    /* erase and write characters to the terminal*/
#define CMD_RM          0x06    /* read modified: read unprotected fields that have changed from the terminal */
#define CMD_EWA         0x0d    /* erase/write alternate */
#define CMD_RMA         0x0e    /* read modified all */
#define CMD_EAU         0x0f    /* erase all unprotected fields on the terminal */
#define CMD_WSF         0x11    /* write structured field: send control information (color, screen size) */

/* 3270 orders */
#define ORDER_PT        0x05    /* program tab */
#define ORDER_GE        0x08    /* graphic escape */
#define ORDER_SBA       0x11    /* set buffer address */
#define ORDER_EUA       0x12    /* erase unprotected to address */
#define ORDER_IC        0x13    /* insert cursor */
#define ORDER_SF        0x1d    /* start field */  
#define ORDER_SA        0x28    /* set attribute */
#define ORDER_SFE       0x29    /* start field extended */
#define ORDER_YALE      0x2b    /* Yale sub command */
#define ORDER_MF        0x2c    /* modify field */
#define ORDER_RA        0x3c    /* repeat to address */

/* AIDs */
#define AID_NO          0x60    /* no AID generated */
#define AID_QREPLY      0x61
#define AID_ENTER       0x7d
#define AID_PF1         0xf1
#define AID_PF2         0xf2
#define AID_PF3         0xf3
#define AID_PF4         0xf4
#define AID_PF5         0xf5
#define AID_PF6         0xf6
#define AID_PF7         0xf7
#define AID_PF8         0xf8
#define AID_PF9         0xf9
#define AID_PF10        0x7a
#define AID_PF11        0x7b
#define AID_PF12        0x7c
#define AID_PF13        0xc1
#define AID_PF14        0xc2 
#define AID_PF15        0xc3
#define AID_PF16        0xc4
#define AID_PF17        0xc5
#define AID_PF18        0xc6
#define AID_PF19        0xc7
#define AID_PF20        0xc8
#define AID_PF21        0xc9
#define AID_PF22        0x4a
#define AID_PF23        0x4b
#define AID_PF24        0x4c
#define AID_OICR        0xe6
#define AID_MSR_MHS     0xe7
#define AID_SELECT      0x7e
#define AID_PA1         0x6c
#define AID_PA2         0x6e
#define AID_PA3         0x6b
#define AID_CLEAR       0x6d
#define AID_SYSREQ      0xf0

#define AID_SF          0x88
#define SFID_QREPLY     0x81


/* Structured fields */
#define SF_READ_PART    0x01    /* read partition */
#define  SF_RP_QUERY    0x02    /*  query */
#define  SF_RP_QLIST    0x03    /*  query list */
#define   SF_RPQ_LIST   0x00    /*   QCODE list */ 
#define   SF_RPQ_EQUIV  0x40    /*   equivalent+ QCODE list */
#define   SF_RPQ_ALL    0x80    /*   all */
#define SF_ERASE_RESET  0x03    /* erase/reset */
#define  SF_ER_DEFAULT  0x00    /*  default */
#define  SF_ER_ALT      0x80    /*  alternate */  
#define SF_SET_REPLY_MODE 0x09  /* set reply mode */   
#define  SF_SRM_FIELD   0x00    /*  field */
#define  SF_SRM_XFIELD  0x01    /*  extended field */
#define  SF_SRM_CHAR    0x02    /*  character */
#define SF_CREATE_PART  0x0c    /* create partition */
#define  CPFLAG_PROT    0x40    /*  protected flag */
#define  CPFLAG_COPY_PS 0x20    /*  local copy to presentation space */  
#define  CPFLAG_BASE    0x07    /*  base character set index */
#define SF_OUTBOUND_DS  0x40    /* outbound 3270 DS */
#define SF_TRANSFER_DATA 0xd0   /* file transfer open request */

enum Color {
  COLOR_DEFAULT = 0,
  COLOR_NEUTRAL_BLACK = 0xF0,
  COLOR_BLUE,
  COLOR_RED,
  COLOR_PINK,
  COLOR_GREEN,
  COLOR_TURQUOISE,
  COLOR_YELLOW,
  COLOR_NEUTRAL_WHITE,
  COLOR_BLACK,
  COLOR_DEEP_BLUE,
  COLOR_ORANGE,
  COLOR_PURPLE,
  COLOR_PALE_GREEN,
  COLOR_PALE_TURQUOISE,
  COLOR_GREY,
  COLOR_WHITE
};

class Out3270 {
  public:
    string data;
    unsigned attr;
    
  protected:
    Color foreground;
    Color background;
    unsigned charset;
    unsigned outline;
    
    unsigned xa_count;
    bool got_foreground:1;
    bool got_background:1;
    bool got_charset:1;
    bool got_outlining:1;
    bool got_highlightning:1;
    
  public:
    Out3270();
    void nop() {
       data[0] = CMD_NOP;
    }
    void erase();
    void eraseUnprotected();
    void wcc(unsigned flags) {
      data[1] = flags;
    }
    
    void gotoxy(unsigned x, unsigned y);
    void insertCursor();
    void eraseAllUnprotected() {
      data[0] = CMD_EAU;
    }
    void text(const string &s);
    void write(int fd);


    void startField();
    void endField();
    
    void setForeground(Color color) {
      if (!got_foreground) {
        ++xa_count;
        got_foreground = true;
      }
      foreground = color;
    }
    void setBackground(Color color) {
      if (!got_background) {
        ++xa_count;
        got_background = true;
      }
      background = color;
    }
    void setCharset(unsigned v) {
      if (!got_charset) {
        ++xa_count;
        got_charset = true;
      }
      charset = v;
    }
    void setOutline(unsigned v) {
      if (!got_outlining) {
        ++xa_count;
        got_outlining = true;
      }
      outline = v;
    }
    
    
    void createPartition(unsigned pid, 
                         unsigned sx, unsigned sy, unsigned sw, unsigned sh,  // presentation space
                         unsigned px, unsigned py, unsigned pw, unsigned ph); // view port
    void activatePartition(unsigned pid);

    void putByte(unsigned v) {
      data.append(1, v);
    }
    void putWord(unsigned v) {
      data.append(1, (v >> 8) & 0xFF);
      data.append(1, v & 0xFF);
    }
};


/* Field attributes. (ORDER_SF) */
#define FA_PRINTABLE     0xc0    /* these make the character "printable" */
#define FA_PROTECT       0x20    /* unprotected (0) / protected (1) */       // field can't/can be changed
#define FA_NUMERIC       0x10    /* alphanumeric (0) /numeric (1) */
#define FA_INTENSITY     0x0c    /* display/selector pen detectable: */      // input invisible
#define FA_INT_NORM_NSEL 0x00   /*  00 normal, non-detect */
#define FA_INT_NORM_SEL  0x04   /*  01 normal, detectable */
#define FA_INT_HIGH_SEL  0x08   /*  10 intensified, detectable */
#define FA_INT_ZERO_NSEL 0x0c   /*  11 nondisplay, non-detect */
#define FA_RESERVED      0x02    /* must be 0 */
#define FA_MODIFY        0x01    /* modified (1) */

/* Extended attributes (ORDER_SFE) */
#define XA_ALL          0x00
#define XA_3270         0xc0
#define XA_VALIDATION   0xc1
#define  XAV_FILL       0x04
#define  XAV_ENTRY      0x02
#define  XAV_TRIGGER    0x01
#define XA_OUTLINING    0xc2
#define  XAO_UNDERLINE  0x01
#define  XAO_RIGHT      0x02
#define  XAO_OVERLINE   0x04
#define  XAO_LEFT       0x08
#define XA_HIGHLIGHTING 0x41
#define  XAH_DEFAULT    0x00
#define  XAH_NORMAL     0xf0
#define  XAH_BLINK      0xf1
#define  XAH_REVERSE    0xf2
#define  XAH_UNDERSCORE 0xf4
#define  XAH_INTENSIFY  0xf8	// monochrome only
#define XA_FOREGROUND   0x42
#define  XAC_DEFAULT    0x00
#define XA_CHARSET      0x43
#define XA_BACKGROUND   0x45
#define XA_TRANSPARENCY 0x46
#define  XAT_DEFAULT    0x00
#define  XAT_OR         0xf0
#define  XAT_XOR        0xf1
#define  XAT_OPAQUE     0xff
#define XA_INPUT_CONTROL 0xfe
#define  XAI_DISABLED   0x00
#define  XAI_ENABLED    0x01

// 1 octet : command
// 1 octed : write controll character
// data

// OR

// write structured field
// structured field
// ...
// structured field

Out3270::Out3270()
{
//  data.append(5, 0);
  data.append(1, CMD_W);
  data.append(1, WCC_KEYBOARD_RESTORE);
}

void
Out3270::erase() {
  char c = data[0];
  switch(c) {
    case CMD_W:
      c = CMD_EW;
      break;
  }
  data[0] = c;
}

// FIXME: screen size is hard coded
unsigned rows = 24;
unsigned cols = 80;

// 12 bit addresses: 2 octets with the lower 6 bits
// 14 bit addresses: 

// this is used in x3270 to convert between cursor position & buffer address
static unsigned char code_table[64] = {
  0x40, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
  0xC8, 0xC9, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
  0xD8, 0xD9, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x60, 0x61, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
  0xE8, 0xE9, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
  0xF8, 0xF9, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
};

void
Out3270::gotoxy(unsigned x, unsigned y)
{
  data.append(1, ORDER_SBA);
  unsigned addr = x + y * cols;
  if (rows * cols > 0x1000) {
    // 14 and 16 bit adressing
    
    // if a device supports 12 and 14 bit, the msb are using this schema:
    // 00 14 bit address
    // 01 12 bit address
    // 10 reserved
    // 11 12 bit address
    
    data.append(1, (addr >> 8) & 0x3F);
    data.append(1, addr & 0xFF);
  } else {
    // 12 bit addressing: only the first 6bits are used
    data.append(1, code_table[(addr >> 6) & 0x3F]);
    data.append(1, code_table[addr & 0x3F]);
  }
}

void
Out3270::createPartition(unsigned pid, 
                         unsigned sx, unsigned sy, unsigned sw, unsigned sh, // presentation space
                         unsigned px, unsigned py, unsigned pw, unsigned ph) // view port
{
  data[5] = CMD_WSF;
  data = data.substr(0, data.size()-1);

  // length (2 octets) of structured field
  putWord(6 + 8 * 2); // length

  putByte(SF_CREATE_PART);

  assert(pid>=00 && pid<=0x7E);
  putByte(0);
  
  // the unit of measure
  // 0000____ character cells
  // 0001____ adressable points (H thru PH)
  // 0010____ adressable points (RV & CV)
  // address mode
  // ____0000 12/14 bit
  // ____0001 16 bit
  // ____0010 text
  putByte(0);
  
  // 0100 0000 : 0 unprotected, 1 protected
  // 0010 0000 : type of local copy: 0=viewport, 1=presentation space
  putByte(0);

  putWord(sh);
  putWord(sw);
  putWord(py);
  putWord(px);
  putWord(ph);
  putWord(pw);
  putWord(sy);
  putWord(sx);
 
//  putWord(1); // number of units in vertical multiple scroll
//  putWord(0); // reserved
  
//  putWord(8); // character width
//  putWord(8); // character height
  
  // word: height of presentation space
  // word: width
  // word y
  // word x
  // 
  
}

void
Out3270::activatePartition(unsigned pid)
{
  data[5] = 0xf3; // SNA_CMD_WSF;
  data = data.substr(0, data.size()-1);

  putWord(4);
//  putByte(SF_ACTIVATE_PART);
  putByte(0x0E);
  putByte(pid);
}

/**
 * Return the number for bytes used to store 'charlen' characters
 * beginning at 'start' in 'text'.
 */
inline size_t
utf8bytecount(const char *ptr)
{
  size_t result = 0;
  ++ptr;
  ++result;
  while( (*ptr & 0xC0) == 0x80 ) {
    ++result; 
    ++ptr;
  }
  return result;
}

void
Out3270::text(const string &text)
{
  for(const char *p = text.c_str(); *p;) {
    size_t l = utf8bytecount(p);
    data += utf82ebcdic(string(p, l));
    p += l;
  }
}

void
Out3270::startField()
{
  xa_count = 0;
  got_foreground = got_background = got_charset = got_outlining = got_highlightning = false;
}

void
Out3270::endField()
{
  if (!xa_count) {
    data.append(1, ORDER_SF);
    data.append(1, attr);
  } else {
    data.append(1, ORDER_SFE);
    data.append(1, xa_count+1);
    data.append(1, XA_3270);
    data.append(1, attr);
    if (got_foreground) {
      data.append(1, XA_FOREGROUND);
      data.append(1, foreground);
    }
    if (got_background) {
      data.append(1, XA_BACKGROUND);
      data.append(1, background);
    }
    if (got_charset) {
      data.append(1, XA_CHARSET);
      data.append(1, charset);
    }
    if (got_outlining) {
      data.append(1, XA_OUTLINING);
      data.append(1, outline);
    }
  }
}

// move cursor to the current output position
void
Out3270::insertCursor()
{
  data.append(1, ORDER_IC);
}

void
Out3270::write(int fd)
{
  // fixme: escape 0xFF

  // append EOR
  data.append(1, 0xFF);
  data.append(1, 0xEF);
  ::write(fd, data.c_str(), data.size());
  
  if (verbose>0) {
    printf("send:\n");
    hexdump(data.c_str(), data.size());
  }
};

// the communication will start with telnet, negotiate the terminal type
// switch to binary and the do the rest using the 3270 protocol

// charset is EBDIC
/*
3270 Data Stream

  Outbound Data Stream from Application to Device can have two variants:

    Command | Write Control Character (WCC) | Data
    Write Structured Field (WSF) (0xF3) | Structured Field | ... | Structured Field
    
  Inbound Data Stream from Device to the Application
  
    Attention Identifier (AID) | Cursor Address | Data
    
    AID (X'88') | Structured Field | ... | Structured Field
    
  Structured Field
  
    <word: length> <id> <information>
    
  Character Buffer
  
    o field attributes
      o is a an invisible character
      o defines the start of a field and its characteristics
        o protected/unprotected
        o alphanumeric(numeric
        o autoskip
        o nondisplay/display/intensified display
        o detectable/nondetectable (by cursor movement)
    o extended field attributes
      o associated with a field attribute
      o color, character set, field validation, field outlining, extended highlightning
    o character attributes
      o associated with a character
      o default/underscore/blink/reverse video
      o color
      o character set
      o mandatory entry (must be modified)
        mandatory fill (must not be empty)
        trigger (signal when leaving field)
      o default/outlining
      o transparency
    o partitions (screen can be devided into partitions)
    
  3270 Commands
  
    01 Write
    02 Read Buffer
    03 NOP
    05 EraseWrite
    06 Read Modified
    0d Erase/Write Alternate
    0e Read Modified All
    0f Erase All Unprotected
    11 Write Structured Field
  
  SNA 3270 Commands
  
    6E      | Read Modified All                        | 3.6.2.5           |
    6F      | Erase All Unprotected                    | 3.5.5             |
    7E      | Erase/Write Alternate                    | 3.5.3             |
    F1      | Write                                    | 3.5.1             |
    F2      | Read Buffer                              | 3.6.1.1           |
    F3      | Write Structured Field                   | 3.5.4             |
    F5      | Erase/Write                              | 3.5.2             |
    F6      | Read Modified                     

  Orders

    05      | Program Tab                              | 4.3.7             |
      go to the next unprotected field, if there is none, got to the upper, left corner
    08      | Graphic Escape                           | 4.3.10            |
    11      | Set Buffer Address
      0x11 <2byte buffer address>
    12      | Erase Unprotected To Address             | 4.3.9             |
    13      | Insert Cursor                            | 4.3.6             |
    1D      | Start Field                              | 4.3.1             |
    28      | Set Attribute                            | 4.3.4             |
    29      | Start Field Extended                     | 4.3.2             |
    2C      | Modify Field                             | 4.3.5             |
    3C      | Repeat To Address                        |
    
  Format Control Orders
  
    00      | Null                                     | 4.3.11 (all)      |
    0A      | New Line                                 |                   |
    0C      | Form Feed                                |                   |
    0D      | Carriage Return                          |                   |
    19      | End Of Medium                            |                   |
    1C      | Duplicate                                |                   |
    1E      | Field Mark                               |                   |
    3F      | Substitute                               |                   |
    FF      | Eight Ones

  Structured Fields (One-Byte ID)

    00      | Reset Partition                          | 5.21              |
    01      | Read Partition                           | 5.19              |
    03      | Erase/Reset                              | 5.9               |
    06      | Load Programmed Symbols                  | 5.13              |
    09      | Set Reply Mode                           | 5.30              |
    0B      | Set Window Origin                        | 5.31              |
    0C      | Create Partition                         | 5.7               |
    0D      | Destroy Partition                        | 5.8               |
    0E      | Activate Partition                       | 5.5               |
    40      | Outbound 3270DS                          | 5.16              |
    41      | SCS Data                                 | 5.23              |
    4A      | Select Format Group                      | 5.25              |
    4B      | Present Absolute Format                  | 5.17              |
    4C      | Present Relative Format                  | 5.18              |
    80      | Inbound 3270 DS

  Structured Fields (Two-Byte ID)

    0F01    | Set MSR Control                          | 5.27              |
    0F02    | Destination/Origin                       | 5.35              |
    0F04    | Select Color Table                       | 5.24              |
    0F05    | Load Color Table                         | 5.10              |
    0F07    | Load Line Type                           | 5.12              |
    0F08    | Set Partition Characteristics            | 5.28              |
    0F0A    | Modify Partition                         | 5.14              |
    0F0F    | Object Data                              | 5.37              |
    0F10    | Object Picture                           | 5.38              |
    0F11    | Object Control                           | 5.36              |
    0F1F    | OEM Data                                 | 5.39              |
    0F21    | Data Chain                               | 5.34              |
    0F22    | Exception/Status                         | 6.2               |
    0F24    | Load Format Storage                      | 5.11              |
    0F71    | Outbound Text Header                     | 5.15              |
    0F83    | Select IPDS Mode                         | 5.41              |
    0F84    | Set Printer Characteristics              | 5.29              |
    0F85    | Begin/End Of File                        | 5.6               |
    0FB1    | Inbound Text Header                      | 6.3               |
    0FC1    | Type 1 Text (Outbound)                   | 5.32              |
    0FC1    | Type 1 Text (Inbound)                    | 6.6               |
    81nn    | Query Reply                              | 6.8               |
      80    |   Summary                                | 6.48              |
      81    |   Usable Area                            | 6.51              |
      82    |   Image                                  | 6.30              |
      83    |   Text Partitions                        | 6.49              |
      84    |   Alphanumeric Partitions                | 6.9               |
      85    |   Character Sets                         | 6.12              |
      86    |   Color                                  | 6.13              |
      87    |   Highlighting                           | 6.28              |
      88    |   Reply Modes                            | 6.42              |
      8A    |   Field Validation                       | 6.23              |
      8B    |   MSR Control                            | 6.34              |
      8C    |   Field Outlining                        | 6.22              |
      8E    |   Partition Characteristics              | 6.38              |
      8F    |   OEM Auxiliary Device                   | 6.36              |
      90    |   Format Presentation                    | 6.24              |
      91    |   DBCS-Asia                              | 6.17              |
      92    |   Save/Restore Format                    | 6.44              |
      94    |   Format Storage Auxiliary Device        | 6.25              |
      95    |   Distributed Data Management            | 6.19              |
      96    |   Storage Pools                          | 6.47              |
      97    |   Document Interchange Arch.             | 6.20              |
      98    |   Data Chaining                          | 6.15              |
      99    |   Auxiliary Device                       | 6.10              |
      9A    |   3270 IPDS                              | 6.52              |
      9C    |   Product Defined Data Stream            | 6.41              |
      9D    |   Anomaly Implementation                 | D.2               |
      9E    |   IBM Auxiliary Device                   | 6.29              |
      9F    |   Begin/End Of File                      | 6.11              |
      A0    |   Device Characteristics                 | 6.18              |
      A1    |   RPQ Names                              | 6.43              |
      A2    |   Data Streams                           | 6.16              |
      A6    |   Implicit Partition                     | 6.31              |
      A7    |   Paper Feed Techniques                  | 6.37              |
      A8    |   Transparency                           | 6.50              |
      A9    |   Settable Printer Chars.                | 6.46              |
      AA    |   IOCA Auxiliary Device                  | 6.32              |
      AB    |   Cooperative Proc. Requestor            | 6.14              |
      B0    |   Segment                                | 6.45              |
      B1    |   Procedure                              | 6.40              |
      B2    |   Line Type                              | 6.33              |
      B3    |   Port                                   | 6.39              |
      B4    |   Graphic Color                          | 6.26              |
      B5    |   Extended Drawing Routine               | 6.21              |
      B6    |   Graphic Symbol Sets                    | 6.27              |
7      FF    |   Null                                   | 6.35              |
    1030    | Request Recovery Data                    | 5.20              |
    1031    | Recovery Data                            | 6.5               |
    1032    | Set Checkpoint Interval                  | 5.26              |
    1033    | Restart                                  | 5.22              |
    1034    | Save/Restore Format 

 SCS Control Codes

   04      | Vertical Channel Select (VCS)            | 8.8 (all)         |
   05      | Horizontal Tab (HT)                      |                   |
   08      | Graphic Escape (GE)                      |                   |
   0B      | Vertical Tab (VT)                        |                   |
   0C      | Form Feed (FF)                           |                   |
   0D      | Carriage Return (CR)                     |                   |
   14      | Enable Presentation (ENP)                |                   |
   15      | New Line (NL)                            |                   |
   16      | Backspace (BS)                           |                   |
   1E      | Interchange Record Separator (IRS)       |                   |
   24      | Inhibit Presentation (INP)               |                   |
   25      | Line Feed (LF)                           |                   |
   28      | Set Attribute (SA)                       |                   |
   2B      | Format (FMT)                             |                   |
   2BC1(L)(|)Set Horizontal Format (SHF)              |                   |
   2BC2(L)(|)Set Vertical Format (SVF)                |                   |
   2BC6(L)(|)Set Line Density (SLD)                   |                   |
   2BD1(L)8|(Set Text Orientation (STO)               |                   |
   2BD2(L)2|(Set Print Density (SPD)                  |                   |
   2BD2(L)4|(Page Presentation Media (PPM)            |                   |
   2BFE(L)3|(ASCII Transparent (ATRN)                 |                   |
   2F      | Bell (BEL)                               |                   |
   35(L)(P)| Transparent (TRN)                        |                   |

   Note:                                                                  |
   L = length                                                             |
   P = parameters      
*/

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

#if 1
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

sleep(10);

#if 1
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

//---------------------------------------------------------------------------
//
// In3270 stuff
//
//---------------------------------------------------------------------------

// 0000 7d 5b f5 11 5a 5f 94 81 99 92 11 5b 6f 87 85 88 '$5.!¬mark.$?geh
// 0010 85 89 94                                        eim


class In3270
{
  public:
    In3270(const char *p, size_t s) {
      ptr = reinterpret_cast<const unsigned char*>(p);
      n = s;
//      ptr += 5;
//      n -= 5;
      aid = *ptr;
      ++ptr;
      --n;
      fetchCursor();
      state = 0;
    }
    
    unsigned aid, x, y;
    string data;
    
    bool parse();
    
    void fetchCursor();
    unsigned state;
    const unsigned char *ptr;
    size_t n;
};

bool
In3270::parse()
{
  if (n==0)
    return false;
  switch(aid) {
    case AID_SF: // AID (X'88') | Structured Field | ... | Structured Field
      printf("SF ");
      break;
    case AID_ENTER: // Attention Identifier (AID) | Cursor Address | Data
      printf("ENTER ");
      ++ptr; // skip 0x11
      --n;
      fetchCursor();
      data.clear();
      while(*ptr >= 0x40 && *ptr < 0xFF && n>0) {
        data += ebcdic2utf8(*ptr);
        ++ptr;
        --n;
      }
      break;
    default:
      printf("Unhandled AID %u ", aid);
      return false;
  }
  return true;
}

void
In3270::fetchCursor()
{
  unsigned c1 = *ptr;
  ++ptr;
  --n;
  unsigned c2 = *ptr;
  ++ptr;
  --n;
  unsigned addr = (((c1 & 0xC0) == 0x00) ?
                    ((c1 & 0x3F) << 8) | c2 :
                    ((c1 & 0x3F) << 6) | (c2 & 0x3F));
  x = addr % cols;
  y = addr / cols;
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
