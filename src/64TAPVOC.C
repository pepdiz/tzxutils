///////////////////////////////////////////////////////////////////////////////
// Play C64 TAP  and  C64 TAP to VOC converter
//                                                                       v0.04b
// (c) 1998 Tomaz Kac
//
// Watcom C 10.0+ specific code...   set TABs to 4 characters

#include "sb.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void Error(char *errstr)
{
// exits with an error message *errstr

printf("\n-- Error: %s\n",errstr);
exit(0);
}

int fh;					// Input File Handle
int ofh;				// Output File Handle
int flen;				// File Length
int files=0;	  		// Number of Files on the command line
char finp[255];   		// Input File  (First Command Line Option)
char fout[255];   		// Output File (Second Command Line Option or First with .VOC)
char errstr[255]; 		// Error String
char *mem;				// File in Memory

char *sbbuf[2];			// SB Buffers
int sbbuflen=1024;		// Length of buffers
int freq=30303;			// Sample Freq. for .VOC file creation
int nfreq=0;			// Frequency set by command line switch
int sbcurrpage=1;		// Buffer that is currently available for writing
int sbpos=0;			// Position in the buffer
int amp;				// Amplitude of the current signal

int voc=1;				// Are we making a .VOC file ?

char vochead[0x20]={ 'C','r','e','a','t','i','v','e',' ','V','o','i','c','e',' ','F','i','l','e',
                     0x1A, 0x1A, 0x00, 0x0A, 0x01, 0x29, 0x11 };
char *vocbuf;			// Buffer for .VOC block
int vocbuflen=0xFFFF;	// Length of .VOC block (and buffer)
char vocstart[4]={ 0x02, 0xFF, 0xFF, 0x00 };
int vocpos;				// Length of current .VOC block
int n, len;

int inv=0;
int ver;
int pv1;

// Get the Intel endian type 4 byte long word from memory
int Get4(char *mem) { return(mem[0]+(mem[1]*256)+(mem[2]*256*256)+(mem[3]*256*256*256)); }

#define LOAMP	0x10	// Low Level Amplitude
#define HIAMP   0xF0	// High Level Amplitude

void InitSB()
{
int n;

// Prepares SoundBlaster Buffers and everything ...
sbcurrpage=1;
sbpos=0;
SB_Speaker(1);
sbbuf[0]=SB_AllocBuffer();
sbbuf[1]=sbbuf[0]+sbbuflen;
SB_SetIntHandler();
for (n=0; n<sbbuflen; n++) sbbuf[0][n]=0x80;
SB_PlayAIDMA(sbbuf[0], freq, sbbuflen<<1);
}

void StopSB()
{
// Stops SoundBlaster output from playing ...
SB_StopDMA();
SB_ResetIntHandler();
SB_FreeBuffer();
SB_Speaker(0);
SB_DSPReset();
}

void InitVOC()
{
// Prepares VOC file to be written to ...
char buf[10];

vocbuf=(char *) malloc(vocbuflen+256);
if (vocbuf==NULL) { free(mem); Error("Not enough memory to set up .VOC file buffer!"); }
ofh=open(fout, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
	           S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
write(ofh,vochead,0x1A);
if (freq<=20000)		// Should we use more accurate format for Frequency ???
  {
  buf[0]=0x01; buf[1]=0x03; buf[2]=buf[3]=buf[5]=buf[6]=0x00;
  buf[4]=(char) (256-(1000000/freq));			// Frequency in 'OLD' format
  write(ofh,buf,7);
  }
else
  {
  buf[0]=0x09; buf[1]=13; buf[2]=buf[3]=buf[4]=buf[5]=buf[6]=0x00;
  write(ofh,buf,4);
  // Frequency in 'NEW' format - more accurate !!!
  // But block 0x09 must be supported by the utility that reads the VOC !!!
  write(ofh,&freq,4);							// ENDIAN depandable!!!
  buf[0]=8;										// 8 bits per sample
  buf[1]=1;										// MONO
  write(ofh,&buf,4); write(ofh,&buf[2],5);
  }
vocpos=0;
}

void StopVOC()
{
// Finishes the VOC recording (puts the last buffer and the terminator to the .VOC file)
char buf[10];

if (vocpos)
	{
	vocstart[1]=vocpos&0xFF;
	vocstart[2]=(vocpos>>8)&0xFF;
	vocstart[3]=(vocpos>>8)>>8;
	write(ofh,vocstart,4);
	write(ofh,vocbuf,vocpos);
	}
buf[0]=buf[1]=buf[2]=buf[3]=0x00;
write(ofh,buf,4);
free(vocbuf);
close(ofh);
}

void PlayVOC(char amp, int len)
{
// Puts amplitude amp to .VOC file buffer for len time ...

while (len)
	{
	vocbuf[vocpos]=amp;
	len--;
	vocpos++;
	if (vocpos==vocbuflen)
		{						// Do we need to write the buffer to the .VOC file ?
		write(ofh,vocstart,4);
		write(ofh,vocbuf,vocbuflen);
		vocpos=0;
		}
	}
}

void PlaySB(char amp, int len)
{
int hit;
int k;
// Puts amplitude amp to SoundBlaster buffer for len time (or calls PlayVOC if converting) ...
// This would be fun to do in C++ :)

if (voc) { PlayVOC(amp, len); return; }

while (len)
	{
	sbbuf[sbcurrpage][sbpos]=amp;
	len--;
	sbpos++;
	if (sbpos==sbbuflen)			// Is the buffer full ? Wait until last bufer finishes ...
		{
		while (sbcurrpage!=SB_DMAComplete)
		{ 
			hit = kbhit();
			if (hit)
			{
			        k=getch();
				if (k == 32)
				{
				      	printf("\n*** PAUSED *** Press ANY KEY to continue !\n",k);
	                                PlaySB(amp,sbbuflen<<1);            // finish last block ...
                                        StopSB();
                                        while (!kbhit()) {} k=GetCh();
                                        InitSB();				
				}
				else
				{
					StopSB();
					free(mem);
					Error("Key pressed!");
				}
			}
		}
		sbcurrpage=!sbcurrpage;
		sbpos=0;
		}
	}
	hit = kbhit();
	if (hit)
	{
	        k=getch();
		if (k == 32)
		{
		      	printf("\n*** PAUSED *** Press ANY KEY to continue !\n",k);
                               PlaySB(amp,sbbuflen<<1);            // finish last block ...
                                      StopSB();
                                      while (!kbhit()) {} k=GetCh();
                                      InitSB();				
		}
		else
		{
			StopSB();
			free(mem);
			Error("Key pressed!");
		}
	}
}

void PauseSB(char amp,int pause)
{
// Waits for pause milliseconds
int p;

p=(int) ((((float) pause)*freq)/1000.0);
PlaySB(amp,p);
}

int getnumber(char *s)
{
// Returns the INT number contained in string *s
int i;

sscanf(s,"%d",&i); return(i);
}

void ChangeFileExtension(char *str,char *ext)
{
// Changes the File Extension of String *str to *ext
int n,m;

n=strlen(str); while (str[n]!='.') n--;
n++; str[n]=0; strcat(str,ext);
}

void ToggleAmp()
{
// Toggles the amplitude of the output

if (amp==LOAMP) amp=HIAMP; else amp=LOAMP;
}

void invalidoption(char *s)
{
// Prints the Invalid Option error

sprintf(errstr,"Invalid Option %s !",s);
error(errstr);
}

void CopyString(char *dest, char *sour, int len)
{
// Could just use strpy ... 
int n;

for (n=0; n<len; n++) dest[n]=sour[n]; dest[n]=0;
}


void main(int argc, char *argv[])
{
printf("\nC64 TAP to VOC Converter & Play C64 TAP v0.04b\n");
if (argc<2)
	{
	printf("\nUsage: 64TAPVOC [switches] FILE.TAP [OUTPUT.VOC]\n\n");
	printf("       Switches:  /play     Output to SB instead of .VOC conversion\n");
	printf("                  /freq n   Set sampling frequency to n Hz\n");
	printf("                  /reverse  Will invert the signal\n");
	exit();
	}
// Check for command line options
for (n=1; n<argc; n++)
	{
	if (argv[n][0]=='/')
		{
		switch (argv[n][1])
			{
			case 'p':	voc=0; freq=44100; break;
			case 'f':	nfreq=getnumber(argv[n+1]); n++; break;
			case 'r':	inv=1; break;
			default :	invalidoption(argv[n]);
			}
		}
	else
		{
		files++;
		switch (files)
			{
			case 1: strcpy(finp,argv[n]); break;
			case 2: strcpy(fout,argv[n]); break;
			default:error("Too Many files on command line!");
			}
		}
	}
if (files==0) error("No Files specified !");
if (files==1) { strcpy(fout,finp); ChangeFileExtension(fout,"VOC"); }
if (nfreq) freq=nfreq;
//
if (!voc) { if (!SB_Detect()) Error("SoundBlaster could not be detected!"); }
if ((fh=open(finp,O_RDONLY | O_BINARY))==-1) Error("File not found");
flen=FileLength(fh);
mem=(char *) malloc(flen);
if (mem==NULL) Error("Not enough memory to load the file!");
// Start reading file...
read(fh,mem,12+4+4); // 'C64-TAPE-RAW' + 4 padded bytes + Data length
ver = (int) mem[12];
mem[12]=0;
if (strcmp(mem,"C64-TAPE-RAW")) { free(mem); Error("File is not in C64 TAP format!"); }
//flen=Get4(mem+12+4);   This doesn't seem to work... get data from filelen
printf("\nTAP File Version = %i\n", ver);
flen-=12+4+4;
read(fh,mem,flen);
if (voc)	printf("\nCreating .VOC file using %d Hz frequency.\n",freq);
else		printf("\nStarting playback on SoundBlaster using %d Hz frequency.\n",freq);
if (!voc) InitSB(); else InitVOC();
amp=LOAMP;
if (inv ==1)
{
  amp = HIAMP;
}
// Starting Playback / Conversion ...
for (n=0; n<flen; n++)
  {
  if (mem[n])
  {
    // normal
    len=((((double) (mem[n]<<3))*(double) freq)/985248.0);
  }
  else
  {
    // pause
    if (ver == 0)
    {
      len=((((double) (2550<<3))*(double) freq)/985248.0);
    }
    else
    {
      pv1=mem[n+1]+256*mem[n+2]+256*256*mem[n+3];
      n+=3;      
      len=((((double) pv1)*(double) freq)/985248.0);
      //printf("DEBUG: Pause found: %i cycles\n",pv1);
    }
  }
  PlaySB(amp, len/2); ToggleAmp();	// Convert TAP len to pulse and play it
  PlaySB(amp, (len/2)+(len%2)); ToggleAmp();
  }
PauseSB(amp,1000);	// Finish the loading with 1s pause ...
if (!voc) StopSB(); else StopVOC();
free(mem);
close(fh);
}

