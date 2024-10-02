#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#ifndef _WIN32
#define __forceinline inline
#endif

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>


/**
* A simple class representing a simple array.
* You may ask, why use the standard library, and things such as vector ?
* You can definetely do that, it is just personal preference.
*/
template<typename T>
class SimpleArray
{
private:
	T* _raw_data{nullptr};

public:

	/*===== Constructor =====*/

	SimpleArray() = default;

	SimpleArray(T* raw_data) :
		_raw_data{ raw_data }
	{

	}

	SimpleArray(uint32_t nb) :
		_raw_data{ (T*)malloc(nb*sizeof(T))}
	{
		memset(_raw_data, 0, nb * sizeof(T));
	}

	~SimpleArray()
	{
		if (_raw_data)
			free(_raw_data);
	}

	/*===== Accessor =====*/

	__forceinline T& operator[](int32_t i)
	{
		return _raw_data[i];
	}

	__forceinline const T& operator[](int32_t i)const
	{
		return _raw_data[i];
	}

	__forceinline T** operator&()const
	{
		return &_raw_data;
	}

	__forceinline T* operator*()const
	{
		return _raw_data;
	}

	/*===== Assignement =====*/

	T* operator=(T* raw_data)
	{
		_raw_data = raw_data;
		return _raw_data;
	}

	/*===== Comparison =====*/

	__forceinline bool operator== (T* to_compare)const
	{
		return _raw_data == to_compare;
	}

	__forceinline bool operator!= (T* to_compare)const
	{
		return _raw_data != to_compare;
	}

	/*===== Memory Management =====*/

	virtual void Clear()
	{
		if (_raw_data)
			free(_raw_data);
		_raw_data = nullptr;
	}

	virtual void Alloc(uint32_t nb)
	{
		_raw_data = (T*)malloc(nb * sizeof(T));
		memset(_raw_data, 0, nb * sizeof(T));
	}

};


template<typename T>
class NumberedArray : public SimpleArray<T>
{
private:
	uint32_t _nb{0};

public:
	/*===== Constructor =====*/

	NumberedArray(T* raw_data) = delete;

	NumberedArray(T* raw_data, uint32_t nb):
		SimpleArray<T>(raw_data),
		_nb{nb}

	{

	}

	NumberedArray(uint32_t nb) :
		SimpleArray<T>(nb),
		_nb{nb}
	{
	}

	/*===== Accessor =====*/

	__forceinline const uint32_t& Nb()const { return _nb; }

	/*===== Assignement =====*/

	T* operator=(T* raw_data) = delete;


	/*===== Memory Management =====*/

	virtual void Clear()override
	{
		SimpleArray<T>::Clear();
		_nb = 0;
	}

	virtual void Alloc(uint32_t nb)override
	{
		SimpleArray<T>::Alloc(nb);
		_nb = nb;
	}

};


#endif //__UTILITIES_H__
