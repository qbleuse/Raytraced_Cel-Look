#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#ifndef _WIN32
#define __forceinline inline
#endif

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>


/**
* A simple class representing an allocated RAM memory block.
* You may ask, why use the standard library, and things such as vector ?
* You can definetely do that, it is just personal preference.
*/
template<typename T>
class HeapMemory
{
private:
	T* _raw_data{nullptr};

public:

	/*===== Constructor =====*/

	HeapMemory() = default;

	HeapMemory(T* raw_data) :
		_raw_data{ raw_data }
	{

	}

	HeapMemory(uint32_t nb) :
		_raw_data{ (T*)malloc(nb*sizeof(T))}
	{
		memset(_raw_data, 0, nb * sizeof(T));
	}

	~HeapMemory()
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

/**
* A simple class representing a simple array of data you would need to loop through.
*/
template<typename T>
class LoopArray : public HeapMemory<T>
{
private:
	uint32_t _nb{0};

public:
	/*===== Constructor =====*/

	LoopArray() = default;

	LoopArray(T* raw_data) = delete;

	LoopArray(T* raw_data, uint32_t nb):
		HeapMemory<T>(raw_data),
		_nb{nb}

	{

	}

	LoopArray(uint32_t nb) :
		HeapMemory<T>(nb),
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
		HeapMemory<T>::Clear();
		_nb = 0;
	}

	virtual void Alloc(uint32_t nb)override
	{
		HeapMemory<T>::Alloc(nb);
		_nb = nb;
	}

};


#endif //__UTILITIES_H__
