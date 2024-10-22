#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#ifndef _WIN32
#define __forceinline inline
#endif

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>


/**
* A simple class representing an allocated RAM memory block, in C format.
* You may ask, why use the standard library, and things such as vector ?
* You can definetely do that, it is just personal preference.
* This uses malloc and free, as such, it should be used for simple C types and not with class.
*/
template<typename T>
class HeapMemory
{
protected:
	T* _raw_data{nullptr};

public:

	/*===== Constructor =====*/

	HeapMemory() = default;

	HeapMemory(T* raw_data) :
		_raw_data{ raw_data }
	{

	}

	HeapMemory(uint32_t nb) :
		_raw_data{ (T*)malloc(nb * sizeof(T)) }
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

	__forceinline T** operator&()
	{
		return &_raw_data;
	}

	__forceinline T*const* operator&()const
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

/*
* A simple class representing an allocated RAM memory block, in cpp.
* in conbtrast with the parent, this uses new and delete, thus preserving vtables for contained classes.
*/
template<typename T>
class SmartHeapMemory : public HeapMemory<T>
{
public:
	/*===== Constructor =====*/

	SmartHeapMemory() = default;

	SmartHeapMemory(uint32_t nb) :
		_raw_data{ new T[nb]; }
	{
	}

	~SmartHeapMemory()
	{
		if (_raw_data)
			delete[] _raw_data;
	}

	/*===== Memory Management =====*/

	virtual void Clear()override
	{
		if (_raw_data)
			delete[] _raw_data;
		_raw_data = nullptr;
	}

	virtual void Alloc(uint32_t nb)override
	{
		_raw_data = new T[nb];
	}
};

/**
* A simple class representing a simple array of data you would need to loop through.
* as being smart, this preserve classes vtable when allocating.
*/
template<typename T>
class SmartLoopArray : public SmartHeapMemory<T>
{
private:
	uint32_t _nb{ 0 };

public:
	/*===== Constructor =====*/

	SmartLoopArray() = default;

	SmartLoopArray(T* raw_data) = delete;

	SmartLoopArray(T* raw_data, uint32_t nb) :
		SmartHeapMemory<T>(raw_data),
		_nb{ nb }

	{

	}

	SmartLoopArray(uint32_t nb) :
		SmartHeapMemory<T>(nb),
		_nb{ nb }
	{
	}

	/*===== Accessor =====*/

	__forceinline const uint32_t& Nb()const { return _nb; }

	/*===== Assignement =====*/

	T* operator=(T* raw_data) = delete;


	/*===== Memory Management =====*/

	virtual void Clear()override
	{
		SmartHeapMemory<T>::Clear();
		_nb = 0;
	}

	virtual void Alloc(uint32_t nb)override
	{
		SmartHeapMemory<T>::Alloc(nb);
		_nb = nb;
	}

};

/**
* A simple class representing the familiar list container.
* an uncontiguous memory container, linking node with pointer from one to the next.
*/
template<typename T>
class List
{
public:
	struct ListNode
	{
		T data;

		ListNode* prev{nullptr};
		ListNode* next{nullptr};

		ListNode* operator++()
		{
			return next;
		}

		ListNode* operator--()
		{
			return prev;
		}

		bool operator==(const ListNode& rhs)
		{
			return data == rhs;
		}
	};

private:
	ListNode*	head{nullptr};//first node
	ListNode*	toe{ nullptr };//last node

	uint32_t	nb{0};
public:

	/*===== Manipulation =====*/

	__forceinline void Add(const T& data)
	{
		ListNode* newNode = static_cast<ListNode*>(malloc(sizeof(ListNode)));
		memset(newNode, 0, sizeof(ListNode));
		newNode->data = data;

		Add(newNode);
	}

	__forceinline void Add(ListNode* newNode)
	{
		if (head == nullptr)
		{
			head = newNode;
			toe = newNode;
		}
		else if (toe != nullptr)
		{
			toe->next = newNode;
			newNode->prev = toe;
			toe = newNode;
		}
		nb++;
	}

	__forceinline void Remove(ListNode* node, bool clearData = true)
	{
		if (node == head)
		{
			if (node->next != nullptr)
			{
				head = node->next;
			}
			else
			{
				head = nullptr;
			}

			if (clearData)
				free(head);

			nb--;
			return;
		}

		ListNode* iNode = head;
		for (uint32_t i = 0; i < nb; i++)
		{
			if (iNode == node)
			{
				if (iNode->prev != nullptr)
				{
					iNode->prev->next = iNode->next;
				}
				if (iNode->next != nullptr)
				{
					iNode->next->prev = iNode->prev;
				}

				if (clearData)
					free(&iNode);

				break;
			}

			iNode++;
		}

		nb--;
	}

	/*===== Accessor =====*/

	__forceinline ListNode* GetHead()const
	{
		return head;
	}

	__forceinline ListNode* GetToe()const
	{
		return toe;
	}

	__forceinline uint32_t GetNb()const
	{
		return nb;
	}

	/*===== Memory Management =====*/

	__forceinline void Clear()
	{
		if (toe == nullptr)
			return;

		//going from back
		ListNode* iNode = toe;
		do
		{
			//getting back next node to clear
			ListNode* prevNode = iNode->prev;
			//freeing current node
			free(iNode);
			//make next node to clear current node
			iNode = prevNode;
		} while (iNode != nullptr);
	}
};




#endif //__UTILITIES_H__
