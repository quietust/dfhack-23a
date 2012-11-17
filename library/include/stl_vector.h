#pragma once

#include <vector>
#include <memory.h>

namespace stl
{

template<class T, class A = std::allocator<T>>
class vector
{
public:
	typedef T value_type;
	typedef T* iterator;
	typedef const T* const_iterator;

protected:
	A _allocator;
	T *_first;
	T *_last;
	T *_end;

	bool _buy (size_t len)
	{
		_first = NULL;
		_last = NULL;
		_end = NULL;
		if (len == 0)
			return false;
		_first = _allocator.allocate(len);
		_last = _first;
		_end = _first + len;
		return true;
	}
	void _destroy (T *first, T *last)
	{
		std::_Destroy_range(first, last, _allocator);
	}
	size_t _grow_to (size_t n)
	{
		// could put some special logic in here to grow by larger increments
		return n;
	}

	bool _inside (const T *ptr) const
	{
		return (ptr >= _first) && (ptr < _last);
	}
	void _reserve (size_t n)
	{
		size_t newlen = size();
		newlen += n;
		if (newlen <= capacity())
			return;
		reserve(newlen);
	}
	void _tidy ()
	{
		if (_first)
		{
			_destroy(_first, _last);
			_allocator.deallocate(_first, _end - _first);
		}
		_first = NULL;
		_last = NULL;
		_end = NULL;
	}
	T *_ucopy (const_iterator first, const_iterator last, T *ptr)
	{
		return _Uninitialized_copy(first, last, ptr, _allocator);
	}
	T *_umove (const_iterator first, const_iterator last, T *ptr)
	{
		return _Uninitialized_move(first, last, ptr, _allocator);
	}
	T *_ufill(T *ptr, size_t n, const T *val)
	{
		_Uninitialized_fill_n(ptr, n, val, _allocator);
		return ptr + n;
	}

public:
	vector() : _allocator(A()), _first(NULL), _last(NULL), _end(NULL) { }
	explicit vector (size_t n) : _allocator(A()), _first(NULL), _last(NULL), _end(NULL) 
	{
		resize(n);
	}
	vector(size_t n, const T& value) : _allocator(A()), _first(NULL), _last(NULL), _end(NULL)
	{
		if (_buy(n))
			_last = _ufill(_first, n, &value);
	}
	vector(const vector<T,A> &vec) : _allocator(A()), _first(NULL), _last(NULL), _end(NULL)
	{
		if (_buy(vec.size()))
			_last = _ucopy(vec.begin(), vec.end(), _first);
	}
	vector(iterator first, iterator last) : _allocator(A()), _first(NULL), _last(NULL), _end(NULL)
	{
		insert(begin(), first, last);
	}
	vector(const std::vector<T,A> &vec) : _allocator(A()), _first(NULL), _last(NULL), _end(NULL)
	{
		insert(begin(), vec.begin(), vec.end());
	}


	~vector()
	{
		_tidy();
	}

	vector<T,A>& operator= (const vector<T,A> &other)
	{
		if (this == &other)
			return *this;
		if (other.size() == 0)
			clear();
		if (other.size() <= size())
		{
			T *ptr = std::_Copy_impl(other._first, other._last, _first);
			_destroy(ptr, _last);
			_last = _first + other.size();
		}
		else if (other.size() <= capacity())
		{
			T *ptr = other._first + size();
			std::_Copy_impl(other._first, ptr, _first);
			_last = _ucopy(ptr, other._last, _last);
		}
		else
		{
			if (_first)
			{
				_destroy(_first, _last);
				_allocator.deallocate(_first, _end - _first);
			}
			if (_buy(other.size()))
				_last = _ucopy(other._first, other._last, _first);
		}
		return *this;
	}

	void reserve (size_t n)
	{
		// throw if too long
		// do we need to do anything?
		if (capacity() >= n)
			return;
		T *ptr = _allocator.allocate(n);
		_umove(_first, _last, ptr);

		size_t s = size();
		if (_first)
		{
			_destroy(_first, _last);
			_allocator.deallocate(_first, _end - _first);
		}
		_end = ptr + n;
		_last = ptr + s;
		_first = ptr;
	}

	size_t capacity() const
	{
		return (_end - _first);
	}

	iterator begin()
	{
		return _first;
	}
	const_iterator begin() const
	{
		return _first;
	}
	iterator end()
	{
		return _last;
	}
	const_iterator end() const
	{
		return _last;
	}

	void resize (size_t n)
	{
		if (n < size())
			erase(begin() + n, end());
		else if (size() < n)
		{
			_reserve(n - size());
			_Uninitialized_default_fill_n(_last, n - size(), (T *)0, _allocator);
			_last += n - size();
		}
	}

	void resize (size_t n, T val)
	{
		if (size() < n)
			insert(end(), n - size(), val);
		else if (n < size())
			erase(begin() + n, end());
	}

	size_t size() const
	{
		return (_last - _first);
	}
	size_t max_size() const
	{
		return _allocator.max_size();
	}

	A get_allocator() const
	{
		return _allocator;
	}

	bool empty() const
	{
		return (_first == _last);
	}

	const T& at (size_t n) const
	{
		return _first[n];
	}
	T& at (size_t n)
	{
		return _first[n];
	}
	const T& operator[] (size_t n) const
	{
		return _first[n];
	}
	T& operator[] (size_t n)
	{
		return _first[n];
	}

	const T* data () const
	{
		return _first;
	}

	T& front ()
	{
		return *begin();
	}
	const T& front () const
	{
		return *begin();
	}

	T& back ()
	{
		return *(end() - 1);
	}
	const T& back() const
	{
		return *(end() - 1);
	}

	void push_back (const T& x)
	{
		if (_inside(&x))
		{
			size_t i = &x - _first;
			if (_last == _end)
				_reserve(1);
			_Cons_val(_allocator, _last, _first[i]);
			++_last;
		}
		else
		{
			if (_last == _end)
				_reserve(1);
			_Cons_val(_allocator, _last, x);
			++_last;
		}
	}

	void pop_back ()
	{
		if (empty())
			return;
		_Dest_val(_allocator, _last - 1);
		--_last;
	}

	void assign (const_iterator first, const_iterator last)
	{
		erase(begin(), end());
		insert(begin(), first, last);
	}
	void assign (size_t n, const T& val)
	{
		T tmp = val;
		erase(begin(), end());
		insert(begin(), n, tmp);
	}

	iterator insert (iterator position, const T& x)
	{
		size_t n = position - _first;
		insert(position, 1, x);
		return _first + n;
	}
	void insert (iterator position, size_t n, const T& x)
	{
		if (n == 0)
			return;
		if (size() + n > capacity())
		{
			size_t newlen = size() + n;
			T *newvec = _allocator.allocate(newlen);
			size_t off = position - _first;
			
			_ufill(newvec + off, n, &x);
			_umove(_first, position, newvec);
			_umove(position, _last, newvec + (off + n));
			n += size();

			if (_first)
			{
				_destroy(_first, _last);
				_allocator.deallocate(_first, _end - _first);
			}
			_end = newvec + newlen;
			_last = newvec + n;
			_first = newvec;			
		}
		else if (_last - position < n)
		{
			T tmp = x;
			_umove(position, _last, position + n);
			_ufill(_last, n - (_last - position), &tmp);
			_last += n;

			std::fill(position, _last - n, tmp);
		}
		else
		{
			T tmp = x;
			T *oldend = _last;
			_last = _umove(oldend - n, oldend, _last);
			std::_Copy_backward(position, oldend - n, oldend);
			std::fill(position, position + n, tmp);
		}
	}
	void insert (iterator position, const_iterator first, const_iterator last)
	{
		size_t n = last - first;
		if (size() + n > capacity())
		{
			size_t newlen = size() + n;
			T *newvec = _allocator.allocate(newlen);
			T *ptr = newvec;
			
			ptr = _umove(_first, position, newvec);
			ptr = _ucopy(first, last, ptr);
			_umove(position, _last, ptr);

			n += size();
			if (_first)
			{
				_destroy(_first, _last);
				_allocator.deallocate(_first, _end - _first);
			}
			_end = newvec + newlen;
			_last = newvec + n;
			_first = newvec;			
		}
		else
		{
			_ucopy(first, last, _last);
			std::rotate(position, _last, _last + n);
			_last += n;
		}
	}

	iterator erase (iterator position)
	{
		size_t pos = position - begin();
		std::_Move(position + 1, _last, position);
		_destroy(_last - 1, _last);
		_last--;
		return begin() + pos;
	}
	iterator erase (iterator first, iterator last)
	{
		if (first == last)
			return first;
		size_t pos = first - begin();
		T *ptr = std::_Move(last, _last, first);
		_destroy(ptr, _last);
		_last = ptr;
		return begin() + pos;
	}

	void clear()
	{
		erase(begin(), end());
	}

	void swap (vector<T,A>& vec)
	{
		if (this == &vec)
			return;
		vector<T,A> tmp = *this;
		*this = vec;
		vec = tmp;
	}

	// conversion to "native" STL vector
	operator std::vector<T,A> () const
	{
		return std::vector<T,A>(begin(), end());
	}
};

template<>
class vector<bool>
{
protected:
	typedef unsigned int base_type;
	static const int base_bits = sizeof(base_type) * 8;
	static size_t base_len (size_t bits)
	{
		return (bits + base_bits - 1) / base_bits;
	}
	void _trim(size_t len)
	{
		size_t n = base_len(len);
		if (n < _vec.size())
			_vec.erase(_vec.begin() + n, _vec.end());
		_size = len;
		len %= base_bits;
		if (len > 0)
			_vec[n - 1] &= (1 << len) - 1;
	}

	vector<base_type> _vec;
	size_t _size;

public:
	class iterator_base : public std::iterator<std::random_access_iterator_tag, bool, int, bool *, bool>
	{
	public:
		iterator_base() : _ptr(NULL), _off(0) {}
		iterator_base(const base_type *ptr, size_t off) : _ptr(ptr), _off(off) {}
		const base_type *_ptr;
		size_t _off;
	};

	class reference : public iterator_base
	{
	protected:
		base_type _mask() const
		{
			return (base_type)(1 << _off);
		}
	public:
		reference() {}
		reference(const iterator_base &other) : iterator_base(other._ptr, other._off) {}
		reference& operator= (const reference& x)
		{
			*this = bool(x);
			return *this;
		}
		reference& operator= (bool x)
		{
			if (x)
				*(base_type*)_ptr |= _mask();
			else
				*(base_type*)_ptr &= ~_mask();
			return *this;
		}
		void flip()
		{
			*(base_type *)_ptr ^= _mask();
		}
		bool operator~ () const
		{
			return !bool(*this);
		}
		operator bool () const
		{
			return (*_ptr & _mask()) != 0;
		}
	};
	typedef bool const_reference;

	class const_iterator : public iterator_base
	{
	public:
		const_iterator() {}
		const_iterator(const base_type *ptr) : iterator_base(ptr, 0) {}
		const_reference operator* () const
		{
			return vector<bool>::reference(*this);
		}
		const_iterator& operator++ ()
		{
			increment();
			return *this;
		}
		const_iterator operator++ (int)
		{
			const_iterator tmp = *this;
			increment();
			return tmp;
		}
		const_iterator& operator-- ()
		{
			decrement();
			return *this;
		}
		const_iterator operator-- (int)
		{
			const_iterator tmp = *this;
			decrement();
			return (tmp);
		}
		const_iterator& operator+= (int x)
		{
			if (x < 0)
			{
				_off += x;
				_ptr -= 1 + ((size_t)-1 - _off) / base_bits;
				_off %= base_bits;
			}
			else
			{
				_off += x;
				_ptr += _off / base_bits;
				_off %= base_bits;
			}
			return *this;
		}
		const_iterator& operator-= (int x)
		{
			*this += -x;
			return *this;
		}
		const_iterator operator+ (int x) const
		{
			const_iterator tmp = *this;
			tmp += x;
			return tmp;
		}
		const_iterator operator- (int x) const
		{
			const_iterator tmp = *this;
			tmp -= x;
			return tmp;
		}
		int operator- (const const_iterator other) const
		{
			return base_bits * (_ptr - other._ptr)
				+ (_off - other._off);
		}
		const_reference operator[] (int x) const
		{
			return *(*this + x);
		}
		bool operator== (const const_iterator& other) const
		{
			return (_ptr == other._ptr && _off == other._off);
		}
		bool operator!= (const const_iterator& other) const
		{
			return !(*this == other);
		}
		bool operator< (const const_iterator& other) const
		{
			return (_ptr < other._ptr)
				|| (_ptr == other._ptr && _off < other._off);
		}
		bool operator> (const const_iterator& other) const
		{
			return other < *this;
		}
		bool operator<= (const const_iterator& other) const
		{
			return !(other < *this);
		}
		bool operator>= (const const_iterator& other) const
		{
			return !(*this < other);
		}
	protected:
		void decrement()
		{
			if (_off == 0)
			{
				_off = base_bits;
				_ptr--;
			}
			_off--;
		}
		void increment()
		{
			_off++;
			if (_off == base_bits)
			{
				_off = 0;
				_ptr++;
			}
		}
	};

	class iterator : public const_iterator
	{
	public:
		iterator() : const_iterator() {}
		iterator(base_type *ptr) : const_iterator(ptr) {}
		vector<bool>::reference operator* () const
		{
			return vector<bool>::reference(*this);
		}
		iterator& operator++ ()
		{
			increment();
			return (*this);
		}
		iterator operator++ (int)
		{
			iterator tmp = *this;
			increment();
			return tmp;
		}
		iterator& operator-- ()
		{
			decrement();
			return (*this);
		}
		iterator operator-- (int)
		{
			iterator tmp = *this;
			decrement();
			return tmp;
		}
		iterator& operator+= (int x)
		{
			_off += x;
			_ptr += _off / base_bits;
			_off %= base_bits;
			return *this;
		}
		iterator& operator-= (int x)
		{
			*this += -x;
			return *this;
		}
		iterator operator+ (int x) const
		{
			iterator tmp = *this;
			tmp += x;
			return tmp;
		}
		iterator operator- (int x) const
		{
			iterator tmp = *this;
			tmp -= x;
			return tmp;
		}

		int operator- (const const_iterator other) const
		{
			return base_bits * (_ptr - other._ptr)
				+ (_off - other._off);
		}
		vector<bool>::reference operator[] (int x) const
		{
			return *(*this + x);
		}
	};

	explicit vector() : _size(0) { }
	explicit vector(size_t n, bool value = false) : _vec(base_len(n), value ? -1 : 0), _size(0)
	{
		_trim(n);
	}
	vector(const_iterator first, const_iterator last) : _size(0)
	{
		assign(first, last);
	}
	vector(const vector<bool> &vec) : _size(0)
	{
		assign(vec.begin(), vec.end());
	}
    /*
	vector(const std::vector<bool> &vec) : _size(0)
	{
		assign(vec.begin(), vec.end());
	}
    */

	~vector()
	{
		_size = 0;
	}

	iterator begin()
	{
		return iterator((base_type *)_vec.data());
	}
	const_iterator begin() const
	{
		return const_iterator((base_type *)_vec.data());
	}
	iterator end()
	{
		return iterator(begin()) + _size;
	}
	const_iterator end() const
	{
		return const_iterator(begin()) + _size;
	}

	size_t size() const
	{
		return _size;
	}
	size_t max_size() const
	{
		return 0x10000000;	// should be big enough
	}
	void resize (size_t n, bool c = false)
	{
		if (n < size())
			erase(begin() + n, end());
		else
			insert(end(), n - size(), c);
	}
	size_t capacity() const
	{
		return _vec.capacity() * base_bits;
	}
	bool empty() const
	{
		return (size() == 0);
	}
	void reserve (size_t n)
	{
		_vec.reserve(base_len(n));
	}

	reference operator[] (size_t n)
	{
		return *(begin() + n);
	}
	const_reference operator[] (size_t n) const
	{
		return *(begin() + n);
	}
	reference at (size_t n)
	{
		return *(begin() + n);
	}
	const_reference at (size_t n) const
	{
		return *(begin() + n);
	}

	reference front ()
	{
		return *begin();
	}
	const_reference front () const
	{
		return *begin();
	}
	reference back ()
	{
		return *(end() - 1);
	}
	const_reference back() const
	{
		return *(end() - 1);
	}

	void assign (const_iterator first, const_iterator last)
	{
		erase(begin(), end());
		insert(begin(), first, last);
	}
	void assign (size_t n, bool val)
	{
		erase(begin(), end());
		insert(begin(), n, val);
	}

	void push_back (bool x)
	{
		insert(end(), x);
	}
	void pop_back ()
	{
		erase(end() - 1);
	}

	iterator insert (iterator position, bool x)
	{
		size_t n = position - begin();
		insert(position, 1, x);
		return begin() + n;
	}
	void insert (iterator position, size_t n, bool x)
	{
		reserve(size() + n);
		for (auto i = end(); i != position; i--)
			*(i + n - 1) = *(i - 1);
		for (int i = 0; i < n; i++)
			*position++ = x;
		_size += n;
	}
	void insert (iterator position, const_iterator first, const_iterator last)
	{
		size_t n = last - first;
		reserve(size() + n);
		for (auto i = end(); i != position; i--)
			*(i + n - 1) = *(i - 1);
		for (; first != last; first++)
			*position++ = *first;
		_size += n;
	}

	iterator erase (iterator position)
	{
		return erase(position, position + 1);
	}
	iterator erase (iterator first, iterator last)
	{
		int i = first - begin();
		size_t n = last - first;
		for (; first != last; first++)
			*first = *(first + n);
		_trim(n);
		return begin() + i;
	}

	void swap (vector<bool>& vec)
	{
		size_t tmp1 = _size;
		_size = vec._size;
		vec._size = tmp1;

		vector<base_type> tmp2 = _vec;
		_vec = vec._vec;
		vec._vec = tmp2;
	}

	void clear()
	{
		erase(begin(), end());
	}

	// conversion to "native" STL bool vector
	operator std::vector<bool> () const
	{
		return std::vector<bool>(begin(), end());
	}
};
};