#ifndef __tcode_io_kqueue_h__
#define __tcode_io_kqueue_h__

#include <tcode/io/pipe.hpp>
#include <tcode/io/buffer.hpp>
#include <tcode/function.hpp>
#include <tcode/time/timespan.hpp>
#include <tcode/operation.hpp>
#include <tcode/threading/spinlock.hpp>
#include <sys/event.h>

namespace tcode { namespace io {
namespace ip {

    class address;

    namespace tcp {
        class operation_connect_base;
        class operation_write_base;
        class operation_read_base;
        class operation_accept_base;
    }
   
    namespace udp {
        class operation_read_base;
        class operation_write_base;
    }
}
    class engine;
    /*!
     * @class kqueue
     * @brief
     */
    class kqueue {
    public:
        /*!
         * @class descriptor
         * @brief 
         */
        struct _descriptor;
        typedef _descriptor* descriptor;

        //! ctor
        kqueue( engine& e );
        //! dtor
        ~kqueue( void );

        //! run complete events
        int run( const tcode::timespan& ts );

        //! wakeup loop
        void wake_up( void );

        //!  
        bool bind( const descriptor& desc );
        //!  
        void unbind( descriptor& desc );
        //!
        void execute( tcode::operation* op ); 
    public:
        //!
        void connect( descriptor& desc 
                , ip::tcp::operation_connect_base* op );
        //!
        void write( descriptor desc 
                , ip::tcp::operation_write_base* op );
        //!
        void read( descriptor desc
                , ip::tcp::operation_read_base* op );
        //!
        bool listen( descriptor& desc 
                , const ip::address& addr );
        //!
        void accept( descriptor listen
                , int /* windows 대응 */ 
                , ip::tcp::operation_accept_base* op );
    public:
        //!
        bool open( descriptor& desc
                , int af , int type , int proto );

        //!
        bool bind( descriptor& desc
                , const ip::address& addr );
        //!
        void write( descriptor& desc 
                , ip::udp::operation_write_base* op );
        //!
        void read( descriptor desc
                , ip::udp::operation_read_base* op );

    public:
        //! post operation write
        int writev( descriptor desc , tcode::io::buffer* buf , int cnt 
                , std::error_code& ec );
        //! post operation read
        int readv( descriptor desc , tcode::io::buffer* buf , int cnt 
                , std::error_code& ec ); 
        //! post operation accept
        int accept( descriptor listen 
                , descriptor& accepted 
                , ip::address& addr
                , std::error_code& ec );

        //! post operation udp read
        int read( descriptor desc 
                , tcode::io::buffer& buf 
                , tcode::io::ip::address& addr 
                , std::error_code& ec );

        //! post operatino udp write
        int write( descriptor desc 
                , const tcode::io::buffer& buf 
                , const tcode::io::ip::address& addr 
                , std::error_code& ec);
        int native_descriptor( descriptor desc );
    private:
        void op_add( tcode::operation* op );
        void op_run( tcode::operation* op );
        void _change( int id , int filt , int flag , void* p );
       
        descriptor descriptor_create(int fd);
		void descriptor_add_ref(descriptor d);
		void descriptor_release(descriptor d);
		void descriptor_dispatch_read(descriptor d , tcode::operation* op);
		void descriptor_dispatch_write(descriptor d , tcode::operation* op);
		void descriptor_dispatch(descriptor d, int events);
    private:
        engine& _engine;
        int _handle;
        std::vector< struct kevent > _changes;
        tcode::io::pipe _wake_up;
        tcode::threading::spinlock _lock;
        tcode::slist::queue< tcode::operation > _op_queue; 
    };
    
}}

#endif
