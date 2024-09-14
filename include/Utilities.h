#ifndef __UTILITIES_H__
#define __UTILITIES_H__

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

	T& operator[](int32_t i)
	{
		return _raw_data[i];
	}

	T** operator&()
	{
		return &_raw_data;
	}

	T* operator*()
	{
		return _raw_data;
	}

	/*===== Assignement =====*/

	T* operator=(T* raw_data)
	{
		return _raw_data;
	}

	/*===== Comparison =====*/

	bool operator== (T* to_compare)
	{
		return _raw_data == to_compare;
	}

	bool operator!= (T* to_compare)
	{
		return _raw_data != to_compare;
	}

	/*===== Memory Management =====*/
	
	void Clear()
	{
		if (_raw_data)
			free(_raw_data);
	}

	void Alloc(uint32_t nb)
	{
		_raw_data = (T*)malloc(nb * sizeof(T));
		memset(_raw_data, 0, nb * sizeof(T));
	}

};


#endif //__UTILITIES_H__