#pragma once

#include "stdafx.h"
#include <string>

class DynNetvar
{
public:
	DynNetvar(const std::string& name);
	DynNetvar(const std::string& propName, const std::string& propDTPath);
	int Offset() const;
	inline int operator()() const
	{
		return Offset();
	};
private:
	int         m_iOffset;
};
