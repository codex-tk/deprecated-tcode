#ifndef __tcode_threading_spinlock_h__
#define __tcode_threading_spinlock_h__

#include <tcode/tcode.hpp>

#if defined( TCODE_WIN32 )
#elif defined( TCODE_APPLE )
#include <libkern/OSAtomic.h>
#else
#include <pthead.h>
#endif

namespace tcode { namespace threading {

    class spinlock{
    public:
        spinlock( void );
        ~spinlock( void );

        void lock( void );
        void unlock( void );
        bool trylock( void );
    private:
#if defined( TCODE_WIN32 )
        CRITICAL_SECTION _lock;
#elif defined( TCODE_APPLE )
        OSSpinLock _lock;
#else
        pthread_spinlock_t _lock;
#endif
    };

}}

#endif
