#ifndef AUTO_HANDLE_H
#define AUTO_HANDLE_H


//TODO namespace?
//TODO destroy should pass by reference?
//TODO conversions between different types of auto_X - not sure these work properly for auto_ptr
//TODO asserts on auto_ptr_x operator* and ->
//TODO auto_ptrx to auto_ptr_x ?


inline void * fixed_null()
{
	return 0;
}

template<int i>
inline int fixed_int()
{
	return i;
}

template<unsigned int i>
inline int fixed_uint()
{
	return i;
}

template<long l>
inline int fixed_long()
{
	return l;
}

template<unsigned long l>
inline int fixed_ulong()
{
	return l;
}

template<class H>
inline H default_val()
{
	return H();
}



template<class H, void destroy(H), H get_null_handle()>
struct auto_handle_ref //Think auto_ptr spec implies it should hold reference to auto_ptr - but it is really an implementation detail anyway
{
	auto_handle_ref(H handle):
		m_handle(handle)
	{}

	H m_handle;
};


//copying H, destroy and get_null_handle must not throw
//H is passed by value so should not be a large type - in that case use a pointer
//H must have copy constructor, operator!= defined
template<class H, void destroy(H), H get_null_handle() = fixed_null >
class auto_handle
{

	public:

	explicit auto_handle(H handle = get_null_handle()) throw()
		: m_handle(handle)
	{}

	auto_handle(auto_handle<H, destroy, get_null_handle> & other) throw()
	{
		reset_impl(other.release());
	}

	template<class O>
	auto_handle(auto_handle<O, destroy, get_null_handle> & other) throw()
	{
		m_handle = other.release();
	}

	auto_handle<H, destroy, get_null_handle> & operator=(auto_handle<H, destroy, get_null_handle> & other) throw()
	{
		reset_impl(other.release());
		return *this;
	}

	template<class O>
	auto_handle<H, destroy, get_null_handle>& operator=(auto_handle<O, destroy, get_null_handle> & other) throw()
	{
		reset_impl(other.release());
		return *this;
	}

	~auto_handle() throw()
	{
		if (m_handle != get_null_handle())
		{
			destroy(m_handle);
		}
	}


	H get() const throw()
	{
		return m_handle;
	}

	H release() throw()
	{
		H handle = m_handle;
		m_handle = get_null_handle();
		return handle;
	}

	void reset(H handle = get_null_handle()) throw()
	{
		reset_impl(handle);
	}

	//automatic conversions
	auto_handle(auto_handle_ref<H, destroy, get_null_handle> ref) throw():
		m_handle(ref.m_handle)
	{}

	auto_handle<H, destroy, get_null_handle> & operator=(auto_handle_ref<H, destroy, get_null_handle> ref) throw()
	{
		reset_impl(ref.m_handle);
		return *this;
	}

	template<class O>
	operator auto_handle_ref<O, destroy, get_null_handle>() throw()
	{
		return auto_handle_ref<O, destroy, get_null_handle>(release());
	}

	template<class O>
	operator auto_handle<O, destroy, get_null_handle>() throw()
	{
		return auto_handle<O, destroy, get_null_handle>(release());
	}

	private:
	void reset_impl(const H & handle) throw() //used in place of reset in implementation to reduce copying of H variables - potentially slow when H is object type
	{
		if (m_handle != handle)
		{
			if (m_handle != get_null_handle())
			{
				destroy(m_handle);
			}
			m_handle = handle;
		}
	}

	H m_handle;

};



template<class T, void destroy(T *)>
struct auto_ptrx_ref //Think auto_ptr spec implies it should hold reference to auto_ptr - but it is really an implementation detail anyway
{
	auto_ptrx_ref(T * ptr):
		m_ptr(ptr)
	{}

	T * m_ptr;
};


//destroy must not throw
template<class T, void destroy(T *)>
class auto_ptrx
{
	public:

	explicit auto_ptrx(T * ptr = 0) throw()
		: m_ptr(ptr)
	{}

	auto_ptrx(auto_ptrx<T, destroy> & other) throw()
	{
		reset(other.release());
	}

	template<class O>
	auto_ptrx(auto_ptrx<O, destroy> & other) throw()
	{
		m_ptr = other.release();
	}

	auto_ptrx<T, destroy> & operator=(auto_ptrx<T, destroy> & other) throw()
	{
		reset(other.release());
		return *this;
	}

	template<class O>
	auto_ptrx<T, destroy>& operator=(auto_ptrx<O, destroy> & other) throw()
	{
		reset(other.release());
		return *this;
	}

	~auto_ptrx() throw()
	{
		if (m_ptr)
		{
			destroy(m_ptr);
		}
	}

	T & operator*() const throw()
	{
		return *m_ptr;
	}

	T * operator->() const throw()
	{
		return m_ptr;
	}

	T * get() const throw()
	{
		return m_ptr;
	}

	T * release() throw()
	{
		T * ptr = m_ptr;
		m_ptr = 0;
		return ptr;
	}

	void reset(T * ptr = 0) throw()
	{
		if (m_ptr != ptr)
		{
			if (m_ptr)
			{
				destroy(m_ptr);
			}
			m_ptr = ptr;
		}
	}

	//automatic conversions
	auto_ptrx(auto_ptrx_ref<T, destroy> ref) throw():
		m_ptr(ref.m_ptr)
	{}

	auto_ptrx<T, destroy> & operator=(auto_ptrx_ref<T, destroy> ref) throw()
	{
		reset(ref.m_ptr);
		return *this;
	}

	template<class O>
	operator auto_ptrx_ref<O, destroy>() throw()
	{
		return auto_ptrx_ref<O, destroy>(release());
	}

	template<class O>
	operator auto_ptrx<O, destroy>() throw()
	{
		return auto_ptrx<O, destroy>(release());
	}

	private:
	T * m_ptr;
};


template<class T>
inline void delete_array(T * array)
{
	delete[] array;
}

template<class T>
struct auto_array_ptr //workaround for lack of template typedefs - see http://www.gotw.ca/gotw/079.htm
{
	typedef auto_ptrx<T, delete_array<T> > type;
};


#endif // AUTO_HANDLE_H

