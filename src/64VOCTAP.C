///////////////////////////////////////////////////////////////////////////////
//  VOC to C64 TAP converter
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

#define BUFLEN 16384	// Buffer length

int fh;					// Input File Handle
int ofh;				// Output File Handle
int tfh;				// Temporary File Handle
int flen;				// File Length
int files=0;	  		// Number of Files on the command line
char finp[255];   		// Input File  (First Command Line Option)
char fout[255];   		// Output File (Second Command Line Option or First with .VOC)
char ftmp[255];			// Temporary File name
char errstr[255]; 		// Error String
char *raw;			// Address of the loaded .VOC file (converted to RAW)
int rawlen;			// Length of the .VOC file in the memory
int rate;			// Sample Rate
int silence=0;		// Are Silence blocks in the .VOC file ?
int shift=0;		// Should we 'shift' the file ? (insert one pulse)
int Treshold=127;	// Treshold value for recognition
int numzeros;
int cur=0;
int last=0;
int n;
int len;
int wlen;
char clen;
char buf[BUFLEN];
int bufpos;
char tapbuf[12+4]={ 'C','6','4','-','T','A','P','E','-','R','A','W',
                      0,0,0,0 };

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

int ReadVOCType(int fh, int *len, int *f)
{
// Reads the TYPE block from the VOC file... skips everything but DATA blocks
// Returns 0 if everything is OK and 1 if there was an unexpected End-Of-File
int l;
char b;
char buff[256];
int t;

do	{
	if (!read(fh,&b,1)) return(1);
	if (!read(fh,buff,3)) return(1);
	*len=((int) (buff[0]) + ((int) (buff[1])*256) + ((int) (buff[2])*256*256));
//	printf("%x %d\n",b,*len);
	switch(b)
		{
		case 0x00:  *len=0; break;
		case 0x01:	*len-=2;
					if (!read(fh,buff,1)) return(1);
					*f=(int) (1000000/(256-(int) buff[0]));
					if (!read(fh,buff,1)) return(1);	// Any compression type... I don't care ;)
					break;
		case 0x02:	b=0x01; break;
		case 0x03:	silence=1;
		case 0x09:	*len-=12;
					// Get the frequency from the 'special' Frequency block!!!
					if (!read(fh,f,4)) return(1);		// ENDIAN depandable !!!
					if (!read(fh,buff,8)) return(1);
					break;
		default:	if (*len) lseek(fh, *len, SEEK_CUR); break;
		}
	} while (b!=0x01 && b!=0x00 && b!=0x09);
return(0);
}

int LoadVOC(char *filename)
{
// Loads a .VOC file to the memory
// Input   : filename
// Outputs : raw    - address of the loaded file
//           rawlen - length of the loaded file
//           rate   - sample rate of the file
// Returns : 0 - when everything went OK
//			 1 - when input file is not found
//           2 - when there is not enough memory to load file
//           3 - when the file is not the right type or corrupt

int fh;
int flen;
int len;
int plen;
char buf[256];
int p=0;
char dummy;
int idummy;
int ee=0;

if ((fh=open(filename,O_RDONLY | O_BINARY))==-1) return(1);
flen=FileLength(fh);
raw=(char *) malloc(flen);
if (raw==NULL) { close(fh); return(2); }
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
// if (ee) printf("-- Warning: Unexpected End-Of-File !\n");
if (silence) printf("-- Warning: Silence blocks in file (Pauses will be wrong) !\n");
printf("Actual RAW size of file in memory: %d bytes\n",p);
close(fh);
rawlen=p;
return(0);
}

int GetPulse()
{
// Finds next Pulse and returns its length

last=cur;
if (raw[cur]>Treshold)	while(raw[cur]> Treshold && cur<rawlen) cur++;
else					while(raw[cur]<=Treshold && cur<rawlen) cur++;
return(cur-last);
}

int PulseLen(int pulse)
{
// Returns length of a pulse in .TAP format
return ((int) (((((double) pulse)*985248.0)/((double) (rate*8)))));
}

void main(int argc, char *argv[])
{
printf("\nVOC to C64 TAP Converter v0.04b\n");
if (argc<2)
	{
	printf("\nUsage: 64TAPVOC FILE.VOC [OUTPUT.TAP]\n\n");
	printf("                  /tresh n  Set Treshold value to n [0-255]\n");
	exit();
	}
// Check for command line options
for (n=1; n<argc; n++)
	{
	if (argv[n][0]=='/')
		{
		switch (argv[n][1])
			{
			case 's':	shift=1; break;
			case 't':	Treshold=getnumber(argv[n+1]); n++; break;
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
if (files==1) { strcpy(fout,finp); ChangeFileExtension(fout,"TAP"); }
//
switch (LoadVOC(finp))
	{
	case 1: Error("Input file not found!"); break;
	case 2: Error("Not enough memory to load input file!"); break;
	case 3: free(raw);
	        Error("Input file corrupt or in a wrong format!"); break;
	}
strcpy(ftmp,fout); ChangeFileExtension(ftmp,"TMP");
tfh=open(ftmp, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
	           S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
ofh=open(fout, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
	           S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
write(ofh,tapbuf,12+4);	// Write the .TAP standard header...
printf("Creating .TAP file ...\n");

bufpos=0; wlen=0;
if (raw[0]>Treshold) raw[0]=255-raw[0];
numzeros=0;

while(cur<rawlen)
{
  if (numzeros == 0)
  {
    len=PulseLen(GetPulse())+PulseLen(GetPulse()); // Two pulses for one byte
    if (len > 255)
      { numzeros=(len/2550); if (numzeros == 0) numzeros=1; }
    else
      { numzeros=0; }
  }
  if (numzeros)
    { buf[bufpos]=0; numzeros--; }
  else
    { buf[bufpos]=(char) len; }
  bufpos++;
  wlen++;
  if (bufpos==BUFLEN)
  	{
    write(tfh, buf, BUFLEN);
	bufpos=0;
	}
  }
if (bufpos)
  write(tfh, buf, bufpos);
close(tfh);
buf[0]=wlen&0xff; wlen>>=8;
buf[1]=wlen&0xff; wlen>>=8;
buf[2]=wlen&0xff; wlen>>=8;
buf[3]=wlen&0xff;
write(ofh,buf,4); // Write length
tfh=open(ftmp,O_RDONLY | O_BINARY);
len=BUFLEN;
// Copy the temporary file to the output file... 
while (len==BUFLEN)
	{
	len=read(tfh,buf,BUFLEN);
	write(ofh,buf,len);
	}
close(tfh);
remove (ftmp);
close(ofh);
free(raw);
}

