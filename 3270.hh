#include <string>

using namespace std;

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
    void write();


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

