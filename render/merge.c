
#include <pic_operation.h>
#include <string.h>

/**********************************************************************
 * �������ƣ� PicMerge
 * ���������� ��СͼƬ�ϲ����ͼƬ��
 * ��������� iX,iY      - СͼƬ�ϲ����ͼƬ��ĳ������, iX/iYȷ�������������Ͻ�����
 *            ptSmallPic - �ں�СͼƬ����������
 *            ptBigPic   - �ں���ͼƬ����������
 * ��������� ��
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
int PicMerge(int iX, int iY, PT_PixelDatas ptSmallPic, PT_PixelDatas ptBigPic)
{
	int i;
	unsigned char *pucSrc;
	unsigned char *pucDst;
	
	if ((ptSmallPic->iWidth > ptBigPic->iWidth)  ||
		(ptSmallPic->iHeight > ptBigPic->iHeight) ||
		(ptSmallPic->iBpp != ptBigPic->iBpp))
	{
		return -1;
	}

	pucSrc = ptSmallPic->aucPixelDatas;
	pucDst = ptBigPic->aucPixelDatas + iY * ptBigPic->iLineBytes + iX * ptBigPic->iBpp / 8;
	for (i = 0; i < ptSmallPic->iHeight; i++)
	{
		memcpy(pucDst, pucSrc, ptSmallPic->iLineBytes);
		pucSrc += ptSmallPic->iLineBytes;
		pucDst += ptBigPic->iLineBytes;
	}
	return 0;
}


/**********************************************************************
 * �������ƣ� PicMergeRegion
 * ���������� ����ͼƬ��ĳ����, �ϲ�����ͼƬ��ָ������
 * ��������� iStartXofNewPic, iStartYofNewPic : ����ͼƬ��(iStartXofNewPic, iStartYofNewPic)���괦��ʼ�����������ںϲ�
 *            iStartXofOldPic, iStartYofOldPic : �ϲ�����ͼƬ��(iStartXofOldPic, iStartYofOldPic)����ȥ
 *            iWidth, iHeight                  : �ϲ�����Ĵ�С
 *            ptNewPic                         : ��ͼƬ
 *            ptOldPic                         : ��ͼƬ
 * ��������� ��
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���          �޸�����
 * -----------------------------------------------
 * 2013/02/08        V1.0     Τ��ɽ          ����
 ***********************************************************************/
int PicMergeRegion(int iStartXofNewPic, int iStartYofNewPic, int iStartXofOldPic, int iStartYofOldPic, int iWidth, int iHeight, PT_PixelDatas ptNewPic, PT_PixelDatas ptOldPic)
{
	int i;
	unsigned char *pucSrc;
	unsigned char *pucDst;
    int iLineBytesCpy = iWidth * ptNewPic->iBpp / 8;

    if ((iStartXofNewPic < 0 || iStartXofNewPic >= ptNewPic->iWidth) || \
        (iStartYofNewPic < 0 || iStartYofNewPic >= ptNewPic->iHeight) || \
        (iStartXofOldPic < 0 || iStartXofOldPic >= ptOldPic->iWidth) || \
        (iStartYofOldPic < 0 || iStartYofOldPic >= ptOldPic->iHeight))
    {
        return -1;
    }
	
	pucSrc = ptNewPic->aucPixelDatas + iStartYofNewPic * ptNewPic->iLineBytes + iStartXofNewPic * ptNewPic->iBpp / 8;
	pucDst = ptOldPic->aucPixelDatas + iStartYofOldPic * ptOldPic->iLineBytes + iStartXofOldPic * ptOldPic->iBpp / 8;
	for (i = 0; i < iHeight; i++)
	{
		memcpy(pucDst, pucSrc, iLineBytesCpy);
		pucSrc += ptNewPic->iLineBytes;
		pucDst += ptOldPic->iLineBytes;
	}
	return 0;
}

