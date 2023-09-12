///////////////////////////////////////////////////////////////////////////////
// TAP to TZX converter
//                                                                       v0.12b
// (c) 1997 Tomaz Kac
//
// Watcom C 10.0+ specific code... Change file commands for other compilers

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int fhi,fho,flen;
char *mem;
char buf[256];
int pos;
int len;
int block;
char tzxbuf[10]={ 'Z','X','T','a','p','e','!', 0x1A, 1, 03 };

void Error(char *errstr)
{
// exits with an error message *errstr

printf("\n-- Error: %s\n",errstr);
exit(0);
}

void ChangeFileExtension(char *str,char *ext)
{
// Changes the File Extension of String *str to *ext
int n,m;

n=strlen(str); while (str[n]!='.') n--;
n++; str[n]=0; strcat(str,ext);
}

void main(int argc, char *argv[])
{
printf("\nZXTape Utilities - TAP to TZX Converter v0.12b\n");
if (argc<2|| argc>3)
	{
	printf("\nUsage: TAP2TZX INPUT.TAP [OUTPUT.TZX]\n");
	exit();
	}
if (argc==2) {	strcpy(buf,argv[1]); ChangeFileExtension(buf,"TZX"); }
else			strcpy(buf,argv[2]);
fhi=open(argv[1],O_RDONLY | O_BINARY);
if (fhi==-1) Error("Input file not found!");
fho=open(buf,O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
		     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
flen=FileLength(fhi);
mem=(char *) malloc(flen);
if (mem==NULL) Error("Not enough memory to load input file!");
read(fhi,mem,flen);
write (fho,tzxbuf,10);
pos=block=0; len=1;
buf[0]=0x10; buf[1]=0xE8; buf[2]=0x03;
while(pos<flen && len)
	{
	len=mem[pos+0]+mem[pos+1]*256;
	pos+=2;
	if (len)
		{
		if (pos+len>=flen) { buf[1]=buf[2]=0; }
		buf[3]=len&0xff; buf[4]=len>>8;
		write(fho,buf,5);
		write(fho,&mem[pos],len);
		}
	pos+=len;
	block++;
	}
printf("\nSuccesfully converted %d blocks!\n",block);
close(fhi);
close(fho);
free(mem);
}
