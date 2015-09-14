#include "stdafx.h"
#include <tcode/io/ip/tcp/pipeline/channel.hpp>
#include <tcode/io/ip/tcp/pipeline/pipeline.hpp>
#include <tcode/tmp/bit.hpp>
#include <tcode/error.hpp>

namespace tcode{ namespace io { namespace io { namespace tcp{ 
namespace detail {
	
const int error_flag = 0x10000000;
const int closed_flag = 0x20000000; 
const int flag_mask = 0x0fffffff;

}

channel::channel( tcp::socket&& fd  ) 
	: _socket( std::move(fd))
{
	handle( fd );	
	_flag.store( 0 );
//	_pipeline.channel( this );
}

channel::~channel( void ) {

}

io::engine& channel::engine( void ){
	return _socket.engine();
}

tcp::pipeline& channel::pipeline( void ){
	return _pipeline;
}

void channel::close( void ){
	close( tcode::error_aborted );
}

void channel::close( const std::error_code& ec ){
    int exp = _flag.load();
    if ( exp & detail::closed_flag  == detail::closed_flag ) 
        return;
    do {
        int val = exp | detail::closed_flag;
        if ( _flag.compare_exchange_strong( exp , val ) ) {
            break;        
        } else {
            if ( exp & detail::closed_flag == detail::closed_flag ) 
                return;
        }
    }while(true);
    engine().execute([this , ec ]{ fire_on_close( ec); });
}

void channel::add_ref( void ){
	_flag.fetch_add(1);
}

int channel::release( void ){
	int val = _flag.fetch_sub( 1 , std::memory_order::memory_order_release ) - 1;
	if ( val & detail::flag_mask == 0 )
		engine().execute([this]{ fire_on_end_reference(); });	
	return val & detail::flag_mask;
}

#if defined( tcode_target_windows )
void channel::fire_on_open( const tcode::io::ip::address& addr ){
	add_ref();
	if ( _loop.in_event_loop() ){
		_loop.dispatcher().bind( handle());
		_pipeline.fire_filter_on_open(addr);
		read(nullptr);	
		/* iocp connector �� fail
		if ( _loop.dispatcher().bind( handle())) {
			_pipeline.fire_filter_on_open(addr);
			read(nullptr);	
		} else {
			tcode::diagnostics::error_code ec = tcode::diagnostics::platform_error();
			_pipeline.fire_filter_on_open(addr);
			close( ec );
		}*/
	} else {
		_loop.execute([this,addr]{ fire_on_open(addr); });
	}
	
}
#elif defined( tcode_target_linux ) 
void channel::fire_on_open( const tcode::io::ip::address& addr ){
	add_ref();
	if ( _loop.in_event_loop() ){
		_pipeline.fire_filter_on_open(addr);
		if ( _write_buffers.empty() && !_loop.dispatcher().bind( handle() , epollin , this ))
			close( tcode::diagnostics::platform_error() );
	} else {
		_loop.execute([this,addr]{ fire_on_open(addr); });
	}
}
#else

#endif
void channel::fire_on_end_reference( void ){
	_pipeline.fire_filter_on_end_reference();
	delete this;
}

void channel::fire_on_close( const std::error_code& ec  ){
	_loop.dispatcher().unbind( handle());
	tcode::io::ip::tcp_base::close();	
	if ( tcode::threading::atomic_bit_reset( _flag , detail::no_error_flag )) 
		_pipeline.fire_filter_on_error( ec );
	_pipeline.fire_filter_on_close();
	release();
}

void channel::do_write( tcode::buffer::byte_buffer buf ){
	if ( !tcode::threading::atomic_bit_on( _flag , detail::not_closed_flag ))
		return;

	if ( _loop.in_event_loop() ){
		bool needwrite = _write_buffers.empty();
		_write_buffers.push_back( buf );
		if ( !needwrite ) 
			return;
#if defined( tcode_target_windows )
		write_remains( nullptr );
#elif defined( tcode_target_linux ) 
		if ( !_loop.dispatcher().bind( handle() , epollin | epollout , this))
			close( tcode::diagnostics::platform_error() );
#endif
	} else {
		tcode::rc_ptr< channel > chan(this);
		_loop.execute([chan,buf]{ chan->do_write( buf ); });
	}
}

void channel::do_write( tcode::buffer::byte_buffer buf1 
				, tcode::buffer::byte_buffer buf2 )
{
	if ( !tcode::threading::atomic_bit_on( _flag , detail::not_closed_flag ))
		return;

	if ( _loop.in_event_loop() ){
		bool needwrite = _write_buffers.empty();
		_write_buffers.push_back( buf1 );
		_write_buffers.push_back( buf2 );
		if ( !needwrite ) 
			return;
#if defined( tcode_target_windows )
		write_remains( nullptr );
#elif defined( tcode_target_linux ) 
		if ( !_loop.dispatcher().bind( handle() , epollin | epollout , this))
			close( tcode::diagnostics::platform_error() );
#endif
	} else {
		tcode::rc_ptr< channel > chan(this);
		_loop.execute([chan,buf1,buf2]{ chan->do_write( buf1,buf2 ); });
	}
}

void channel::packet_buffer( packet_buffer_ptr& ptr ){
	_packet_buffer = ptr;
}

#if defined( tcode_target_windows )
void channel::read( completion_handler_read* h ) {
	if ( !tcode::threading::atomic_bit_on( _flag , detail::not_closed_flag )) {
		if ( h )  delete h;
		return;
	}
		
	if ( h == nullptr ) 
		h = new completion_handler_read( *this );
		
	h->prepare();

	if ( _packet_buffer.get() == nullptr )
		_packet_buffer = new simple_packet_buffer( channel_config::read_buffer_size  );

	tcode::iovec iov = _packet_buffer->read_iovec();
	dword flag = 0;
    if ( wsarecv(	handle() 
                    , &iov
                    , 1
                    , nullptr 
                    , &flag 
					, h
                    , nullptr ) == socket_error )
    {
        std::error_code ec = tcode::diagnostics::platform_error();
        if ( ec.value() != wsa_io_pending ){
			h->error_code(ec);
			_loop.execute_handler( h );
        }
    }
}

void channel::handle_read( const tcode::diagnostics::error_code& ec 
		, const int completion_bytes 
		, completion_handler_read* h )
{
	if ( !tcode::threading::atomic_bit_on( _flag , detail::not_closed_flag )) {
		if ( h )  delete h;
		return;
	}
	if ( ec )
		close(ec);
	else {
		if ( _packet_buffer->complete(completion_bytes))
			_pipeline.fire_filter_on_read( _packet_buffer->detach_packet());	
		read(h);
		return;
	}
}

void channel::handle_write( const tcode::diagnostics::error_code& ec 
		, const int completion_bytes 
		, completion_handler_write* h )
{	
	if ( !tcode::threading::atomic_bit_on( _flag , detail::not_closed_flag )) {
		if ( h )  delete h;
		return;
	}
	if ( ec )
		close(ec);
	else {
		int bytes = completion_bytes;
		auto it = _write_buffers.begin();
		while( it != _write_buffers.end() && bytes > 0 ) {
			bytes -= it->rd_ptr( bytes );
			++it;
		}
		_write_buffers.erase( 
			std::remove_if( _write_buffers.begin() , _write_buffers.end() , 
					[] ( tcode::buffer::byte_buffer& buf ) -> bool {
						return buf.length() == 0;
					}) , _write_buffers.end());
		bool flush = _write_buffers.empty();
		_pipeline.fire_filter_on_write( completion_bytes , flush );
		if ( !flush ) {
			write_remains( h );
			return;				
		}	
	}
}

void channel::write_remains( completion_handler_write* h ){
	if ( !tcode::threading::atomic_bit_on( _flag , detail::not_closed_flag )) {
		if ( h )  delete h;
		return;
	}
	if ( h == nullptr )
		h = new completion_handler_write( *this );

	iovec write_buffers[detail::max_writev_count];
	int count = 0 ;
	auto it = _write_buffers.begin();
	for ( ; count < detail::max_writev_count ; ++count , ++it) {
		if ( it == _write_buffers.end() ) 
			break;
		write_buffers[count] = tcode::io::write_buffer( *it );
	}
	h->prepare();
	dword flag = 0;
    if ( wsasend( handle() 
				, write_buffers
				, count
				, nullptr 
				, flag 
				, h
				, nullptr ) == socket_error )
	{
		std::error_code ec = tcode::diagnostics::platform_error();
        if ( ec.value() != wsa_io_pending ){
			h->error_code(ec);
			_loop.execute_handler(h);
        }
	}
}
	
#elif defined( tcode_target_linux )
void channel::operator()( const int events ){
	if ( !tcode::threading::atomic_bit_on( _flag , detail::not_closed_flag ))
		return;

	if ( events & ( epollerr | epollhup )) {
		if ( tcode::threading::atomic_bit_reset( _flag , detail::not_closed_flag ) )
			fire_on_close( events & epollerr ? tcode::diagnostics::epoll_err : tcode::diagnostics::epoll_hup);
	} else {
		if ( events & epollin ) handle_read();
		if ( events & epollout ) handle_write();
	}		
}

int channel::read( iovec* iov , int cnt ) {
	int result = -1;
	do{
		result = readv( handle() , iov , cnt );
	} while( result < 0 &&  errno == eintr  );
	return result;
}

int channel::write( iovec* iov , int cnt ){
	int result = -1;
	do{
		result = writev( handle() , iov , cnt );
	} while( result < 0 &&  errno == eintr  );
	return result;
}

void channel::write_reamins( void ){
	iovec write_buffers[detail::max_writev_count];
	int count = 0 ;
	auto it = _write_buffers.begin();
	for ( ; count < detail::max_writev_count ; ++count , ++it) {
		if ( it == _write_buffers.end() )
			break;
		write_buffers[count] = tcode::io::write_buffer( *it );
	}
	int completion_bytes = write( write_buffers , count );
	if ( completion_bytes < 0 )
		close( tcode::diagnostics::platform_error() );
	else if ( completion_bytes == 0 )
		close( std::error_code( econnreset , tcode::diagnostics::posix_category()) );
	else {
		int bytes = completion_bytes;
		auto it = _write_buffers.begin();
		while( it != _write_buffers.end() && bytes > 0 ) {
			bytes -= it->rd_ptr( bytes );
			++it;
		}
		_write_buffers.erase( 
			std::remove_if( _write_buffers.begin() , _write_buffers.end() , 
					[] ( tcode::buffer::byte_buffer& buf ) -> bool {
						return buf.length() == 0;
					}) , _write_buffers.end());
		bool flush = _write_buffers.empty();
		_pipeline.fire_filter_on_write( completion_bytes , flush );
	}
}

void channel::handle_read( void ){
	while ( true ) {
		if ( !tcode::threading::atomic_bit_on( _flag , detail::not_closed_flag ))
			return;

		if ( _packet_buffer.get() == nullptr )
			_packet_buffer = new simple_packet_buffer( channel_config::read_buffer_size  );
	
		tcode::iovec iov = _packet_buffer->read_iovec();
		int result = read( &iov , 1 );
		if ( result < 0 && errno == eagain )
			break;
		else if ( result < 0 )
			close( tcode::diagnostics::platform_error() );
		else if ( result == 0 )
			close( std::error_code( econnreset , tcode::diagnostics::posix_category()) );
		else {
			if ( _packet_buffer->complete(result)) {
				auto packet = _packet_buffer->detach_packet();
				_pipeline.fire_filter_on_read( packet );	
			}
				
		}	
	}	
}

void channel::handle_write( void ){
	if ( !tcode::threading::atomic_bit_on( _flag , detail::not_closed_flag ))
		return;

	write_reamins();
	if ( _write_buffers.empty() && !_loop.dispatcher().bind( handle(), epollin, this ) ) 
		close( tcode::diagnostics::platform_error() );
}

#endif

}}}
