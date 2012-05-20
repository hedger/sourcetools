#include "stdafx.h"

#include "DynNetvar.h"

#include <assert.h>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <memory>
#include <exception>

#include <boost/noncopyable.hpp>

#include "..\Logging\console.h"
#include "..\OBSDK\public\client_class.h"
#include "..\Core\Interfaces.hpp"

class NetVarException : public std::exception
{
public:
	NetVarException(const std::string& msg, const std::string& objName) : m_sMsg(msg + ": " + objName) {} ;
	const char* what() const throw()
	{
		return m_sMsg.c_str();
	}	
private:
	std::string m_sMsg;
};

struct NetVar 
{
	typedef std::shared_ptr<NetVar> Ptr;

	int						m_iOffset;
	std::string				m_sTableName;

	NetVar(RecvProp* pProp) : m_sTableName("")
	{
		m_iOffset = pProp->GetOffset();
		if (RecvTable* pTable = pProp->GetDataTable())
			m_sTableName = pTable->GetName();
	};
};

struct NetClass 
{
	typedef std::shared_ptr<NetClass> Ptr;

	std::map<const std::string, NetVar::Ptr>   m_Props;
	std::map<const std::string, NetClass::Ptr> m_SubClasses;

	NetClass(RecvTable* pTable)
	{
		Init(pTable);
	};

	NetClass(ClientClass* pClass)
	{
		Init(pClass->m_pRecvTable);
	};

	NetVar::Ptr FindProp(const std::string& name)
	{
		auto pProp =  m_Props.find(name); 
		if (pProp == m_Props.end())
			throw NetVarException(_(PROPERTYNO, "Property not found"), name);
		return pProp->second;
	};

	NetClass::Ptr FindSubTable(const std::string& name)
	{
		auto pClass = m_SubClasses.find(name);
		if (pClass == m_SubClasses.end())
			throw NetVarException(_(SUBCLASSNO, "Subclass not found"), name);
		return pClass->second;
	};

private:
	void Init(RecvTable* pTable)
	{
		for (int i=0; i < pTable->m_nProps; ++i)
		{
			auto pProp = &pTable->m_pProps[i];
			RecvTable* pSubTable = pProp->GetDataTable();
			if (!strcmp(pProp->GetName(), _(BASECLASS, "baseclass"))) // a superclass
			{
				// do smth?..
			}
			else 
			{
				if (pSubTable)  // storing a subclass
					m_SubClasses[pSubTable->GetName()] = NetClass::Ptr(new NetClass(pSubTable));
				m_Props[pProp->GetName()] = NetVar::Ptr(new NetVar(pProp)); // storing a property
			};
		};
	};
};

class NetvarManager : public boost::noncopyable
{
public:
	inline static NetvarManager& Instance() 
	{
		static NetvarManager inst;
		return inst;
	};

	int FindOffset(std::string propName, std::string dtPath)
	{
		std::string rootClass, propLine;
		int classDot = dtPath.find('.');
		rootClass  = dtPath.substr(0, classDot);
		if (classDot != -1)
			propLine = dtPath.substr(classDot + 1);

		auto netclass = m_vRootClasses.find(rootClass);
		if (netclass == m_vRootClasses.end())
			throw NetVarException(_(CLASSNOTFO, "Class not found"), rootClass);

		NetClass::Ptr netClassData = netclass->second;

		while (!propLine.empty())
		{
			std::string dtName;
			int propDelim =  propLine.find('.');
			if (propDelim != -1)
			{
				dtName.assign(propLine.substr(0, propDelim));
				propLine.assign(propLine.substr(propDelim+1));
			}
			else
			{
				dtName.assign(propLine);
				propLine = "";
			};
			netClassData = netClassData->FindSubTable(dtName);
		};
		
		auto netprop = netClassData->FindProp(propName);
		return netprop->m_iOffset;
	};

	int FindOffset(std::string fullname)
	{
		int offset = 0;

		std::string clsName, propLine;
		int classDot = fullname.find('.');
		clsName  = fullname.substr(0, classDot);
		propLine = fullname.substr(classDot + 1);

		auto netclass = m_vRootClasses.find(clsName);
		if (netclass == m_vRootClasses.end())
			throw NetVarException(_(CLASSNOTFO, "Class not found"), clsName);

		NetClass::Ptr netClassData = netclass->second;

		do		// TODO: somewhat messy, rewrite?
		{
			std::string propName;
			int propDelim =  propLine.find('.');
			if (propDelim != -1)
			{
				propName.assign(propLine.substr(0, propDelim));
				propLine.assign(propLine.substr(propDelim+1));
			}
			else
			{
				propName.assign(propLine);
				propLine = "";
			};
			auto netprop = netClassData->FindProp(propName);
			offset += netprop->m_iOffset;
			if (!propLine.empty())
				netClassData = netClassData->FindSubTable(netClassData->FindProp(propName)->m_sTableName);
		}
		while (!propLine.empty());
		return offset;
	};

protected:
	void CollectClasses()
	{
		ClientClass* node = INTERFACES.GetBaseClient()->GetAllClasses();
		do 
			m_vRootClasses[node->GetName()] = NetClass::Ptr(new NetClass(node));
		while ((node = node->m_pNext) != nullptr);
	};

	NetvarManager()
	{
		CollectClasses();
	};

	std::map<const std::string, NetClass::Ptr> m_vRootClasses;
};

#define NETVARS NetvarManager::Instance()

/// Finally, the DynNetvar implementation.

DynNetvar::DynNetvar(const std::string& name) : m_iOffset(-1)
{
	try
	{
		m_iOffset = NETVARS.FindOffset(name);
	}
	catch(const NetVarException& e)
	{
		__log(e.what());
	};
	//__log("I am %s, off = 0x%X\n", name.c_str(), m_iOffset);
};

DynNetvar::DynNetvar(const std::string& propName, const std::string& propDTPath) : m_iOffset(-1)
{
	try
	{
		m_iOffset = NETVARS.FindOffset(propName, propDTPath);
	}
	catch(const NetVarException& e)
	{
		__log(e.what());
	};
	//__log("I am %s of %s, off = 0x%X\n", propName.c_str(), propDTPath.c_str(), m_iOffset);
};

int DynNetvar::Offset() const
{
	assert(m_iOffset != -1);
	return m_iOffset;
};
