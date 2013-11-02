#include <avr/pgmspace.h>
#include <util/delay.h>
#include "TFT.h"
#include "folder.h"
#include "jpegicon.h"
#include "arrow.h"
#include "unknownfile.h"
#include "ff.h"
#include "exiticon.h"
#include "tjpgd.h"
void drawstrpart(char * str,uint8_t amount,uint16_t x,uint8_t y){
	uint8_t z;
	for(z=0;z<amount;++z){
		tft_drawChar(*str, y, x, 1, WHITE);
		++str;
		x-=8;
	}
}
void drawFile(char * fn,uint16_t x,uint8_t y){
	uint8_t l=strlen(fn);
	if(l>6){
		drawstrpart(fn,6,x,y);
		y+=8;
		fn+=6;
		l-=6;
	}
	drawstrpart(fn,l,x,y);
}
static void put_rc (FRESULT rc,uint16_t x,uint16_t y){
	const char *p;
	static const char str[] PROGMEM =
		"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
		"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
		"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
		"LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0";
	FRESULT i;

	for (p = str, i = 0; i != rc && pgm_read_byte_near(p); i++) {
		while(pgm_read_byte_near(p++));
	}
	tft_drawStringP(p,x,y,1,WHITE);
}
uint8_t listFiles(DIR * Dir){
	//first draw the Down arrow
	FRESULT res;
	FILINFO fno;
	uint8_t x,y,z;
	uint16_t yy,xx;
	char * fn;
	yy=0;
	tft_drawImageVf_P(arrow_icon,32,32,0,0);
	tft_drawImage_P(exit_icon,32,32,0,104);
	for(y=0;y<5;++y){
		xx=320;
		for(x=0;x<5;++x){
			PORTG|=1<<5;//turn on led turn disk access
			res = f_readdir(Dir, &fno);                   /* Read a directory item */
			PORTG&=~(1<<5);
            if (res != FR_OK || fno.fname[0] == 0) goto exitLoop;  /* Break on error or end of dir */
			fn = fno.fname;
			if(fno.fattrib & AM_DIR){                    /* It is a directory */
				/*sprintf(&path[i], "/%s", fn);
				res = scan_files(path);
				if (res != FR_OK) break;
				path[i] = 0;*/
				tft_drawImage_P(folder_icon,32,25,xx-32,yy);
				drawFile(fn,xx,yy+25);
            }else{/* It is a file. */
				//check extention
				char * ext=strrchr(fn,'.');
				++ext;
				if(strcmp("JPG",ext)==0)
					tft_drawImage_P(jpeg_icon,24,32,xx-24,yy);
				else
					tft_drawImage_P(unkownfile_icon,24,32,xx-24,yy);
				drawFile(fn,xx,yy+32);
				//printf("%s/%s\n", path, fn);
            }
            xx-=54;
		}
		yy+=48;
	}
	tft_drawImage_P(arrow_icon,32,32,0,208);
	return 1;
exitLoop:
	put_rc(res,yy,xx);
	return 0;
}
uint8_t skipFiles(DIR * Dir,uint16_t amount){
	if(amount==0)
		return 0;
	{
		FRESULT res;
		FILINFO fno;
		uint16_t x;
		for(x=0;x<amount;++x){
			PORTG|=1<<5;//turn on led turn disk access
			res = f_readdir(Dir, &fno);                   /* Read a directory item */
			PORTG&=~(1<<5);
			if (res != FR_OK || fno.fname[0] == 0) return 1;  /* Break on error or end of dir */
		}
	}
	return 0;
}
FIL File;
/* User defined call-back function to input JPEG data */
static UINT tjd_input (
	JDEC* jd,		/* Decompression object */
	BYTE* buff,		/* Pointer to the read buffer (NULL:skip) */
	UINT nd			/* Number of bytes to read/skip from input stream */
)
{
	WORD rb;
	jd = jd;	/* Suppress warning (device identifier is not needed in this appication) */
	if (buff) {	/* Read nd bytes from the input strem */
		PORTG|=1<<5;
		f_read(&File,buff, nd, &rb);
		PORTG&=~(1<<5);
		return rb;	/* Returns number of bytes could be read */

	} else {	/* Skip nd bytes on the input stream */
		return (f_lseek(&File,File.fptr + nd) == FR_OK) ? nd : 0;
	}
}



/* User defined call-back function to output RGB bitmap */
static UINT tjd_output (
	JDEC* jd,		/* Decompression object of current session */
	void* bitmap,	/* Bitmap data to be output */
	JRECT* rect		/* Rectangular region to output */
)
{
	jd = jd;	/* Suppress warning (device identifier is not needed in this appication) */

	/* Check user interrupt at left end */
	//if (!rect->left && uart_test()) return 0;	/* Abort to decompression */

	/* Put the rectangular into the display device */
	//disp_blt(rect->left, rect->right, rect->top, rect->bottom, (uint16_t*)bitmap);
	tft_drawImage(bitmap,rect->right-rect->left+1,rect->bottom-rect->top+1,320-rect->left,rect->top);
	return 1;	/* Continue to decompression */
}
void waitTouchUP(void){
	uint16_t x,y,z;
	do{
		getPoint(&x,&y,&z);
	}while(z<10);
	do{
		getPoint(&x,&y,&z);
	}while(z>10);//Wait for release of touch screen
}
void loadJpeg(char * path,char * fn){
	JDEC jd;
	JRESULT rc;
	char fp[256];
	if(path[0]==0){
		fp[0]='/';
		fp[1]=0;
	}else
		fp[0]=0;
	strcat(fp,path);
	strcat(fp,fn);
	f_open(&File,fp,FA_READ);
	uint8_t work[3092];
	rc=jd_prepare(&jd, tjd_input, work, 3092, 0);
	if (rc != JDR_OK){
		char buf[16];
		itoa(rc,buf,10);
		tft_drawStringP(PSTR("Error:"),64,320,1,WHITE);
		tft_drawString(buf,72,320,1,WHITE);
		waitTouchUP();
		f_close(&File);
	}
	uint8_t scale;
	uint16_t x,y,z;
	if((jd.width>2560)||(jd.height>1920)){
		f_close(&File);
		tft_drawStringP(PSTR("Error jpeg too large"),64,320,1,WHITE);
		waitTouchUP();
		return;
	}
	for (scale=0;scale<3;++scale){
			if ((jd.width >> scale) <= 320 && (jd.height >> scale) <= 240) break;
	}
	jd_decomp(&jd, tjd_output, scale);	/* Start to decompress */
	f_close(&File);
	do{
		getPoint(&x,&y,&z);
	}while(z<10);
}
void browserSD(void){
	FRESULT res;
	DIR Dir;
	char currentDir[256];
	currentDir[0]=0;
	res=f_opendir(&Dir,currentDir);//each file takes 48x48 pixels so I can fit 6*5 files per screen
	if(res!=FR_OK)
		put_rc(res,64,320);
	uint16_t x,y,z;
	uint8_t exit,canDown;
	exit=1;
	uint16_t files=0;
	while(exit){
		/*tft_drawString(currentDir,0,320,2,WHITE);
		do{
			getPoint(&x,&y,&z);
		}while(z<10);//Wait for release of touch screen
		do{
			getPoint(&x,&y,&z);
		}while(z>10);//Wait for release of touch screen
		tft_paintScreenBlack();*/
		canDown=listFiles(&Dir);//1 if more files 0 if no more files
		while(1){
			do{
				getPoint(&x,&y,&z);
			}while(z<10);
			if(y<=32){
				if(x<=32){
					if(files!=0){
						f_closedir(&Dir);
						res=f_opendir(&Dir,currentDir);
						if(res!=FR_OK)
							put_rc(res,64,320);
						files-=25;
						skipFiles(&Dir,files);
						break;
					}
				}else if((x>=104)&&(x<=136)){
					//see if the file browser should be closed or if the folder should be exited
					if(currentDir[1]!=0){
						f_closedir(&Dir);
						char * c=strrchr(currentDir,'/');
						memset(c,0,strlen(c));
						res=f_opendir(&Dir,currentDir);
						if(res!=FR_OK)
							put_rc(res,64,320);
					}else
						exit=0;
					break;
				}else if(x>=208){
					if(canDown){
						files+=25;
						break;
					}
				}
			}else{
				y=320-y;//really should be called x
				y/=54;
				if(y<5){
					x/=48;//really should be called y
					if(x<5){
						//process file
						tft_fillRectangle(64,32,16,32,BLACK);
						char temp[6];
						utoa(x,temp,10);
						tft_drawString(temp,64,32,1,WHITE);
						utoa(y,temp,10);
						tft_drawString(temp,72,32,1,WHITE);
						f_closedir(&Dir);
						res=f_opendir(&Dir,currentDir);
						if(res!=FR_OK)
							put_rc(res,64,320);
						skipFiles(&Dir,files+y+(x*5));
						FILINFO fno;
						PORTG|=1<<5;
						res = f_readdir(&Dir, &fno);
						PORTG&=~(1<<5);
						char * fn=fno.fname;
						if(fno.fattrib & AM_DIR){                    /* It is a directory */
							//go in folder
							f_closedir(&Dir);
							strcat(currentDir,"/");
							strcat(currentDir,fn);
							res=f_opendir(&Dir,currentDir);
							if(res!=FR_OK)
								put_rc(res,64,320);
							files=0;
							break;
						}else{/* It is a file. */
							//check extention
							tft_paintScreenBlack();
							tft_drawString(fn,0,320,3,WHITE);
							do{
								getPoint(&x,&y,&z);
							}while(z<10);
							do{
								getPoint(&x,&y,&z);
							}while(z>10);//Wait for release of touch screen
							char * ext=strrchr(fn,'.');
							++ext;
							if(strcmp("JPG",ext)==0)
								loadJpeg(currentDir,fn);
							//return files back to correct position
							f_closedir(&Dir);
							res=f_opendir(&Dir,currentDir);
							if(res!=FR_OK)
								put_rc(res,64,320);
							skipFiles(&Dir,files);
							break;
							//printf("%s/%s\n", path, fn);
						}
					}
				}
			}
			do{
				getPoint(&x,&y,&z);
			}while(z>10);//Wait for release of touch screen
		}
		tft_paintScreenBlack();
	}
	f_closedir(&Dir);
}
