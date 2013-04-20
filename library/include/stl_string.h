#pragma once

#include <ostream>
#include <memory.h>

namespace stl
{

template<class E, class A = std::allocator<E>>
class basic_string
{
protected:
	A _allocator;
	E *_data;
	size_t _size;
	size_t _alloc;

	static const E _empty = 0;

	void _copy (size_t newlen, size_t oldlen)
	{
		E *newdata = _allocator.allocate(newlen + 2);
		if (oldlen > 0)
			memcpy(newdata + 1, _getptr(), oldlen);
		_tidy(true);
		_data = newdata + 1;
		_data[-1] = 0;
		_alloc = newlen;
		_setlen(oldlen);
	}
	void _setlen (size_t len)
	{
		_getptr()[len] = 0;
		_size = len;
	}
	void _freeze()
	{
		if (_data && _data[-1] && _data[-1] != 255)
			_grow(_size);
		if (_data)
			_data[-1] = 255;
	}
	bool _grow (size_t len, bool trim = false)
	{
		// throw if too long
		if (_data && _data[-1] && _data[-1] != 255) // check for non-zero reference count
		{
			if (len == 0)
			{
				_data[-1]--;
				_tidy();
				return false;
			}
			else
			{
				_copy(len, size());
				return true;
			}
		}
		if (len > _alloc)
			_copy(len, size());
		else if (trim && len == 0)
			_tidy(true);
		else if (len == 0)
			_setlen(0);
		return (len > 0);
	}
	bool _inside (const E *ptr)
	{
		if (!ptr)
			return false;
		if (ptr < _getptr())
			return false;
		if (ptr >= _getptr() + _size)
			return false;
		return true;
	}
	void _split()
	{
		if (_data && _data[-1] && _data[-1] != 255)
		{
			E *tmp = _data;
			_tidy(true);
			assign(tmp);
		}
	}
	void _tidy(bool built = false)
	{
		if (!built)
			;
		else if (_data)
		{
			if (_data[-1] && _data[-1] != 255)
				_data[-1]--;
			else
			{
				E *ptr = _data - 1;
				_allocator.deallocate(ptr, _alloc + 2);
			}
		}
		_data = 0;
		_size = 0;
		_alloc = 0;
	}
	E *_getptr()
	{
		return (_data ? _data : (E *)&_empty);
	}
	const E *_getptr() const
	{
		return (_data ? _data : (E *)&_empty);
	}

public:
	typedef E * iterator;
	typedef const E * const_iterator;

	basic_string () : _allocator(A())
	{
		_tidy();
	}
	basic_string (const basic_string<E,A> &other) : _allocator(A())
	{
		_tidy();
		assign(other, 0, -1);
	}
	basic_string (const std::basic_string<E> &other) : _allocator(A())
	{
		_tidy();
		assign(other.c_str(), other.size());
	}
	basic_string (const basic_string<E,A> &other, size_t pos, size_t n = -1) : _allocator(A())
	{
		_tidy();
		assign(other, pos, n);
	}
	basic_string (const E *s, size_t n) : _allocator(A())
	{
		_tidy();
		assign(s, n);
	}
	basic_string (const E *s) : _allocator(A())
	{
		_tidy();
		assign(s);
	}
	basic_string (size_t n, E c) : _allocator(A())
	{
		_tidy();
		assign(n, c);
	}
	basic_string (iterator begin, iterator end) : _allocator(A())
	{
		_tidy();
		assign(begin, end);
	}

	basic_string<E,A>& operator= (const basic_string<E,A> &other)
	{
		return assign(other);
	}
	basic_string<E,A>& operator= (const E *s)
	{
		return assign(s);
	}
	basic_string<E,A>& operator= (E c)
	{
		return assign(1, c);
	}

	basic_string<E,A>& operator+= (const basic_string<E,A> &other)
	{
		return append(other);
	}
	basic_string<E,A>& operator+= (const E *s)
	{
		return append(s);
	}
	basic_string<E,A>& operator+= (E c)
	{
		return append(1, c);
	}

	basic_string<E,A>& append(const basic_string<E,A> &other)
	{
		return append(other, 0, -1);
	}
	basic_string<E,A>& append(const basic_string<E,A> &other, size_t pos, size_t n)
	{
		// throw if out of range
		size_t newlen = other.size() - pos;
		if (n > newlen)
			n = newlen;
		// throw if too long
		newlen = size() + n;
		if (n > 0 && _grow(newlen))
		{
			memcpy(_getptr() + size(), other._getptr() + pos, n);
			_setlen(newlen);
		}
		return *this;
	}
	basic_string<E,A>& append(const E *s, size_t n)
	{
		if (_inside(s))
			return append(*this, s - _getptr(), n);
		// throw if too long
		size_t newlen = size() + n;
		if (n > 0 && _grow(newlen))
		{
			memcpy(_getptr() + size(), s, n);
			_setlen(newlen);
		}
		return *this;
	}
	basic_string<E,A>& append(const E *s)
	{
		return append(s, strlen(s));
	}
	basic_string<E,A>& append(size_t n, E c)
	{
		// throw if too long
		size_t newlen = size() + n;
		if (n > 0 && _grow(newlen))
		{
			memset(_getptr() + size(), c, n);
			_setlen(newlen);
		}
		return *this;
	}
	basic_string<E,A>& append(iterator first, iterator last)
	{
		return append(first, last - first);
	}

	basic_string<E,A>& assign(const basic_string<E,A> &other)
	{
		return assign(other, 0, -1);
	}
	basic_string<E,A>& assign(const basic_string<E,A> &other, size_t pos, size_t n)
	{
		// throw if out of range
		size_t newlen = other.size() - pos;
		if (newlen > n)
			newlen = n;

		if (this == &other)
		{
			// substring
			erase(pos + newlen);
			erase(0, pos);
		}
		else if (_grow(newlen))
		{
			memcpy(_getptr(), other._getptr() + pos, newlen);
			_setlen(newlen);
		}
		return (*this);
	}
	basic_string<E,A>& assign(const E *s, size_t n)
	{
		if (_inside(s))
			return assign(*this, s - _getptr(), n);
		if (_grow(n))
		{
			memcpy(_getptr(), s, n);
			_setlen(n);
		}
		return *this;
	}
	basic_string<E,A>& assign(const E *s)
	{
		return assign(s, strlen(s));
	}
	basic_string<E,A>& assign(size_t n, E c)
	{
		// throw if too long
		if (_grow(n))
		{
			memset(_getptr(), c, n);
			_setlen(n);
		}
		return *this;
	}
	basic_string<E,A>& assign(iterator first, iterator last)
	{
		return assign(first, last - first);
	}

	basic_string<E,A> &insert (size_t pos1, const basic_string<E,A> &other)
	{
		return insert(pos1, other, 0, -1);
	}
	basic_string<E,A> &insert (size_t pos1, const basic_string<E,A> &other, size_t pos2, size_t n)
	{
		// throw if out of range
		size_t newlen = other.size() - pos2;
		if (n > newlen)
			n = newlen;
		// throw if too long
		newlen = size() + n;
		if (n > 0 && _grow(newlen))
		{
			memmove(_getptr() + pos1 + n, _getptr() + pos1, size() - pos1);
			if (this == &other)
				memmove(_getptr() + pos1, _getptr() + (pos1 < pos2 ? pos2 + n : pos2), n);
			else
				memcpy(_getptr() + pos1, other._getptr() + pos2, n);
			_setlen(newlen);
		}
		return *this;
	}
	basic_string<E,A> &insert (size_t pos1, const E *s, size_t n)
	{
		if (_inside(s))
			return insert(pos1, *this, s - _getptr(), n);
		// throw if out of range
		// throw if too long
		size_t newlen = size() + n;
		if (n > 0 && _grow(newlen))
		{
			memmove(_getptr() + pos1 + n, _getptr() + pos1, size() - pos1);
			memcpy(_getptr() + pos1, s, n);
			_setlen(newlen);
		}
		return *this;
	}
	basic_string<E,A> &insert (size_t pos1, const E *s)
	{
		return insert(pos1, s, strlen(s));
	}
	basic_string<E,A> &insert (size_t pos1, size_t n, E c)
	{
		// throw if out of range
		// throw if too long
		size_t newlen = size() + n;
		if (n > 0 && _grow(newlen))
		{
			memmove(_getptr() + pos1 + n, _getptr() + pos1, size() - pos1);
			memset(_getptr() + pos1, c, n);
			_setlen(newlen);
		}
		return *this;
	}
	iterator insert (iterator p)
	{
		return insert(p, 0);
	}
	iterator insert (iterator p, E c)
	{
		size_t pos = p - begin();
		insert(pos, 1, c);
		return begin() + pos;
	}
	void insert (iterator p, size_t n, E c)
	{
		insert(p - begin(), n, c);
	}
	void insert (iterator p, iterator first, iterator last)
	{
		insert(p - begin(), first, last - first);
	}

	basic_string<E,A>& erase (size_t pos = 0, size_t n = -1)
	{
		// throw if out of range
		_split();
		if (n > size() - pos)
			n = size() - pos;
		if (n > 0)
		{
			memmove(_getptr() + pos, _getptr() + pos + n, size() - pos - n);
			_setlen(size() - n);
		}
		return *this;
	}
	iterator erase (iterator p)
	{
		size_t n = p - begin();
		erase(n, 1);
		return begin() + n;
	}
	iterator erase (iterator first, iterator last)
	{
		size_t n = first - begin();
		erase(n, last - first);
		return begin() + n;
	}
	void clear()
	{
		_setlen(0);
	}

	iterator begin()
	{
		_freeze();
		return _getptr();
	}
	const_iterator begin() const
	{
		return _getptr();
	}
	iterator end()
	{
		_freeze();
		return _getptr() + size();
	}
	const_iterator end() const
	{
		return _getptr() + size();
	}

	// omitted rbegin() and rend()

	E& at (size_t pos)
	{
		// throw if out of range
		_freeze();
		return _getptr()[pos];
	}
	const E& at (size_t pos) const
	{
		// throw if out of range
		return _getptr()[pos];
	}
	E& operator[] (size_t pos)
	{
		_freeze();
		return _getptr()[pos];
	}
	const E& operator[] (size_t pos) const
	{
		return _getptr()[pos];
	}

	void push_back (E c)
	{
		insert(end(), c);
	}

	const E *c_str() const
	{
		return _getptr();
	}
	const E * data() const
	{
		return _getptr();
	}

	size_t size() const
	{
		return _size;
	}
	size_t length() const
	{
		return size();
	}
	size_t max_size() const
	{
		return _allocator.max_size();
	}
	A get_allocator() const
	{
		return _allocator;
	}
	void resize (size_t n)
	{
		resize(n, 0);
	}
	void resize (size_t n, E c)
	{
		if (n <= size())
			erase(n);
		else
			append(n - size(), c);
	}
	size_t capacity() const
	{
		return _alloc;
	}
	void reserve (size_t res_arg = 0)
	{
		if (size() <= res_arg && capacity() != res_arg)
		{
			size_t newlen = size();
			if (_grow(res_arg, true))
				_setlen(newlen);
		}
	}
	bool empty() const
	{
		return (size() == 0);
	}

	// omitted replace()

	// Begin deprecated stuff

	size_t copy (E* s, size_t n, size_t pos = 0) const
	{
		// throw if out of range
		if (n > size() - pos)
			n = size() - pos;
		memcpy(s, _getptr() + pos, n);
		return n;
	}

	void swap (basic_string<E,A>& str)
	{
		if (this == &str)
			return;

		basic_string<E,A> tmp = *this;
		*this = str;
		str = tmp;
	}

	// omitted find(), rfind(), find_first_of(), find_last_of(), find_first_not_of(), find_last_not_of()

	basic_string<E,A> substr (size_t pos = 0, size_t n = -1) const
	{
		return basic_string<E,A>(*this, pos, n);
	}

	int compare (const basic_string<E,A> &str) const
	{
		if (size() && str.size())
			return strcmp(begin(), str.begin());
		else if (size())
			return 1;
		else if (str.size())
			return -1;
		else
			return 0;
	}
	int compare (const E *str) const
	{
		if (size() && str)
			return strcmp(begin(), str);
		else if (size())
			return 1;
		else if (str)
			return -1;
		else
			return 0;
	}
	// omitted position-based compares

	~basic_string()
	{
		_tidy(true);
	}

	// conversion to "native" STL string
	operator std::basic_string<E> () const
	{
		return std::basic_string<E>(c_str(), size());
	}
};

template<class E, class A>
inline basic_string<E,A> operator+ (const basic_string<E,A>& lhs, const basic_string<E,A>& rhs)
{
	basic_string<E,A> out;
	out.reserve(lhs.size() + rhs.size());
	out += lhs;
	out += rhs;
	return out;
}
template<class E, class A>
inline basic_string<E,A> operator+ (const E *lhs, const basic_string<E,A>& rhs)
{
	basic_string<E,A> out;
	out.reserve(strlen(lhs) + rhs.size());
	out += lhs;
	out += rhs;
	return out;
}
template<class E, class A>
inline basic_string<E,A> operator+ (E lhs, const basic_string<E,A>& rhs)
{
	basic_string<E,A> out;
	out.reserve(1 + rhs.size());
	out += lhs;
	out += rhs;
	return out;
}
template<class E, class A>
inline basic_string<E,A> operator+ (const basic_string<E,A>& lhs, const E *rhs)
{
	basic_string<E,A> out;
	out.reserve(lhs.size() + strlen(rhs));
	out += lhs;
	out += rhs;
	return out;
}
template<class E, class A>
inline basic_string<E,A> operator+ (const basic_string<E,A>& lhs, E rhs)
{
	basic_string<E,A> out;
	out.reserve(lhs.size() + 1);
	out += lhs;
	out += rhs;
	return out;
}
template<class E, class A>
inline void swap (basic_string<E,A>& lhs, basic_string<E,A>& rhs)
{
	lhs.swap(rhs);
}

template<class E, class A>
inline bool operator== (const basic_string<E,A>& str1, const basic_string<E,A>& str2) { return str1.compare(str2) == 0; }
template<class E, class A>
inline bool operator== (const E* str1, const basic_string<E,A>& str2) { return str2.compare(str1) == 0; }
template<class E, class A>
inline bool operator== (const std::basic_string<E>& str1, const basic_string<E,A>& str2) { return str2.compare(str1.c_str()) == 0; }
template<class E, class A>
inline bool operator== (const basic_string<E,A>& str1, const E* str2) { return str1.compare(str2) == 0; }
template<class E, class A>
inline bool operator== (const basic_string<E,A>& str1, const std::basic_string<E>& str2) { return str1.compare(str2.c_str()) == 0; }

template<class E, class A>
inline bool operator!= (const basic_string<E,A>& str1, const basic_string<E,A>& str2) { return !(str1 == str2); }
template<class E, class A>
inline bool operator!= (const E* str1, const basic_string<E,A>& str2) { return !(str1 == str2); }
template<class E, class A>
inline bool operator!= (const std::basic_string<E>& str1, const basic_string<E,A>& str2) { return !(str1.c_str() == str2); }
template<class E, class A>
inline bool operator!= (const basic_string<E,A>& str1, const E* str2) { return !(str1 == str2); }
template<class E, class A>
inline bool operator!= (const basic_string<E,A>& str1, const std::basic_string<E>& str2) { return !(str1 == str2.c_str()); }

template<class E, class A>
inline bool operator< (const basic_string<E,A>& str1, const basic_string<E,A>& str2) { return str1.compare(str2) < 0; }
template<class E, class A>
inline bool operator< (const E* str1, const basic_string<E,A>& str2) { return str2.compare(str1) > 0; }
template<class E, class A>
inline bool operator< (const std::basic_string<E>& str1, const basic_string<E,A>& str2) { return str2.compare(str1.c_str()) > 0; }
template<class E, class A>
inline bool operator< (const basic_string<E,A>& str1, const E* str2) { return str1.compare(str2) < 0; }
template<class E, class A>
inline bool operator< (const basic_string<E,A>& str1, const std::basic_string<E>& str2) { return str1.compare(str2.c_str()) < 0; }

template<class E, class A>
inline bool operator> (const basic_string<E,A>& str1, const basic_string<E,A>& str2) { return (str2 < str1); }
template<class E, class A>
inline bool operator> (const E* str1, const basic_string<E,A>& str2) { return (str2 < str1); }
template<class E, class A>
inline bool operator> (const std::basic_string<E>& str1, const basic_string<E,A>& str2) { return (str2 < str1.c_str()); }
template<class E, class A>
inline bool operator> (const basic_string<E,A>& str1, const E* str2) { return (str2 < str1); }
template<class E, class A>
inline bool operator> (const basic_string<E,A>& str1, const std::basic_string<E>& str2) { return (str2.c_str() < str1); }

template<class E, class A>
inline bool operator<= (const basic_string<E,A>& str1, const basic_string<E,A>& str2) { return !(str2 < str1); }
template<class E, class A>
inline bool operator<= (const E* str1, const basic_string<E,A>& str2) { return !(str2 < str1); }
template<class E, class A>
inline bool operator<= (const std::basic_string<E>& str1, const basic_string<E,A>& str2) { return !(str2 < str1.c_str()); }
template<class E, class A>
inline bool operator<= (const basic_string<E,A>& str1, const E* str2) { return !(str2 < str1); }
template<class E, class A>
inline bool operator<= (const basic_string<E,A>& str1, const std::basic_string<E>& str2) { return !(str2.c_str() < str1); }

template<class E, class A>
inline bool operator>= (const basic_string<E,A>& str1, const basic_string<E,A>& str2) { return !(str1 < str2); }
template<class E, class A>
inline bool operator>= (const E* str1, const basic_string<E,A>& str2) { return !(str1 < str2); }
template<class E, class A>
inline bool operator>= (const std::basic_string<E>& str1, const basic_string<E,A>& str2) { return !(str1.c_str() < str2); }
template<class E, class A>
inline bool operator>= (const basic_string<E,A>& str1, const E* str2) { return !(str1 < str2); }
template<class E, class A>
inline bool operator>= (const basic_string<E,A>& str1, const std::basic_string<E>& str2) { return !(str1 < str2.c_str()); }

template<class E, class A>
inline std::basic_ostream<E>& operator<<(std::basic_ostream<E>& os, const basic_string<E,A>& str) { os << str.c_str(); return os; }

typedef basic_string<char> string;
};