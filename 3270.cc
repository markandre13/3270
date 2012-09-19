#include <map>
#include <cassert>

#include "3270.hh"

using namespace std;

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
// Bracket, CP 037 Modified (x3270 default to this for some reason)
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

//---------------------------------------------------------------------------
//
// In3270 stuff
//
//---------------------------------------------------------------------------

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

