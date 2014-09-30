#ifndef WXMULTITHREADHELPER_H
#define WXMULTITHREADHELPER_H

#include <wx/thread.h>
#include <wx/vector.h>


// wxMultiThreadHelperThread class
// --------------------------
class wxMultiThreadHelper;
class wxMultiThreadHelperThread : public wxThread
{
public:
    // constructor only creates the C++ thread object 
    // and doesn't create (or start) the real thread
    wxMultiThreadHelperThread( wxMultiThreadHelper& owner )
        : wxThread( wxTHREAD_JOINABLE ), m_owner( owner )
    { }

protected:
    // entry point for the thread -- calls Entry() in owner.
    virtual void *Entry();

private:
    // the owner of the thread
    wxMultiThreadHelper& m_owner;

    // no copy ctor/assignment operator
    wxMultiThreadHelperThread( const wxMultiThreadHelperThread& );
    wxMultiThreadHelperThread& operator=( const wxMultiThreadHelperThread& );
};


// ----------------------------------------------------------------------------
// wxMultiThreadHelper: this class implements the threading logic to run a
// background task in another object (such as a window).  It is a mix-in: just
// derive from it to implement a threading background task in your class.
// ----------------------------------------------------------------------------
class wxMultiThreadHelper
{
public:
    // destructor deletes m_threads
    virtual ~wxMultiThreadHelper()
    {
        for( size_t i = 0; i < m_threads.size(); ++i )
        {
            if( NULL != m_threads[i] )
            {
                if( m_threads[i]->IsRunning() )
                {
                    m_threads[i]->Wait();
                }

                delete m_threads[i];
            }
        }
    }

    // create a new thread (and optionally set the stack size on platforms that
    // support/need that), call Run() to start it
    wxThreadError CreateThread( unsigned int stackSize = 0 )
    {
        m_threads.push_back( new wxMultiThreadHelperThread( *this ) );
        return m_threads.back()->Create(stackSize);
    }

    // entry point for the thread - called by Run() and executes in the context
    // of this thread.
    virtual void* Entry() = 0;

    // returns a pointer to the thread which can be used to call Run()
    wxVector< wxThread* >& GetThreads()
    {
        return m_threads;
    }

protected:
    wxVector< wxThread* > m_threads;

    friend class wxMultiThreadHelperThread;
};


// call Entry() in owner, put it down here to avoid circular declarations
inline void* wxMultiThreadHelperThread::Entry()
{
    return m_owner.Entry();
}

#endif