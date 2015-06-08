/**
 *    @file       shortlistentask.cpp
 *
 *    @brief      
 *    @details    
 *
 *    @version    1.0
 *    @date       2015/4/28
 *
 *    @author     zhangjiaxing, zhangjiaxing@baofeng.com
 */
#include "shortlistentask.h"
#include "workthread.h"
#include "myepoll.h"
#include "netprocessthread.h"
#include "myserver.h"
#include "recvtask.h"
#include "commmsg.h"
#include "recvandsendtask.h"
#include "listenthread.h"

CShortListenTask::CShortListenTask(CTcpSocket *pSocket,CNetProcessThread *pWorkThread, int nTaskType, int nSendMsgType)
		:CSocketTask(pSocket,  pWorkThread)
{
	m_iTaskType = nTaskType; 
	m_nSendMsgType  = nSendMsgType;
	m_nTaskTimeOut = -1;
}

CShortListenTask::~CShortListenTask()
{
}

int CShortListenTask::Init()
{
	AddToEpoll();
	return 0;
}

int CShortListenTask::Process(CMsg *pMsg)
{
	//不作处理
	return 0;
}

int CShortListenTask::HandleProcess(int events)
{
	//CListenThread *pThread = (CListenThread*)m_pThread;
	//pThread->GetListenBlock()->mutex.Lock();
	if (events&(MYPOLLIN|MYPOLLRDNORM))
	{
		struct sockaddr_in tmpAddr;
		memset(&tmpAddr, 0, sizeof(sockaddr_in));
		int iSocketSize=sizeof(sockaddr_in);

		while (1)
		{
			int iSocket = accept(m_pTcpSocket->GetSocket(), (struct sockaddr *)&tmpAddr, (socklen_t*)&iSocketSize);
			if (iSocket > 0)
			{
				WRITE_TRACE("Recv a client connect request");
				CMyserver::GetInstance()->GetCommonStat().AddNormalStat(RECVPKGNUMSTAT);

				if (CTcpSocket::GetNum() > CMyserver::GetInstance()->GetBaseConf().GetConfigItem().nMaxFdSize) //如果太忙了， 不再接入
				{
					WriteRunInfo::WriteInfoRun("The fdsize is %d, greater than %d, close the connect", CTcpSocket::GetNum(),
					CMyserver::GetInstance()->GetBaseConf().GetConfigItem().nMaxFdSize);
					close(iSocket);
					continue;
				}
				CTimeStat timestat;
				timestat.Begin(); //打印开始的时间

				if (CMyserver::GetInstance()->GetBaseConf().GetConfigItem().nRecvTaskType == 0) //发到收发线程中去
				{
					CRecvMsgBody *pBody = new CRecvMsgBody(iSocket, tmpAddr);
					pBody->SetTimeStat(timestat);
					m_pThread->SendMsg(RECVANDSENDMSGENTITY, RANDOMENTITYID, 0, m_nSendMsgType, pBody, 0);
				}
				else //发到本线程
				{
					CTcpSocket *pSocket = new CTcpSocket(tmpAddr, iSocketSize, iSocket);
					//生成一个接收数据的任务
					CSocketTask *pRecvTask = NULL;
					pRecvTask = CMyserver::GetInstance()->GetClassFactory()->GenSocketTask((CNetProcessThread*)m_pThread,  
					pSocket, RECVTASK);
					if (pRecvTask != NULL)
					{
						pRecvTask->Init();
						pRecvTask->SetTimeStat(timestat);
					}
					else
					{
						close(iSocket);
					}
				}
			}
			else  //句柄accept完毕
			{
				break;
			}
		}
	}
	else
	{
		WriteRunInfo::WriteRunLog("Receive invalid event %d", events);
	}
	// pThread->GetListenBlock()->mutex.UnLock();

	return 0;
}

int CShortListenTask::PutMsgToSendList(const char *pBuf, int nLen)
{
	return 0;
}

int CShortListenTask::DestroySelf()
{
	delete this;
	return 0;
}

int CShortListenTask::AddToEpoll()
{
	int tmpEvent = MYPOLLIN|MYPOLLRDNORM;
	CNetProcessThread* pNetThread = (CNetProcessThread*)m_pThread;
	CEpoll *pEoll = pNetThread->GetEpoll();
	pEoll->AddToEpoll(this, tmpEvent);
	return 0;
}
