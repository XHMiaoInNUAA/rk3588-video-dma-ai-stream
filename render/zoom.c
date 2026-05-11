#include <config.h>
#include <pic_operation.h>
#include <stdlib.h>
#include <string.h>


/**********************************************************************
 * �������ƣ� PicZoom
 * ���������� ����ȡ����ֵ��������ͼƬ
 *            ע��ú���������ڴ���������ź��ͼƬ,�����Ҫ��free�����ͷŵ�
 *            "����ȡ����ֵ"��ԭ����ο�����"lantianyu520"������"ͼ�������㷨"
 * ��������� ptOriginPic - �ں�ԭʼͼƬ����������
 *            ptBigPic    - �ں����ź��ͼƬ����������
 * ��������� ��
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
int PicZoom(PT_PixelDatas ptOriginPic, PT_PixelDatas ptZoomPic)
{
    DBG_PRINTF("22222222\n");
    unsigned long dwDstWidth = ptZoomPic->iWidth;
    unsigned long* pdwSrcXTable;
	unsigned long x;
	unsigned long y;
	unsigned long dwSrcY;
	unsigned char *pucDest;
	unsigned char *pucSrc;
	unsigned long dwPixelBytes = ptOriginPic->iBpp/8;

    DBG_PRINTF("src:\n");
    DBG_PRINTF("%d x %d, %d bpp, data: 0x%x\n", ptOriginPic->iWidth, ptOriginPic->iHeight, ptOriginPic->iBpp, (unsigned int)ptOriginPic->aucPixelDatas);

    DBG_PRINTF("dest:\n");
    DBG_PRINTF("%d x %d, %d bpp, data: 0x%x\n", ptZoomPic->iWidth, ptZoomPic->iHeight, ptZoomPic->iBpp, (unsigned int)ptZoomPic->aucPixelDatas);

	if (ptOriginPic->iBpp != ptZoomPic->iBpp)
	{
		return -1;
	}

    pdwSrcXTable = malloc(sizeof(unsigned long) * dwDstWidth);
    if (NULL == pdwSrcXTable)
    {
        DBG_PRINTF("malloc error!\n");
        return -1;
    }

    for (x = 0; x < dwDstWidth; x++)//���ɱ� pdwSrcXTable
    {
        pdwSrcXTable[x]=(x*ptOriginPic->iWidth/ptZoomPic->iWidth);
    }

    for (y = 0; y < ptZoomPic->iHeight; y++)
    {			
        dwSrcY = (y * ptOriginPic->iHeight / ptZoomPic->iHeight);

		pucDest = ptZoomPic->aucPixelDatas + y*ptZoomPic->iLineBytes;
		pucSrc  = ptOriginPic->aucPixelDatas + dwSrcY*ptOriginPic->iLineBytes;
		
        for (x = 0; x <dwDstWidth; x++)
        {
            /* ԭͼ����: pdwSrcXTable[x]��srcy
             * ��������: x, y
			 */
			 memcpy(pucDest+x*dwPixelBytes, pucSrc+pdwSrcXTable[x]*dwPixelBytes, dwPixelBytes);
        }
    }

    free(pdwSrcXTable);
	return 0;
}

