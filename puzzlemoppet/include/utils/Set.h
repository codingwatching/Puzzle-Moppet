
#ifndef UTILS_SET_H
#define UTILS_SET_H

#include "litha_internal.h"
#include <vector>

namespace utils
{

// A simple container that does not contain duplicate elements.
// It's not necessarily fast, but that could presumably be remedied.
// Designed for storing pointers and basic types.
template<class Type>
class Set
{
	std::vector<Type> elements;
	
public:
	
	// returns true if the set did not already contain the element
	inline bool Insert(Type element)
	{
		if (Contains(element))
			return false;
		
		elements.push_back(element);
		return true;
	}
	
	// returns true if the set did contain the element
	inline bool Remove(Type element)
	{
		const u32 num_elements = elements.size();

		for (u32 i = 0; i < num_elements; i ++)
		{
			if (elements[i] == element)
			{
				elements.erase(elements.begin()+i);
				return true;
			}
		}
		return false;
	}
	
	inline bool Contains(Type element) const
	{
		for (auto& elem : elements)
		{
			if (elem == element)
				return true;
		}
		return false;
	}
	
	// adds all elements from another set to this set, ensuring no duplicates.
	// Returns true if this set changes as a result. (i.e. a new element
	// has been added that wasn't already present)
	inline bool Union(const Set<Type> &other)
	{
		bool changed = false;
		
		for (u32 i = 0; i < other.size(); i ++)
		{
			if (Insert(other[i]))
				changed = true;
		}
		
		return changed;
	}
	
	// Probably slow. Copies entire vector.
	inline std::vector<Type> ToVector() const
	{
		return elements;
	}
	
	inline u32 size() const
	{
		return elements.size();
	}
	
	inline void clear()
	{
		elements.clear();
	}
	
	// does not return reference as we do not want this to be modified!
	/*Type operator[](u32 index) const
	{
		ASSERT(index < elements.size());
		return elements[index];
	}*/
	
	inline const Type &operator[](u32 index) const
	{
		ASSERT(index < elements.size());
		return elements[index];
	}
	
	// Useful when elements are removed one by one by some external code.
	// Should ensure at least one element exists with size() before calling this.
	inline const Type &GetAnyForRemoval() const
	{
		ASSERT(elements.size());
		// front will be found first by Remove method.
		return elements.front();
	}

	// For range-based for loops
	inline auto begin() -> decltype (elements.begin())
	{
		return elements.begin();
	}

	inline auto end() -> decltype (elements.end())
	{
		return elements.end();
	}
};

}

#endif

