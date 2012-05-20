#include "stdafx.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/assign.hpp>
#include <algorithm>

#include <iostream>

struct WeaponDesc
{
	WeaponDesc(const boost::property_tree::ptree& node)
	{
		memset(this, 0, sizeof(WeaponDesc));
		m_iDefId = node.get<int>("defindex");
		m_sClass = node.get("item_class", "<unknown>");
		m_sName = node.get("item_name", "<unknown>");
		m_bCanCrit = true;
		try
		{
			BOOST_FOREACH(const boost::property_tree::ptree::value_type& attrib, node.get_child("attributes"))
			{
				if ((attrib.second.get("class", "") == "mult_crit_chance") &&
					(attrib.second.get<float>("value") == 0))
				{
					m_bCanCrit = false;
					break;
				}
			}
		}
		catch(const boost::property_tree::ptree_error& e)
		{
			// no attributes, ignoring
		};
	};

	void Log() const
	{
		std::cout << m_iDefId << ": " << m_bCanCrit << " | " << m_sName << "(" << m_sClass << ")" << std::endl;
	};

	int			m_iDefId;
	bool		m_bCanCrit;
	std::string m_sName;
	std::string m_sClass;
};

class SchemaParser
{
public:
	typedef std::vector<WeaponDesc> WeaponList;
	SchemaParser(const std::string& fname) : out(std::cout.rdbuf())
	{
		static std::vector<const char*> weaponTypes = boost::assign::list_of("melee")("primary")("secondary")("pda")("pda2");
		boost::property_tree::ptree pt;
		boost::property_tree::read_xml(fname, pt); // can throw
		int wtf = pt.get("result.status", 0);
		BOOST_FOREACH(const boost::property_tree::ptree::value_type &v,
			pt.get_child("result.items"))
		{
			auto node = v.second;
			if (std::find(weaponTypes.begin(), weaponTypes.end(), node.get("item_slot", "")) == weaponTypes.end())
				continue;

			auto weapInfo = WeaponDesc(node);
			allWeapons.push_back(weapInfo);
			weaponClasses.insert(weapInfo.m_sClass);
		}
	};

	template<typename _T>
	void SetOutStream(_T strbuf)
	{
		out.rdbuf(strbuf);
	};

	template<typename _Pr>
	WeaponList GetFiltered(_Pr _Pred)
	{
		WeaponList ret;
		std::copy_if(allWeapons.begin(), allWeapons.end(), std::back_inserter(ret), _Pred);
		return ret;
	};

	void DumpList(const WeaponList& list, const std::string& comment = "")
	{
		if (comment.length())
			out << comment << std::endl;
		BOOST_FOREACH(const WeaponDesc &weap, list)
			weap.Log();
		out << std::endl;\
	};

	void DumpCppList(const WeaponList& list, const std::string& name)
	{
		out << "const WeaponSet " << name << " = boost::assign::list_of";
		BOOST_FOREACH(const WeaponDesc &weap, list)
			out << "(" << weap.m_iDefId << ")";
		out << ";" << std::endl;
	};

	void DumpCppMap(const WeaponList& list, const std::string& name)
	{
		out << "const WeaponNameMap " << name << " = boost::assign::map_list_of\\" << std::endl;
		BOOST_FOREACH(const WeaponDesc &weap, list)
			out << "\t(" << weap.m_iDefId << ", \"" << weap.m_sName << "\")\\" << std::endl;
		out << ";" << std::endl;
	};

	void DumpClasses()
	{
		out << "Weapon class dump: " << std::endl;
		BOOST_FOREACH(const std::string &str, weaponClasses)
			out << str << std::endl;
	};

protected:
	WeaponList allWeapons;
	std::set<std::string> weaponClasses;
	std::ostream out;
};

int _tmain(int argc, _TCHAR* argv[])
{
	auto knifeFilter = [](const WeaponDesc& info){return (!info.m_sClass.compare("tf_weapon_knife"));};
	auto medigunFilter = [](const WeaponDesc& info){return (!info.m_sClass.compare("tf_weapon_medigun"));};
	auto rifleFilter = [](const WeaponDesc& info){return (!info.m_sClass.find("tf_weapon_sniperrifle") && info.m_bCanCrit);};

	auto hitscanFilter = [](const WeaponDesc& info) -> bool
		{
			static std::vector<const char*> hitscanWeaps = boost::assign::list_of\
				("tf_weapon_handgun_scout_primary")
				("tf_weapon_handgun_scout_secondary")
				("tf_weapon_minigun")
				("tf_weapon_pistol")
				("tf_weapon_revolver")
				("tf_weapon_scattergun")
				("tf_weapon_shotgun")
				("tf_weapon_shotgun_primary")
				("tf_weapon_smg")
				("tf_weapon_soda_popper");
			return (std::find(hitscanWeaps.begin(), hitscanWeaps.end(), info.m_sClass) != hitscanWeaps.end());
		};

	auto nullfilter = [](const WeaponDesc& info){return true; };

	SchemaParser parser("sch.xml");

	std::ofstream outf("tf2_weapon_info.cpp");
	outf << "#include \"tf2_weapon_info.h\"" << std::endl;
	parser.SetOutStream(outf.rdbuf());

	parser.DumpCppList(parser.GetFiltered(knifeFilter), "SpyMeelee");
	parser.DumpCppList(parser.GetFiltered(medigunFilter), "HealingWeapon");
	parser.DumpCppList(parser.GetFiltered(rifleFilter), "HsWeapon");
	parser.DumpCppList(parser.GetFiltered(hitscanFilter), "HitscanWeapon");
	parser.DumpCppMap(parser.GetFiltered(nullfilter), "weaponNames");

	//parser.DumpList(parser.GetFiltered(knifeFilter), "Knives!");

	//int  i;
	//std::cin >> i;
	return 0;
}

