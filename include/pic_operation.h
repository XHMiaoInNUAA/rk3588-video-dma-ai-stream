#ifndef _PIC_OPERATION_H
#define _PIC_OPERATION_H


typedef struct PixelDatas {
	int iWidth;   
	int iHeight;  
	int iBpp;     
	int iLineBytes;  
	int iTotalBytes; 
	unsigned char *aucPixelDatas; 
}T_PixelDatas, *PT_PixelDatas;


#endif /* _PIC_OPERATION_H */

