#include "StdAfx.h"
#include "BufferedSocket.h"

#include <iostream>
using namespace std;

CBufferedSocket::CBufferedSocket(void) : m_object( NULL ), m_bSocketOpen( false ), m_bThreadActive( false ) {
  m_hReceiveThread = CreateThread( NULL, 10000, ReceiveThread, this, CREATE_SUSPENDED, &m_ReceiveThreadId );
  if ( NULL == m_hReceiveThread ) {
    DWORD err = GetLastError();
    cout << "CreateThread error " << err << endl;
  }
}

CBufferedSocket::~CBufferedSocket(void) {
  BOOL bIOIsPending, bReturnValue;
  DWORD dwThreadExitCode;
  bReturnValue = GetExitCodeThread( m_hReceiveThread, &dwThreadExitCode );
  ASSERT( 0 != bReturnValue );
  if ( STILL_ACTIVE == dwThreadExitCode ) {
    std::cout << "CBufferedSocket recieve thread is still active" << std::endl;
    bReturnValue = GetThreadIOPendingFlag( m_hReceiveThread, &bIOIsPending );
    ASSERT( 0 != bReturnValue );
    if ( bIOIsPending ) {
      std::cout << "CBufferedSocket has IO Pending" << std::endl;
    }
    TerminateThread( m_hReceiveThread, 0 );
  }
}

//http://msdn2.microsoft.com/en-us/library/ms740668.aspx

DWORD WINAPI CBufferedSocket::ReceiveThread( LPVOID lpParameter ) {

  CBufferedSocket *bs = ( CBufferedSocket * ) lpParameter;

  bs->m_bThreadActive = true;

  char buf[DATA_BUFSIZE];
  WSAOVERLAPPED AcceptOverlapped;
  WSABUF DataBuf;
  DWORD EventTotal = 0, 
    //RecvBytes = 0, 
    Flags = 0,
    BytesTransferred = 0, 
    CallBack = 0;

  char str[DATA_BUFSIZE];  // string built from taking characters from buf
  unsigned short ixSrc = 0;  // position for next character in str
  unsigned short ixDst = 0;
  char ch;

  HANDLE hEvent = WSACreateEvent();

  DWORD ix;
  DataBuf.len = DATA_BUFSIZE;
  DataBuf.buf = buf;
  while ( 1 ) {
    Flags = 0;
    BytesTransferred = 0;
    ZeroMemory(&AcceptOverlapped, sizeof(WSAOVERLAPPED));
    AcceptOverlapped.hEvent = hEvent;
    int nError = WSARecv( bs->m_socket, &DataBuf, 1, &BytesTransferred, &Flags, &AcceptOverlapped, NULL );
    if ( 0 != nError ) {
      if ( SOCKET_ERROR == nError ) {
        int e = WSAGetLastError();
        if ( WSA_IO_PENDING != e ) {  // an error which isn't an error
          //std::cerr << "WSARecv error " << e << std::endl;
          break;
        }
      }
      else {
        int i = 1;
        break;
      }
    }

    if ( 0 != BytesTransferred ) {
      int i = 10;
    }
    else {
      ix = WSAWaitForMultipleEvents( 1, &hEvent, FALSE, WSA_INFINITE, FALSE );
      WSAResetEvent( hEvent );
      WSAGetOverlappedResult( bs->m_socket, &AcceptOverlapped, &BytesTransferred, FALSE, &Flags );
    }

    if ( 0 == BytesTransferred ) { // connection has been closed
      break;  // end while loop, exit RecieveThread, and close out thread
    }
    else {
      // Process data in buffer here.
      for ( ixSrc = 0; ixSrc < BytesTransferred; ixSrc ++ ) {
        ch = buf[ ixSrc ];
        if ( 0x0a == ch ) {
          str[ ixDst++ ] = 0x00;  
          ASSERT( DATA_BUFSIZE > ixDst );
          if ( NULL != bs->OnNewResponse ) bs->OnNewResponse( ixDst, str, bs->m_object );
          ixDst = 0;
        }
        else {
          if ( 0x0d == ch ) {
            // ignore character
          }
          else {
            str[ ixDst++ ] = ch;  // need to test for buffer overflow
            ASSERT( DATA_BUFSIZE > ixDst );
          }
        }
      }
    }
  }
  bs->m_bThreadActive = false;
  return 0;
}

void CBufferedSocket::Open( const char *pAddress, unsigned short Port ) {

  sockaddr_in service;
  service.sin_family = AF_INET;
  service.sin_port = htons( Port );
  service.sin_addr.S_un.S_addr = inet_addr( pAddress );

  // do try catch around this sometime

  m_socket = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );
  if ( INVALID_SOCKET == m_socket ) {
    int e = WSAGetLastError();
    cout << "WSASocket error 1: " << e << endl;
  }

  if ( SOCKET_ERROR == WSAConnect( m_socket, (SOCKADDR*) &service, sizeof( service ), NULL, NULL, NULL, NULL ) ) {
    int e = WSAGetLastError();
    cout << "WSAConnect error 2: " << e  << endl;
  }

  m_bSocketOpen = true;
  ResumeThread( m_hReceiveThread );
}

void CBufferedSocket::Send( const char *pCommand ) {
  WSABUF buf;
  buf.buf = (char*) pCommand;
  buf.len = (u_long) strlen( pCommand );
  m_sLastCommand.assign( buf.buf, buf.len );
  m_cntBytesSent = m_Flags = 0;
  if ( SOCKET_ERROR == WSASend( m_socket, &buf, 1, &m_cntBytesSent, m_Flags, NULL, NULL ) ) {
    int e = WSAGetLastError();
    cout << "WSASend error 3: " << e << endl;
  }
}

void CBufferedSocket::Close( void ) {
  closesocket( m_socket );
  unsigned short nLoopCount = 10;
  while ( 0 != nLoopCount ) {
    Sleep(100);
    if ( !m_bThreadActive ) {
      break;
    }
    --nLoopCount;
  }
  if ( 0 != nLoopCount ) {
    std::cout << "CBufferedSocket thread is still active" << std::endl;
  }

  // todo:  check that buffer is empty
  
  //TerminateThread( hReceiveThread ); // need to use ExitThread in object instead !!!!!!
  
  
  //WSACloseEvent(EventArray[Index - WSA_WAIT_EVENT_0]);

}

  //char* ip;
  //hostent* thisHost;
  //thisHost = gethostbyname("localhost");
  //ip = inet_ntoa (*(struct in_addr *)*thisHost->h_addr_list);

  //sockaddr_in service;
  //service.sin_family = AF_INET;
  //service.sin_addr.S_un.S_addr = inet_addr( "10.10.10.104" );
  //service.sin_addr.S_un.S_addr = *((u_long*) *thisHost->h_addr_list);
  //service.sin_port = htons(port);
