///////////////////////////////////////////////////////////////////////////////
// Name:        wxSortableMsgQueue.h
// Purpose:     Sortable message queue for inter-thread communication
// Author:      genpfault
// BasedOnWorkBy: Evgeniy Tarassov
// Created:     2014-09-22
// Copyright:   (C) 2007 TT-Solutions SARL
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SORTABLEMSGQUEUE_H_
#define _WX_SORTABLEMSGQUEUE_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/thread.h"

#if wxUSE_THREADS

#include "wx/stopwatch.h"

#include "wx/beforestd.h"
#include <algorithm>
#include <deque>
#include "wx/afterstd.h"

enum wxSortableMessageQueueError
{
    wxSORTABLEMSGQUEUE_NO_ERROR = 0, // operation completed successfully
    wxSORTABLEMSGQUEUE_TIMEOUT,      // no messages received before timeout expired
    wxSORTABLEMSGQUEUE_MISC_ERROR    // some unexpected (and fatal) error has occurred
};

// ---------------------------------------------------------------------------
// Sortable message queue allows passing message between threads.
//
// This class is typically used for communicating between the main and worker
// threads. The main thread calls Post() and the worker thread calls Receive().
//
// For this class a message is an object of arbitrary type T. Notice that
// typically there must be some special message indicating that the thread
// should terminate as there is no other way to gracefully shutdown a thread
// waiting on the message queue.
// ---------------------------------------------------------------------------
template <typename T>
class wxSortableMessageQueue
{
public:
    // The type of the messages transported by this queue
    typedef T Message;

    // Default ctor creates an initially empty queue
    wxSortableMessageQueue()
       : m_conditionNotEmpty(m_mutex)
    {
    }

    // Add a message to this queue and signal the threads waiting for messages.
    //
    // This method is safe to call from multiple threads in parallel.
    wxSortableMessageQueueError Post(const Message& msg)
    {
        wxMutexLocker locker(m_mutex);

        wxCHECK( locker.IsOk(), wxSORTABLEMSGQUEUE_MISC_ERROR );

        m_messages.push_back(msg);

        m_conditionNotEmpty.Signal();

        return wxSORTABLEMSGQUEUE_NO_ERROR;
    }

    // Remove all messages from the queue.
    //
    // This method is meant to be called from the same thread(s) that call
    // Post() to discard any still pending requests if they became unnecessary.
    wxSortableMessageQueueError Clear()
    {
        wxCHECK( IsOk(), wxSORTABLEMSGQUEUE_MISC_ERROR );

        wxMutexLocker locker(m_mutex);

        std::deque<T> empty;
        std::swap(m_messages, empty);

        return wxSORTABLEMSGQUEUE_NO_ERROR;
    }

    // Wait no more than timeout milliseconds until a message becomes available.
    //
    // Setting timeout to 0 is equivalent to an infinite timeout. See Receive().
    wxSortableMessageQueueError ReceiveTimeout(long timeout, T& msg)
    {
        wxCHECK( IsOk(), wxSORTABLEMSGQUEUE_MISC_ERROR );

        wxMutexLocker locker(m_mutex);

        wxCHECK( locker.IsOk(), wxSORTABLEMSGQUEUE_MISC_ERROR );

        const wxMilliClock_t waitUntil = wxGetLocalTimeMillis() + timeout;
        while ( m_messages.empty() )
        {
            wxCondError result = m_conditionNotEmpty.WaitTimeout(timeout);

            if ( result == wxCOND_NO_ERROR )
                continue;

            wxCHECK( result == wxCOND_TIMEOUT, wxSORTABLEMSGQUEUE_MISC_ERROR );

            const wxMilliClock_t now = wxGetLocalTimeMillis();

            if ( now >= waitUntil )
                return wxSORTABLEMSGQUEUE_TIMEOUT;

            timeout = (waitUntil - now).ToLong();
            wxASSERT(timeout > 0);
        }

        msg = m_messages.front();
        m_messages.pop();

        return wxSORTABLEMSGQUEUE_NO_ERROR;
    }

    // Same as ReceiveTimeout() but waits for as long as it takes for a message
    // to become available (so it can't return wxSORTABLEMSGQUEUE_TIMEOUT)
    wxSortableMessageQueueError Receive(T& msg)
    {
        wxCHECK( IsOk(), wxSORTABLEMSGQUEUE_MISC_ERROR );

        wxMutexLocker locker(m_mutex);

        wxCHECK( locker.IsOk(), wxSORTABLEMSGQUEUE_MISC_ERROR );

        while ( m_messages.empty() )
        {
            wxCondError result = m_conditionNotEmpty.Wait();

            wxCHECK( result == wxCOND_NO_ERROR, wxSORTABLEMSGQUEUE_MISC_ERROR );
        }

        msg = m_messages.front();
        m_messages.pop_front();

        return wxSORTABLEMSGQUEUE_NO_ERROR;
    }

    // Return false only if there was a fatal error in ctor
    bool IsOk() const
    {
        return m_conditionNotEmpty.IsOk();
    }

    // Sort the message queue with the given comparison functor
    template< class Compare >
    wxSortableMessageQueueError Sort( Compare comp )
    {
        wxCHECK( IsOk(), wxSORTABLEMSGQUEUE_MISC_ERROR );

        wxMutexLocker locker(m_mutex);
        wxCHECK( locker.IsOk(), wxSORTABLEMSGQUEUE_MISC_ERROR );

        std::sort( m_messages.begin(), m_messages.end(), comp );

        return wxSORTABLEMSGQUEUE_NO_ERROR;
    }

private:
    // Disable copy ctor and assignment operator
    wxSortableMessageQueue(const wxSortableMessageQueue<T>& rhs);
    wxSortableMessageQueue<T>& operator=(const wxSortableMessageQueue<T>& rhs);

    mutable wxMutex m_mutex;
    wxCondition     m_conditionNotEmpty;

    std::deque<T>   m_messages;
};

#endif // wxUSE_THREADS

#endif // _WX_SORTABLEMSGQUEUE_H_
