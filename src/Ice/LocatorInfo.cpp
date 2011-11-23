// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Ice/LocatorInfo.h>
#include <Ice/Locator.h>
#include <Ice/LocalException.h>
#include <Ice/Instance.h>
#include <Ice/TraceLevels.h>
#include <Ice/LoggerUtil.h>
#include <Ice/Endpoint.h>
#include <Ice/Reference.h>
#include <Ice/Functional.h>
#include <Ice/IdentityUtil.h>

#include <iterator>

using namespace std;
using namespace Ice;
using namespace IceInternal;

void IceInternal::incRef(LocatorManager* p) { p->__incRef(); }
void IceInternal::decRef(LocatorManager* p) { p->__decRef(); }

void IceInternal::incRef(LocatorInfo* p) { p->__incRef(); }
void IceInternal::decRef(LocatorInfo* p) { p->__decRef(); }

void IceInternal::incRef(LocatorTable* p) { p->__incRef(); }
void IceInternal::decRef(LocatorTable* p) { p->__decRef(); }

IceInternal::LocatorManager::LocatorManager() :
    _tableHint(_table.end())
{
}

void
IceInternal::LocatorManager::destroy()
{
    IceUtil::Mutex::Lock sync(*this);

    for_each(_table.begin(), _table.end(), Ice::secondVoidMemFun<const LocatorPrx, LocatorInfo>(&LocatorInfo::destroy));

    _table.clear();
    _tableHint = _table.end();

    _locatorTables.clear();
}

LocatorInfoPtr
IceInternal::LocatorManager::get(const LocatorPrx& loc)
{
    if(!loc)
    {
	return 0;
    }

    LocatorPrx locator = LocatorPrx::uncheckedCast(loc->ice_locator(0)); // The locator can't be located.

    //
    // TODO: reap unused locator info objects?
    //

    IceUtil::Mutex::Lock sync(*this);

    map<LocatorPrx, LocatorInfoPtr>::iterator p = _table.end();
    
    if(_tableHint != _table.end())
    {
	if(_tableHint->first == locator)
	{
	    p = _tableHint;
	}
    }
    
    if(p == _table.end())
    {
	p = _table.find(locator);
    }

    if(p == _table.end())
    {
	//
	// Rely on locator identity for the adapter table. We want to
	// have only one table per locator (not one per locator
	// proxy).
	//
	map<Identity, LocatorTablePtr>::iterator t = _locatorTables.find(locator->ice_getIdentity());
	if(t == _locatorTables.end())
	{
	    t = _locatorTables.insert(_locatorTables.begin(),
				      pair<const Identity, LocatorTablePtr>(locator->ice_getIdentity(), new LocatorTable()));
	}

	_tableHint = _table.insert(_tableHint, pair<const LocatorPrx, LocatorInfoPtr>(locator, new LocatorInfo(locator, t->second)));
    }
    else
    {
	_tableHint = p;
    }

    return _tableHint->second;
}

IceInternal::LocatorTable::LocatorTable()
{
}

void
IceInternal::LocatorTable::clear()
{
     IceUtil::Mutex::Lock sync(*this);

     _adapterEndpointsMap.clear();
     _objectMap.clear();
}

bool
IceInternal::LocatorTable::getAdapterEndpoints(const string& adapter, vector<EndpointPtr>& endpoints) const
{
    IceUtil::Mutex::Lock sync(*this);
    
    map<string, vector<EndpointPtr> >::const_iterator p = _adapterEndpointsMap.find(adapter);
    
    if(p != _adapterEndpointsMap.end())
    {
	endpoints = p->second;
	return true;
    }
    else
    {
	return false;
    }
}

void
IceInternal::LocatorTable::addAdapterEndpoints(const string& adapter, const vector<EndpointPtr>& endpoints)
{
    IceUtil::Mutex::Lock sync(*this);
    
    _adapterEndpointsMap.insert(make_pair(adapter, endpoints));
}

vector<EndpointPtr>
IceInternal::LocatorTable::removeAdapterEndpoints(const string& adapter)
{
    IceUtil::Mutex::Lock sync(*this);
    
    map<string, vector<EndpointPtr> >::iterator p = _adapterEndpointsMap.find(adapter);
    if(p == _adapterEndpointsMap.end())
    {
	return vector<EndpointPtr>();
    }

    vector<EndpointPtr> endpoints = p->second;

    _adapterEndpointsMap.erase(p);
    
    return endpoints;
}

bool 
IceInternal::LocatorTable::getProxy(const Identity& id, ObjectPrx& proxy) const
{
    IceUtil::Mutex::Lock sync(*this);
    
    map<Identity, ObjectPrx>::const_iterator p = _objectMap.find(id);
    
    if(p != _objectMap.end())
    {
	proxy = p->second;
	return true;
    }
    else
    {
	return false;
    }
}

void 
IceInternal::LocatorTable::addProxy(const Identity& id, const ObjectPrx& proxy)
{
    IceUtil::Mutex::Lock sync(*this);
    
    _objectMap.insert(make_pair(id, proxy));
}

ObjectPrx 
IceInternal::LocatorTable::removeProxy(const Identity& id)
{
    IceUtil::Mutex::Lock sync(*this);
    
    map<Identity, ObjectPrx>::iterator p = _objectMap.find(id);
    if(p == _objectMap.end())
    {
	return 0;
    }

    ObjectPrx proxy = p->second;

    _objectMap.erase(p);
    
    return proxy;
}

IceInternal::LocatorInfo::LocatorInfo(const LocatorPrx& locator, const LocatorTablePtr& table) :
    _locator(locator),
    _table(table)
{
    assert(_locator);
    assert(_table);
}

void
IceInternal::LocatorInfo::destroy()
{
    IceUtil::Mutex::Lock sync(*this);

    _locatorRegistry = 0;
    _table->clear();
}

bool
IceInternal::LocatorInfo::operator==(const LocatorInfo& rhs) const
{
    return _locator == rhs._locator;
}

bool
IceInternal::LocatorInfo::operator!=(const LocatorInfo& rhs) const
{
    return _locator != rhs._locator;
}

bool
IceInternal::LocatorInfo::operator<(const LocatorInfo& rhs) const
{
    return _locator < rhs._locator;
}

LocatorPrx
IceInternal::LocatorInfo::getLocator() const
{
    //
    // No mutex lock necessary, _locator is immutable.
    //
    return _locator;
}

LocatorRegistryPrx
IceInternal::LocatorInfo::getLocatorRegistry()
{
    IceUtil::Mutex::Lock sync(*this);
    
    if(!_locatorRegistry) // Lazy initialization.
    {
	_locatorRegistry = _locator->getRegistry();

	//
	// The locator registry can't be located.
	//
	_locatorRegistry = LocatorRegistryPrx::uncheckedCast(_locatorRegistry->ice_locator(0));
    }
    
    return _locatorRegistry;
}

vector<EndpointPtr>
IceInternal::LocatorInfo::getEndpoints(const IndirectReferencePtr& ref, bool& cached)
{
    vector<EndpointPtr> endpoints;
    ObjectPrx object;
    cached = true;    

    try
    {
	if(!ref->getAdapterId().empty())
	{
	    if(!_table->getAdapterEndpoints(ref->getAdapterId(), endpoints))
	    {
		cached = false;
	    
		object = _locator->findAdapterById(ref->getAdapterId());
		if(object)
		{
		    endpoints = object->__reference()->getEndpoints();
		    _table->addAdapterEndpoints(ref->getAdapterId(), endpoints);
		}
	    }
	}
	else
	{
	    if(!_table->getProxy(ref->getIdentity(), object))
	    {
		cached = false;
		
		object = _locator->findObjectById(ref->getIdentity());
	    }

	    if(object)
	    {
		DirectReferencePtr odr = DirectReferencePtr::dynamicCast(object->__reference());
		if(odr)
		{
		    endpoints = odr->getEndpoints();
		}
		else
		{
		    IndirectReferencePtr oir = IndirectReferencePtr::dynamicCast(object->__reference());
		    assert(oir);
		    if(!oir->getAdapterId().empty())
		    {
			endpoints = getEndpoints(oir, cached);
		    }
		}
	    }

	    if(!cached && !endpoints.empty())
	    {
		_table->addProxy(ref->getIdentity(), object);
	    }
	}
    }
    catch(const AdapterNotFoundException&)
    {
	NotRegisteredException ex(__FILE__, __LINE__);
	ex.kindOfObject = "object adapter";
	ex.id = ref->getAdapterId();
	throw ex;
    }
    catch(const ObjectNotFoundException&)
    {
	NotRegisteredException ex(__FILE__, __LINE__);
	ex.kindOfObject = "object";
	ex.id = identityToString(ref->getIdentity());
	throw ex;
    }
    catch(const NotRegisteredException&)
    {
	throw;
    }
    catch(const LocalException& ex)
    {
	if(ref->getInstance()->traceLevels()->location >= 1)
	{
	    Trace out(ref->getInstance()->logger(), ref->getInstance()->traceLevels()->locationCat);
	    out << "couldn't contact the locator to retrieve adapter endpoints\n";
	    if(!ref)
	    {
		out << "object = " << identityToString(ref->getIdentity()) << "\n";
	    }
	    else
	    {
		out << "adapter = " << ref->getAdapterId() << "\n";
	    }
	    out << "reason = " << ex;
	}
	throw;
    }
    
    if(ref->getInstance()->traceLevels()->location >= 1 && !endpoints.empty())
    {
	if(cached)
	{
	    trace("found endpoints in locator table", ref, endpoints);
	}
	else
	{
	    trace("retrieved endpoints from locator, adding to locator table", ref, endpoints);
	}
    }

    return endpoints;
}

void
IceInternal::LocatorInfo::clearObjectCache(const IndirectReferencePtr& ref)
{
    if(ref->getAdapterId().empty())
    {
	ObjectPrx object = _table->removeProxy(ref->getIdentity());

	if(ref->getInstance()->traceLevels()->location >= 2 && object)
	{
	    vector<EndpointPtr> endpoints = object->__reference()->getEndpoints();
	    if(!endpoints.empty())
	    {
		trace("removed endpoints from locator table", ref, endpoints);
	    }
	}
    }
}

void 
IceInternal::LocatorInfo::clearCache(const IndirectReferencePtr& ref)
{
    if(!ref->getAdapterId().empty())
    {
	vector<EndpointPtr> endpoints = _table->removeAdapterEndpoints(ref->getAdapterId());

	if(!endpoints.empty() && ref->getInstance()->traceLevels()->location >= 2)
	{
	    trace("removed endpoints from locator table", ref, endpoints);
	}
    }
    else
    {
	ObjectPrx object = _table->removeProxy(ref->getIdentity());
	if(object)
	{
	    IndirectReferencePtr oir = IndirectReferencePtr::dynamicCast(object->__reference());
	    if(oir)
	    {
	        if(!oir->getAdapterId().empty())
		{
		    IndirectReferencePtr ir = IndirectReferencePtr::dynamicCast(object->__reference());
		    if(ir)
		    {
			clearCache(ir);
		    }
		}
	    }
	    else
	    {
		if(ref->getInstance()->traceLevels()->location >= 2)
		{
		    trace("removed endpoints from locator table", ref, object->__reference()->getEndpoints());
		}
	    }
	}
    }
}

void
IceInternal::LocatorInfo::trace(const string& msg, const ReferencePtr& ref, const vector<EndpointPtr>& endpoints)
{
    Trace out(ref->getInstance()->logger(), ref->getInstance()->traceLevels()->locationCat);
    out << msg << '\n';
    IndirectReferencePtr ir = IndirectReferencePtr::dynamicCast(ref);
    if(!ir)
    {
	out << "object = "  << identityToString(ref->getIdentity()) << '\n';
    }
    else
    {
	out << "adapter = "  << ir->getAdapterId() << '\n';
    }

    const char* sep = endpoints.size() > 1 ? ":" : "";
    ostringstream o;
    transform(endpoints.begin(), endpoints.end(), ostream_iterator<string>(o, sep),
	      Ice::constMemFun(&Endpoint::toString));
    out << "endpoints = " << o.str();
}
