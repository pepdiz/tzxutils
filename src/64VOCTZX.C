/******************************************************************************
/ VOC (or 64 TAP) to TZX converter
/                                                                       v0.08b
/ (c) 1998 Tomaz Kac
/          Markus Brenner
/          Ben Castricum
/
/ BiiG Thanx to following people: Richard Storer, Sami Silaste, Hakan Sundell,
/                                 Thomas Whitek, Martijn v.d. Heide and
/                                 everyone else who contributed files to test!
/
/******************************************************************************/

//#define unix       /* Undefine this if you are compiling under WATCOM for DOS */
	
#ifdef  unix

#define O_BINARY 0
#define STR_CHECK_OK  "+"
#define STR_CHECK_NOT "-"

#else

#define STR_CHECK_OK  "û"
#define STR_CHECK_NOT "ù"

#endif        

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void Error(unsigned char *errstr) 
{
  /* exits with an error message *errstr */
  printf("\n-- Error: %s\n", errstr);
  exit(0);
}

#define BUFLEN                  16384           /* Buffer length */

/* Data Endianess defines (some loaders load bits in the opposite direction) */

#define MSB                     1
#define LSB                     0

/* WAVE lengths for various loaders */

#define ROM_S_HALF              616             /* ROM Loader SHORT  Half Wave = 616 */
#define ROM_M_HALF              896             /* ROM Loader MEDIUM Half Wave = 896 */
#define ROM_L_HALF              1176            /* ROM Loader LONG   Half Wave = 1176 */

#define ROM_S                   ROM_S_HALF *2   /* ROM Loader SHORT  Wave */
#define ROM_M                   ROM_M_HALF *2   /* ROM Loader MEDIUM Wave */
#define ROM_L                   ROM_L_HALF *2   /* ROM Loader LONG   Wave */

#define STT_0_HALF              426             /* Standard Turbo Tape BIT 0 Half Wave */
#define STT_1_HALF              596             /* Standard Turbo Tape BIT 1 Half Wave */

#define STT_0                   STT_0_HALF *2   /* Standard Turbo Tape BIT 0 Wave */
#define STT_1                   STT_1_HALF *2   /* Standard Turbo Tape BIT 1 Wave */

#define G1_0_HALF               511             /* Game Standard 1 BIT 0 Half Wave = 511 */
#define G1_1_HALF               923             /* Game Standard 1 BIT 1 Half Wave = 923 */

#define G1_0                    G1_0_HALF *2    /* Game Standard 1 BIT 0 Wave */
#define G1_1                    G1_1_HALF *2    /* Game Standard 1 BIT 1 Wave */

#define JOE_0_HALF              682             /* Players 1 BIT 0 Half Wave = 682 */
#define JOE_1_HALF              1008            /* Players 1 BIT 1 Half Wave = 1008 */

#define JOE_0                   JOE_0_HALF *2   /* Players 1 BIT 0 Wave */
#define JOE_1                   JOE_1_HALF *2   /* Players 1 BIT 1 Wave */

#define NOVA_0_HALF             511             /* NovaLoad 1 BIT 0 Half Wave */
#define NOVA_1_HALF             1222            /* NovaLoad 1 BIT 1 Half Wave */

#define NOVA_0                  NOVA_0_HALF *2  /* NovaLoad 1 BIT 0 Wave */
#define NOVA_1                  NOVA_1_HALF *2  /* NovaLoad 1 BIT 1 Wave */

#define MAST_0_HALF             539             /* Mastertronic 1 BIT 0 Half Wave */
#define MAST_1_HALF             1165            /* Mastertronic 1 BIT 1 Half Wave */

#define MAST_0                  MAST_0_HALF *2  /* Mastertronic 1 BIT 0 Wave */
#define MAST_1                  MAST_1_HALF *2  /* Mastertronic 1 BIT 1 Wave */

#define KET_0                   384*2			/* Alligata's Kettle */
#define KET_1                   526*2

#define RAIN_0                  767*2           /* Rainbird Jewels of Darkness */
#define RAIN_1                  1009*2          /* Rainbird Jewels of Darkness */

#define ZOI_0                   STT_0           /* Zoids Loader */
#define ZOI_1                   STT_1

/* Definitions for determining various loader types */

#define TYPE_ROM                0               /* ROM Loader */
#define TYPE_STT                1               /* Standard Turbo Tape Loader */
#define TYPE_G180               2               /* 180 Loader */
#define TYPE_G720               3               /* 720 Degress Loader */
#define TYPE_ACE                4               /* Ace of Aces Loader */
#define TYPE_ERE                5               /* Erebus Loader */
#define TYPE_JOE                6               /* Players Loader */
#define TYPE_NOVA               7               /* NovaLoad */
#define TYPE_OCN                8               /* Ocean/Imagine Loader */
#define TYPE_MAST               9               /* Mastertronic Loader */
#define TYPE_KET               10               /* Kettle Loader */
#define TYPE_RAIN              11               /* Rainbird Loader */
#define TYPE_ZOI               12               /* Kettle Loader */

/* Special Cases */

#define TYPE_NONE               254             /* NO Loader */
#define TYPE_ALL                255             /* ALL Loaders */

/* Tresholds between Waves for various parts of loaders */

#define TRES_LEAD               30              /* Leader Tone treshold for ROM type */

/* Default Minimal/Maximal values */

#define MIN_TURBO_PILOT         5               /* Minimal Turbo Tape Pilot Lead-In Bytes */
#define MIN_NOVA_PILOT          1000            /* Minimal NovaLoad Pilot Tone Waves */
#define MIN_OCN_PILOT           3000            /* Minimal Ocean/Imagine Pilot Tone Waves */
int     MIN_ROM_PILOT =         79;             /* Minimal ROM Pilot Tone Waves = 79 */

int fh;                                         /* Input File Handle */
int ofh;                                        /* Output File Handle */
int tfh;                                        /* Temporary File Handle */
int flen;                                       /* File Length */
int files=0;                                    /* Number of Files on the command line */
unsigned char finp[255];                        /* Input File  (First Command Line Option) */
unsigned char fout[255];                        /* Output File (Second Command Line Option or First with .VOC) */
unsigned char ftmp[255];                        /* Temporary File name */
unsigned char errstr[255];                      /* Error String */
int tap=0;                                      /* tap=0   TAP file */
                                                /*     1   VOC file */
unsigned char *raw;                             /* Address of the loaded .VOC or .TAP file (converted to RAW) */
int rawlen;                                     /* Length of the .VOC or .TAP file in the memory */
int rate;                                       /* Sample Rate */
int silence=0;                                  /* Are Silence blocks in the .VOC file ? */
int Treshold=127;                               /* Treshold value for recognition */
double cycle;                                   /* How much Z80 cycles is one Sample (byte) */

int cur=0;                                      /* Current position in the sample */
int last;                                       /* Position of last Pulse in the sample */
int wav=0;                                      /* Current Wave length */
int lwav=0;                                     /* Last Wave length */
int dataleft=1;                                 /* Is there some data left to process ? */
double pause=0.0;                               /* Pause length between blocks */
int bytechecksum=1;                             /* Is the checksum of all BYTES in the ROM Data OK ? */
int pilot;                                      /* Number of Pilot waves */
double total;                                   /* Total length of Pilot */
int pos;                                        /* Position in the converted data */
int block=0;                                    /* Number of blocks recognised */
int lst_pilot=0;                                /* Last pilot length ... */
int bittresh;                                   /* Treshold value for Turbo Tape bit recognition */
int nextdatalen;                                /* Length of the Next Data Block in Turbo Tape */
unsigned char turboid;                          /* ID of the current Turbo Tape block */
int overflow=0;                                 /* Too big wave read from the tape ? */
int blocklen;                                   /* Players Loader block Length & End address */
int blockend;
int trailing;                                   /* Number of Trailing Tone Waves */

int rom=4;                                      /* Number of ROM Loading blocks at the beginning */
int stt=0;                                      /* Standard Turbo Tape blocks */
int g180=0;                                     /* 180 blocks */
int g720=0;                                     /* 720 Degress blocks */
int ace=0;                                      /* Ace of Aces blocks */
int ere=0;                                      /* Erebus blocks */
int joe=0;                                      /* Number of Joe Blade blocks */
int nova=0;                                     /* NovaLoad blocks */
int ocn=0;                                      /* Ocean/Imagine blocks */
int mast=0;                                     /* Mastertronic blocks */
int ket=0;                                      /* Kettle blocks */
int rain=0;                                     /* Rainbird blocks */
int zoi=0;                                      /* Zoids blocks */

unsigned char data[65536*2];                    /* The converted data for each block (I hope 128k is enough for starters) */

int n;
int len;
int wlen;
unsigned char clen;
unsigned char buf[BUFLEN];
int bufpos;										/* Position in the buffer */
int GotData=0;									/* Do we have some Data ? */
int DataType;									/* Type of the block - loader */
int nextblock, previousblock;					/* What kind of loader was previous, next block */
int SyncOK, DataOK, PilotOK;					/* Did we get some part of the block */
double tot_bit0, tot_bit1;						/* Not used now */
int num_bit0, num_bit1;							/*     -||-     */
unsigned char novacheck[2];						/* Checksum for NovaLoader */
int num_blocks;									/* Number of Sub-Blocks in some loaders */
int zero=2550;									/* Default value for 0x00 in .TAP files */
unsigned char addbit_value;						/* Value of Additional Bit that was read */
unsigned char sync_read;						/* Sync value read from the tape */
int bits_in_byte;								/* Number of bits in byte that was read */
/* Mastertronic Loader starting values - change almost every block ! */
int mast_bits=1;								/* Number of additional bits for Mastertronic loader */
int mast_header=0;								/* Number of additional header bits for Mastertronic */
int mast_endian=MSB;							/* Endianess of data for Mastertronic */
int mast_leadin=1;								/* Read lead-in tone for Mastertronic ? */
int old_mast_bits;
int old_mast_endian;
int old_mast_header;
int old_mast_leadin;
int mast_combined=0;							/* Combined Length of Mastertronic blocks */

int tap_version = 0;

unsigned char tzxbuf[10]={ 'Z','X','T','a','p','e','!', 0x1A, 1, 13 };
unsigned char c64buf[5]={ 0x33, 1, 0, 0x1C, 1 };

unsigned char headerless[26]="---------------------";

void Put1(unsigned char *s, int n) { s[0]=(unsigned char) n&0xff; }
void Put2(unsigned char *s, int n) { s[0]=(unsigned char) n&0xff; n>>=8; s[1]=(unsigned char) n&0xff; }
void Put3(unsigned char *s, int n) { s[0]=(unsigned char) n&0xff; n>>=8; s[1]=(unsigned char) n&0xff; n>>=8;
                                     s[2]=(unsigned char) n&0xff; }
void Put4(unsigned char *s, int n) { s[0]=(unsigned char) n&0xff; n>>=8; s[1]=(unsigned char) n&0xff; n>>=8;
                                     s[2]=(unsigned char) n&0xff; n>>=8; s[3]=(unsigned char) n&0xff; n>>=8; }
int Get2(unsigned char *mem) { return(mem[0] + (mem[1] * 256)); }
int Get3(unsigned char *mem) { return(mem[0] + (mem[1] * 256) + (mem[2] * 256 * 256)); }
int Get4(unsigned char *mem) { return(mem[0] + (mem[1] * 256) + (mem[2] * 256 * 256) + (mem[3] * 256 * 256 * 256)); }

int getnumber(unsigned char *s)
{
  /* Returns the INT number contained in string *s */
  int i;

  sscanf(s,"%d",&i); return(i);
}

int FileLength(int fh)
{
  int curpos, size;

  curpos = lseek(fh, 0, SEEK_CUR);
  size = lseek(fh, 0, SEEK_END);
  lseek(fh, curpos, SEEK_SET);
  return(size);
}

void ChangeFileExtension(unsigned char *str,unsigned char *ext)
{
  /* Changes the File Extension of String *str to *ext */
  int n,m;

  n=strlen(str);
  while (str[n]!='.') n--;
  n++; str[n]=0;
  strcat(str,ext);
}

int GetFileType(unsigned char *str)
{
  /* Changes the File Extension of String *str to *ext */
  int n,m;
  
  n=strlen(str); while (str[n]!='.') n--;
  n++;
  if (str[n] == 'T' || str[n] == 't') return(1);
  else                                return(0);
}

void invalidoption(unsigned char *s)
{
  /* Prints the Invalid Option error */

  sprintf(errstr,"Invalid Option %s !",s);
  error(errstr);
}

void CopyString(unsigned char *dest, unsigned char *sour, int len)
{
  /* Could just use strpy ... */
  int n;

  for (n=0; n<len; n++) dest[n]=sour[n]; dest[n]=0;
}

int ReadVOCType(int fh, int *len, int *f)
{
  /* Reads the TYPE block from the VOC file... skips everything but DATA blocks
     Returns 0 if everything is OK and 1 if there was an unexpected End-Of-File */
  int l;
  unsigned char b;
  unsigned char buff[256];
  int t;

  do
    {
      if (!read(fh,&b,1)) return(1);
      if (!read(fh,buff,3)) return(1);
      *len=((int) (buff[0]) + ((int) (buff[1])*256) + ((int) (buff[2])*256*256));
      switch(b)
	{
	case 0x00:
	  *len=0;
	  break;
	case 0x01: 
	  *len-=2;
	  if (!read(fh,buff,1)) return(1);
	  *f=(int) (1000000/(256-(int) buff[0]));
	  if (!read(fh,buff,1)) return(1);       /* Any compression type... I don't care ;) */
	  break;
	case 0x02: 
	  b=0x01;
	  break;
	case 0x03: 
	  silence=1;
	case 0x09: 
	  *len-=12;
	  /* Get the frequency from the 'special' Frequency block!!! */
	  if (!read(fh,buff,4)) return(1);
	  *f=Get4(buff);
	  if (!read(fh,buff,8)) return(1);
	  break;
	default:  
	  if (*len) lseek(fh, *len, SEEK_CUR); break;
	}
    } while (b != 0x01 && b != 0x00 && b != 0x09);
  return(0);
}

int LoadVOC(unsigned char *filename)
{
/*      Loads a .VOC file to the memory */
/*      Input   : filename */
/*      Outputs : raw    - address of the loaded file */
/*                rawlen - length of the loaded file */
/*                rate   - sample rate of the file */
/*      Returns : 0 - when everything went OK */
/*                1 - when input file is not found */
/*                2 - when there is not enough memory to load file */
/*                3 - when the file is not the right type or corrupt */
  int fh;
  int flen;
  int len;
  int plen;
  unsigned char buf[256];
  int p=0;
  unsigned char dummy;
  int idummy;
  int ee=0;

  if ((fh=open(filename,O_RDONLY | O_BINARY)) == -1) return(1);
  flen=FileLength(fh);
  raw=(unsigned char *) malloc(flen);
  if (raw == NULL) { close(fh); return(2); }
  if (!read(fh,buf,19)) { close(fh); return(3); } buf[19]=0;
  if (strcmp(buf,"Creative Voice File")) { close(fh); return(3); }
  read(fh,buf,7);
  if (ReadVOCType(fh, &len, &rate)) { close(fh); return(3); }
  printf("\nSampling Rate: %5i Hz\n",rate);
  printf("Loading File (%d bytes) ...\n",flen);
  while (len && ee==0)
    {
      plen=read(fh,raw+p,len); p+=plen;
      ee=ReadVOCType(fh,&len,&idummy);
    }
  /* if (ee) printf("-- Warning: Unexpected End-Of-File !\n"); */
  if (silence) printf("-- Warning: Silence blocks in file (Pauses will be wrong) !\n");
  printf("Actual RAW size of file in memory: %d bytes\n",p);
  close(fh);
  rawlen=p;
  return(0);
}

int LoadTAP(unsigned char *filename)
{
  /* Loads a .TAP file to the memory 
     Input   : filename 
     Outputs : raw    - address of the loaded file 
               rawlen - length of the loaded file 
     Returns : 0 - when everything went OK 
               1 - when input file is not found 
               2 - when there is not enough memory to load file 
               3 - when the file is not the right type or corrupt */
  int fh;
  int flen;
  int len;
  int plen;
  unsigned char buf[256];
  int p=0;
  unsigned char dummy;
  int idummy;
  int ee=0;
  
  if ((fh=open(filename,O_RDONLY | O_BINARY))==-1) return(1);
  flen=FileLength(fh);
  raw=(unsigned char *) malloc(flen);
  if (raw==NULL) { close(fh); return(2); }
  if (!read(fh,buf,12+4+4)) { close(fh); return(3); }
  tap_version = (int) buf[12];
  buf[12]=0; /* 'C64-TAPE-RAW' + 4 padded bytes + Data length */
  if (strcmp(buf,"C64-TAPE-RAW")) { close(fh); return(3); }
  /* flen=Get4(mem+12+4);   This doesn't seem to work always ... get data from filelen */
  flen-=12+4+4;
  p=read(fh,raw,flen);
  printf("\nLoading File (%d bytes) ...\n",flen);
  printf("\nTAP File Version = %i\n", tap_version);
  close(fh);
  rawlen=p;
  return(0);
}

int LoadFile(unsigned char* filename)
{
  if (tap)
    return (LoadTAP(filename));
  else
    return (LoadVOC(filename));
}

/******************************************************************************/
/* Generic Functions                                                          */
/******************************************************************************/

int Diff(int a,int b)
{
  /* Returns the Difference (in %) between a and b (a+(a*(d/100))=b) d=diff */
  int d;
  float aa,bb;
  
  if (!a && !b) return(0);
  if (a>b) { aa=(float) a; bb=(float) b;}
  else     { aa=(float) b; bb=(float) a;}
  return (abs((int) (0.5+(100.0*((bb-aa)/aa)))));
}

int GetWave()
{
  /* Finds next Wave and returns its length */
  int real;
  int pv1;

  if (tap)
    {
      last=cur;
      cur++;
      if (cur == rawlen) dataleft=0;
      real=raw[cur];
      if (real == 0)
      {
      	if (tap_version == 1)
      	{
          pv1=raw[cur+1]+256*raw[cur+2]+256*256*raw[cur+3];
          real=(pv1)*3.5524;
//          printf("\nDEBUG: Pause found: %i tap %x,%x,%x real: %i \n",pv1,raw[cur+1],raw[cur+2],raw[cur+3], real);
          cur+=3;      
          return real;
      	  
      	}
      	else
      	{
      	  real = ((zero<<3)*3.5524);
    	}
      }
      return ((real<<3)*3.5524);
    }
  else
    {
      last=cur;   /* This might be just the opposite !!! (this is for LOW->HIGH) */
      while (raw[cur] <= Treshold && cur < rawlen) cur++;
      while (raw[cur] > Treshold && cur < rawlen) cur++;
      if (cur == rawlen) dataleft=0;
      return ((cur-last)/cycle + 0.5);
    }
}

unsigned char MirrorByte(unsigned char s)
{
  return((s<<7)+((s<<5)&64)+((s<<3)&32)+((s<<1)&16)+((s>>1)&8)+((s>>3)&4)+((s>>5)&2)+(s>>7));
}

/******************************************************************************/
/* Save the blocks to TZX file                                                */
/******************************************************************************/

void SaveROMData()
{
  int n;
  unsigned char buf[256];
  unsigned char *t;

  t=buf;
  Put1(t, 0x16); t+=1;                                          /* ID 16 (C64 ROM Data Block) */
  Put4(t, 36+pos); t+=4;                                        /* Total Length */

  Put2(t, ROM_S_HALF); t+=2; Put2(t, lst_pilot); t+=2;          /* PILOT Pulse, Len */
  Put2(t, ROM_L_HALF); t+=2; Put2(t, ROM_M_HALF); t+=2;         /* SYNC Pulses */
  Put2(t, ROM_S_HALF); t+=2; Put2(t, ROM_M_HALF); t+=2;         /* ZERO Pulses */
  Put2(t, ROM_M_HALF); t+=2; Put2(t, ROM_S_HALF); t+=2;         /* ONE Pulses */
  Put1(t, 1); t+=1;                                             /* XOR Type */
  Put2(t, ROM_L_HALF); t+=2; Put2(t, ROM_M_HALF); t+=2;         /* FINISH BYTE Pulses */
  Put2(t, ROM_L_HALF); t+=2; Put2(t, ROM_S_HALF); t+=2;         /* FINISH DATA Pulses */
  Put2(t, ROM_S_HALF); t+=2; Put2(t, trailing); t+=2;           /* TRAILING TONE Pulse, Len */
  Put1(t, 8); t+=1;                                             /* Last Byte has 8 Bits */
  Put1(t, 0); t+=1;                                             /* Data Endianess = LSb first */
  Put2(t, pause/3500); t+=2;                                    /* Pause after block */
  Put3(t, pos); t+=3;                                           /* Data Length */

  write(ofh,buf,1+4+36);
  write(ofh,data,pos);
}

void SaveTurboTapeData(unsigned char pilotchar, int bit0, int bit1, int endian,
                       int trailing, unsigned char tbyte, unsigned char addbit)
{
  int n;
  unsigned char buf[256];
  unsigned char *t;

  t=buf;
  Put1(t, 0x17); t+=1;                                          /* ID 17 (C64 Turbo Tape Data) */
  Put4(t, 18+pos); t+=4;                                        /* Total Length */

  Put2(t, bit0>>1); t+=2;                                       /* ZERO Bit Pulse */
  Put2(t, bit1>>1); t+=2;                                       /* ONE  Bit Pulse */
  Put1(t, addbit); t+=1;                                        /* Additional Bit mode */
  Put2(t, lst_pilot); t+=2;                                     /* Number of Lead-In Bytes */
  Put1(t, pilotchar); t+=1;                                     /* Lead-In Byte Value */
  Put1(t, 8); t+=1;                                             /* Last Byte has 8 Bits */
  Put1(t, endian); t+=1;                                        /* Data Endianess */
  Put2(t, trailing); t+=2;                                      /* Number of Trailing Bytes */
  Put1(t, tbyte); t+=1;                                         /* Trailing Byte Value */

  Put2(t, pause/3500); t+=2;                                    /* Pause after block */
  Put3(t, pos); t+=3;                                           /* Data Length */

  write(ofh,buf,1+4+18);
  write(ofh,data,pos);
}

void SaveGroupEnd()
{
unsigned char buf[2];

buf[0]=0x22;
write(ofh,buf,1);
}

/******************************************************************************/
/* Get Pilot tones                                                            */
/******************************************************************************/

/* The next two Turbo Tape related functions are also used by Get DATA functions !!! */

int GetTurboTapeBit(int wav, int bit0, int bit1)
{
  /* Get one Bit - the bittresh MUST be calculated somewhen before this is called, also
     if overflow was detected the corresponding flag will be set ! */

  if (wav > (bit1<<1)) overflow=1;	/* End of data ? */
  if (wav > bittresh) return (1);       /* Bit 1 MUST be higher valued as Bit 0 !!! */
  else                return (0);
}

unsigned char GetTurboTapeByte(int bit0, int bit1, int *len, int endian, int lookover, int addbit)
{
  /* Get one Turbo Tape Byte... bit0, bit1 are timings, len is combined length of the byte in
     pulses, endian is MSB or LSB.
     If lookover is 1 then the check for OVERFLOW will be made and if the overflow in a bit is
                      detected then all other bits will be filled with 0 ! */
  int n=8;
  unsigned char rbyte=0;
  bits_in_byte=0;

  while (addbit)   /* Read additional bit at the BEGINNING of the byte */
    {
	  wav=GetWave();
	  *len+=wav;
	  if (GetTurboTapeBit(wav, bit0, bit1)) addbit_value = 1;
	  else                                  addbit_value = 0;
	  if (!dataleft) return (0);
	  addbit--;
    }
  *len=0;
  while (n)
    {
      if (endian) rbyte<<=1;
      else        rbyte>>=1;
      if (!(overflow && lookover))
        {
		  bits_in_byte++;
          wav=GetWave();
          *len+=wav;
          if (GetTurboTapeBit(wav, bit0, bit1))
            {
              if (endian) rbyte+=1;
              else        rbyte+=0x80;
            }
        }
      if (!dataleft) return (0);
      n--;
    }
  return (rbyte);
}

int GetTurboTapePilot(unsigned char pilotbyte, unsigned char syncbyte, int bit0, int bit1,
                      int endian, unsigned char addbit, int anysync, int min)
{
  /* This will try and recognise a Turbo Tape PILOT signal which consists
     of series (any number) of Bytes pilotbyte and it is ended with the
     syncbyte. The timings for bit 0 and 1 are in bit0 and bit1
	 addbit is 0 when there is no additional bit, 1 when it is in the beginning, 2 at the end

     Try to recognise all 8 bits of a byte ... (I am sure this is NOT the
     most optimised way of doing it ... but no time to oprimise the shit :)

     Turbo Tape uses MOST SIGNIFICANT BIT First method (oposite of LSB of
     ROM Loader !!! Which means bytes must be stored in the MSb fashion */

  int bitcount=8;                               /* Number of bits that still need to be recognised */
  int atpos=0;
  int n=0;
  double total=0;
  int len;
  unsigned char byteread;
  int start;
  
  bittresh = bit0 + ((bit1-bit0)/2);            /* Treshold value for recognition */
  /* The treshold is only calculated ONCE per block, so the conversion is faster ... All following
     Turbo Tape functions will use the value calculated here !!! */
  do
    {
      start=cur;  
      byteread=GetTurboTapeByte(bit0, bit1, &len, endian, 0, addbit);
      if (byteread != pilotbyte || (addbit && addbit_value == 0))
        {
          cur=start; wav=GetWave();
          pause+=wav;
        }
    } while ((byteread != pilotbyte || (addbit && addbit_value == 0)) && dataleft);
  
  if (!dataleft)
    {
      return (0);
    }
  total=len;
  pilot=1;                                      /* One lead-in byte already read before */
  while ((byteread=GetTurboTapeByte(bit0, bit1, &len, endian, 0, addbit)) == pilotbyte &&
         (!addbit || addbit_value == 1) && dataleft)
    {
      pilot++;
      total+=len;
    }
  if (addbit && addbit_value == 0) return (0);  /* Additional bit was not 1 when it was supposed */
  total+=len;
  if (!dataleft) return (0);
  sync_read=byteread;
  if ((!anysync && byteread != syncbyte) || pilot < min)
    {
      pause+=total;
      return (0);
    }
  return (1);
}

int GetContinousPilot(int p_len, int s_len, int min_len)
{
  /* Gets the Continous Pilot tone with Pulse length p_len until s_len...
     returns 1 if successfull p_len MUST be LOWER than s_len !!! */
  int diff;

  wav=0;
  do
    {
      lwav=wav; wav=GetWave();
      pause+=(double) wav;
    } while (Diff(wav, lwav) > TRES_LEAD && dataleft);
  if (dataleft)
    {
      /* Found the possible start of the pilot tone ... start counting the pilot waves */
      total=(double) (lwav+wav);                          /* Total length of the pilot */
      pause-=total;                                       /* Correct the pause value */
      pilot=1;                                            /* Number of pilot pulses */
      do
	{
	  lwav=wav; wav=GetWave();
	  pilot++; total+=(double) wav;
	} while (wav < (p_len+((s_len-p_len)/2)) && Diff(wav, lwav) <= TRES_LEAD && dataleft);
      total-=(double) wav;                                /* Correct the total length of the pilot */
      if(pilot < min_len)                   /* Not big enough ? */
	{
	  pause += total;                           /* Regard it as pause ... */
	  return (0);
	}
      if (dataleft)
	{
	  return (1);                               /* Succesfully converted the Pilot tone */
	}
    }
  return (0);
}

/******************************************************************************/
/* Get Sync waves                                                             */
/******************************************************************************/

int GetROMSync()
{
  /* Gets the ROM Sync values
     We already have first Sync Wave read ... read the second one
     The timings don't matter... we will write the exact ones */

  lwav=GetWave();
  if (dataleft) return (1);
  else          return (0);
}


/******************************************************************************/
/* Get DATA from different loaders                                            */
/******************************************************************************/

/* ROM Loader */

int GetROMBit()
{
  /* Get next bit value - simple calc :) */

  lwav=GetWave();
  if (dataleft) wav=GetWave();
  if (wav < lwav) return(1);
  else            return(0);
}

int GetROMEndMarker()
{
  /* Determines if End of DATA or End of BYTE Marker is present
     returns:   0 = End of DATA
                1 = End of BYTE */

  lwav=GetWave();
  if (dataleft) wav=GetWave();
  if (wav >= (ROM_S+((ROM_M-ROM_S)/2))) return (0);
  else                                  return (1);
}

int GetROMData()
{
  /* Get the Data saved by the C64 ROM SAVE routine ... */
  int bit;
  unsigned char xor, txor;
  int endofdata;
  int n;
  
  pos=0;                /* Position in the data (later it means the Length of the data */
  bytechecksum=1;       /* Checksum of each byte ? When this goes to 0 means there was an error */
  trailing=0;           /* Number of Trailing Tone waves */
  
  do
    {
      xor=1; data[pos]=0;
      for (n=0; n < 8 && dataleft; n++)
	{
	  int bit=GetROMBit();
	  data[pos]>>=1;
	  data[pos]+=bit<<7;
	  xor^=bit;
	}
      if (dataleft)
	{
	  txor=GetROMBit();
	  if (xor != txor)
	    {
	      bytechecksum=0;
	    }
	}
      else
	{
	  bytechecksum=0;
	  break;
	}
      if (dataleft)
	{
	  endofdata=GetROMEndMarker();
	}
      else
	{
	  break;
	}
      pos++;
    } while (!endofdata && dataleft);
  
  if (!(data[0]==0x89 && data[1]==0x88 && data[2]==0x87 && data[3]==0x86 &&
        data[4]==0x85 && data[5]==0x84 && data[6]==0x83 && data[7]==0x82 &&
        data[8]==0x81))	/* Did we just read a REPEATED Block - If yes then read Trailing Waves? */
    {
      wav=GetWave();		/* Get the first Trailing Wave */
      do
	{
	  lwav=wav; wav=GetWave();
	  trailing++;
	} while (Diff(wav, lwav) <= TRES_LEAD && dataleft && trailing < 79);
      /* Read until different Wave is found or 79 Trailing Waves is exceeded ... */
      cur=last;	/* Go back one wave so the different wave can be read again */
    }
  
  if (pos) return (1);
  else     return (0);
}

/* Turbo Tape Type Loaders */

int GetNumberOfTurboTapeBytes(int count, int bit0, int bit1, int *len, int endian, double *total, int addbit)
{
  unsigned char br;
  
  bittresh = bit0 + ((bit1-bit0)/2);            /* Treshold value for recognition */
  
  overflow=0;  
  for (n=0; n < count && !overflow; n++)
    {
      br=GetTurboTapeByte(bit0, bit1, len, endian, 1, addbit);
      data[pos]=br;
      pos++;
      *total+=*len;
      if (!dataleft)
	{
	  return (n);
	}
    }
  return (n);
}  

int GetStandardTurboTapeData()
{
  int n;
  int br;
  int len;
  double total=0;

  pos=1;

  data[0]=0x09; /* We already read this one - SYNC Byte */
  
  for (n=0x08; n >= 0x01; n--)
    {
      br=GetTurboTapeByte(STT_0, STT_1, &len, MSB, 0, 0);
      data[pos]=br;
      pos++;
      total+=len;
      if (br != n || !dataleft)  /* Check if this is really a Turbo Tape block - countdown at start */
	{
	  break;
	}
    }
  if (n != 0x00)
    {
      pause+=total;
      return (0);
    }
  turboid=GetTurboTapeByte(STT_0, STT_1, &len, MSB, 0, 0); /* Get The ID byte */
  data[pos]=turboid;
  pos++;
  if (turboid != 0x00)          /* Header ! */
    {
      if (!GetNumberOfTurboTapeBytes(4, STT_0, STT_1, &len, MSB, &total, 0)) return (0);
      nextdatalen=Get2(&data[pos-2])-Get2(&data[pos-4])+1;
      if (!GetNumberOfTurboTapeBytes(32-14, STT_0, STT_1, &len, MSB, &total, 0)) return (0);
    }
  else
    {
      if (!GetNumberOfTurboTapeBytes(nextdatalen, STT_0, STT_1, &len, MSB, &total, 0)) return (0);
    }
  return (1);
}

int Get180Data()
{
  int len;
  double total=0;
  int blocklen;

  pos=1;

  data[0]=0x5A;                /* The SYNC Byte was already read from the tape */

  if (!GetNumberOfTurboTapeBytes(4, G1_0, G1_1, &len, MSB, &total, 0)) return (0);
  blocklen=Get2(&data[3])-Get2(&data[1])+1;
  if (!GetNumberOfTurboTapeBytes(blocklen, G1_0, G1_1, &len, MSB, &total, 0)) return (0);
  
  return (1);
}

int Get720Data()
{
  int len;
  double total=0;
  int blocklen;

  pos=1;

  data[0]=0xFF;                /* The SYNC Byte was already read from the tape */

  if (!GetNumberOfTurboTapeBytes(16+4, G1_0, G1_1, &len, MSB, &total, 0)) return (0);
  blocklen=Get2(&data[3+16])-Get2(&data[1+16])+1;
  if (!GetNumberOfTurboTapeBytes(blocklen, G1_0, G1_1, &len, MSB, &total, 0)) return (0);
  
  return (1);
}

int GetAceData()
{
  int len;
  double total=0;
  int blocklen;

  pos=1;

  data[0]=0xFF;                /* The SYNC Byte was already read from the tape */

  if (!GetNumberOfTurboTapeBytes(4, G1_0, G1_1, &len, MSB, &total, 0)) return (0);
  blocklen=Get2(&data[3])-Get2(&data[1])+1+16;
  if (!GetNumberOfTurboTapeBytes(blocklen, G1_0, G1_1, &len, MSB, &total, 0)) return (0);
  
  return (1);
}

int GetErebusData()  // DOESN'T WORK YET !!! Strange stuff here ...
{
  int len;
  double total=0;
  int blocklen;

  pos=1;

  data[0]=0x50;                /* The SYNC Byte was already read from the tape */

  if (!GetNumberOfTurboTapeBytes(5, G1_0, G1_1, &len, MSB, &total, 0)) return (0);
  blocklen=Get2(&data[3+1])-Get2(&data[1+1])+1+2;
  if (!GetNumberOfTurboTapeBytes(blocklen, G1_0, G1_1, &len, MSB, &total, 0)) return (0);
  
  return (1);
}

int GetJoeData()
{
  int len;
  double total=0;
  int got;

  if (joe == 1)
    {
      pos=1;
      data[0]=0x0A;                /* The SYNC Byte was already read from the tape */
      if (!GetNumberOfTurboTapeBytes(13, JOE_0, JOE_1, &len, LSB, &total, 0)) return (0);
      if (data[1] != 0x09 || data[2] != 0x08 || data[3] != 0x07 || data[4] != 0x06 || data[5] != 0x05 ||
	  data[6] != 0x04 || data[7] != 0x03 || data[8] != 0x02 || data[9] != 0x01)
	{
	  pause+=total;
	  return (0);
	}
      blockend=Get2(&data[10]);
      blocklen=Get2(&data[12]);
    }
  else
    {
      pos=0;
      got=GetNumberOfTurboTapeBytes(4, JOE_0, JOE_1, &len, LSB, &total, 0);
      if (got < 4) return (0);
      blockend=Get2(&data[0]);
      blocklen=Get2(&data[2]);
    }  
  joe++;
  if (!GetNumberOfTurboTapeBytes(blocklen+2, JOE_0, JOE_1, &len, LSB, &total, 0)) return (0);
  return (1);
}

int GetNovaLoadData()
{
  int n;
  int len;
  double total=0;
  int got;
  int remain;
  int get;
  int oldpos;
  
  int check=0;
  int check_ok=1;
  
  unsigned char loadadr=0xFF;
  unsigned char nextend=0xF0;

  pos=1;
  data[0]=0x80;	    /* 00000001 in LSB on tape - SYNC */
  num_blocks=0;
  
  if (!GetNumberOfTurboTapeBytes(2, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);
  if (data[1] != 0xAA) return (0);          /* FLAG must be 0xAA */
  if (data[2] == 0x55)       /* SPECIAL CASE !!! FIRST Block of most NOVALOADS */
    {
      if (!GetNumberOfTurboTapeBytes(1, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);         /* Get the Load addres High Byte */
      check=loadadr=data[pos-1];
      while(loadadr != nextend)
	  {
	    if (!GetNumberOfTurboTapeBytes(256, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);
	    num_blocks++;
	    for (n=0; n < 256; n++) check=(check+data[pos-n-1])&0xff;
	    if (!GetNumberOfTurboTapeBytes(1, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);
	    if (check != data[pos-1]) check_ok=0;
	    if (!GetNumberOfTurboTapeBytes(1, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);   /* Get the Load address High Byte */
	    check=loadadr=data[pos-1];
	    if (loadadr == 0xF0) nextend=0x00;                                 /* When 0xF0 address has encountered wait for 0x00 */
	  }
    }
  else                       /* NORMAL BLOCKS */
    {
	  /* Check if we have a filename and if we do then load it ... */
	  if (data[2])
	  	if (!GetNumberOfTurboTapeBytes(data[2], NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);              /* Get the filename */
      /* Get the Load address (minus 0x100), End Address, Length (plus 0x100) */
      if (!GetNumberOfTurboTapeBytes(6, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0); 
      remain=Get2(&data[pos-2])-0x100;
      for (n=2; n < pos; n++) check=(check+data[n])&0xff;                                         /* Calculate Checksum So far */
      while (remain)
	  {
	    if (!GetNumberOfTurboTapeBytes(1, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);                /* Get the Checksum */
	    if (check != data[pos-1]) check_ok=0;
	    check=(check+data[pos-1])&0xff;
	    if (remain>=256) get=256;
        else             get=remain;
	    if (!GetNumberOfTurboTapeBytes(get, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);            /* Get the next data */
	    num_blocks++;
	    for (n=0; n < get; n++) check=(check+data[pos-n-1])&0xff;                                 /* Calculate its checksum */
	    remain-=get;
	  }
      if (!GetNumberOfTurboTapeBytes(1, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);             /* Get the final checksum */
      if (check != data[pos-1]) check_ok=0;
    }
  /* Get the TRAILING Tone Bytes now !!! */
  oldpos=pos; /* Remember the length of the data without the trailing tone */
  trailing=GetNumberOfTurboTapeBytes(999999, NOVA_0, NOVA_1, &len, LSB, &total, 0);
  pos=oldpos; /* Set the correct data length back ... */

  if (check_ok == 0) strcpy(novacheck,STR_CHECK_NOT);
  else               strcpy(novacheck,STR_CHECK_OK);
  return (1);
}

int GetOceanData()
{
  int len;
  double total=0;
  unsigned char adr=0xFF;
  int oldpilot;
  unsigned char end_adr;

  pos=1;  
  data[0]=0x80;	         /* 00000001 in LSB on tape - SYNC */
  num_blocks=0;

  if (ocn > 1)           /* With Ocean Loaders 2,3 Get the FLAG Byte 0xAA */
  {
    if (!GetNumberOfTurboTapeBytes(1, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);
    if (data[pos-1] != 0xAA) return (0);
  }
  if (ocn == 3)
  {
    end_adr=0x02;       /* Ocean Loader 3 has End_Address set to 0x02 */
  }
  else
  {
    end_adr=0x00;       /* Ocean Loaders 1 & 2 have End_Address set to 0x00 */
  }
  while (adr != end_adr)
    {
      if (!GetNumberOfTurboTapeBytes(2, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);
      adr = data[pos-1];
	  if (adr != end_adr)
	    {
        if (!GetNumberOfTurboTapeBytes(256, NOVA_0, NOVA_1, &len, LSB, &total, 0)) return (0);
		num_blocks++;
		}
    }
  oldpilot=pilot;
  GetContinousPilot(NOVA_0, NOVA_1, 0);
  trailing=pilot>>3;
  pilot=oldpilot;
  return (1);
}

int GetMastertronicData()
{
  int len;
  double total=0;
  int blockend;
  int blockstart;
  int blocklen;
  unsigned char buf[256];

  if (num_blocks == 0)
  {
	// Save the GROUP Name
	buf[0]=0x21;
	buf[1]=18;
	sprintf(&buf[2],"Mastertronic Block");
	write(ofh,buf,20);
  }

  old_mast_bits=mast_bits;
  old_mast_endian=mast_endian;
  old_mast_header=mast_header;
  old_mast_leadin=mast_leadin;

  pos=0;
  if (mast_leadin)
  {
    pos=1;
    data[0]=sync_read;                /* The SYNC Byte was already read from the tape */
  }
  overflow=0;
  mast_leadin = 0;
  if (mast_header) { if (!GetNumberOfTurboTapeBytes(mast_header, MAST_0, MAST_1, &len, mast_endian, &total, mast_bits)) return (0); }
  if (!GetNumberOfTurboTapeBytes(5, MAST_0, MAST_1, &len, mast_endian, &total, mast_bits)) return (0);
  blockend  =data[pos-5]*256+data[pos-4];
  blockstart=data[pos-3]*256+data[pos-2];
  blocklen=blockend-blockstart;
  switch (blockstart)
    {
    case 0x34b: mast_bits = data[pos-1]-8;
                break;
  	case 0x3a4: mast_header	= data[pos-1]-3;
  	            break;	 
  	case 0x347: if (data[pos-1] == 0x26) mast_endian = MSB;
  	            else                     mast_endian = LSB;
  				break;
  	case 0x3bb: mast_leadin = 1;
  	            break;
    }
//  printf("E: %04X S: %04X L: %04X B: %02X H: %02X E: %02X L: %02X S: %02X P: %3d ",
//         blockend, blockstart, blocklen, old_mast_bits, old_mast_header, old_mast_endian, old_mast_leadin, sync_read, pilot);
  if (blocklen-1 > 0) { if (!GetNumberOfTurboTapeBytes(blocklen-1, MAST_0, MAST_1, &len, mast_endian, &total, mast_bits)) return (0); }
  num_blocks++;
  mast_combined+=pos;
  return (1);
}


int GetKettleData(int tick_0, int tick_1)
{
  int n;
  int br;
  int len;
  double total=0;
  int blocklen;

  pos=1;

  data[0]=0x64;                /* The SYNC Byte was already read from the tape */

  for (n=0x65; n <= 0xff; n++)
  {
    br=GetTurboTapeByte(tick_0, tick_1, &len, MSB, 0, 0);
    data[pos]=br;
    pos++;
    total+=len;
    if (br != n || !dataleft)  /* Check if this is really a Kettle block - 'countup' at start */
	{
	  break;
	}
  }
  if (n != 0x100)
  {
    pause+=total;
    return (0);
  }

  br=GetTurboTapeByte(tick_0, tick_1, &len, MSB, 0, 0);
  data[pos]=br;
  pos++;
  total+=len;
  if (br == 0)
  {
    pause+=total;
    return (0);
  }

  if (!GetNumberOfTurboTapeBytes(10, tick_0, tick_1, &len, MSB, &total, 0)) return (0);
  blocklen=Get2(&data[0x9f])-Get2(&data[0x9d])+1;
  if (!GetNumberOfTurboTapeBytes(blocklen, tick_0, tick_1, &len, MSB, &total, 0)) return (0);
  
  return (1);
}


int GetZoidsData()
{
  int n;
  int br;
  int len;
  double total=0;
  int blocklen;

  pos=1;

  data[0]=0x09;                /* The SYNC Byte was already read from the tape */

  for (n=0x08; n >= 0x01; n--)
    {
      br=GetTurboTapeByte(STT_0, STT_1, &len, MSB, 0, 0);
      data[pos]=br;
      pos++;
      total+=len;
      if (br != n || !dataleft)  /* Check if this is really a Turbo Tape block - countdown at start */
	{
	  break;
	}
    }
  if (n != 0x00)
    {
      pause+=total;
      return (0);
    }

  br=GetTurboTapeByte(ZOI_0, ZOI_1, &len, MSB, 0, 0);
  data[pos]=br;
  pos++;
  total+=len;
  if (br == 0)
  {
    pause+=total;
    return (0);
  }

  if (!GetNumberOfTurboTapeBytes(4, ZOI_0, ZOI_1, &len, MSB, &total, 0)) return (0);
  blocklen=Get2(&data[0x0c])-Get2(&data[0x0a]);
  if (!GetNumberOfTurboTapeBytes(blocklen, ZOI_0, ZOI_1, &len, MSB, &total, 0)) return (0);
  
  return (1);
}


/******************************************************************************/
/* Print Information on the screen                                            */
/******************************************************************************/

unsigned char *GetJoeCheckSum()
{
  unsigned char cc;
  int ad;

  if (joe == 2) ad=14;
  else          ad=4;

  cc=0x00;
  for (n=0; n<blocklen; n++)
    cc^=data[ad+blocklen-n]^((blockend-(blocklen-n-1))&0xff);
  if (cc == data[ad+blocklen+1] || (blocklen == 0 && blockend == 0))
    return(STR_CHECK_OK);
  else
    return(STR_CHECK_NOT);
}

unsigned char *GetCheckSum(int start)
{
  unsigned char c=0;
  int n=start;
  
  while (n < pos-1) { c^=data[n]; n++; }

  if (c == data[pos-1])
    return(STR_CHECK_OK);
  else
    return(STR_CHECK_NOT);
}

void GetName(unsigned char * name, int start, int size)
{
  unsigned char d;

  for (n=0; n < 16; n++)
    {
      d=data[start+n]; 
      if (d<32 || d>125 || n >= size)
	name[n]=' ';
      else
	name[n]=d;
    }
  name[n]=0;
}

void PrintInfo(unsigned char * loader,
               int name_type, int name_start, int name_length,
               int has_trail,
			   int has_blocks,
			   int endian,
			   int checksum_type, int checksum_pos)
{
  unsigned char name[255];
  unsigned char * file_name;
  
  printf("%s ",loader);                    /* Loader name */

  switch (name_type)
  {
    case TYPE_ROM  : if (data[0]==0x89 && data[1]==0x88 && data[2]==0x87 && data[3]==0x86 &&
                         data[4]==0x85 && data[5]==0x84 && data[6]==0x83 && data[7]==0x82 &&
                         data[8]==0x81)
                     {
                       if (pos==202)
                       {
                         strcpy(name,"Head:");
                         GetName(name+5, name_start, name_length);
                       }
                       else
                       {
	                     strcpy(name,"Data Block           ");
					   }
					   has_trail=0;
					 }
					 else
					 {
                       strcpy(name,headerless);
					   has_trail=1;
					 }
					 file_name=name;
					 break;
    case TYPE_NOVA : if (data[2] == 0x55)
	                 {
                       strcpy(name,"Loader and Main Block");
                       file_name=name;
                       break;
                     }
                     if (data[2] == 0x00)
					 {
					   file_name=headerless;
					   break;
					 } /* Else use TYPE_ALL !!! */
    case TYPE_ALL  : strcpy(name,"Data:");
                     GetName(name+5, name_start, name_length);
                     file_name=name;
                     break;
    case TYPE_STT  : if (turboid != 0x00)
                     {
                       strcpy(name,"Head:");
                       GetName(name+5, name_start, name_length);
                       file_name=name;
                       break;
                     }  /* Else use TYPE_NONE !!! */
    case TYPE_NONE : file_name=headerless;
                     break;
    case TYPE_MAST : strcpy(name,"Combined Block       ");
                     file_name=name;
                     break;
  }
  printf("%s Len:%5d Plt:%5d ",file_name, pos, pilot);

  if (has_trail && trailing)  printf("Trl:%4d ", trailing);
  else                        printf("Trl:---- ");

  if (has_blocks) printf("Blk:%3d ", num_blocks);
  else            printf("Blk:--- ");

  if (endian == 1)      printf("M ");
  else if (endian == 0) printf("L ");
       else             printf("- ");

  switch(checksum_type)
  {
    case TYPE_ROM  : if (bytechecksum) strcpy(name,STR_CHECK_OK);
                     else              strcpy(name,STR_CHECK_NOT);
                     printf("C%s ",name);
                     break;
    case TYPE_ALL  : printf("C%s ",GetCheckSum(checksum_pos)); break;
    case TYPE_JOE  : printf("C%s ",GetJoeCheckSum()); break;
    case TYPE_NOVA : printf("C%s ",novacheck); break;
    case TYPE_STT  :
      if (turboid == 0x00)
	{
	  printf("C%s ", GetCheckSum(checksum_pos));
	  break;
	}  /* Else use TYPE_NONE */
    case TYPE_NONE : printf("   "); break;
  }
}


void PrintPause()
{
  unsigned char space[2];

  if (pause/3500000 < 10.0) strcpy(space," ");
  else                      strcpy(space,"");
  printf("Pa:%s%02.3f\n", space, pause/3500000);
}

/******************************************************************************/
/* Main CONVERSION Functions                                                  */
/******************************************************************************/

void SaveBlock()
{
  /* Save the previously decoded block ... */
    
  switch (previousblock)
    {
    case TYPE_ROM  :SaveROMData(); break;
    case TYPE_STT  :SaveTurboTapeData(0x02, STT_0, STT_1, MSB,        0, 0x00, 0x00); break;
    case TYPE_G180 :SaveTurboTapeData(0x40,  G1_0,  G1_1, MSB,        0, 0x00, 0x00); break;
    case TYPE_G720 :SaveTurboTapeData(0x20,  G1_0,  G1_1, MSB,        0, 0x00, 0x00); break;
    case TYPE_ACE  :SaveTurboTapeData(0x80,  G1_0,  G1_1, MSB,        0, 0x00, 0x00); break;
    case TYPE_ERE  :SaveTurboTapeData(0xAA,  G1_0,  G1_1, MSB,        0, 0x00, 0x00); break;
    case TYPE_JOE  :SaveTurboTapeData(0xA0, JOE_0, JOE_1, LSB,        0, 0x00, 0x00); break;
    case TYPE_NOVA :SaveTurboTapeData(0x00,NOVA_0,NOVA_1, LSB, trailing, 0x00, 0x00); break;
    case TYPE_OCN  :SaveTurboTapeData(0x00,NOVA_0,NOVA_1, LSB, trailing, 0x00, 0x00); break;
    case TYPE_MAST :SaveTurboTapeData(0x00,MAST_0,MAST_1, old_mast_endian,0, 0x00, old_mast_bits|0x08); break;
    case TYPE_KET  :SaveTurboTapeData(0x63, KET_0, KET_1, MSB,        0, 0x00, 0x00); break;
    case TYPE_RAIN :SaveTurboTapeData(0x63,RAIN_0,RAIN_1, MSB,        0, 0x00, 0x00); break;
    case TYPE_ZOI  :SaveTurboTapeData(0x02, ZOI_0, ZOI_1, MSB,        0, 0x00, 0x00); break;
    }
}

void Convert()
{
  ofh=open(fout, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  write(ofh,tzxbuf,10);         /* Write TZX header */
  write(ofh,c64buf,5);          /* And the C64 identifier */
  if (!tap) cycle=(double) rate/3500000.0;
  printf("\nCreating 1.13 version .TZX file ... \n\n");
  /* Start the conversion ... */
  GotData=0;
  
  while (dataleft)
    {
      /* Write the Block Types in the REVERSED order as they should be used !!! */
      if (mast) nextblock=TYPE_MAST;
      if (ocn)  nextblock=TYPE_OCN;
      if (nova) nextblock=TYPE_NOVA;
      if (joe)  nextblock=TYPE_JOE;
      if (ere)  nextblock=TYPE_ERE;
      if (ace)  nextblock=TYPE_ACE;
      if (g720) nextblock=TYPE_G720;
      if (g180) nextblock=TYPE_G180;
      if (stt)  nextblock=TYPE_STT;
      if (ket)  nextblock=TYPE_KET;
      if (rain) nextblock=TYPE_RAIN;
      if (zoi)  nextblock=TYPE_ZOI;
      if (rom)  nextblock=TYPE_ROM;

      
      /* No more blocks to recognise ? */
      if (!rom && !stt && !g180 && !g720 && !ace && !ere && !joe && !nova && !ocn && !mast && !ket && !rain && !zoi) break;
      if (joe > 1 && blocklen == 0 && blockend == 0) break;	/* Players END */
      
      switch (nextblock)                                  /* Find next block ... */
	{
	case TYPE_ROM  :PilotOK=GetContinousPilot(ROM_S, ROM_L, MIN_ROM_PILOT); break;
	case TYPE_STT  :PilotOK=GetTurboTapePilot(0x02, 0x09, STT_0, STT_1, MSB, 0x00, 0, MIN_TURBO_PILOT); break;
	case TYPE_G180 :PilotOK=GetTurboTapePilot(0x40, 0x5A,  G1_0,  G1_1, MSB, 0x00, 0, MIN_TURBO_PILOT); break;
	case TYPE_G720 :PilotOK=GetTurboTapePilot(0x20, 0xFF,  G1_0,  G1_1, MSB, 0x00, 0, MIN_TURBO_PILOT); break;
	case TYPE_ACE  :PilotOK=GetTurboTapePilot(0x80, 0xFF,  G1_0,  G1_1, MSB, 0x00, 0, MIN_TURBO_PILOT); break;
	case TYPE_ERE  :PilotOK=GetTurboTapePilot(0xAA, 0x50,  G1_0,  G1_1, MSB, 0x00, 0, MIN_TURBO_PILOT); break;
	case TYPE_JOE  :
	  if (joe == 1) { PilotOK=GetTurboTapePilot(0xA0, 0x0A, JOE_0, JOE_1, LSB, 0x00, 0, MIN_TURBO_PILOT); }
	  else          { PilotOK=1; pilot=0; }	
	  break;
	case TYPE_NOVA :PilotOK=GetContinousPilot(NOVA_0, NOVA_1, MIN_NOVA_PILOT); pilot>>=3; break;
	case TYPE_OCN  :PilotOK=GetContinousPilot(NOVA_0, NOVA_1, MIN_OCN_PILOT);  pilot>>=3; break;
	case TYPE_MAST :
	  if (mast_leadin == 1) { PilotOK=GetTurboTapePilot(0x00, 0x16,  G1_0,  G1_1, mast_endian, mast_bits, 0x00, 1); }
	  else                  { PilotOK=1; pilot=0; }
	  break;
	case TYPE_KET  :PilotOK=GetTurboTapePilot(0x63, 0x64, KET_0, KET_1, MSB, 0x00, 0, MIN_TURBO_PILOT); break;
	case TYPE_RAIN :PilotOK=GetTurboTapePilot(0x63, 0x64, RAIN_0, RAIN_1, MSB, 0x00, 0, MIN_TURBO_PILOT); break;
	case TYPE_ZOI  :PilotOK=GetTurboTapePilot(0x02, 0x09, ZOI_0, ZOI_1, MSB, 0x00, 0, MIN_TURBO_PILOT); break;
	}
    if (PilotOK)
	{
	  if (block && GotData)                               /* We have a previous block... */
	    {                                                 /* If Pilot was succesfully recognised BUT there was */
	      if (nextblock != TYPE_MAST || num_blocks == 0)
	        PrintPause();       /* no data read then the PAUSE will be WRONG !!! */
	      SaveBlock();		   
	    }
	  lst_pilot=pilot;                                    /* Rember last pilot length */
	  switch (nextblock)                                  /* The only thing (for now) that needs */
	    {                                                 /* to be remembered from Pilot tone ... */
	    case TYPE_ROM :SyncOK=GetROMSync(); break;
	    default       :SyncOK=1; break;	              /* No other loader uses SYNC for now */
	    }
	  if (SyncOK)
	    {
	      switch (nextblock)
		{
		case TYPE_ROM  :DataOK=GetROMData(); break;
		case TYPE_STT  :DataOK=GetStandardTurboTapeData(); break;
		case TYPE_G180 :DataOK=Get180Data(); break;
		case TYPE_G720 :DataOK=Get720Data(); break;
		case TYPE_ACE  :DataOK=GetAceData(); break;
		case TYPE_ERE  :DataOK=GetErebusData(); break;
		case TYPE_JOE  :DataOK=GetJoeData(); break;
		case TYPE_NOVA :DataOK=GetNovaLoadData(); break;
		case TYPE_OCN  :DataOK=GetOceanData(); break;
		case TYPE_MAST :DataOK=GetMastertronicData(); break;
		case TYPE_KET  :DataOK=GetKettleData(KET_0, KET_1); break;
		case TYPE_RAIN :DataOK=GetKettleData(RAIN_0, RAIN_1); break;
		case TYPE_ZOI  :DataOK=GetZoidsData(); break;
		}
	    if (DataOK)
		{
		  switch (nextblock)
		    {
		    case TYPE_ROM  :PrintInfo("ROM  ", TYPE_ROM , 14,      16, 1, 0, LSB, TYPE_ROM ,  0); rom--; break;
		    case TYPE_STT  :PrintInfo("Turbo", TYPE_STT , 15,      16, 0, 0, MSB, TYPE_STT , 10); break;
		    case TYPE_G180 :PrintInfo("180  ", TYPE_NONE,  0,       0, 0, 0, MSB, TYPE_ALL ,  5); break;
		    case TYPE_G720 :PrintInfo("720  ", TYPE_ALL ,  1,      16, 0, 0, MSB, TYPE_NONE,  0); break;
		    case TYPE_ACE  :PrintInfo("Ace  ", TYPE_ALL ,  5,      10, 0, 0, MSB, TYPE_ALL , 21); break;
		    case TYPE_ERE  :PrintInfo("Erbus", TYPE_NONE,  0,       0, 0, 0, MSB, TYPE_ALL ,  9); break;
		    case TYPE_JOE  :PrintInfo("Plyrs", TYPE_NONE,  0,       0, 0, 0, LSB, TYPE_JOE ,  0); break;
		    case TYPE_NOVA :PrintInfo("Nova ", TYPE_NOVA,  3, data[2], 1, 1, LSB, TYPE_NOVA,  0); break;
		    case TYPE_OCN  :PrintInfo("Ocean", TYPE_NONE,  0,       0, 1, 1, LSB, TYPE_NONE,  0); break;
		    case TYPE_KET  :PrintInfo("Kettl", TYPE_NONE,  0,       0, 0, 0, MSB, TYPE_ALL,0xa7); break;
		    case TYPE_RAIN :PrintInfo("Rainb", TYPE_NONE,  0,       0, 0, 0, MSB, TYPE_ALL,0xa7); break;
		    case TYPE_ZOI  :PrintInfo("Zoids", TYPE_NONE,  0,       0, 0, 0, MSB, TYPE_NONE,  0); zoi--; break;
		    }
		  block++;
		  GotData=1;
		  previousblock=nextblock;
		  pause=0.0;
		}
	      else
		{
		  GotData=0;
		}
	    }
	}
    }
  if (block && GotData)
    {
      SaveBlock();        
      if (nextblock == TYPE_MAST && num_blocks)
  	    {
	      pos=mast_combined;
	      pilot=256;
          PrintInfo("Mastr", TYPE_MAST,  0,       0, 0, 1, 2, TYPE_NONE,  0);
	      SaveGroupEnd();
	    }
      printf("\n");
    }
    
  if (!block)
    {
      printf("-- Error: No blocks found... please consult the documentation!\n");
    }
  close(ofh);
}

/******************************************************************************/
/* Main                                                                       */
/******************************************************************************/

void main(int argc, unsigned char *argv[])
{
  printf("\nVOC or C64 TAP to TZX Converter v0.08b\n");
  if (argc<2)
    {
      printf("\nUsage: 64VOCTZX [Switches] FILE.VOC | FILE.TAP [OUTPUT.TZX]\n\n");
      printf("       Switches:  /tresh n  Set Treshold value to n [0-255] (VOC only)\n");
      printf("                  /pilot n  Set Minimal Number of ROM Pilot Waves to n\n");
	  printf("                  /zero  n  Set 'Zero' of TAP files to n (default 2550)\n");
      printf("                  /rom   n  First Read n ROM Saved blocks (default 4)\n");
      printf("                  /stt      Standard Turbo Tape\n");
      printf("                  /180      180 Loader\n");
      printf("                  /720      720 Degress Loader\n");
      printf("                  /ace      Ace of Aces Loader\n");
      printf("                  /joe      Players Loader\n");
      printf("                  /nova     NovaLoad\n");
      printf("                  /ocean n  Ocean/Imagine Loader Type n (1-3) \n");
      printf("                  /mast     Mastertronic Loader\n");
      printf("                  /ket      Kettle Loader\n");
      printf("                  /rain     Rainbird Loader\n");
      printf("                  /zoi      Zoids Loader\n");
//      printf("                  /erebus   Erebus Loader\n");
      exit(0);
    }
  /* Check for command line options */
  for (n=1; n<argc; n++)
    {
      if (argv[n][0]=='/')
	{
	  switch (argv[n][1])
	    {
	    case 't': Treshold=getnumber(argv[n+1]); n++; break;
	    case 'r': if (argv[n][2] == 'a') { rain=1; break; }
                  rom=getnumber(argv[n+1]); n++; break;
	    case 'p': MIN_ROM_PILOT=getnumber(argv[n+1]); n++; break;
	    case 's': stt=1; break;
	    case '1': g180=1; break;
	    case '7': g720=1; break;
	    case 'a': ace=1; break;
	    case 'e': ere=1; break;
	    case 'j': joe=1; rom=6; break;
	    case 'n': nova=1; break;
	    case 'o': ocn=getnumber(argv[n+1]); n++; break;
	    case 'z': if (argv[n][2] == 'o') { zoi=1; g180=1; break; }
                  zero=getnumber(argv[n+1]); n++; break;
	    case 'm': mast=1; break;
	    case 'k': ket=1; break;
	    default : invalidoption(argv[n]);
	    }
	}
      else
	{
	  files++;
	  switch (files)
	    {
	    case 1:  strcpy(finp,argv[n]); break;
	    case 2:  strcpy(fout,argv[n]); break;
	    default: error("Too Many files on command line!");
	    }
	}
    }
  if (files == 0) error("No Files specified !");
  if (files == 1) { strcpy(fout,finp); ChangeFileExtension(fout,"tzx"); }
  tap=GetFileType(finp);
  
  switch (LoadFile(finp))
    {
    case 1: Error("Input file not found!"); break;
    case 2: Error("Not enough memory to load input file!"); break;
    case 3:
      free(raw);
      Error("Input file corrupt or in a wrong format!");
      break;
    }
  Convert();
  free(raw);
}
