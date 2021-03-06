#ifndef __tcode_io_buffer_h__
#define __tcode_io_buffer_h__

#if defined( TCODE_WIN32 )

#else
#include <sys/uio.h>
#endif

namespace tcode { namespace io {
#if defined( TCODE_WIN32 )
    typedef WSABUF iovec;
#endif

	/*!
		@class buffer
		@brief 
	*/
    class buffer : public iovec {
    public:
        buffer( void );
		//! 
        buffer( const iovec& iov );
		//! 
        buffer( const buffer& rhs );
		//! 
        buffer( char* buf , int len );
		//! 
        buffer& operator=( const buffer& rhs );
		//! 
        ~buffer( void );
		//! 
        char* buf(void) const;
		//! 
        int len(void) const;
		//! 
        void set( char* buf , int len );
		//! 
        void skip( int n );
    };

}}

#endif
