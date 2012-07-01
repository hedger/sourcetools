#include "stdafx.h"

#include <algorithm>
#include <regex>

#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/assign.hpp>
#include <boost/lexical_cast.hpp>

struct WeaponDesc
{
  WeaponDesc(const boost::property_tree::ptree& node)
  {
    memset(this, 0, sizeof(WeaponDesc));
    m_iDefId = node.get<int>("defindex");
    m_sClass = node.get("item_class", "<unknown>");
    m_sName  = node.get("item_name", "<unknown>");
    try
    {
      auto wpnProps = node.get_child("attributes");
      m_bCanCrit = ( std::find_if(wpnProps.begin(), wpnProps.end(), [](const boost::property_tree::ptree::value_type& attrib)
        { 
          return ((attrib.second.get("class", "") == "mult_crit_chance") &&
            (attrib.second.get<float>("value") == 0));
        } ) == wpnProps.end() );
    }
   catch(const boost::property_tree::ptree_error& e)
    {
      m_bCanCrit = true;
   };
   UpdateFilteredName(m_sName);
  };

  void Log() const
  {
    std::cout << m_iDefId << ": " << m_bCanCrit << " | " << m_sName \
      << "(" << m_sClass << ")" << std::endl;
  };

  void UpdateFilteredName(const std::string& refname)
  {
    std::string temp;
    temp.assign(refname); // workaround for strange "incompatible iterator" error
    m_sFilteredName = "WPN_" + std::regex_replace(temp, nameFilterRE, std::string(""));
  }

  static std::regex nameFilterRE;
  int      m_iDefId;
  bool     m_bCanCrit;
  std::string m_sName, m_sFilteredName, m_sClass;
};

std::regex WeaponDesc::nameFilterRE = std::regex("\\W");

class SchemaParser
{
public:
  typedef std::vector<WeaponDesc> WeaponList;

  SchemaParser()
    : codeStream(std::cout.rdbuf()), headerStream(std::cout.rdbuf())
  { };

  void Process(const std::string& fname)
  {
    allWeapons.clear();
    nameHits.clear();

    static std::vector<const char*> weaponTypes = \
      boost::assign::list_of("melee")("primary")("secondary")("pda")("pda2");
    boost::property_tree::ptree pt;
    boost::property_tree::read_xml(fname, pt); // can throw
    int wtf = pt.get("result.status", 0);
    BOOST_FOREACH(const boost::property_tree::ptree::value_type &v,
      pt.get_child("result.items"))
    {
      auto node = v.second;
      if (std::find(weaponTypes.begin(), weaponTypes.end(), node.get("item_slot", "")) == \
        weaponTypes.end())
          continue;

      auto weapInfo = WeaponDesc(node);
      int& hits = nameHits[weapInfo.m_sName]; // numer of weapons with the same name
      if (hits++)
        weapInfo.UpdateFilteredName(weapInfo.m_sName + boost::lexical_cast<std::string>(hits));
      
      allWeapons.push_back(weapInfo);
      weaponClasses.insert(weapInfo.m_sClass);
    }
  }

  template<typename _T>
  void SetOutStreams(_T _headerStream, _T _codeStream)
  {
    headerStream.rdbuf(_headerStream);
    codeStream.rdbuf(_codeStream);
  };

  template<typename _Pr>
  WeaponList GetFiltered(_Pr _Pred)
  {
    WeaponList ret;
    std::copy_if(allWeapons.begin(), allWeapons.end(), std::back_inserter(ret), _Pred);
    return ret;
  };

  void DumpCppList(const WeaponList& list, const std::string& name)
  {
    headerStream << "extern const WeaponSet " << name << ";" << std::endl;
    codeStream << "const WeaponSet " << name << " = boost::assign::list_of";
    BOOST_FOREACH(const WeaponDesc &weap, list)
      codeStream << "(" << weap.m_sFilteredName << ")";
    codeStream << ";" << std::endl;
  };

  void DumpCppMap(const WeaponList& list, const std::string& name)
  {
    headerStream << "enum TF2Weapons\n{" << std::endl;
    BOOST_FOREACH(const WeaponDesc &weap, list)
      headerStream << "\t" << weap.m_sFilteredName << " = " << weap.m_iDefId << "," << std::endl;
    headerStream << "};" << std::endl;

    headerStream << "extern const WeaponNameMap " << name << ";" << std::endl;
    codeStream << "const WeaponNameMap " << name << " = boost::assign::map_list_of\\" << std::endl;
    BOOST_FOREACH(const WeaponDesc &weap, list)
      codeStream << "\t(" << weap.m_sFilteredName << ", \"" << weap.m_sName << "\")\\" << std::endl;
    codeStream << ";" << std::endl;
  };

  void DumpClasses()
  {
    codeStream << "//Weapon class dump: /*" << std::endl;
    BOOST_FOREACH(const std::string &str, weaponClasses)
      codeStream << str << std::endl;
    codeStream << "*/" << std::endl;
  };

protected:
  WeaponList            allWeapons;
  std::set<std::string> weaponClasses;
  std::map<std::string, int> nameHits; // yeah, a rocketfast string key
  std::ostream          codeStream, headerStream;
};

int _tmain(int argc, _TCHAR* argv[])
{
  auto knifeFilter = 
    [](const WeaponDesc& info){return (!info.m_sClass.compare("tf_weapon_knife"));};
  auto medigunFilter = 
    [](const WeaponDesc& info){return (!info.m_sClass.compare("tf_weapon_medigun"));};
  auto flamethrowerFilter = 
    [](const WeaponDesc& info){return (!info.m_sClass.compare("tf_weapon_flamethrower"));};
  auto rifleFilter = 
    [](const WeaponDesc& info){return (!info.m_sClass.find("tf_weapon_sniperrifle") && info.m_bCanCrit);};

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

  // http://api.steampowered.com/IEconItems_440/GetSchema/v0001/?key=YOUR_KEY&format=xml&language=en
  SchemaParser parser;
  parser.Process("sch.xml");

  std::ofstream outCode("tf2_weapon_info.cpp"), outHeader("tf2_weapon_info.h");

  outCode << "#include \"tf2_weapon_info.h\"\n#include <boost/assign.hpp>" << std::endl;

  outHeader << "#pragma once\n\
#include <map>\n\
#include <set>\n\
typedef std::set<int> WeaponSet;\n\
typedef std::map<int, const char*> WeaponNameMap;" << std::endl;

  parser.SetOutStreams(outHeader.rdbuf(), outCode.rdbuf());

  parser.DumpCppList(parser.GetFiltered(knifeFilter), "SpyMelee");
  parser.DumpCppList(parser.GetFiltered(medigunFilter), "HealingWeapon");
  parser.DumpCppList(parser.GetFiltered(flamethrowerFilter), "FlamethrowerWeapon");
  parser.DumpCppList(parser.GetFiltered(rifleFilter), "HsWeapon");
  parser.DumpCppList(parser.GetFiltered(hitscanFilter), "HitscanWeapon");

  parser.DumpCppMap(parser.GetFiltered(nullfilter), "weaponNames");
  //parser.DumpList(parser.GetFiltered(knifeFilter), "Knives!");

  return 0;
}

