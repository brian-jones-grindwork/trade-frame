#include "StdAfx.h"
#include "SimulationSymbol.h"

//#include "HDF5DataManager.h"
//using namespace H5;

#include "HDF5TimeSeriesContainer.h"
#include "HDF5IterateGroups.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define WM_QUOTEEVENT ( WM_GUITHREADCROSSING + 1)
#define WM_TRADEEVENT ( WM_GUITHREADCROSSING + 2)

IMPLEMENT_DYNAMIC(CSimulationSymbol, CGuiThreadCrossing)

// sDirectory needs to be available on instantiation to enable signal availability
CSimulationSymbol::CSimulationSymbol( const std::string &sSymbol, const std::string &sDirectory) 
: CSymbol(sSymbol), CGuiThreadCrossing(), m_sDirectory( sDirectory )
{
  m_pMainThread = AfxGetThread(); // comparison for crossing
  m_hQuoteEventSignal = CreateEvent( NULL, FALSE, TRUE, "" );
  assert( NULL != m_hQuoteEventSignal );
  m_hTradeEventSignal = CreateEvent( NULL, FALSE, TRUE, "" );
  assert( NULL != m_hTradeEventSignal );

  // this is dealt with in the SimulationProvider, but we don't have a .Remove
  //m_OnTrade.Add( MakeDelegate( &m_simExec, &CSimulateOrderExecution::NewTrade ) );
}

CSimulationSymbol::~CSimulationSymbol(void) {
  // we don't yet have a .Remove for this in SimulationProvider yet.
  //m_OnTrade.Remove( MakeDelegate( &m_simExec, &CSimulateOrderExecution::NewTrade ) );

  CloseHandle( m_hQuoteEventSignal );
  CloseHandle( m_hTradeEventSignal );
}

void CSimulationSymbol::StartTradeWatch( void ) {
  std::string sPath( m_sDirectory + "/trades/" + m_sSymbolName );
  CHDF5TimeSeriesContainer<CTrade> tradeRepository( sPath );
  CHDF5TimeSeriesContainer<CTrade>::iterator begin, end;
  begin = tradeRepository.begin();
  end = tradeRepository.end();
  m_trades.Resize( end - begin );
  tradeRepository.Read( begin, end, &m_trades );
}

void CSimulationSymbol::StopTradeWatch( void ) {
}

void CSimulationSymbol::StartQuoteWatch( void ) {
  std::string sPath( m_sDirectory + "/quotes/" + m_sSymbolName );
  CHDF5TimeSeriesContainer<CQuote> quoteRepository( sPath );
  CHDF5TimeSeriesContainer<CQuote>::iterator begin, end;
  begin = quoteRepository.begin();
  end = quoteRepository.end();
  m_quotes.Resize( end - begin );
  quoteRepository.Read( begin, end, &m_quotes );
}

void CSimulationSymbol::StopQuoteWatch( void ) {
}

void CSimulationSymbol::StartDepthWatch( void ) {
}

void CSimulationSymbol::StopDepthWatch( void ) {
}

//bool CSimluationSymbol::AddTradeHandler( CSymbol::tradehandler_t handler ) {
//  return CSymbol::AddTradeHandler( handler );
//}

//bool CSimulationSymbol::RemoveTradeHandler( CSymbol::tradehandler_t handler ) {
//  return CSymbol::RemoveTradeHandler( handler );
//}

void CSimulationSymbol::HandleQuoteEvent( const CDatedDatum &datum ) {
  CWinThread *pThread = AfxGetThread();
  if ( m_pMainThread == pThread ) {
    m_OnQuote( dynamic_cast<const CQuote &>( datum ) ); 
    BOOL b = SetEvent( m_hQuoteEventSignal );   // get more thread overlap with set here
    assert( b );
  }
  else {
    // need a lock here if entered before previous conversion completion
    // remember that this is a delayed thing, and datum has to be valid through out the cycle
    DWORD dw = WaitForSingleObject( m_hQuoteEventSignal, INFINITE);  // helps to keep event queue minimal
    assert( WAIT_OBJECT_0 == dw );
    BOOL b = ::PostMessage( CWnd::m_hWnd, WM_QUOTEEVENT, reinterpret_cast<WPARAM>( &datum ), reinterpret_cast<LPARAM>( this ) );
    assert( b ); 
  }
}

void CSimulationSymbol::HandleTradeEvent( const CDatedDatum &datum ) {
  CWinThread *pThread = AfxGetThread();
  if ( m_pMainThread == pThread ) {
    m_OnTrade( dynamic_cast<const CTrade &>( datum ) );  
    BOOL b = SetEvent( m_hTradeEventSignal ); // set after so trade is available through full sequence
    assert( b );
  }
  else {
    // need a lock here if entered before previous conversion completion
    // remember that this is a delayed thing, and datum has to be valid through out the cycle
    DWORD dw = WaitForSingleObject( m_hTradeEventSignal, INFINITE);
    assert( WAIT_OBJECT_0 == dw );
    BOOL b = ::PostMessage( CWnd::m_hWnd, WM_TRADEEVENT, reinterpret_cast<WPARAM>( &datum ), reinterpret_cast<LPARAM>( this ) );
    assert( b );
  }
}

BEGIN_MESSAGE_MAP(CSimulationSymbol, CGuiThreadCrossing)
  ON_MESSAGE( WM_QUOTEEVENT, OnCrossThreadArrivalQuoteEvent )
  ON_MESSAGE( WM_TRADEEVENT, OnCrossThreadArrivalTradeEvent )
END_MESSAGE_MAP()

LRESULT CSimulationSymbol::OnCrossThreadArrivalQuoteEvent( WPARAM w, LPARAM l ) {
  CSimulationSymbol *pSym = reinterpret_cast<CSimulationSymbol *>( l );
  pSym->HandleQuoteEvent( *(reinterpret_cast<const CDatedDatum *>( w ) ) );
  return 1;
}

LRESULT CSimulationSymbol::OnCrossThreadArrivalTradeEvent( WPARAM w, LPARAM l ) {
  CSimulationSymbol *pSym = reinterpret_cast<CSimulationSymbol *>( l );
  pSym->HandleTradeEvent( *(reinterpret_cast<const CDatedDatum *>( w ) ) );
  return 1;
}
