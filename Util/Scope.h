#pragma once
#include <utility>

template<typename T>
class Scope {
public:
	Scope(T&& callable)
		: callable(std::forward<T>(callable))
	{
	}

	Scope(const Scope&) = delete;
	Scope& operator=(const Scope&) = delete;

	Scope(Scope&& other)
		: callable(std::move(other.callable))
	{
		other.callable = {};
	}

	~Scope()
	{
		if (callable) {
			callable();
		}
	}

private:
	T callable;
};