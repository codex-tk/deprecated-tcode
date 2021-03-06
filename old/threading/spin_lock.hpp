#ifndef __tcode_spin_lock_h__
#define __tcode_spin_lock_h__

namespace tcode {
namespace threading {

class spin_lock {
public:
	spin_lock( void );
	~spin_lock( void );

	void lock( void );
	void unlock( void );
	bool trylock( void );
#if defined(TCODE_TARGET_WINDOWS)
private:
	CRITICAL_SECTION _lock;
#else
	pthread_mutex_t& handle( void );
private:
	pthread_mutex_t _lock;
#endif
};


template < typename T = tcode::threading::spin_lock >
class scoped_lock {
public:
	scoped_lock( T& lock ) : _lock( lock ) {
		_lock.lock();
	}
	~scoped_lock( void ) {
		_lock.unlock();
	}
private:
	T& _lock;
};

template < typename T = tcode::threading::spin_lock >
class scoped_unlock {
public:
	scoped_unlock( T& lock ) : _lock( lock ) {
		_lock.unlock();
	}
	~scoped_unlock( void ) {
		_lock.lock();
	}
private:
	T& _lock;
};

template < typename T = tcode::threading::spin_lock >
class scoped_try_lock {
public:
	scoped_try_lock ( T& lock ) : _lock( lock ) {
		_locked = _lock.trylock();
	}
	~scoped_try_lock( void ) {
        if ( _locked ) 
		    _lock.unlock();
	}

    bool locked( void ) {
        return _locked;
    }
private:
	T& _lock;
    bool _locked;
};

class no_lock {
public:
	no_lock( void ) {}
	~no_lock( void ) {}
	void lock( void ) {}
	void unlock( void ){}
};

}}


#endif
