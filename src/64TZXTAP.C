/******************************************************************************
/ TZX to TAP (C64) converter
/                                                                       v0.03b
/ (c) 1998 Tomaz Kac
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

#define MAJREV 1        // Major revision of the format this program supports
#define MINREV 13       // Minor revision -||-

#define BUFLEN 16384	// File Buffer length

char tapbuf[12+4]={ 'C','6','4','-','T','A','P','E','-','R','A','W',1,0,0,0 };

void Error(unsigned char *errstr) 
{
  /* exits with an error message *errstr */
  printf("\n-- Error: %s\n", errstr);
  exit(0);
}


int fh;                                         /* Input File Handle */
int ofh;                                        /* Output File Handle */
int tfh;                                        /* Temporary File Handle */
int flen;                                       /* File Length */
int files=0;                                    /* Number of Files on the command line */
unsigned char finp[255];                        /* Input File  (First Command Line Option) */
unsigned char fout[255];                        /* Output File (Second Command Line Option or First with .VOC) */
unsigned char ftmp[255];                        /* Temporary File name */
unsigned char errstr[255];                      /* Error String */

unsigned char filebuf[BUFLEN];                  /* Buffer for output file */
int bufpos = 0;                                 /* Position in file buffer */
int wlen = 0;                                   /* Length of the TAP file */
int len;

unsigned char *raw;                             /* Address of the loaded .TZX file */
int rawlen;                                     /* Length of the .TZX file in the memory */

int pos = 0;                                    /* Position in the TZX file */
int numblocks = 0;                              /* Number of blocks in the TZX file */
int convblocks = 0;                             /* Number of converted blocks */
int notimpl = 0;                                /* Are there any blocks that are not yet implemented ? */
int non64data = 0;                              /* Are there any DATA blocks that are not yet implemented ? */
int not_rec = 0;                                /* Is there any unrecognised block ? */

int pilotwave;                                  /* Variables used when reading a block */
int pilotlen;
int sync1;
int sync2;
int zero1;
int zero2;
int one1;
int one2;
unsigned char xortype;
int finishbyte1;
int finishbyte2;
int finishdata1;
int finishdata2;
int trailingwave;
int trailinglen;
unsigned char usedbits;
unsigned char endian;
int pause;
int datalen;
unsigned char * data;
int datapos;
unsigned char data_byte;
unsigned char numbits;
unsigned char xorvalue;
unsigned char bit;
int first;
int second;
int n;
int zero;
int one;
unsigned char additional;
int leadinlen;
int leadinbyte;
int trailinglen;
int trailingbyte;

/* General endian-independed Put/Get functions */

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

int LoadFile(unsigned char *filename)
{
  /* Loads a .TZX file to the memory 
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
  /* Start reading the file */
  if (!read(fh,buf,10)) { close(fh); return(3); }
  buf[7]=0;
  if (strcmp(buf,"ZXTape!")) { close(fh); return(3); }
  printf("\nZXTape file revision %d.%02d\n",buf[8],buf[9]);
  if (!buf[8]) Error("Development versions of ZXTape format are not supported!");
  if (buf[8]>MAJREV) printf("\n-- Warning: Some blocks may not be recognised and used!\n");
  if (buf[8]==MAJREV && buf[9]>MINREV) printf("\n-- Warning: Some of the data might not be properly recognised!\n");
  flen-=10;
  p=read(fh,raw,flen);
  printf("\nLoading File (%d bytes) ...\n",flen);
  close(fh);
  rawlen=p;
  return(0);
}

/******************************************************************************/
/* Generic Functions                                                          */
/******************************************************************************/

unsigned char MirrorByte(unsigned char s)
{
  return((s<<7)+((s<<5)&64)+((s<<3)&32)+((s<<1)&16)+((s>>1)&8)+((s>>3)&4)+((s>>5)&2)+(s>>7));
}

/******************************************************************************/
/* Main CONVERT functions                                                     */
/******************************************************************************/

int GetTAP(int tzx)
{
  /* Convert TZX value to TAP value */
  return ((int) ((float) tzx/14.21));
}

void WriteWave(int len)
{
  wlen++;
  filebuf[bufpos]=(unsigned char) len;
  bufpos++;
  if (bufpos == BUFLEN)
    {
      write(tfh, filebuf, BUFLEN);
      bufpos=0;
    }
}

void WritePause(int len)
{
  int tap;
  

/* Version 0 here
  tap = len * 125;
  while (tap > 2550)
    {
      WriteWave(0);
      tap -= 2550;
    }
  if (tap > 255) tap = 0;
  WriteWave(tap);
*/

//  tap=(tap)/3.5524;

tap = len * 1000;  
//          printf("DEBUG: Pause found: %i cycles\n",tap);
          
        
          
  WriteWave(0);
  WriteWave(tap&255);
  WriteWave((tap>>8)&255);
  WriteWave((tap>>16)&255);
}

void WriteTurboByte(unsigned char data, unsigned char bits)
{
  int add_num;
  int playbit;
  
  add_num=additional & 3;
  if (add_num && !(additional & 4))
    {
      while (add_num)
	{
	  if (additional & 8) WriteWave(one);
	  else                WriteWave(zero);
	  add_num--;
	}
    }
  while (bits)
    {
      if (!endian) playbit = data & 0x01;
      else         playbit = data & 0x80;
      if (playbit) WriteWave(one);
      else         WriteWave(zero);
      if (!endian) data >>= 1;
      else         data <<= 1;
      bits--;
    }
  if (add_num && (additional & 4))
    {
      while (add_num)
	{
	  if (additional & 8) WriteWave(one);
	  else                WriteWave(zero);
	  add_num--;
	}
    }
}

void Convert()
{
  /* Write all data to a Temporary file (so we can find out the length of the file to write into TAP file */
  strcpy(ftmp, fout); ChangeFileExtension(ftmp, "tmp");
  tfh=open(ftmp, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  bufpos = 0;
  pos = 0;

  printf("Converting file ...\n");

  while (pos < rawlen)
    {
      pos++;
      switch (raw[pos-1])
	{
	case 0x10: /* Standard Speed Data Block */
	  {
	    pos+=Get2(&raw[pos+0x02])+0x04;
	    non64data = 1;
	    break;
	  }
        case 0x11: /* Turbo Loading Data Block */
	  {
	    pos+=Get3(&raw[pos+0x0F])+0x12;
	    non64data = 1;
	    break;
	  }
        case 0x12: /* Pure Tone Data Block */
	  {
	    pos+=0x04;
	    non64data = 1;
	    break;
	  }
        case 0x13: /* Sequence of pulses of different lengths */
	  {
	    pos+=(raw[pos+0x00]*0x02)+0x01;
	    non64data = 1;
	    break;
	  }
        case 0x14: /* Pure data block */
	  {
	    pos+=Get3(&raw[pos+0x07])+0x0A; 
	    non64data = 1;
	    break;
	  }
        case 0x15: /* Direct Recording block */
	  {
	    pos+=Get3(&raw[pos+0x05])+0x08; 
	    non64data = 1;
	    break;
	  }
        case 0x16: /* C64 ROM Type Data block */
	  {
	    convblocks++;
	    data=&raw[pos+4];
	    pos+=Get4(&raw[pos+0x00])+0x04; 
	    /* Lets convert the Data from the TZX file to TAP format ... */
	    pilotwave    = GetTAP (Get2 (data));
	    pilotlen     =         Get2 (data+2);
	    sync1        = GetTAP (Get2 (data+4));
	    sync2        = GetTAP (Get2 (data+6));
	    zero1        = GetTAP (Get2 (data+8));
	    zero2        = GetTAP (Get2 (data+10));
	    one1         = GetTAP (Get2 (data+12));
	    one2         = GetTAP (Get2 (data+14));
	    xortype      =               data[16];
	    finishbyte1  = GetTAP (Get2 (data+17));
	    finishbyte2  = GetTAP (Get2 (data+19));
	    finishdata1  = GetTAP (Get2 (data+21));
	    finishdata2  = GetTAP (Get2 (data+23));
	    trailingwave = GetTAP (Get2 (data+25));
	    trailinglen  =         Get2 (data+27);
	    usedbits     =               data[29];
	    endian       =               data[30];
	    pause        =         Get2 (data+31);
	    datalen      =         Get3 (data+33);
	    data+=36; /* Go to the real data position */
	    while (pilotlen) { WriteWave(pilotwave); pilotlen--; }                /* Write the pilot */
	    if (sync1) WriteWave(sync1);                                          /* Write Sync pulses */
	    if (sync2) WriteWave(sync2);
	    datapos=0;                    /* Lets play the Data now ... */
	    while (datalen)
	      {
		data_byte=data[datapos];
		if (datalen == 1) numbits=usedbits;  /* If we are playing last byte ... */
		else              numbits=8;
		/* Play one Byte */
                xorvalue=xortype;
		while (numbits)
		  {
		    if (!endian) bit=data_byte & 0x01;
		    else         bit=data_byte & 0x80;
		    xorvalue^=bit;
		    if (bit) { first=one1;  second=one2;  }
		    else     { first=zero1; second=zero2; }
		    if (first)  WriteWave(first);
		    if (second) WriteWave(second);
		    if (!endian) data_byte >>= 1;
		    else         data_byte <<= 1;
		    numbits--;
		  }
		if (xortype != 0xff) /* Are we using XOR on bits ? */
		  {
		    if (xorvalue) { first=one1;  second=one2;  }
		    else          { first=zero1; second=zero2; }
		    if (first)  WriteWave(first);
		    if (second) WriteWave(second);
		  }
		if (datalen == 1)   /* If last byte is being written use Finish DATA ! */
		  {
		    if (finishdata1) WriteWave(finishdata1);
		    if (finishdata2) WriteWave(finishdata2);
		  }
		else               /* Else use Finish BYTE ! */
		  {
		    if (finishbyte1) WriteWave(finishbyte1);
		    if (finishbyte2) WriteWave(finishbyte2);
		  }
		datalen--; datapos++;
	      }
	    while (trailinglen) { WriteWave(trailingwave); trailinglen--; } /* Write the Trailing tone */
	    if (pause) WritePause(pause);
	    break;
	  }
        case 0x17: /* C64 Turbo Tape Data block */
	  {
	    convblocks++;
	    data=&raw[pos+4];
	    pos+=Get4(&raw[pos+0x00])+0x04; 
	    zero         = GetTAP (Get2 (data));
            one          = GetTAP (Get2 (data+2));
	    additional   =               data[4];
	    leadinlen    =         Get2 (data+5);
	    leadinbyte   =               data[7];
	    usedbits     =               data[8];
	    endian       =               data[9];
	    trailinglen  =         Get2 (data+10);
	    trailingbyte =               data[12];
	    pause        =         Get2 (data+13);
	    datalen      =         Get3 (data+15);
	    data+=18;
	    while (leadinlen) { WriteTurboByte(leadinbyte, 8); leadinlen--; } /* Playing Lead-in tone */
	    datapos=0;                    /* Lets play the Data now ... */
	    while (datalen)
	      {
		data_byte=data[datapos];
		if (datalen == 1) numbits=usedbits;  /* If we are playing last byte ... */
		else              numbits=8;
		WriteTurboByte(data_byte, numbits);
		datalen--; datapos++;
	      }
	    while (trailinglen) { WriteTurboByte(trailingbyte, 8); trailinglen--; } /* Playing Trailing tone */
	    if (pause) WritePause(pause);
	    break;
	  }
        case 0x20: /* Pause block */
	  {
	    convblocks++;
	    data=&raw[pos];
	    pos+=0x02;
	    WritePause(Get2 (data));
	    break;
	  }
        case 0x21: /* Group Start */
	  {
	    pos+=raw[pos+0x00]+0x01;
	    break;
	  }
        case 0x22: /* Group End */
	  {
	    break;
	  }
        case 0x23: /* Jump To Block */
	  {
	    pos+=0x02;
	    notimpl = 1;
	    break;
	  }
        case 0x24: /* Loop Start */
	  {
	    pos+=0x02;
	    notimpl = 1;
	    break;
	  }
        case 0x25: /* Loop End */
	  {
	    notimpl = 1;
	    break;
	  }
        case 0x26: /* Call Sequence */
	  {
	    pos+=Get2(&raw[pos+0x00])*0x02+0x02;
	    notimpl = 1;
	    break;
	  }
        case 0x27: /* Return from Call */
	  {
	    notimpl = 1;
	    break;
	  }
        case 0x28: /* Select Block */
	  {
	    pos+=Get2(&raw[pos+0x00])+0x02; 
	    notimpl = 1;
	    break;
	  }
        case 0x2A: /* Stop Tape if in 48k mode */
	  {
	    pos+=0x04;
	    break;
	  }
        case 0x30: /* Text Description */
	  {
	    pos+=raw[pos+0x00]+0x01; 
	    break;
	  }
        case 0x31: /* Message Block */
	  {
	    pos+=raw[pos+0x01]+0x02; 
	    break;
	  }
        case 0x32: /* Archive Info */
	  {
	    pos+=Get2(&raw[pos+0x00])+0x02; 
	    break;
	  }
        case 0x33: /* Hardware Type */
	  {
	    pos+=(raw[pos+0x00]*0x03)+0x01; 
	    break;
	  }
        case 0x34: /* Emulation Info */
	  {
	    pos+=0x08; 
	    break;
	  }
        case 0x35: /* Custom info */
	  {
	    pos+=Get4(&raw[pos+0x10])+0x14; 
	    break;
	  }
        case 0x40: /* Snapshot Block */
	  {
	    pos+=Get3(&raw[pos+0x01])+0x04;
	    notimpl = 1;
	    break;
	  }
        case 0x5A: /* Glued Tapes block */
	  {
	    pos+=0x09; 
	    break;
	  }
        default:   /* Unknown block */
	  {
	    pos+=Get4(&raw[pos+0x00])+0x04;
	    not_rec=1;
	  }
        }
      numblocks++;
    }

  /* Finish the file write */
  if (bufpos)
    write(tfh, filebuf, bufpos);  
  close(tfh);

  ofh=open(fout, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
	         S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  write(ofh,tapbuf,12+4);	/* Write the .TAP standard header... */

  filebuf[0]=wlen&0xff; wlen>>=8;
  filebuf[1]=wlen&0xff; wlen>>=8;
  filebuf[2]=wlen&0xff; wlen>>=8;
  filebuf[3]=wlen&0xff;
  write(ofh,filebuf,4); /* Write length */
			 
  tfh=open(ftmp,O_RDONLY | O_BINARY);
  len=BUFLEN;
  /* Copy the temporary file to the output file... */
  while (len==BUFLEN)
    {
      len=read(tfh,filebuf,BUFLEN);
      write(ofh,filebuf,len);
    }
  close(tfh);
  remove (ftmp);
  close(ofh);

  if (notimpl)
    printf("\n-- Warning: Some blocks used are NOT IMPLEMENTED YET in this converter!\n");
  if (non64data)
    printf("\n-- Warning: Some DATA blocks used are NOT IMPLEMENTED YET in this converter!\n");
  if (not_rec)
    printf("\n-- Warning: Some blocks were not recognised!\n");

  printf("\nConverted %d out of %d blocks\n\n", convblocks, numblocks);
}

/******************************************************************************/
/* Main                                                                       */
/******************************************************************************/

void main(int argc, unsigned char *argv[])
{
  printf("\nTZX to C64 TAP Converter v0.03b\n");
  if (argc<2)
    {
      printf("\nUsage: 64TZXTAP FILE.TZX [OUTPUT.TAP]\n\n");
      exit(0);
    }
  /* Check for command line options */
  for (n=1; n<argc; n++)
    {
      if (argv[n][0]=='/')
	{
	  switch (argv[n][1])
	    {
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
  if (files == 1) { strcpy(fout,finp); ChangeFileExtension(fout,"tap"); }
  
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
